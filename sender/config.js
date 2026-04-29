// 本文件是 Node Sender 的唯一默认配置入口，只放配置值和字段说明。
const CONFIG = {
  // 串口路径。留空时，如果 autoSelectPort 为 true，程序会尝试自动选择疑似 M5StickC Plus 的 USB 串口。
  port: '',

  // 串口波特率。必须和 ESP32 固件端 Serial.begin(...) 使用的波特率一致。
  baudRate: 115200,

  // 指标发送间隔，单位毫秒。建议保持在 1000-2000ms，最小允许值为 500ms。
  intervalMs: 2000,

  // 是否在 port 为空时自动选择串口。设为 false 时必须手动填写 port 或通过 --port 指定。
  autoSelectPort: true,

  // 是否输出调试日志。开启后会打印每次写入串口的协议文本。
  verbose: false,
};

module.exports = CONFIG;
