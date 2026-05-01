# Node Sender 规格

## 1. 职责边界

Node Sender 是电脑端服务，负责采集系统指标，并通过 USB Serial 或 BLE GATT 发送给 M5StickC Plus。

负责：

- 采集 CPU 使用率。
- 采集内存使用率。
- 将指标编码为项目协议。
- 在启动后的首次发送中携带时间同步字段。
- 打开 USB Serial 串口或连接 BLE GATT 服务。
- 按固定间隔发送数据。
- 提供命令行参数和日志。

不负责：

- M5StickC Plus 屏幕布局。
- ESP32 本地时钟维护。
- 固件端连接状态判断。
- 按钮交互。
- 系统级蓝牙配对。

## 2. 技术栈

- Node.js。
- `systeminformation`：采集 CPU、内存等系统数据。
- `serialport`：USB 串口通信。
- `@abandonware/noble`：BLE Central / GATT Client 通信。
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
    ├── percent.js
    ├── protocol.js
    ├── serialTransport.js
    ├── bleTransport.js
    └── logger.js
```

## 4. package.json

`sender/package.json` 声明 Node.js 项目依赖和脚本：

- `npm start`：运行 `node src/index.js`。
- `npm run list-ports`：运行 `node src/index.js --list-ports`。
- `npm run list-ble`：运行 `node src/index.js --list-ble`。

依赖：

- `commander`
- `@abandonware/noble`
- `serialport`
- `systeminformation`

## 5. 配置

`sender/config.js` 是唯一默认配置入口，只放置配置值和字段说明。

字段说明：

| 字段 | 默认值 | 含义 |
| --- | --- | --- |
| `port` | `''` | 串口路径 |
| `baudRate` | `115200` | 串口波特率 |
| `transport` | `serial` | 传输方式，支持 `serial` 或 `ble` |
| `intervalMs` | `2000` | 指标发送间隔，单位毫秒 |
| `timezoneOffsetHours` | `8` | 首次发送给 ESP32 的时区偏移，单位小时 |
| `autoSelectPort` | `true` | `port` 为空时是否自动选择疑似 M5 设备 |
| `bleName` | `''` | BLE 设备名筛选 |
| `bleId` | `''` | BLE 设备 ID 或地址筛选 |
| `bleServiceUuid` | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | BLE GATT 服务 UUID |
| `bleMetricsCharacteristicUuid` | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | BLE 指标写入 characteristic UUID |
| `bleScanTimeoutMs` | `8000` | BLE 扫描超时时间，单位毫秒 |
| `bleConnectDelayMs` | `1000` | BLE 连接成功后等待 GATT discovery 的时间 |
| `bleDiscoveryTimeoutMs` | `5000` | BLE GATT discovery 单次超时时间 |
| `bleDiscoveryRetries` | `3` | BLE GATT discovery 失败后的重试次数 |
| `verbose` | `false` | 是否输出调试日志 |

配置合并优先级：

```text
CLI 参数 > CONFIG
```

校验规则：

- `baudRate` 必须为正整数。
- `intervalMs` 必须为大于等于 `500` 的整数。
- `timezoneOffsetHours` 必须为 `-12` 到 `+14` 之间的整数。
- `autoSelectPort = false` 时，`port` 必须非空。
- `transport` 必须为 `serial` 或 `ble`。
- `bleScanTimeoutMs` 必须为大于等于 `1000` 的整数。

## 6. 命令行参数

```text
--port <path>       指定串口路径
--baud <number>     指定波特率，默认 115200
--transport <type>  指定传输方式，serial 或 ble
--interval <ms>     指定发送间隔，默认 2000
--list-ports        列出可用串口后退出
--list-ble          扫描 BLE monitor 设备后退出
--ble-name <name>   指定 BLE 设备名
--ble-id <id>       指定 BLE 设备 ID 或地址
--ble-scan-timeout <ms> 指定 BLE 扫描超时
--ble-connect-delay <ms> 指定连接后等待 GATT discovery 的时间
--ble-discovery-timeout <ms> 指定 GATT discovery 超时
--ble-discovery-retries <number> 指定 GATT discovery 重试次数
--verbose           输出调试日志
```

## 7. 传输协议

每条消息是一行文本，以 `\n` 结尾。

启动后的首次发送携带时间同步字段：

```text
C:025|M:060|T:1714440000|Z:+8\n
```

启动首次发送之后，定时发送只携带指标字段：

```text
C:025|M:060\n
```

字段说明：

| 字段 | 含义 | 类型 | 范围 | 必填 |
| --- | --- | --- | --- | --- |
| `C` | CPU 使用率 | 整数百分比 | `0-100` | 否 |
| `M` | 内存使用率 | 整数百分比 | `0-100` | 否 |
| `T` | Unix 时间戳 | 秒级整数 | `0-4294967295` | 否 |
| `Z` | 时区偏移 | 整数小时 | `-12` 到 `+14` | 否 |

编码规则：

- 首次发送字段顺序为 `C`、`M`、`T`、`Z`。
- 定时指标包字段顺序为 `C`、`M`。
- 字段之间使用 `|` 分隔。
- key 与 value 之间使用 `:` 分隔。
- 输出使用 UTF-8 文本。
- 输出包含结尾换行符。
- CPU 和内存百分比统一为整数。
- CPU 和内存百分比使用 3 位补零，例如 `005`、`025`、`100`。
- CPU 和内存缺失值按 `0` 处理。
- CPU 和内存小数四舍五入。
- CPU 和内存小于 `0` 时按 `0` 处理，大于 `100` 时按 `100` 处理。
- 时间使用秒级 Unix 时间戳。
- 时区使用小时偏移，正数带 `+`，例如 `Z:+8`。

USB Serial 参数：

- Baud rate：`115200`
- Encoding：UTF-8 文本
- 默认发送间隔：`2000ms`

BLE GATT 参数：

| 项 | 值 |
| --- | --- |
| Device Name | `M5Monitor-Plus` |
| Service UUID | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Metrics Characteristic UUID | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| Characteristic Properties | `Write`, `Write Without Response` |
| Payload | UTF-8 文本协议 |

第一版 BLE 不走系统级蓝牙配对。Node Sender 按 Service UUID 扫描设备，只有多台设备同时出现时才需要 `--ble-id` 或 `--ble-name` 指定目标。

## 8. 模块职责

`src/index.js`：

- 作为 PC Sender 入口。
- 解析命令行参数。
- 处理 `--list-ports` 一次性命令。
- 通过 `configTools.js` 获取运行配置。
- 打开 transport。
- 启动后立即发送一次数据，并在该次发送中携带 `T/Z`。
- 启动首次发送之后按 `intervalMs` 定时发送 `C/M`。
- 监听退出信号并关闭 transport。

`src/configTools.js`：

- 读取 `sender/config.js` 中的 `CONFIG`。
- 合并命令行覆盖项。
- 校验最终运行配置。
- 把字符串、数值和布尔值规范化为运行时稳定类型。

`src/metrics.js`：

- 采集系统指标。
- 把第三方库返回值转换为项目内部结构。
- 返回 `{ cpu, memory }`。
- 不处理串口。
- 不编码协议字符串。

`src/percent.js`：

- 统一处理 `0-100` 百分比归一化。
- 避免采集层和协议层重复实现边界处理。

`src/protocol.js`：

- 编码项目协议。
- 提供 `encodeMetrics`、`formatTimestamp`、`formatTimezoneOffsetHours`、`padPercent`、`clampPercent`。
- `encodeMetrics(metrics, { includeTime: true, timezoneOffsetHours })` 会追加 `T/Z`。
- `encodeMetrics(metrics)` 只输出 `C/M`。
- 不采集系统指标。
- 不写 transport。

`src/serialTransport.js`：

- 封装 `serialport`。
- 负责列出端口、选择端口、打开端口、写入数据、关闭端口。
- 指定 `port` 时使用指定端口。
- 未指定 `port` 时按设备信息自动选择疑似 M5 设备。
- 多个候选设备同时存在时抛出错误。
- 不采集指标。
- 不编码协议。

`src/bleTransport.js`：

- 封装 `@abandonware/noble`。
- 按 BLE Service UUID 扫描 M5 monitor 设备。
- 支持通过 `bleName` 或 `bleId` 筛选设备。
- 连接后发现 metrics characteristic。
- 写入 UTF-8 协议文本。
- 写入失败或断开后，下一次写入会重新扫描连接。
- 不采集指标。
- 不编码协议。

`src/logger.js`：

- 统一输出日志。
- 支持普通日志、调试日志、警告日志、错误日志。
- `debug` 仅在 `verbose = true` 时输出。
- 错误日志使用 `console.error`。
- 普通运行状态使用 `console.log`。

## 9. 运行逻辑

```text
main:
  解析 CLI 参数
  如果 --list-ports:
    输出可用串口并退出
  如果 --list-ble:
    扫描 BLE monitor 设备并退出
  生成并校验运行配置
  打开 transport
  立即发送 C/M/T/Z
  每 intervalMs 发送 C/M
  收到退出信号:
    停止定时器
    关闭 transport
