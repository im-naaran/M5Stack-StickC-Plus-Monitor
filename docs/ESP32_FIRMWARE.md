# ESP32 Firmware 开发规格

## 1. 职责边界

ESP32 Firmware 是 M5StickC Plus 固件，负责通过 USB Serial 接收电脑端数据，解析协议，并刷新 LCD。

负责：

- 初始化 M5StickC Plus。
- 读取 USB Serial 串口数据。
- 解析 CPU、内存指标。
- 维护连接状态。
- 绘制固定横屏仪表盘。
- Button B 调节屏幕亮度。

不负责：

- 采集电脑系统指标。
- 打开电脑端串口。
- 命令行参数。
- 无线通信。
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
    └── SerialReceiver.cpp
```

## 4. 串口协议

Node Sender 每隔 1-2 秒发送一行文本：

```text
C:025|M:060|T:2241\n
```

字段说明：

| 字段 | 含义 | 类型 | 范围 | 必填 |
| --- | --- | --- | --- | --- |
| `C` | CPU 使用率 | 整数百分比 | `0-100` | 是 |
| `M` | 内存使用率 | 整数百分比 | `0-100` | 是 |
| `T` | 电脑本地时间 | `HHMM` | `0000-2359` | 否 |

解析规则：

- 字段之间使用 `|` 分隔。
- key 与 value 之间使用 `:` 分隔。
- 接收端必须允许补零格式，例如 `C:025|M:060`。
- 接收端必须允许非补零格式，例如 `C:5|M:60`。
- 接收端允许时间字段，例如 `T:2241`，用于底部时间显示为 `22:41`。
- 未知字段忽略。
- 缺少 `C` 或 `M` 时认为消息无效。
- 百分比小于 `0` 时按 `0` 处理。
- 百分比大于 `100` 时按 `100` 处理。
- 解析成功后更新 `lastUpdateMs`。
- 超过 `5000ms` 未收到有效消息时，状态为 disconnected。

串口参数：

- Baud rate：`115200`
- Line ending：`\n`

## 5. platformio.ini

职责：

- 定义 M5StickC Plus 固件构建环境。
- 固定串口监视器波特率。
- 声明 M5StickCPlus 依赖。

建议配置：

```ini
[env:m5stick-c-plus]
platform = espressif32
board = m5stick-c
framework = arduino
monitor_speed = 115200

lib_deps =
    m5stack/M5StickCPlus
```

如果本地 PlatformIO 已支持更准确的 M5StickC Plus board id，可后续替换 `board`。

## 6. src/AppState.h

职责：

- 定义固件端共享状态结构。
- 存放当前指标、连接状态、亮度等 UI 状态。
- 只放简单数据结构，不写业务逻辑。

建议结构：

```cpp
#pragma once

#include <Arduino.h>
#include "FirmwareConfig.h"

struct MetricsState {
  int cpuPercent = 0;
  int memoryPercent = 0;
};

struct AppState {
  MetricsState metrics;
  String timeText = "--:--";
  bool connected = false;
  unsigned long lastUpdateMs = 0;
  uint8_t brightnessIndex = FirmwareConfig::DEFAULT_BRIGHTNESS_INDEX;
  bool screenOn = true;
};
```

字段说明：

- `cpuPercent`：当前 CPU 使用率。
- `memoryPercent`：当前内存使用率。
- `timeText`：底部显示时间，格式为 `HH:MM`。
- `connected`：是否在最近 5 秒内收到有效数据。
- `lastUpdateMs`：最后一次有效消息时间，来自 `millis()`。
- `brightnessIndex`：当前亮度档位索引。
- `screenOn`：后续支持息屏时使用，本版本可先保留。

## 7. src/Protocol.h

职责：

- 声明协议解析函数。
- 提供百分比裁剪函数。
- 不依赖 LCD、Serial、按钮等硬件逻辑。

建议接口：

```cpp
#pragma once

#include <Arduino.h>
#include "AppState.h"

struct ParseResult {
  bool ok = false;
  MetricsState metrics;
  bool hasTime = false;
  String timeText;
};

ParseResult parseMetricsLine(const String& line);
int clampPercent(int value);
```

函数说明：

- `parseMetricsLine(const String& line)`：解析一行协议文本，成功返回 `ok = true`。
- `clampPercent(int value)`：把数值限制到 `0-100`。

## 8. src/Protocol.cpp

职责：

- 实现 `C:025|M:060|T:2241` 文本解析。
- 忽略未知字段。
- 对异常输入返回失败，不修改全局状态。

建议实现逻辑：

```text
parseMetricsLine(line):
  去除首尾空白
  初始化 hasCpu = false, hasMemory = false
  按 | 拆分 token
  对每个 token:
    查找 :
    取 key 和 value
    如果 key == C:
      解析整数并 clamp
      hasCpu = true
    如果 key == M:
      解析整数并 clamp
      hasMemory = true
    如果 key == T:
      解析 HHMM 并转为 HH:MM
    其他 key 忽略
  如果 hasCpu && hasMemory:
    ok = true
  否则:
    ok = false
