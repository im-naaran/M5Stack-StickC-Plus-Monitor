# ESP32 Firmware 规格

## 1. 职责边界

ESP32 Firmware 是 M5StickC Plus 固件，负责通过 USB Serial 或 BLE GATT 接收电脑端数据，解析项目协议，维护本地显示状态，并刷新 LCD。

负责：

- 初始化 M5StickC Plus。
- 读取 USB Serial 串口数据。
- 暴露 BLE GATT 自定义服务并接收写入数据。
- 解析 CPU、内存、时间戳、时区字段。
- 维护连接状态。
- 维护 ESP32 本地时钟。
- 绘制固定横屏仪表盘。
- Button B 调节屏幕亮度。

不负责：

- 采集电脑系统指标。
- 打开电脑端串口。
- 处理电脑端命令行参数。
- 系统级蓝牙配对。
- 多显示模式。

## 2. 技术栈

- PlatformIO。
- Arduino C++。
- M5StickCPlus。

## 3. 目录结构

```text
firmware/
├── platformio.ini
└── src/
    ├── main.cpp
    ├── AppState.h
    ├── FirmwareConfig.h
    ├── Protocol.h
    ├── Protocol.cpp
    ├── DisplayView.h
    ├── DisplayView.cpp
    ├── SerialReceiver.h
    ├── SerialReceiver.cpp
    ├── BleReceiver.h
    └── BleReceiver.cpp
```

## 4. 构建配置

`firmware/platformio.ini` 定义 M5StickC Plus 固件构建环境：

- env：`m5stick-c-plus`
- platform：`espressif32`
- board：`m5stick-c`
- framework：`arduino`
- monitor_speed：`115200`
- lib_deps：`m5stack/M5StickCPlus`

## 5. 传输协议

每条消息是一行文本，以 `\n` 结尾。字段之间使用 `|` 分隔，key 与 value 之间使用 `:` 分隔。

首次携带时间同步字段的消息：

```text
C:025|M:060|T:1714440000|Z:+8\n
```

仅携带指标字段的消息：

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

解析规则：

- `C/M/T/Z` 均为可选字段。
- 字段存在且解析成功时，只更新该字段对应的状态。
- 字段缺失时，不修改对应状态。
- 未知字段忽略。
- `C/M` 允许补零格式，例如 `C:025|M:060`。
- `C/M` 允许非补零格式，例如 `C:5|M:60`。
- `C/M` 小于 `0` 时按 `0` 处理，大于 `100` 时按 `100` 处理。
- `T` 只接受非负秒级 Unix 时间戳。
- `Z` 接受带符号或不带符号的整数小时偏移，例如 `Z:+8`、`Z:8`、`Z:-5`。
- 任意已知字段解析成功后，消息视为有效消息。
- 有效消息会更新 `lastUpdateMs` 并设置 `connected = true`。
- 超过 `5000ms` 未收到有效消息时，状态变为 disconnected。

串口参数：

- Baud rate：`115200`
- Line ending：`\n`

BLE GATT 参数：

| 项 | 值 |
| --- | --- |
| Device Name | `M5Monitor-Plus` |
| Service UUID | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Metrics Characteristic UUID | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| Characteristic Properties | `Write`, `Write Without Response` |
| Payload | UTF-8 文本协议 |

USB Serial 和 BLE 共用同一套解析规则。第一版 BLE 不启用系统级 pairing/bonding；电脑端按 Service UUID 扫描并连接。

## 6. 时间规则

- 默认时区偏移为 `+8`，来自 `FirmwareConfig::DEFAULT_TIMEZONE_OFFSET_HOURS`。
- 收到有效 `Z` 字段时，更新本地时区偏移。
- 收到有效 `T` 字段时，记录 `syncedEpochSeconds` 和当前 `millis()`。
- 收到 `T` 后，ESP32 通过 `millis()` 计算已流逝秒数并维护本地时间。
- 与电脑端断开后，只要 ESP32 仍在运行，本地时间继续推进。
- 未收到过有效 `T` 前，时间显示为 `--:--`。
- 本地时钟刷新间隔为 `1000ms`。
- 屏幕显示时间格式为 `HH:MM`。
- 显示层只在状态变化时重绘；当 `HH:MM` 未变化时，时间文本不会触发可见变化。

## 7. 运行配置

`firmware/src/FirmwareConfig.h` 集中维护固件运行参数：

