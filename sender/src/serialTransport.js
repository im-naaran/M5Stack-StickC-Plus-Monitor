// 本文件封装 serialport，负责串口枚举、选择、打开、写入和关闭。
const { SerialPort } = require('serialport');

// 返回当前系统可见的串口列表。
async function listPorts() {
  return SerialPort.list();
}

// 根据用户指定路径或自动识别规则选择一个串口路径。
function selectPort(ports, preferredPath) {
  if (!Array.isArray(ports) || ports.length === 0) {
    throw new Error('No serial ports found. Connect M5StickC Plus and try again.');
  }

  if (preferredPath) {
    const matched = ports.find((port) => port.path === preferredPath);

    if (!matched) {
      throw new Error(
        `Serial port not found: ${preferredPath}. Run "npm run list-ports" to see available ports.`,
      );
    }

    return matched.path;
  }

  const candidates = ports.filter(isLikelyM5Port);

  if (candidates.length === 1) {
    return candidates[0].path;
  }

  if (candidates.length > 1) {
    const bestCandidate = selectBestPortCandidate(candidates);

    if (bestCandidate) {
      return bestCandidate.path;
    }

    const paths = candidates.map((port) => port.path).join(', ');
    throw new Error(
      `Multiple likely M5 serial ports found: ${paths}. Specify one with --port <path>.`,
    );
  }

  throw new Error(
    'Could not auto-select a serial port. Run "npm run list-ports" and specify --port <path>.',
  );
}

// 打开串口并返回项目内部使用的 transport 对象。
async function openSerialTransport(options) {
  const transport = createReconnectableSerialTransport({
    preferredPath: options.path || options.port,
    baudRate: Number(options.baudRate),
  });

  await transport.open();
  return transport;
}

// 创建可重连串口 transport。USB 断开后，后续 write 会重新枚举并打开端口。
function createReconnectableSerialTransport(options) {
  let serialPort = null;
  let currentPath = '';
  let opening = null;

  async function open() {
    if (serialPort && serialPort.isOpen) {
      return;
    }

    if (opening) {
      await opening;
      return;
    }

    opening = openFreshSerialPort(options)
      .then((opened) => {
        serialPort = opened.serialPort;
        currentPath = opened.path;
      })
      .finally(() => {
        opening = null;
      });

    await opening;
  }

  function markClosed(port) {
    if (serialPort === port) {
      serialPort = null;
    }
  }

  return {
    get path() {
      return currentPath;
    },

    open,

    // 写入一行 UTF-8 文本并等待串口缓冲区 drain 完成。
    write: async (line) => {
      await open();

      const activePort = serialPort;

      try {
        await new Promise((resolve, reject) => {
          activePort.write(line, 'utf8', (writeError) => {
            if (writeError) {
              reject(writeError);
              return;
            }

            activePort.drain((drainError) => {
              if (drainError) {
                reject(drainError);
                return;
              }

              resolve();
            });
          });
        });
      } catch (error) {
        markClosed(activePort);
        throw error;
      }
    },

    // 关闭已打开的串口，未打开时直接完成。
    close: async () => {
      const activePort = serialPort;
      serialPort = null;

      if (!activePort || !activePort.isOpen) {
        return;
      }

      await new Promise((resolve, reject) => {
        activePort.close((error) => {
          if (error) {
            reject(error);
            return;
          }

          resolve();
        });
      });
    },
  };
}

// 枚举并打开一个新的 SerialPort 实例。
async function openFreshSerialPort(options) {
  const ports = await listPorts();
  const path = selectPort(ports, options.preferredPath);
  const serialPort = new SerialPort({
    path,
    baudRate: Number(options.baudRate),
    autoOpen: false,
  });

  serialPort.on('error', () => {});

  await new Promise((resolve, reject) => {
    serialPort.open((error) => {
      if (error) {
        reject(createSerialOpenError(error, path));
        return;
      }

      resolve();
    });
  });

  return {
    path,
    serialPort,
  };
}

// 包装串口打开失败错误，补充常见占用场景和本机排查命令。
function createSerialOpenError(error, path) {
  const message = error && error.message ? error.message : String(error);

  if (/resource busy|busy|cannot open|access denied|permission/i.test(message)) {
    return new Error(
      [
        `Cannot open serial port ${path}: ${message}.`,
        'Close any app using this port, such as Arduino Serial Monitor, M5Burner, screen, minicom, or another sender process.',
        `On macOS you can run: lsof ${path}`,
      ].join(' '),
    );
  }

  return new Error(`Cannot open serial port ${path}: ${message}`);
}

// 多个候选端口同时出现时，优先选择 macOS 主动连接更适用的 /dev/tty.*。
function selectBestPortCandidate(candidates) {
  const cuCandidates = candidates.filter((port) => String(port.path || '').startsWith('/dev/tty.'));

  if (cuCandidates.length === 1) {
    return cuCandidates[0];
  }

  return null;
}

// 根据路径、厂商名和 USB vendorId 判断端口是否像 M5/USB 串口设备。
function isLikelyM5Port(port) {
  const manufacturer = String(port.manufacturer || '').toLowerCase();
  const path = String(port.path || '').toLowerCase();
  const vendorId = String(port.vendorId || '').toLowerCase();

  return (
    path.includes('/dev/tty.usbserial') ||
    path.includes('/dev/tty.usbmodem') ||
    manufacturer.includes('wch') ||
    manufacturer.includes('silicon labs') ||
    manufacturer.includes('ftdi') ||
    manufacturer.includes('m5stack') ||
    vendorId === '1a86' ||
    vendorId === '10c4' ||
    vendorId === '0403'
  );
}

module.exports = {
  createReconnectableSerialTransport,
  listPorts,
  selectPort,
  openSerialTransport,
};
