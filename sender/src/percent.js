// 本文件集中处理项目中 0-100 百分比数值的归一化。

function clampPercent(value) {
  const number = Number(value);

  if (!Number.isFinite(number)) {
    return 0;
  }

  return Math.min(100, Math.max(0, Math.round(number)));
}

module.exports = {
  clampPercent,
};
