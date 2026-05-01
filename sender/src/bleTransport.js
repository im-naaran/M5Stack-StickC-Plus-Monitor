// 本文件封装 BLE GATT client，负责扫描、连接 M5 设备、写入指标特征值和关闭连接。
const CONFIG = require('../config');

function normalizeUuid(uuid) {
  return String(uuid || '').replace(/-/g, '').toLowerCase();
}

function normalizeId(value) {
  return String(value || '').replace(/:/g, '').toLowerCase();
}

function loadNoble() {
  try {
    return require('@abandonware/noble');
  } catch (error) {
    const message = error && error.message ? error.message : String(error);

    if (error && error.code === 'MODULE_NOT_FOUND') {
      throw new Error(
        'BLE transport requires @abandonware/noble. Run "pnpm install" in sender/ before using --transport ble.',
      );
    }

    if (/No native build was found/i.test(message)) {
      throw new Error(
        [
          `BLE transport native module is not available for Node ${process.versions.node} ABI ${process.versions.modules}.`,
          'Reinstall sender dependencies with the current Node version: pnpm install.',
          'USB serial mode can continue to run on the current Node version.',
        ].join(' '),
      );
    }

    throw error;
  }
}

async function openBleTransport(options = {}) {
  const transport = createReconnectableBleTransport(options);
  await transport.open();
  return transport;
}

function createReconnectableBleTransport(options = {}) {
  const noble = loadNoble();
  const runtimeOptions = normalizeOptions(options);
  let peripheral = null;
  let characteristic = null;
  let opening = null;
  let closed = false;
  let currentLabel = '';

  async function open() {
    if (characteristic && peripheral && peripheral.state === 'connected') {
      return;
    }

    if (opening) {
      await opening;
      return;
    }

    opening = openFreshBleConnection(noble, runtimeOptions)
      .then((opened) => {
        peripheral = opened.peripheral;
        characteristic = opened.characteristic;
        currentLabel = opened.label;
        peripheral.once('disconnect', () => {
          characteristic = null;
          peripheral = null;
        });
      })
      .finally(() => {
        opening = null;
      });

    await opening;
  }

  return {
    get path() {
      return currentLabel;
    },

    open,

    write: async (line) => {
      if (closed) {
        throw new Error('BLE transport is closed');
      }

      await open();

      const activeCharacteristic = characteristic;

      try {
        await activeCharacteristic.writeAsync(Buffer.from(line, 'utf8'), true);
      } catch (error) {
        characteristic = null;
        peripheral = null;
        throw error;
      }
    },

    close: async () => {
      closed = true;
      stopScanning(noble);

      const activePeripheral = peripheral;
      characteristic = null;
      peripheral = null;

      if (!activePeripheral || activePeripheral.state !== 'connected') {
        return;
      }

      await activePeripheral.disconnectAsync();
    },
  };
}

async function listBleDevices(options = {}) {
  const noble = loadNoble();
  const runtimeOptions = normalizeOptions(options);
  const devices = await scanForPeripherals(noble, {
    ...runtimeOptions,
    allowMultiple: true,
  });

  return devices.map(formatPeripheral);
}

async function openFreshBleConnection(noble, options) {
  let lastError = null;

  for (let attempt = 1; attempt <= options.discoveryRetries; attempt++) {
    const peripheral = await selectPeripheral(noble, options);

    try {
      await peripheral.connectAsync();
      if (options.connectDelayMs > 0) {
        await delay(options.connectDelayMs);
      }

      const characteristic = await discoverMetricsCharacteristic(peripheral, options);

      return {
        peripheral,
        characteristic,
        label: describePeripheral(peripheral),
      };
    } catch (error) {
      lastError = error;
      await peripheral.disconnectAsync().catch(() => {});

      if (attempt < options.discoveryRetries) {
        await delay(options.discoveryRetryDelayMs);
      }
    }
  }

  throw lastError || new Error('BLE connection failed.');
}

async function discoverMetricsCharacteristic(peripheral, options) {
  const discovered = await withTimeout(
    peripheral.discoverSomeServicesAndCharacteristicsAsync(
      [options.serviceUuid],
      [options.characteristicUuid],
    ),
    options.discoveryTimeoutMs,
    [
      'BLE GATT discovery timed out.',
      'Try power-cycling the M5 device, toggling Bluetooth off/on, or increasing --ble-connect-delay.',
    ].join(' '),
  );

  const characteristic = discovered.characteristics[0];
  if (!characteristic) {
    throw new Error('BLE metrics characteristic not found on selected device.');
  }

  return characteristic;
}

