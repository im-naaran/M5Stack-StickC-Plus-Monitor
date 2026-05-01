# M5StickC Plus PC Monitor

使用 M5StickC Plus 作为电脑状态监控小屏，通过 USB 串口或 BLE GATT 显示 CPU 使用率、内存使用率、连接状态和电脑本地时间。

## 功能

- USB 有线串口通信，波特率 `115200`。
- BLE GATT 自定义服务通信，电脑端作为 Central，M5StickC Plus 作为 Peripheral。
- 展示 CPU 使用率、内存使用率、PC 连接状态。
- 底部显示时间。电脑端启动时发送秒级 Unix 时间戳和时区，ESP32 本地维护时钟并显示 `HH:MM`。
- Button B 循环切换低、中、高三档亮度。
- USB 或 BLE 断开后固件显示 `Disconnected`；重新连接后 Node Sender 会自动重连。

## 项目结构

```text
firmware/   M5StickC Plus 固件，PlatformIO + Arduino C++
sender/     电脑端 Node.js 服务，采集指标并写入 USB Serial
docs/       开发规格和协议说明
```

详细开发规格：

- [Node Sender](docs/NODE_SENDER.md)
- [ESP32 Firmware](docs/ESP32_FIRMWARE.md)

## 串口协议

Node Sender 启动时发送一行带时间同步信息的文本：

```text
C:025|M:060|T:1714440000|Z:+8\n
```

后续指标包可以只发送变化字段：

```text
C:025|M:060\n
```

字段：

| 字段 | 含义 | 示例 |
| --- | --- | --- |
| `C` | CPU 使用率，`0-100` | `C:025` |
| `M` | 内存使用率，`0-100` | `M:060` |
| `T` | 秒级 Unix 时间戳 | `T:1714440000` |
| `Z` | 时区偏移，单位小时 | `Z:+8` |

ESP32 接收端按字段增量更新：字段存在就读取，不存在就保持当前状态。

USB Serial 和 BLE 共用同一套文本协议。BLE 模式下，Node Sender 将每行 UTF-8 文本写入 GATT characteristic。

## BLE GATT

第一版 BLE 不走系统级蓝牙配对，采用应用层设备发现和连接。

| 项 | 值 |
| --- | --- |
| Device Name | `M5Monitor-Plus` |
| Service UUID | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Metrics Characteristic UUID | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| Characteristic Properties | `Write`, `Write Without Response` |
| Payload | UTF-8 协议文本，例如 `C:025|M:060\n` |

## 刷写固件

先停止 Node Sender 或其他串口监视器，避免串口被占用。

```bash
cd firmware
pio run -t upload --upload-port /dev/tty.usbserial-xxxx # 设备号可以通过下文 node 服务获取
# 如果只有一台设备，可简写为
pio run -t upload 
```

如果串口名不同，先查看可用端口：

```bash
cd sender
npm run list-ports
```

## 启动服务

安装依赖：

```bash
cd sender
npm install
```

启动发送端：

```bash
npm run start -- --port /dev/tty.usbserial-xxxx --interval 2000
```

启动 BLE 发送端：

```bash
npm run list-ble
npm run start -- --transport ble --interval 2000
# 多台设备时指定设备
npm run start -- --transport ble --ble-name M5Monitor-Plus --interval 2000
# GATT discovery 不稳定时可增加连接后的等待时间
npm run start -- --transport ble --ble-connect-delay 2000 --interval 2000
```

BLE 使用 Node 依赖 `@abandonware/noble`。如果看到 `No native build was found ...`，用当前 Node 版本重新安装依赖：

```bash
cd sender
npm install
npm run list-ble
```

调试协议输出：

```bash
npm run start -- --port /dev/tty.usbserial-xxxx --interval 2000 --verbose
npm run start -- --transport ble --interval 2000 --verbose
```