```

注意事项：

- 不要在解析函数里调用 `Serial.print`。
- 不要在解析函数里更新 `AppState`，由 `main.cpp` 决定是否应用结果。
- 输入可能包含 `\r`，需要 trim。

## 9. src/SerialReceiver.h

职责：

- 封装串口逐字节读取。
- 缓存一行消息，遇到 `\n` 后交给调用方处理。
- 避免在 `main.cpp` 中散落字符串拼接逻辑。

建议接口：

```cpp
#pragma once

#include <Arduino.h>

class SerialReceiver {
public:
  void begin(unsigned long baudRate);
  bool readLine(String& outLine);
  void clear();

private:
  String buffer;
  static const size_t MAX_LINE_LENGTH = 64;
};
```

函数说明：

- `begin(unsigned long baudRate)`：调用 `Serial.begin(baudRate)`。
- `readLine(String& outLine)`：如果读到完整一行，返回 `true` 并写入 `outLine`。
- `clear()`：清空内部缓冲。

实现要求：

- 忽略 `\r`。
- 遇到 `\n` 返回当前缓冲并清空。
- 缓冲长度超过 `MAX_LINE_LENGTH` 时清空，防止异常输入占用内存。
- 单次调用尽量快速，适合在 `loop()` 中频繁执行。

## 10. src/SerialReceiver.cpp

职责：

- 实现串口读取细节。

建议实现逻辑：

```text
readLine(outLine):
  while Serial.available() > 0:
    ch = Serial.read()
    如果 ch == '\r':
      continue
    如果 ch == '\n':
      outLine = buffer
      buffer = ""
      return outLine.length() > 0
    否则:
      buffer += ch
      如果 buffer.length() > MAX_LINE_LENGTH:
        buffer = ""
        return false
  return false
```

## 11. src/DisplayView.h

职责：

- 声明所有 LCD 绘制函数。
- 屏幕布局只在该模块维护。
- 对外只暴露“绘制状态”的接口。

建议接口：

```cpp
#pragma once

#include <Arduino.h>
#include "AppState.h"

class DisplayView {
public:
  void begin();
  void drawBoot();
  void draw(const AppState& state);
  void drawDisconnected(const AppState& state);
  void setBrightnessByIndex(uint8_t index);

private:
  void drawLayout();
  void drawMetricBlock(int x, int y, const char* label, int percent);
  void drawProgressBar(int x, int y, int w, int h, int percent, uint16_t color);
  void drawFooter(const AppState& state);
  uint16_t colorForPercent(int percent);

