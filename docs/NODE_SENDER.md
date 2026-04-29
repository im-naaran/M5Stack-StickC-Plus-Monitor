# Node Sender 开发规格

## 1. 职责边界

Node Sender 是电脑端服务，负责采集系统指标并通过 USB Serial 发送给 M5StickC Plus。

负责：

- 采集 CPU 使用率。
- 采集内存使用率。
- 将指标编码为项目协议。
- 打开 USB Serial 串口。
- 按固定间隔发送数据。
- 提供命令行参数和日志。

不负责：

- M5StickC Plus 屏幕布局。
- 固件端连接状态判断。
- 按钮交互。
- 无线通信。

## 2. 技术栈

- Node.js。
- `systeminformation`：采集 CPU、内存等系统数据。
- `serialport`：USB 串口通信。
- `commander`：命令行参数解析。

## 3. 目录结构

```text
sender/
├── package.json
├── config.js
└── src/
    ├── index.js
    ├── configTools.js
    ├── metrics.js
    ├── protocol.js
    ├── serialTransport.js
    └── logger.js
```

## 4. 串口协议

每条消息是一行文本，以 `\n` 结尾：

```text
C:025|M:060|T:2241\n
```

字段说明：

| 字段 | 含义 | 类型 | 范围 | 必填 |
| --- | --- | --- | --- | --- |
| `C` | CPU 使用率 | 整数百分比 | `0-100` | 是 |
| `M` | 内存使用率 | 整数百分比 | `0-100` | 是 |
| `T` | 电脑本地时间 | `HHMM` | `0000-2359` | 否 |

编码规则：

- 字段顺序固定为 `C`、`M`、`T`。
- 字段之间使用 `|` 分隔。
- key 与 value 之间使用 `:` 分隔。
- 百分比统一为整数。
- 输出使用 3 位补零，例如 `005`、`025`、`100`。
- 时间使用电脑本地时间，格式为 `HHMM`。
- 输出必须包含结尾换行符。
- 缺失值按 `0` 处理。
- 小数四舍五入。
- 小于 `0` 按 `0` 处理，大于 `100` 按 `100` 处理。

串口参数：

- Baud rate：`115200`
- Encoding：UTF-8 文本。
- 发送间隔默认：`2000ms`。

## 5. package.json

职责：

- 声明 Node.js 项目依赖。
- 提供启动、列出串口等脚本。

建议依赖：

```json
{
  "dependencies": {
    "commander": "^12.0.0",
    "serialport": "^12.0.0",
    "systeminformation": "^5.0.0"
  }
}
```

建议脚本：

```json
{
  "scripts": {
    "start": "node src/index.js",
    "list-ports": "node src/index.js --list-ports"
  }
}
```

## 6. config.js

职责：

- 提供唯一的默认配置入口。
- 用 JS 注释说明每个配置字段。
- 只保留 `CONFIG` 配置值，不放置配置处理逻辑。

建议内容：

```js
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
```

字段说明：

- `port`：串口路径，为空时尝试自动选择。
- `baudRate`：串口波特率，必须和固件一致。
- `intervalMs`：指标发送间隔。
- `autoSelectPort`：没有指定端口时是否自动选择疑似 M5 设备。
- `verbose`：是否输出调试日志。

配置合并优先级：

```text
CLI 参数 > CONFIG
```

## 7. src/index.js

职责：

- 作为 PC Sender 入口。
- 解析命令行参数。
- 通过 `configTools.js` 获取运行配置。
- 打开串口。
- 启动定时采集和发送循环。
- 处理退出信号。

建议函数：

```js
async function main()
function parseCliArgs(argv)
async function runSender(config)
async function sendOnce(transport, logger)
function setupShutdownHandlers(transport, logger)
```

函数说明：

- `main()`：程序入口，捕获异常并设置退出码。
- `parseCliArgs(argv)`：解析 `--port`、`--baud`、`--interval`、`--list-ports`、`--verbose`。
- `runSender(config)`：打开串口，启动 `setInterval`。
- `sendOnce(transport, logger)`：采集指标、编码协议、写入串口。
- `setupShutdownHandlers(transport, logger)`：监听 `SIGINT`、`SIGTERM`，关闭串口。

命令行参数：

```text
--port <path>       指定串口路径
--baud <number>     指定波特率，默认 115200
--interval <ms>     发送间隔，默认 2000
--list-ports        列出可用串口后退出
--verbose           输出调试日志
```

运行逻辑：

```text
main:
  解析 CLI 参数
  如果 --list-ports:
    输出可用串口并退出
  从 configTools.js 生成并校验运行配置
  打开串口
  每 intervalMs:
    collectMetrics()
    encodeMetrics(metrics)
    transport.write(line)
  收到退出信号:
    关闭串口
```

## 8. 配置约束

所有默认配置和配置字段说明都集中在 `sender/config.js`。
配置合并和校验逻辑放在 `sender/src/configTools.js`。
不再支持额外 JSON 配置文件。

`sender/src/configTools.js` 职责：

- 读取 `sender/config.js` 中的 `CONFIG`。
- 合并命令行覆盖项。
- 校验最终运行配置。
- 把字符串、数值和布尔值规范化为运行时稳定类型。