| 配置 | 值 | 含义 |
| --- | ---: | --- |
| `SERIAL_BAUD_RATE` | `115200` | 串口波特率 |
| `BLE_DEVICE_NAME` | `M5Monitor-Plus` | BLE 广播设备名 |
| `BLE_SERVICE_UUID` | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | BLE GATT 服务 UUID |
| `BLE_METRICS_CHARACTERISTIC_UUID` | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | BLE 指标写入 characteristic UUID |
| `DISCONNECT_TIMEOUT_MS` | `5000` | 连接超时时间 |
| `BUTTON_DEBOUNCE_MS` | `25` | 按钮消抖时间 |
| `LOOP_DELAY_MS` | `10` | 主循环延迟 |
| `CLOCK_REFRESH_INTERVAL_MS` | `1000` | 本地时钟刷新间隔 |
| `DEFAULT_TIMEZONE_OFFSET_HOURS` | `8` | 默认时区偏移 |
| `DEFAULT_BRIGHTNESS_INDEX` | `1` | 默认亮度档位 |
| `BRIGHTNESS_LEVELS` | `20, 60, 100` | 亮度档位 |

## 8. 模块职责

`src/AppState.h`：

- 定义固件端共享状态结构。
- 存放指标、时间、连接、亮度等 UI 状态。
- 不包含业务逻辑。

`src/Protocol.h` / `src/Protocol.cpp`：

- 解析一行协议文本。
- 输出字段存在标记：`hasCpu`、`hasMemory`、`hasTimestamp`、`hasTimezone`。
- 输出字段值：CPU、内存、Unix 时间戳、时区偏移。
- 对 CPU 和内存百分比执行 `0-100` 裁剪。
- 对时间戳和时区执行范围校验。
- 不读取串口。
- 不调用 LCD。
- 不直接修改 `AppState`。

`src/SerialReceiver.h` / `src/SerialReceiver.cpp`：

- 封装串口逐字节读取。
- 缓存当前行内容。
- 遇到 `\n` 后向调用方返回完整行。
- 忽略 `\r`。
- 限制单行最大长度，异常超长输入会清空缓冲。

`src/BleReceiver.h` / `src/BleReceiver.cpp`：

- 初始化 BLE Device、GATT Server、Service 和 writable characteristic。
- 广播固定 Service UUID。
- 接收 characteristic 写入的 UTF-8 文本。
- 按 `\n` 拆分完整协议行。
- 使用短队列把 BLE callback 中收到的完整行交给主循环处理。
- 不解析协议。
- 不调用 LCD。

`src/DisplayView.h` / `src/DisplayView.cpp`：

- 初始化 LCD 横屏显示。
- 绘制启动页、连接页、断开页和底部状态栏。
- 显示 CPU、RAM、连接状态和 `HH:MM` 时间。
- 根据状态差异跳过重复绘制。
- 根据百分比返回绿色、黄色、红色显示颜色。
- 根据亮度档位调用 M5StickC Plus 屏幕亮度接口。

`src/main.cpp`：

- 固件入口。
- 初始化 M5、串口接收器和显示层。
- 在主循环中处理串口输入、BLE 输入、本地时钟、连接超时、按钮和屏幕刷新。
- 按字段存在情况独立更新 `AppState`。
- 收到时间戳后建立本地时间基准。

## 9. 主循环

```text
setup:
  M5.begin()
  serialReceiver.begin(115200)
  bleReceiver.begin()
  displayView.begin()
  displayView.drawBoot()

loop:
  M5.update()
  handleSerialInput()
  handleBleInput()
  updateClock()
  handleConnectionTimeout()
  handleButtons()
  displayView.draw(appState)
  delay(10)
```

状态更新规则：

- 解析失败的消息直接丢弃。
- 解析成功的字段独立更新对应状态。
- 收到有效消息后立即设置 `connected = true`。
- 超过 5 秒未收到有效消息后设置 `connected = false`。
- 断开状态不清空最后一次指标值。
- Button B 短按在低、中、高三档亮度中循环。
- Button A 当前版本不绑定功能。

## 10. UI 规则

- 屏幕使用横屏，目标分辨率按 `240 x 135` 设计。
- 背景使用深色。
- CPU 和 RAM 使用大字号显示。
- 底部显示 PC 连接状态和本地时间。
- 正常状态显示 `PC Connected`。
- 断开状态显示 `PC Disconnected` 和 `Disconnected`。
- 数值颜色阈值：
  - `0-59`：绿色。
  - `60-84`：黄色。
  - `85-100`：红色。

## 11. 开发边界

- `main.cpp` 负责流程编排。
- `DisplayView` 负责显示。
- `Protocol` 负责解析。
- `SerialReceiver` 负责收集完整行。
- `BleReceiver` 负责 BLE GATT 接入和收集完整行。
- 串口协议字段只追加，不改变现有 `C`、`M` 含义。
- 串口协议变更需要同步更新 Node Sender 文档和实现。
