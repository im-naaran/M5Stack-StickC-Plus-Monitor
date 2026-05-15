# Python Sender

Python Sender 是运行在电脑端的指标发送器。它采集本机 CPU 和内存使用率，将数据编码成固件可解析的文本协议，并通过 USB Serial 或 BLE GATT 写入 M5StickC Plus。

## 设计思路

发送端只负责电脑侧的采集、编码和传输，不负责固件显示、按钮交互、设备设置或系统级蓝牙配对。固件端只要持续收到有效协议行，就会更新连接状态和屏幕内容。

核心流程：

```text
load config
merge CLI args
validate runtime config
open serial or BLE transport
send first metrics line with time sync
send metrics line every interval
reopen transport on write failure
close transport on SIGINT/SIGTERM
```

配置默认值集中在 `config.py`。`main.py` 负责命令行解析、配置合并校验、指标采集、协议编码、传输连接和主循环。当前实现保持单文件运行逻辑，便于在小型工具里直接排查问题。

## 数据协议

每条消息是一行 UTF-8 文本，以 `\n` 结尾。字段之间使用 `|` 分隔，key 与 value 使用 `:` 分隔。

启动后的第一次发送会携带时间同步字段：

```text
C:025|M:060|T:1714440000|Z:+8\n
```

后续定时发送只携带指标字段：

```text
C:025|M:060\n
```

字段说明：

| 字段 | 含义 | 格式 |
| --- | --- | --- |
| `C` | CPU 使用率 | `0-100` 整数，发送时补齐 3 位 |
| `M` | 内存使用率 | `0-100` 整数，发送时补齐 3 位 |
| `T` | Unix 时间戳 | 秒级整数 |
| `Z` | 时区偏移 | 整数小时，非负数带 `+` |

CPU 和内存百分比会做边界处理：非法值按 `0`，小于 `0` 裁剪为 `0`，大于 `100` 裁剪为 `100`，小数按四舍五入处理。

## 指标采集

- CPU 使用率来自 `psutil.cpu_percent(interval=0.1)`。
- 内存使用率优先使用 `psutil.virtual_memory().active / total`，没有 `active` 字段时回退到 `used / total`。
- 默认发送间隔为 `2000ms`，最小允许值为 `500ms`。
- 默认时区偏移为 `UTC+8`，用于第一次时间同步消息。

## 传输方式

### USB Serial

USB Serial 使用 `pyserial`，默认波特率 `115200`，需要和固件端一致。

未指定 `--port` 时，程序会根据串口路径、manufacturer、vendor id 自动选择疑似 M5StickC Plus 的设备。若发现多个候选设备，会优先选择唯一的 `/dev/tty.*` 候选；仍无法确认时要求手动传入 `--port`。

写入失败后会丢弃当前串口对象，下一次发送前重新查找并打开串口。

### BLE GATT

BLE 使用 `bleak`，电脑端作为 Central，M5StickC Plus 作为 Peripheral。扫描时优先按 Service UUID 过滤，必要时回退到普通扫描；没有显式指定设备时，也会匹配默认设备名。

| 项 | 值 |
| --- | --- |
| Device Name | `M5Monitor-Plus` |
| Service UUID | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Metrics Characteristic UUID | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| 写入方式 | `Write Without Response` |

多台设备同时存在时，用 `--ble-name` 或 `--ble-id` 指定目标。连接后会等待 GATT characteristic 可用；若 discovery 不稳定，可增加 `--ble-connect-delay` 或调整 discovery 超时/重试参数。

macOS 下 BLE 依赖 CoreBluetooth/pyobjc。本项目要求 Python `>=3.12,<3.14`，不建议使用 Python 3.14 运行 BLE。

## 配置

运行时配置由默认 `Config()` 和命令行参数合并得到，优先级为：

```text
CLI 参数 > config.py 默认值
```

## 执行和部署

本目录使用 uv 管理 Python 版本、虚拟环境和依赖：

```bash
cd sender
uv python install 3.12
uv sync
```

列出可用串口：

```bash
uv run python main.py --list-ports
```

通过 USB Serial 自动选择设备并发送：

```bash
uv run python main.py
```

指定 USB Serial 端口：

```bash
uv run python main.py --port /dev/tty.usbserial-xxxx
```

列出 BLE monitor 设备：

```bash
uv run python main.py --list-ble
```

通过 BLE 发送：

```bash
uv run python main.py --transport ble --ble-name M5Monitor-Plus
```

调试协议输出：

```bash
uv run python main.py --port /dev/tty.usbserial-xxxx --verbose
uv run python main.py --transport ble --verbose
```

常见处理：

- 串口打不开：关闭 Arduino Serial Monitor、M5Burner、`screen`、`minicom` 或其他 sender 进程。
- 自动选择串口失败：先运行 `--list-ports`，再用 `--port` 指定。
- BLE 找不到设备：确认固件设置页 `ble` 为 `on`，设备处于开机状态，电脑端蓝牙权限已开启。
- BLE discovery 超时：尝试重启设备、开关电脑蓝牙，或增加 `--ble-connect-delay 2000`。

## 支持参数

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| `--transport <serial|ble>` | 传输方式 | `serial` |
| `--port <path>` | USB 串口路径 | 自动选择 |
| `--baud <number>` | USB 串口波特率 | `115200` |
| `--interval <ms>` | 指标发送间隔 | `2000` |
| `--list-ports` | 列出串口后退出 | - |
| `--list-ble` | 扫描 BLE 设备后退出 | - |
| `--ble-name <name>` | 按 BLE 设备名筛选 | - |
| `--ble-id <id>` | 按 BLE 设备 ID 或地址筛选 | - |
| `--ble-scan-timeout <ms>` | BLE 扫描超时时间 | `8000` |
| `--ble-connect-delay <ms>` | BLE 连接后等待 GATT discovery 的时间 | `1000` |
| `--ble-discovery-timeout <ms>` | BLE GATT discovery 超时时间 | `5000` |
| `--ble-discovery-retries <number>` | BLE GATT discovery 重试次数 | `3` |
| `--ble-discovery-retry-delay <ms>` | BLE GATT discovery 重试间隔 | `500` |
| `--verbose` | 输出每次写入的协议文本 | 关闭 |