  AppState lastDrawnState;
  bool hasLastDrawnState = false;
};
```

函数说明：

- `begin()`：初始化屏幕方向、字体、背景色。
- `drawBoot()`：绘制启动页面，例如 `Waiting for PC...`。
- `draw(const AppState& state)`：根据当前状态绘制主界面。
- `drawDisconnected(const AppState& state)`：绘制断开提示。
- `setBrightnessByIndex(uint8_t index)`：根据亮度档位设置屏幕亮度。
- `drawLayout()`：绘制固定标题、分隔线等不频繁变化的元素。
- `drawMetricBlock(...)`：绘制单个指标块，例如 CPU 或 RAM。
- `drawProgressBar(...)`：绘制百分比进度条。
- `drawFooter(...)`：绘制底部状态，例如 `USB Connected`。
- `colorForPercent(...)`：根据百分比返回绿色、黄色、红色。

## 12. src/DisplayView.cpp

职责：

- 实现所有屏幕绘制。
- 统一维护坐标、颜色、字体大小。

UI 规范：

- 屏幕使用横屏，目标分辨率按 `240 x 135` 设计。
- 背景建议使用黑色或深色。
- CPU 和 RAM 数字要大，优先保证可读。
- 底部状态文字要小，不抢占主要指标空间。
- 避免高频整屏清空，优先局部刷新数值区域。

建议布局：

```text
+--------------------------------+
| CPU  25%        RAM  60%        |
| █████░░░░░      ██████░░░░      |
|                                |
|                                |
| USB Connected                  |
+--------------------------------+
```

建议坐标：

| 元素 | x | y | w | h |
| --- | ---: | ---: | ---: | ---: |
| CPU 标题/数字 | 12 | 16 | 96 | 44 |
| CPU 进度条 | 12 | 64 | 96 | 10 |
| RAM 标题/数字 | 132 | 16 | 96 | 44 |
| RAM 进度条 | 132 | 64 | 96 | 10 |
| 底部状态 | 12 | 112 | 216 | 16 |

颜色阈值：

- `0-59`：绿色。
- `60-84`：黄色。
- `85-100`：红色。

绘制要求：

- `begin()` 中调用 `M5.Lcd.setRotation(1)`。
- `drawBoot()` 首次清屏。
- `draw()` 中根据 `connected` 决定绘制正常状态或断开状态。
- 如果状态没有变化，可以跳过重绘，减少闪烁。
- 绘制数字前先用背景色填充旧数字区域。

## 13. src/FirmwareConfig.h

职责：

- 集中维护固件运行参数。
- 避免串口、超时、亮度档位等常量散落在业务代码中。

当前配置：

```cpp
namespace FirmwareConfig {
static const unsigned long SERIAL_BAUD_RATE = 115200;
static const unsigned long DISCONNECT_TIMEOUT_MS = 5000;
static const unsigned long BUTTON_DEBOUNCE_MS = 25;
static const unsigned long LOOP_DELAY_MS = 10;

static const uint8_t DEFAULT_BRIGHTNESS_INDEX = 1;
static const uint8_t BRIGHTNESS_LEVELS[] = { 20, 60, 100 };
}
```

## 14. src/main.cpp

职责：

- 固件入口。
- 初始化 M5、串口接收、屏幕。
- 维护 `AppState`。
- 调用协议解析和屏幕绘制。
- 处理按钮。
- 使用 `FirmwareConfig.h` 中的统一配置。

建议全局对象：

```cpp
AppState appState;
SerialReceiver serialReceiver;
DisplayView displayView;
```

建议函数：

```cpp
void setup();
void loop();
void handleSerialInput();
void handleConnectionTimeout();
void handleButtons();
void applyMetrics(const ParseResult& result);
void cycleBrightness();
```

函数说明：

- `setup()`：
  - `M5.begin()`。
  - 初始化串口。
  - 初始化屏幕。
  - 绘制启动页。
- `loop()`：
  - `M5.update()`。
  - 调用 `handleSerialInput()`。
  - 调用 `handleConnectionTimeout()`。
  - 调用 `handleButtons()`。
  - 调用 `displayView.draw(appState)`。
- `handleSerialInput()`：
  - 从 `SerialReceiver` 读取完整行。
  - 调用 `parseMetricsLine()`。
  - 解析成功后调用 `applyMetrics()`。
- `handleConnectionTimeout()`：
  - 如果 `millis() - lastUpdateMs > DISCONNECT_TIMEOUT_MS`，设置 `connected = false`。
- `handleButtons()`：
  - Button B 短按调用 `cycleBrightness()`。
  - Button A 本版本不绑定功能，预留给后续显示模式。
- `applyMetrics(...)`：
  - 更新 CPU、RAM。
  - 如果消息包含时间字段，更新时间显示文本。
  - 设置 `connected = true`。
  - 更新 `lastUpdateMs = millis()`。
- `cycleBrightness()`：
  - 在亮度档位中循环。
  - 调用 `displayView.setBrightnessByIndex()`。

运行逻辑：

```text
setup:
  M5.begin()
  serialReceiver.begin(FirmwareConfig::SERIAL_BAUD_RATE)
  displayView.begin()
  displayView.drawBoot()

loop:
  M5.update()
  handleSerialInput()
  handleConnectionTimeout()
  handleButtons()
  displayView.draw(appState)
  delay(FirmwareConfig::LOOP_DELAY_MS)
```

状态更新规则：

- 只有解析成功的消息会更新 `metrics`。
- 解析失败的消息直接丢弃。
- 收到有效消息后立即设置 `connected = true`。
- 超过 5 秒未收到有效消息后设置 `connected = false`。
- 断开状态不清空最后一次指标值，可由 UI 决定是否显示旧值。

按钮规则：

- Button A：本版本预留，不绑定功能；后续可用于切换显示模式。
- Button B 短按：亮度在低、中、高三档循环。
- Button B 长按：本版本暂不实现；后续可用于息屏。

## 15. 开发规范

- `main.cpp` 只做流程编排，不堆积绘制细节。
- `DisplayView` 只负责显示，不读取串口。
- `Protocol` 只负责解析，不直接修改全局状态。
- `SerialReceiver` 只负责收集完整行，不解析业务字段。
- 避免长时间阻塞 `loop()`。
- 避免频繁整屏刷新造成闪烁。
- 字符串缓冲要限制长度，避免异常输入占用内存。
- 字段命名与协议保持一致，`C` 对应 `cpuPercent`，`M` 对应 `memoryPercent`。
- 串口协议变更必须同步通知 Node Sender 开发者。
- 新增协议字段时只追加，不改变现有 `C`、`M` 含义。

## 16. 后续预留项

以下内容不在本次开发范围内：

- 显示模式：Button A 后续可用于切换仪表盘、历史曲线、详细列表。
- 扩展指标：协议可追加 `U`、`D`、`G`、`T` 等字段。
- 息屏控制：Button B 长按后续可用于息屏 / 唤醒。
- 无线连接：后续应作为新的 transport 实现，不改动现有串口接收和显示模块。