async function selectPeripheral(noble, options) {
  const devices = await scanForPeripherals(noble, options);

  if (devices.length === 0) {
    throw new Error('No BLE M5 monitor found. Make sure the device is powered on and advertising.');
  }

  if (options.deviceId || options.deviceName) {
    return devices[0];
  }

  if (devices.length === 1) {
    return devices[0];
  }

  const labels = devices.map(describePeripheral).join(', ');
  throw new Error(`Multiple BLE M5 monitors found: ${labels}. Specify --ble-id or --ble-name.`);
}

async function scanForPeripherals(noble, options) {
  await waitForPoweredOn(noble, options.scanTimeoutMs);

  const discovered = new Map();
  const matching = [];

  function onDiscover(peripheral) {
    if (!matchesPeripheral(peripheral, options)) {
      return;
    }

    if (!discovered.has(peripheral.id)) {
      discovered.set(peripheral.id, peripheral);
      matching.push(peripheral);
    }
  }

  noble.on('discover', onDiscover);
  stopScanning(noble);
  await noble.startScanningAsync([options.serviceUuid], false);

  try {
    await delay(options.scanTimeoutMs);
  } finally {
    noble.removeListener('discover', onDiscover);
    stopScanning(noble);
  }

  return matching;
}

async function waitForPoweredOn(noble, timeoutMs) {
  if (noble.state === 'poweredOn') {
    return;
  }

  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      cleanup();
      reject(new Error(`Bluetooth adapter is not powered on. Current state: ${noble.state}`));
    }, timeoutMs);

    function cleanup() {
      clearTimeout(timer);
      noble.removeListener('stateChange', onStateChange);
    }

    function onStateChange(state) {
      if (state === 'poweredOn') {
        cleanup();
        resolve();
        return;
      }

      if (state === 'unsupported' || state === 'unauthorized' || state === 'poweredOff') {
        cleanup();
        reject(new Error(`Bluetooth adapter is ${state}.`));
      }
    }

    noble.on('stateChange', onStateChange);
  });
}

function stopScanning(noble) {
  try {
    noble.stopScanning();
  } catch (_) {}
}

function normalizeOptions(options) {
  return {
    serviceUuid: normalizeUuid(options.serviceUuid || CONFIG.bleServiceUuid),
    characteristicUuid: normalizeUuid(
      options.characteristicUuid || CONFIG.bleMetricsCharacteristicUuid,
    ),
    deviceName: String(options.deviceName || '').trim(),
    deviceId: normalizeId(options.deviceId),
    scanTimeoutMs: Number(options.scanTimeoutMs) || CONFIG.bleScanTimeoutMs,
    connectDelayMs: Number.isInteger(Number(options.connectDelayMs))
      ? Number(options.connectDelayMs)
      : CONFIG.bleConnectDelayMs,
    discoveryTimeoutMs: Number(options.discoveryTimeoutMs) || CONFIG.bleDiscoveryTimeoutMs,
    discoveryRetries: Number(options.discoveryRetries) || CONFIG.bleDiscoveryRetries,
    discoveryRetryDelayMs: Number(options.discoveryRetryDelayMs) || CONFIG.bleDiscoveryRetryDelayMs,
  };
}

function matchesPeripheral(peripheral, options) {
  if (options.deviceId && !peripheralIds(peripheral).includes(options.deviceId)) {
    return false;
  }

  if (options.deviceName) {
    const localName = String(peripheral.advertisement && peripheral.advertisement.localName || '');
    if (localName !== options.deviceName) {
      return false;
    }
  }

  return true;
}

function peripheralIds(peripheral) {
  return [peripheral.id, peripheral.uuid, peripheral.address].map(normalizeId).filter(Boolean);
}

function formatPeripheral(peripheral) {
  return {
    id: peripheral.id,
    address: peripheral.address,
    name: peripheral.advertisement && peripheral.advertisement.localName || '',
    rssi: peripheral.rssi,
  };
}

function describePeripheral(peripheral) {
  const formatted = formatPeripheral(peripheral);
  const name = formatted.name || 'unnamed';
  const id = formatted.address && formatted.address !== 'unknown'
    ? formatted.address
    : formatted.id;

  return `${name} (${id}, rssi=${formatted.rssi})`;
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function withTimeout(promise, timeoutMs, message) {
  let timer = null;

  const timeout = new Promise((_, reject) => {
    timer = setTimeout(() => reject(new Error(message)), timeoutMs);
  });

  return Promise.race([promise, timeout]).finally(() => {
    if (timer) {
      clearTimeout(timer);
    }
  });
}

module.exports = {
  createReconnectableBleTransport,
  listBleDevices,
  openBleTransport,
  normalizeUuid,
};
