# Python Sender

Python Sender 是电脑端指标发送器，用于采集本机 CPU 和内存使用率，并通过 USB Serial 或 BLE GATT 发送给 M5StickC Plus。

功能：

- 采集 CPU 使用率和内存使用率。
- 使用与固件一致的文本协议，例如 `C:025|M:060\n`。
- 启动后第一次发送会附带时间同步字段，例如 `T:1714440000|Z:+8`。
- 支持 USB Serial 和 BLE 两种传输方式。
- 写入失败后，下一次发送会重新打开串口或重新扫描连接 BLE。

## 环境

本目录按 uv 管理 Python 版本、虚拟环境和依赖。推荐使用 Python 3.12。macOS 下 BLE 依赖 CoreBluetooth/pyobjc，Python 3.14 目前不建议用于 BLE。

```bash
cd sender_py
uv python install 3.12
uv sync
```

## USB Serial

列出串口：

```bash
uv run python main.py --list-ports
```

自动选择疑似 M5StickC Plus 的串口并发送：

```bash
uv run python main.py
```

指定串口：

```bash
uv run python main.py --port /dev/tty.usbserial-xxxx
```

指定发送间隔：

```bash
uv run python main.py --port /dev/tty.usbserial-xxxx --interval 2000
```

## BLE

列出 BLE 设备：

```bash
uv run python main.py --list-ble
```

使用 BLE 发送：

```bash
uv run python main.py --transport ble
```

按设备名连接：

```bash
uv run python main.py --transport ble --ble-name M5Monitor-Plus
```

按设备 ID 或地址连接：

```bash
uv run python main.py --transport ble --ble-id D3CDE304-9ACF-FE45-A0EF-0615A8433A86
```

## 参数

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

## 示例

USB Serial 调试：

```bash
uv run python main.py --port /dev/tty.usbserial-xxxx --interval 2000 --verbose
```

BLE 调试：

```bash
uv run python main.py --transport ble --ble-name M5Monitor-Plus --interval 2000 --verbose
```

如果 BLE discovery 不稳定，可以增加连接后的等待时间：

```bash
uv run python main.py --transport ble --ble-connect-delay 2000
```

macOS 使用 BLE 时，需要给运行命令的程序开启蓝牙权限，例如 Terminal、iTerm、VS Code 或 wrap.app。
