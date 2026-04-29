// 本文件负责把指标对象编码为 ESP32 固件端约定的串口文本协议。

// 将任意输入转换为 0-100 的整数百分比。
function clampPercent(value) {
  const number = Number(value);

  if (!Number.isFinite(number)) {
    return 0;
  }

  return Math.min(100, Math.max(0, Math.round(number)));
}

// 将百分比格式化为三位补零文本，例如 5 -> 005。
function padPercent(value) {
  return String(clampPercent(value)).padStart(3, '0');
}

// 将 Date 格式化成本地时间 HHMM，减少串口协议中的分隔符。
function formatTime(date = new Date()) {
  const normalized = date instanceof Date ? date : new Date(date);

  if (Number.isNaN(normalized.getTime())) {
    return '0000';
  }

  const hours = String(normalized.getHours()).padStart(2, '0');
  const minutes = String(normalized.getMinutes()).padStart(2, '0');

  return `${hours}${minutes}`;
}

// 按固定字段顺序编码指标，并追加换行符作为串口消息边界。
function encodeMetrics(metrics = {}, date = new Date()) {
  return `C:${padPercent(metrics.cpu)}|M:${padPercent(metrics.memory)}|T:${formatTime(date)}\n`;
}

module.exports = {
  encodeMetrics,
  formatTime,
  padPercent,
  clampPercent,
};