```

异常处理规则：

- 配置校验失败时进程退出并输出错误。
- 采集或发送失败时记录警告，下一轮继续重试。
- 退出信号触发后停止定时器并关闭 transport。

## 10. 运行命令

```bash
cd sender
npm install
npm run list-ports
npm start -- --port /dev/tty.usbserial-xxxx --interval 2000
npm run list-ble
npm start -- --transport ble --interval 2000
```

如果执行 BLE 命令出现 `No native build was found ...`，用当前 Node 版本重新安装依赖：

```bash
cd sender
npm install
npm run list-ble
```

本项目在 `package.json` 的 `pnpm.onlyBuiltDependencies` 中允许了 BLE 和串口所需的原生依赖构建脚本，包括 `@abandonware/noble`、`@abandonware/bluetooth-hci-socket`、`@serialport/bindings-cpp` 和 `usb`。pnpm v10 若提示 ignored build scripts，通常需要重新执行 `pnpm install` 和 `pnpm rebuild`。

## 11. 开发边界

- `metrics.js` 不依赖串口。
- `protocol.js` 不依赖 `systeminformation`。
- `serialTransport.js` 和 `bleTransport.js` 不负责采集或编码。
- 默认配置集中在 `sender/config.js`。
- 配置合并和校验集中在 `sender/src/configTools.js`。
- 串口协议字段只追加，不改变现有 `C`、`M` 含义。
- 串口协议变更需要同步更新 ESP32 Firmware 文档和实现。
