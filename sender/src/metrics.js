// 本文件负责采集系统 CPU/内存指标，并转换成项目内部的百分比结构。
const systeminformation = require('systeminformation');

// 将任意输入转换为 0-100 的整数百分比。
function clampPercent(value) {
  const number = Number(value);

  if (!Number.isFinite(number)) {
    return 0;
  }

  return Math.min(100, Math.max(0, Math.round(number)));
}

// 将 systeminformation 返回的原始结构转换为 { cpu, memory }。
function normalizeMetrics(raw = {}) {
  const cpu = raw.cpu && raw.cpu.currentLoad;
  const totalMemory = raw.memory && raw.memory.total;
  const activeMemory = raw.memory && raw.memory.active;
  const memory =
    totalMemory > 0 ? (Number(activeMemory) / Number(totalMemory)) * 100 : 0;

  return {
    cpu: clampPercent(cpu),
    memory: clampPercent(memory),
  };
}

// 并行采集 CPU 和内存数据，并返回标准化后的指标。
async function collectMetrics() {
  const [cpu, memory] = await Promise.all([
    systeminformation.currentLoad(),
    systeminformation.mem(),
  ]);

  return normalizeMetrics({ cpu, memory });
}

module.exports = {
  collectMetrics,
  normalizeMetrics,
  clampPercent,
};
