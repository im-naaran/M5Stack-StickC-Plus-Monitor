// 本文件负责把默认配置和 CLI 参数合并成最终运行配置，并完成基础校验。
const CONFIG = require('../config');

const CLI_CONFIG_KEYS = [
  'port',
  'baudRate',
  'intervalMs',
  'verbose',
  'transport',
  'bleName',
  'bleId',
  'bleScanTimeoutMs',
  'bleConnectDelayMs',
  'bleDiscoveryTimeoutMs',
  'bleDiscoveryRetries',
];

// 基于默认 CONFIG 和命令行参数生成最终运行配置。
function createRuntimeConfig(cliOptions = {}) {
  return validateConfig(mergeCliOptions(CONFIG, cliOptions));
}

// 将命令行参数覆盖到默认配置上，未指定的 CLI 参数保持 CONFIG 原值。
function mergeCliOptions(config, cliOptions) {
  const runtimeConfig = { ...config };

  for (const key of CLI_CONFIG_KEYS) {
    if (cliOptions[key] !== undefined) {
      runtimeConfig[key] = cliOptions[key];
    }
  }

  return runtimeConfig;
}

// 校验配置范围，并把数值/布尔/字符串字段转换为运行时使用的稳定类型。
function validateConfig(config) {
  const errors = [];
  const baudRate = Number(config.baudRate);
  const intervalMs = Number(config.intervalMs);
  const timezoneOffsetHours = Number(config.timezoneOffsetHours);
  const transport = String(config.transport || 'serial').trim().toLowerCase();
  const bleScanTimeoutMs = Number(config.bleScanTimeoutMs);
  const bleConnectDelayMs = Number(config.bleConnectDelayMs);
  const bleDiscoveryTimeoutMs = Number(config.bleDiscoveryTimeoutMs);
  const bleDiscoveryRetries = Number(config.bleDiscoveryRetries);

  if (!Number.isInteger(baudRate) || baudRate <= 0) {
    errors.push('baudRate must be a positive integer');
  }

  if (!Number.isInteger(intervalMs) || intervalMs < config.minIntervalMs) {
    errors.push(`intervalMs must be an integer >= ${config.minIntervalMs}`);
  }

  if (
    !Number.isInteger(timezoneOffsetHours) ||
    timezoneOffsetHours < config.minTimezoneOffsetHours ||
    timezoneOffsetHours > config.maxTimezoneOffsetHours
  ) {
    errors.push(
      `timezoneOffsetHours must be an integer between ${config.minTimezoneOffsetHours} and ${config.maxTimezoneOffsetHours}`,
    );
  }

  if (transport === 'serial' && !config.autoSelectPort && !String(config.port || '').trim()) {
    errors.push('port is required when autoSelectPort is false');
  }

  if (!['serial', 'ble'].includes(transport)) {
    errors.push('transport must be "serial" or "ble"');
  }

  if (!Number.isInteger(bleScanTimeoutMs) || bleScanTimeoutMs < config.minBleScanTimeoutMs) {
    errors.push(`bleScanTimeoutMs must be an integer >= ${config.minBleScanTimeoutMs}`);
  }

  if (!Number.isInteger(bleConnectDelayMs) || bleConnectDelayMs < config.minBleConnectDelayMs) {
    errors.push(`bleConnectDelayMs must be an integer >= ${config.minBleConnectDelayMs}`);
  }

  if (
    !Number.isInteger(bleDiscoveryTimeoutMs) ||
    bleDiscoveryTimeoutMs < config.minBleDiscoveryTimeoutMs
  ) {
    errors.push(
      `bleDiscoveryTimeoutMs must be an integer >= ${config.minBleDiscoveryTimeoutMs}`,
    );
  }

  if (
    !Number.isInteger(bleDiscoveryRetries) ||
    bleDiscoveryRetries < config.minBleDiscoveryRetries
  ) {
    errors.push(`bleDiscoveryRetries must be an integer >= ${config.minBleDiscoveryRetries}`);
  }

  if (errors.length > 0) {
    throw new Error(`Invalid config: ${errors.join('; ')}`);
  }

  return {
    ...config,
    port: String(config.port || '').trim(),
    baudRate,
    intervalMs,
    timezoneOffsetHours,
    transport,
    bleName: String(config.bleName || '').trim(),
    bleId: String(config.bleId || '').trim(),
    bleServiceUuid: String(config.bleServiceUuid || '').trim(),
    bleMetricsCharacteristicUuid: String(config.bleMetricsCharacteristicUuid || '').trim(),
    bleScanTimeoutMs,
    bleConnectDelayMs,
    bleDiscoveryTimeoutMs,
    bleDiscoveryRetries,
    autoSelectPort: Boolean(config.autoSelectPort),
    verbose: Boolean(config.verbose),
  };
}

module.exports = {
  createRuntimeConfig,
  mergeCliOptions,
  validateConfig,
};
