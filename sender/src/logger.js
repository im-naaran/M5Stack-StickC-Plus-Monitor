// 本文件提供统一日志输出，集中控制普通、调试、警告和错误日志格式。

// 创建 logger；verbose 为 true 时才输出 debug 日志。
function createLogger(options = {}) {
  const verbose = Boolean(options.verbose);

  return {
    // 输出普通运行状态日志。
    info: (...args) => console.log('[info]', ...args),

    // 输出调试日志，仅在 verbose 开启时生效。
    debug: (...args) => {
      if (verbose) {
        console.log('[debug]', ...args);
      }
    },

    // 输出可恢复问题或重试提示。
    warn: (...args) => console.warn('[warn]', ...args),

    // 输出错误日志。
    error: (...args) => console.error('[error]', ...args),
  };
}

module.exports = {
  createLogger,
};
