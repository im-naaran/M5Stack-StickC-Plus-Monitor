# ESP32 Firmware 说明

本文记录 M5StickC Plus 固件的职责、协议、运行状态和主要模块划分。面向日常使用的启动命令和按键说明见项目根目录 [README](../README.md)。

## 1. 职责边界

ESP32 Firmware 负责通过 USB Serial 或 BLE GATT 接收电脑端数据，解析项目协议，维护本地状态，并刷新 M5StickC Plus LCD。

负责：

- 初始化 M5StickC Plus、LCD、IMU 和 USB Serial，并按持久化设置决定是否初始化 BLE。
- 读取 USB Serial 串口数据。
- 按设置启停 BLE GATT 服务。
- 解析 CPU、内存、时间戳、时区字段。
- 维护连接状态、本地时钟、电池估算、设置页状态。
- 绘制主页面、断连页、设置页。
- 处理 A/B 按键交互。
- 持久化亮度、BLE 开关和自动旋转开关。
- 执行基础省电策略：断连熄屏、熄屏慢轮询、跳过绘制、暂停自动旋转采样、CPU 降频。

不负责：

- 采集电脑系统指标。
- 打开电脑端串口。
- 处理电脑端命令行参数。
- 系统级蓝牙配对。
- 持久化时区、指标、连接状态、电池百分比或当前时间戳。

## 2. 技术栈

- PlatformIO
- Arduino C++
- M5StickCPlus
- ESP32 Arduino BLE

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
    ├── BleReceiver.cpp
    ├── SettingsStore.h
    └── SettingsStore.cpp
```

## 4. 构建配置

`firmware/platformio.ini` 定义固件构建环境：

| 项 | 值 |
| --- | --- |
| env | `m5stick-c-plus` |
| platform | `espressif32` |
| board | `m5stick-c` |
| framework | `arduino` |
| monitor_speed | `115200` |
| lib_deps | `m5stack/M5StickCPlus` |

常用命令：

```bash
cd firmware
pio run
pio run -t upload
pio device monitor
```

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
- `C/M` 允许补零或非补零格式。
- `C/M` 小于 `0` 时按 `0` 处理，大于 `100` 时按 `100` 处理。
- `T` 只接受非负秒级 Unix 时间戳。
- `Z` 接受带符号或不带符号的整数小时偏移。
- 任意已知字段解析成功后，消息视为有效消息。
- 有效消息会更新 `lastUpdateMs` 并设置 `connected = true`。
- 超过 `DISCONNECT_TIMEOUT_MS` 未收到有效消息时，状态变为 disconnected。

USB Serial 参数：

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

USB Serial 和 BLE 共用同一套解析规则。

## 6. 按键和设置页

正常页面：

- B 按住满 `BUTTON_LONG_PRESS_MS`，自动进入设置页。
- 进入设置页不需要松手触发。
- 长按进入设置页后，松手不会再触发一次 B 短按。
- 断连熄屏后，按 A 或 B 会唤醒屏幕。

设置页：

- B 短按：选择下一个设置项。
- A 短按：修改当前设置项。
- 选中 `exit` 后 A 短按：退出设置页。

设置页采用滚动列表。显示层固定显示 `SETTINGS_VISIBLE_ROWS = 4` 行，并根据 `selectedSettingsOption` 计算首个可见选项。右上角显示当前位置，例如 `3/5`。

当前设置项：

| 设置项 | AppState 字段 | 行为 |
| --- | --- | --- |
| `battery` | `batteryPercentKnown` / `batteryPercent` | 只读，显示当前电量估算 |
| `brightness` | `brightnessIndex` | A 短按在 5 档亮度间循环 |
| `ble` | `bleEnabled` | A 短按启停 BLE |
| `rotate` | `autoRotateEnabled` | A 短按开关自动旋转 |
| `exit` | `settingsOpen` | A 短按退出设置页 |

设置项显示顺序为 `battery`、`brightness`、`ble`、`rotate`、`exit`。进入设置页时默认选中 `battery`。

`brightness`、`ble` 和 `rotate` 会写入 ESP32 NVS。普通 `pio run -t upload` 不会清除这些设置；执行 `pio run -t erase` 或代码主动清理 NVS 时才会恢复默认。

持久化字段：

| 字段 | NVS key | 默认值 | 说明 |
| --- | --- | --- | --- |
| `brightnessIndex` | `bright` | `2` | 默认第 3 档亮度 |
| `bleEnabled` | `ble` | `true` | 启动时为 `false` 则不初始化 BLE |
| `autoRotateEnabled` | `rotate` | `true` | 启动时控制是否采样 IMU 自动旋转 |

NVS 兼容策略：

- namespace 为 `m5mon`，schema key 为 `schema`，当前版本为 `1`。
- 老字段不存在时使用默认值。
- 亮度索引超出当前 `BRIGHTNESS_LEVEL_COUNT` 时恢复为 `DEFAULT_BRIGHTNESS_INDEX`。
- 读到未来版本 schema 时清空 `m5mon` namespace 并恢复默认值。
- 时区、CPU/RAM、连接状态、电池百分比和当前时间戳不持久化。

## 7. 省电策略

当前固件使用的是轻量省电策略，不进入 ESP32 light sleep 或 deep sleep。

已启用策略：

- `setCpuFrequencyMhz(80)`：启动时将 CPU 频率设为 `80MHz`。
- 主页面绘制限频：状态变化后也最多按 `MAIN_DISPLAY_REFRESH_INTERVAL_MS` 频率重绘。
- 电池读取限频：`BATTERY_REFRESH_INTERVAL_MS = 5000`。
- BLE 广播间隔：`BLE_ADVERTISING_MIN_INTERVAL_UNITS = 800`，`BLE_ADVERTISING_MAX_INTERVAL_UNITS = 1600`。
- 断连熄屏：进入 disconnected 状态后屏幕保持亮起 `DISCONNECTED_SCREEN_SLEEP_MS`，超时关闭背光。
- 断连降亮：进入 disconnected 状态超过 `DISCONNECTED_SCREEN_DIM_MS` 后先把亮度降到最低档。
- 熄屏慢轮询：屏幕熄灭后 loop delay 从 `LOOP_DELAY_MS` 切到 `SCREEN_SLEEP_LOOP_DELAY_MS`。
- 熄屏跳过绘制：`screenSleeping = true` 时不调用 `displayView.draw()`。
- 熄屏暂停自动旋转采样：`screenSleeping = true` 时不调用 `updateDisplayOrientation()`。
- `rotate off` 时跳过 IMU 加速度采样。
- `ble off` 时停止 BLE advertising，并断开当前 BLE 客户端。

断连熄屏状态机：

```text
connected:
  screen on
  draw main page
  loop delay = 20ms