校验规则：

- `baudRate` 必须为正整数。
- `intervalMs` 建议不小于 `500`。
- 如果 `autoSelectPort = false`，则 `port` 必须非空。

## 9. src/metrics.js

职责：

- 采集系统指标。
- 把第三方库返回值转换为项目内部结构。
- 不处理串口和协议字符串。

建议导出：

```js
async function collectMetrics()
function normalizeMetrics(raw)
function clampPercent(value)

module.exports = {
  collectMetrics,
  normalizeMetrics,
  clampPercent,
};
```

返回结构：

```js
{
  cpu: 25,
  memory: 60
}
```

函数说明：

- `collectMetrics()`：
  - 调用 `systeminformation.currentLoad()` 获取 CPU。
  - 调用 `systeminformation.mem()` 获取内存。
  - 返回标准化后的指标对象。
- `normalizeMetrics(raw)`：
  - 把第三方库数据转换成 `{ cpu, memory }`。
- `clampPercent(value)`：
  - 保证数值在 `0-100`。

内存计算建议：

```js
memory = Math.round((mem.active / mem.total) * 100)
```

异常处理：

- 采集失败时向上抛出错误，由 `index.js` 记录日志并在下一轮重试。
- 单次采集失败不应导致进程退出。

## 10. src/protocol.js

职责：

- 编码项目协议。
- 提供协议相关工具函数。
- 不采集系统指标，不写串口。

建议导出：

```js
function encodeMetrics(metrics)
function formatTime(date)
function padPercent(value)
function clampPercent(value)

module.exports = {
  encodeMetrics,
  formatTime,
  padPercent,
  clampPercent,
};
```

函数说明：

- `encodeMetrics(metrics)`：返回 `C:025|M:060|T:2241\n`。
- `formatTime(date)`：把本地时间转为 `HHMM`。
- `padPercent(value)`：把 `5` 转为 `005`。
- `clampPercent(value)`：保证百分比在 `0-100`。

编码示例：

```js
encodeMetrics({ cpu: 25, memory: 60 })
// "C:025|M:060|T:2241\n"
```

## 11. src/serialTransport.js

职责：

- 封装 `serialport`。
- 负责列出端口、选择端口、打开端口、写入数据、关闭端口。
- 不采集指标，不编码协议。

建议导出：

```js
async function listPorts()
function selectPort(ports, preferredPath)
async function openSerialTransport(options)

module.exports = {
  listPorts,
  selectPort,
  openSerialTransport,
};
```

`openSerialTransport()` 返回对象建议：

```js
{
  path: '/dev/tty.usbserial-xxxx',
  write: async (line) => {},
  close: async () => {}
}
```

函数说明：

- `listPorts()`：返回 `SerialPort.list()` 的结果。
- `selectPort(ports, preferredPath)`：
  - 如果指定 `preferredPath`，优先返回对应端口。
  - 未指定时尝试根据 `manufacturer`、`path`、`vendorId` 自动选择。
  - 无法选择时抛出清晰错误，并提示用户使用 `--list-ports`。
- `openSerialTransport(options)`：
  - 打开指定串口。
  - 等待 open 成功后返回 transport 对象。
  - 写入时使用 UTF-8 文本。
  - USB 断开导致写入失败后，下一轮写入应重新枚举并打开串口。

自动选择建议：

- 自动选择逻辑可以兼容 `/dev/tty.usbserial`、`/dev/tty.usbmodem`。
- Windows 常见路径为 `COM3`、`COM4` 等。
- 多个候选设备同时存在时，不要静默选择第一个，应提示用户指定 `--port`。

## 12. src/logger.js

职责：

- 统一输出日志。
- 支持普通日志、调试日志、警告日志、错误日志。
- 避免各模块直接散落 `console.log`。

建议导出：

```js
function createLogger(options)

module.exports = {
  createLogger,
};
```

返回对象建议：

```js
{
  info: (...args) => {},
  debug: (...args) => {},
  warn: (...args) => {},
  error: (...args) => {}
}
```

规则：

- `debug` 只有在 `verbose = true` 时输出。
- 错误日志使用 `console.error`。
- 普通运行状态使用 `console.log`。

日志示例：

```text
[info] using serial port: /dev/tty.usbserial-xxxx
[info] sender started, interval: 2000ms
[debug] write: C:025|M:060|T:2241
[warn] failed to collect metrics, retrying...
[error] serial port disconnected
```

## 13. 开发规范

- `metrics.js` 不依赖串口。
- `protocol.js` 不依赖 `systeminformation`。
- `serialTransport.js` 不负责采集或编码。
- 所有异步错误必须捕获并输出清晰日志。
- 程序退出时必须关闭串口。
- 定时发送过程中单次失败不应立即退出，可下一轮重试。
- 串口协议变更必须同步通知固件端开发者。
- 新增协议字段时只追加，不改变现有 `C`、`M` 含义。

## 14. 运行命令

```bash
cd sender
npm install
npm run list-ports
npm start -- --port /dev/tty.usbserial-xxxx --interval 2000
```
