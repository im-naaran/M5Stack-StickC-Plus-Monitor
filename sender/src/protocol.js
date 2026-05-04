// 本文件负责把指标对象编码为 ESP32 固件端约定的串口文本协议。
const { clampPercent } = require('./percent');

// 将百分比格式化为三位补零文本，例如 5 -> 005。
function padPercent(value) {
  return String(clampPercent(value)).padStart(3, '0');
}

// 将 Date 格式化为秒级 Unix 时间戳。
function formatTimestamp(date = new Date()) {
  const normalized = date instanceof Date ? date : new Date(date);

  if (Number.isNaN(normalized.getTime())) {
    return '0';
  }

  return String(Math.floor(normalized.getTime() / 1000));
}

// 将小时级时区偏移格式化为协议值，例如 8 -> +8。
function formatTimezoneOffsetHours(value = 8) {
  const number = Number(value);

  if (!Number.isInteger(number)) {
    return '+8';
  }

  return `${number >= 0 ? '+' : ''}${number}`;
}

// 按固定字段顺序编码指标，并追加换行符作为串口消息边界。
function encodeMetrics(metrics = {}, options = {}) {
  const normalizedOptions = options || {};
  const fields = [`C:${padPercent(metrics.cpu)}`, `M:${padPercent(metrics.memory)}`];

  if (normalizedOptions.includeTime) {
    fields.push(`T:${formatTimestamp(normalizedOptions.date || new Date())}`);
    fields.push(`Z:${formatTimezoneOffsetHours(normalizedOptions.timezoneOffsetHours)}`);
  }

  return `${fields.join('|')}\n`;
}

module.exports = {
  encodeMetrics,
  formatTimestamp,
  formatTimezoneOffsetHours,
  padPercent,
};