disconnected + screen on:
  draw disconnected page
  wait 20s
  dim to lowest brightness
  wait until 60s

disconnected + screen sleeping:
  LCD backlight off
  skip draw
  skip orientation sampling
  loop delay = 200ms
  keep USB/BLE/button handling alive

button A/B or valid metric input:
  wake screen
  restore configured brightness
  force next draw
```

注意：电池百分比由电池电压线性估算，短时间跳动不一定代表真实容量线性下降。

## 8. 运行配置

`firmware/src/FirmwareConfig.h` 集中维护固件运行参数：

| 配置 | 当前值 | 含义 |
| --- | ---: | --- |
| `SERIAL_BAUD_RATE` | `115200` | 串口波特率 |
| `CPU_FREQUENCY_MHZ` | `80` | ESP32 CPU 频率 |
| `DISCONNECT_TIMEOUT_MS` | `5000` | 连接超时时间 |
| `BUTTON_LONG_PRESS_MS` | `3000` | B 键进入设置页的长按时间 |
| `LOOP_DELAY_MS` | `20` | 正常 loop 延迟 |
| `SCREEN_SLEEP_LOOP_DELAY_MS` | `200` | 熄屏后的 loop 延迟 |
| `CLOCK_REFRESH_INTERVAL_MS` | `1000` | 本地时钟刷新间隔 |
| `BATTERY_REFRESH_INTERVAL_MS` | `5000` | 电池读取间隔 |
| `MAIN_DISPLAY_REFRESH_INTERVAL_MS` | `250` | 主页面绘制限频 |
| `DISCONNECTED_SCREEN_DIM_MS` | `20000` | 进入 disconnected 状态后的降亮时间 |
| `DISCONNECTED_SCREEN_SLEEP_MS` | `60000` | 进入 disconnected 状态后的自动熄屏时间 |
| `DEFAULT_TIMEZONE_OFFSET_HOURS` | `8` | 默认时区偏移 |
| `DEFAULT_BRIGHTNESS_INDEX` | `2` | 默认亮度索引，对应第 3 档 |
| `BRIGHTNESS_LEVELS` | `20, 40, 60, 80, 100` | 亮度档位 |
| `BLE_ADVERTISING_MIN_INTERVAL_UNITS` | `800` | BLE 广播最小间隔，单位 0.625ms |
| `BLE_ADVERTISING_MAX_INTERVAL_UNITS` | `1600` | BLE 广播最大间隔，单位 0.625ms |
| `BATTERY_EMPTY_VOLTAGE` | `3.20` | 电量估算空电压 |
| `BATTERY_FULL_VOLTAGE` | `4.20` | 电量估算满电压 |

## 9. 模块职责

`src/AppState.h`：

- 定义固件端共享状态结构。
- 存放指标、时间、连接、BLE、自动旋转、亮度、电池、设置页和屏幕睡眠状态。
- 定义 `SettingsOption` 枚举。
- 不包含业务逻辑。

`src/Protocol.h` / `src/Protocol.cpp`：

- 解析一行协议文本。
- 输出字段存在标记和值。
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

- 初始化或停止 BLE Device、GATT Server、Service 和 writable characteristic。
- 广播固定 Service UUID。
- 接收 characteristic 写入的 UTF-8 文本。
- 按 `\n` 拆分完整协议行。
- 使用短队列把 BLE callback 中收到的完整行交给主循环处理。
- 队列满时丢弃最早一行，保留较新的监控数据。
- 不解析协议。
- 不调用 LCD。

`src/SettingsStore.h` / `src/SettingsStore.cpp`：

- 使用 ESP32 Arduino `Preferences` 读写 NVS。
- 保存 `brightnessIndex`、`bleEnabled` 和 `autoRotateEnabled`。
- 读取时处理缺失字段、亮度范围变化和 schema 版本不兼容。
- 不保存时区、指标、连接状态、电池百分比或当前时间戳。

`src/DisplayView.h` / `src/DisplayView.cpp`：

- 初始化 LCD 横屏显示。
- 绘制启动页、主页面、断连页和滚动设置页。
- 显示 CPU、RAM、连接状态、时间、电池和设置项。
- 根据状态差异跳过重复绘制。
- 根据百分比返回绿色、黄色、红色显示颜色。
- 根据亮度档位调用 M5StickC Plus 屏幕亮度接口。
- 控制屏幕背光睡眠和唤醒。

`src/main.cpp`：

- 固件入口。
- 初始化 CPU 频率、M5、NVS 设置、串口接收器、BLE 接收器和显示层。
- 先读取 NVS 设置，再根据 `bleEnabled` 决定是否初始化 BLE。
- 显示层初始化后按 `brightnessIndex` 应用保存的亮度。
- 在主循环中处理串口输入、BLE 输入、本地时钟、电池读取、连接超时、按钮、屏幕电源和屏幕刷新。
- 按字段存在情况独立更新 `AppState`。
- 收到时间戳后建立本地时间基准。
- 实现设置项行为和省电状态机。

## 10. 主循环

```text
setup:
  setCpuFrequencyMhz(80)
  M5.begin()
  M5.Imu.Init()
  settingsStore.load(appState)
  serialReceiver.begin(115200)
  if appState.bleEnabled:
    bleReceiver.begin()
  displayView.begin()
  displayView.setBrightnessByIndex(appState.brightnessIndex)
  displayView.drawBoot()

