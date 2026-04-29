// 本文件负责把默认配置和 CLI 参数合并成最终运行配置，并完成基础校验。
const CONFIG = require('../config');

// 基于默认 CONFIG 和命令行参数生成最终运行配置。
function createRuntimeConfig(cliOptions = {}) {
  return validateConfig(mergeCliOptions(CONFIG, cliOptions));
}

// 将命令行参数覆盖到默认配置上，未指定的 CLI 参数保持 CONFIG 原值。
function mergeCliOptions(config, cliOptions) {
  const runtimeConfig = { ...config };

  if (cliOptions.port !== undefined) {
    runtimeConfig.port = cliOptions.port;
  }

  if (cliOptions.baudRate !== undefined) {
    runtimeConfig.baudRate = cliOptions.baudRate;
  }

  if (cliOptions.intervalMs !== undefined) {
    runtimeConfig.intervalMs = cliOptions.intervalMs;
  }

  if (cliOptions.verbose !== undefined) {
    runtimeConfig.verbose = cliOptions.verbose;
  }

  return runtimeConfig;
}

// 校验配置范围，并把数值/布尔/字符串字段转换为运行时使用的稳定类型。
function validateConfig(config) {
  const errors = [];
  const baudRate = Number(config.baudRate);
  const intervalMs = Number(config.intervalMs);

  if (!Number.isInteger(baudRate) || baudRate <= 0) {
    errors.push('baudRate must be a positive integer');
  }

  if (!Number.isInteger(intervalMs) || intervalMs < 500) {
    errors.push('intervalMs must be an integer >= 500');
  }

  if (!config.autoSelectPort && !String(config.port || '').trim()) {
    errors.push('port is required when autoSelectPort is false');
  }

  if (errors.length > 0) {
    throw new Error(`Invalid config: ${errors.join('; ')}`);
  }

  return {
    ...config,
    port: String(config.port || '').trim(),
    baudRate,
    intervalMs,
    autoSelectPort: Boolean(config.autoSelectPort),
    verbose: Boolean(config.verbose),
  };
}

module.exports = {
  createRuntimeConfig,
  mergeCliOptions,
  validateConfig,
};
