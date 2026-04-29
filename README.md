# M5StickC Plus PC Monitor

使用 M5StickC Plus 作为电脑状态监控小屏，通过 USB 串口显示 CPU 使用率、内存使用率、连接状态和电脑本地时间。

## 功能

- USB 有线串口通信，波特率 `115200`。
- 展示 CPU 使用率、内存使用率、USB 连接状态。
- 底部显示电脑本地时间，协议发送 `HHMM`，屏幕显示 `HH:MM`。
- Button B 循环切换低、中、高三档亮度。
- USB 断开后固件显示 `Disconnected`；重新插上后 Node Sender 会自动重连。

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

Node Sender 每隔 1-2 秒发送一行文本：

```text
C:025|M:060|T:2241\n
```

字段：

| 字段 | 含义 | 示例 |
| --- | --- | --- |
| `C` | CPU 使用率，`0-100` | `C:025` |
| `M` | 内存使用率，`0-100` | `M:060` |
| `T` | 电脑本地时间，`HHMM` | `T:2241` |

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

调试协议输出：

```bash
npm run start -- --port /dev/tty.usbserial-xxxx --interval 2000 --verbose
```