loop:
  M5.update()
  handleSerialInput()
  handleBleInput()
  updateBleDiagnostics()
  refreshClockText(false)
  updateBatteryState(false)
  handleConnectionTimeout()
  handleButtons()
  updateScreenPower()
  if screen is on:
    updateDisplayOrientation()
    displayView.draw(appState)
  delay(loopDelayMs())
```

状态更新规则：

- 解析失败的消息直接丢弃。
- 解析成功的字段独立更新对应状态。
- 收到有效消息后立即设置 `connected = true`。
- 超过 5 秒未收到有效消息后设置 `connected = false`。
- 断开状态不清空最后一次指标值。
- BLE 关闭时不处理 BLE 队列输入。
- 自动旋转关闭时不采样 IMU。

## 11. UI 规则

- 屏幕使用横屏，目标分辨率按 `240 x 135` 设计。
- 背景使用深色。
- CPU 和 RAM 使用大字号显示。
- 底部显示 PC 连接状态和本地时间。
- 正常状态显示 `PC Connected`。
- 断开状态显示 `PC Disconnected` 和 `Disconnected`。
- BLE 关闭时断连页显示 `BLE off`。
- 数值颜色阈值：
  - `0-59`：绿色。
  - `60-84`：黄色。
  - `85-100`：红色。

## 12. 开发边界

- `main.cpp` 负责流程编排和业务状态切换。
- `DisplayView` 负责显示和背光控制。
- `Protocol` 负责解析。
- `SerialReceiver` 负责收集完整行。
- `BleReceiver` 负责 BLE GATT 接入和收集完整行。
- 串口协议字段只追加，不改变现有 `C`、`M` 含义。
- 串口协议变更需要同步更新 Node Sender 文档和实现。
