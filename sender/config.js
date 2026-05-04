// 本文件是 Node Sender 的唯一默认配置入口，只放配置值和字段说明。
const CONFIG = {
  // 串口路径。留空时，如果 autoSelectPort 为 true，程序会尝试自动选择疑似 M5StickC Plus 的 USB 串口。
  port: '',

  // 串口波特率。必须和 ESP32 固件端 Serial.begin(...) 使用的波特率一致。
  baudRate: 115200,

  // 传输方式。serial 保持现有 USB 串口方案，ble 使用 BLE GATT 自定义服务。
  transport: 'serial',

  // 指标发送间隔，单位毫秒。建议保持在 1000-2000ms，最小允许值为 500ms。
  intervalMs: 2000,
  minIntervalMs: 500,

  // 发送给 ESP32 的时区偏移，单位小时。默认 UTC+8，对应协议字段 Z:+8。
  timezoneOffsetHours: 8,
  minTimezoneOffsetHours: -12,
  maxTimezoneOffsetHours: 14,

  // 是否在 port 为空时自动选择串口。设为 false 时必须手动填写 port 或通过 --port 指定。
  autoSelectPort: true,

  // BLE 设备名。留空时按服务 UUID 自动发现；多台设备同时出现时建议指定。
  bleName: '',

  // BLE 设备 ID 或地址。优先用于区分多台同名设备。
  bleId: '',

  // 固件端广播的 BLE GATT 服务 UUID。当前使用 Nordic UART Service 形态的自定义服务。
  bleServiceUuid: '6e400001-b5a3-f393-e0a9-e50e24dcca9e',

  // 固件端用于写入指标协议文本的 BLE characteristic UUID。
  bleMetricsCharacteristicUuid: '6e400002-b5a3-f393-e0a9-e50e24dcca9e',

  // BLE 扫描超时时间，单位毫秒。
  bleScanTimeoutMs: 8000,
  minBleScanTimeoutMs: 1000,

  // BLE 连接建立后等待 GATT discovery 的时间。过早 discovery 可能失败时可适当增加。
  bleConnectDelayMs: 1000,
  minBleConnectDelayMs: 0,

  // BLE GATT discovery 单次超时时间，单位毫秒。
  bleDiscoveryTimeoutMs: 5000,
  minBleDiscoveryTimeoutMs: 1000,

  // BLE GATT discovery 失败后的重试次数。
  bleDiscoveryRetries: 3,
  minBleDiscoveryRetries: 1,

  // BLE discovery 失败后下一次尝试前的等待时间，单位毫秒。
  bleDiscoveryRetryDelayMs: 500,
  minBleDiscoveryRetryDelayMs: 0,

  // 是否输出调试日志。开启后会打印每次写入串口的协议文本。
  verbose: false,
};

module.exports = CONFIG;
