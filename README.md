# M5StickC Plus PC Monitor

使用 M5StickC Plus 作为电脑状态监控小屏，通过 USB 串口或 BLE GATT 显示 CPU 使用率、内存使用率、连接状态和电脑本地时间。

## 功能概览

- USB Serial 通信，默认波特率 `115200`。
- BLE GATT 自定义服务通信，电脑端作为 Central，M5StickC Plus 作为 Peripheral。
- 显示 CPU、RAM、PC 连接状态和 `HH:MM` 时间。
- 长按 B 满 3 秒进入设置页，设置页支持滚动列表。
- 设置页可调整亮度、查看电量、开关 BLE、开关自动旋转。
- 显示 `Disconnected` 后 20 秒降到最低亮度，60 秒关闭屏幕背光，按 A/B 任意键唤醒。
- 屏幕熄灭后暂停绘制和自动旋转采样，并降低主循环频率。
- 固件默认将 ESP32 CPU 频率设为 `80MHz`。

## 项目结构

```text
firmware/   M5StickC Plus 固件，PlatformIO + Arduino C++
sender/     电脑端 Node.js 服务，采集指标并写入 USB Serial 或 BLE
docs/       固件、发送端和协议的开发说明
```

详细说明：

- [ESP32 Firmware](docs/ESP32_FIRMWARE.md)
- [Node Sender](docs/NODE_SENDER.md)

## 快速开始

### 1. 刷写固件

先停止 Node Sender 或其他串口监视器，避免串口被占用。

```bash
cd firmware
pio run -t upload
```

如果需要指定设备端口：

```bash
cd firmware
pio run -t upload --upload-port /dev/tty.usbserial-xxxx
```

不知道串口名时可以先列出端口：

```bash
cd sender
pnpm install
pnpm run list-ports
```

### 2. 启动 USB 发送端

```bash
cd sender
pnpm install
pnpm run start -- --port /dev/tty.usbserial-xxxx --interval 2000
```

如果只连接了一台疑似 M5StickC Plus 设备，可以省略 `--port`，发送端会尝试自动选择：

```bash
cd sender
pnpm run start -- --interval 2000
```

### 3. 启动 BLE 发送端

先确认固件设置页中的 `ble` 为 `on`。

```bash
cd sender
pnpm install
pnpm run list-ble
pnpm run start -- --transport ble --interval 2000
```

多台设备同时存在时指定设备名：

```bash
pnpm run start -- --transport ble --ble-name M5Monitor-Plus --interval 2000
```

如果 GATT discovery 不稳定，可增加连接后的等待时间：

```bash
pnpm run start -- --transport ble --ble-connect-delay 2000 --interval 2000
```

### 4. 调试协议输出

```bash
pnpm run start -- --port /dev/tty.usbserial-xxxx --interval 2000 --verbose
pnpm run start -- --transport ble --interval 2000 --verbose
```

## 设备操作

正常页面：

- 长按 B 满 3 秒：进入设置页。
- USB 或 BLE 收到有效数据后显示 `PC Connected`。
- 超过 5 秒未收到有效数据后显示 `Disconnected`。
- 显示 `Disconnected` 后 20 秒先降到最低亮度，60 秒后屏幕背光关闭；按 A 或 B 唤醒。

设置页：

- 短按 B：移动到下一个设置项。
- 短按 A：修改当前设置项。
- 选中 `exit` 后短按 A：退出设置页。
- 设置项超过屏幕高度时会自动滚动，右上角显示当前位置，例如 `3/5`。

当前设置项：

| 设置项 | 取值 | 说明 |
| --- | --- | --- |
| `brightness` | `1/5` 到 `5/5` | 屏幕亮度，依次对应 `20, 40, 60, 80, 100` |
| `battery` | 百分比或 `--` | 当前电池电量估算值，只读 |
| `ble` | `on/off` | BLE 开关，默认 `on` |
| `rotate` | `on/off` | 自动旋转开关，默认 `on` |
| `exit` | - | 短按 A 退出设置页 |

说明：

- `battery` 是按电池电压估算的百分比，短时间跳动不一定代表真实容量快速下降。
- `ble off` 后无法通过 BLE 连接设备，但 USB Serial 仍可使用；需要在设置页重新打开 BLE 才能用 BLE。
- `rotate off` 后不会继续采样 IMU，屏幕保持当前方向。

## 串口协议

USB Serial 和 BLE 共用同一套文本协议。每条消息是一行，以 `\n` 结尾。

启动时发送带时间同步信息的文本：

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

## BLE GATT

| 项 | 值 |
| --- | --- |
| Device Name | `M5Monitor-Plus` |
| Service UUID | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Metrics Characteristic UUID | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| Characteristic Properties | `Write`, `Write Without Response` |
| Payload | UTF-8 协议文本，例如 `C:025|M:060\n` |

BLE 不走系统级蓝牙配对。电脑端按 Service UUID 扫描并连接。

## 常见问题

### 串口上传失败

确认 Node Sender、串口监视器或其他占用串口的程序已经停止，然后重新执行上传命令。

### BLE 找不到设备

确认设置页 `ble` 为 `on`。如果屏幕已经熄灭，可以按 A/B 唤醒后重新执行：

```bash
cd sender
pnpm run list-ble
```

### BLE 原生依赖报错

BLE 使用 Node 依赖 `@abandonware/noble`。如果看到 `No native build was found ...`，用当前 Node 版本重新安装依赖：

```bash
cd sender
pnpm install
pnpm run list-ble
```

### 电量显示下降很快

电量百分比是电压估算值。屏幕背光、BLE、CPU 负载都会影响瞬时电压，短时间看到百分比跳动不一定等同于真实电池容量线性下降。
