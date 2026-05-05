#include "DisplayView.h"

#include <M5StickCPlus.h>
#include "FirmwareConfig.h"

namespace {
const uint16_t COLOR_BACKGROUND = 0x0841;
const uint16_t COLOR_PANEL = 0x10A2;
const uint16_t COLOR_TEXT = TFT_WHITE;
const uint16_t COLOR_MUTED = 0x9CF3;
const uint16_t COLOR_DIM = 0x4208;
const uint16_t COLOR_GREEN = 0x07E0;
const uint16_t COLOR_YELLOW = 0xFFE0;
const uint16_t COLOR_RED = 0xF800;
const uint8_t ROTATION_LANDSCAPE = 1;
const uint8_t ROTATION_LANDSCAPE_INVERTED = 3;
const uint8_t SETTINGS_VISIBLE_ROWS = 4;
const uint8_t SETTINGS_ROW_START_Y = 42;
const uint8_t SETTINGS_ROW_HEIGHT = 22;

const char* settingLabel(uint8_t option) {
  switch (option) {
    case SETTINGS_OPTION_BRIGHTNESS:
      return "brightness";
    case SETTINGS_OPTION_BATTERY:
      return "battery";
    case SETTINGS_OPTION_BLE:
      return "ble";
    case SETTINGS_OPTION_ROTATE:
      return "rotate";
    case SETTINGS_OPTION_EXIT:
      return "exit";
    default:
      return "";
  }
}

String settingValue(const AppState& state, uint8_t option) {
  switch (option) {
    case SETTINGS_OPTION_BRIGHTNESS:
      return String(state.brightnessIndex + 1) + "/" + FirmwareConfig::BRIGHTNESS_LEVEL_COUNT;
    case SETTINGS_OPTION_BATTERY:
      return state.batteryPercentKnown ? String(state.batteryPercent) + "%" : "--";
    case SETTINGS_OPTION_BLE:
      return state.bleEnabled ? "on" : "off";
    case SETTINGS_OPTION_ROTATE:
      return state.autoRotateEnabled ? "on" : "off";
    default:
      return "";
  }
}
}

void DisplayView::begin() {
  applyRotation();
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  M5.Lcd.fillScreen(COLOR_BACKGROUND);
}

void DisplayView::drawBoot() {
  M5.Lcd.fillScreen(COLOR_BACKGROUND);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  M5.Lcd.drawString("Waiting for PC", 120, 58);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  M5.Lcd.drawString("USB Serial / BLE RX", 120, 82);
  M5.Lcd.setTextDatum(TL_DATUM);
  hasLastDrawnState = false;
}

void DisplayView::draw(const AppState& state) {
  if (state.settingsOpen) {
    drawSettings(state);
    return;
  }

  if (!state.connected) {
    drawDisconnected(state);
    return;
  }

  if (!stateChanged(state)) {
    return;
  }

  unsigned long now = millis();
  if (hasLastDrawnState &&
      lastDrawnState.connected &&
      !lastDrawnState.settingsOpen &&
      now - lastMainDrawMs < FirmwareConfig::MAIN_DISPLAY_REFRESH_INTERVAL_MS) {
    return;
  }

  drawLayout();
  drawMetricBlock(12, 18, "CPU", state.metrics.cpuPercent);
  drawMetricBlock(132, 18, "RAM", state.metrics.memoryPercent);
  drawProgressBar(12, 70, 96, 10, state.metrics.cpuPercent, colorForPercent(state.metrics.cpuPercent));
  drawProgressBar(132, 70, 96, 10, state.metrics.memoryPercent, colorForPercent(state.metrics.memoryPercent));
  drawFooter(state);

  lastDrawnState = state;
  hasLastDrawnState = true;
  lastMainDrawMs = now;
}

void DisplayView::drawSettings(const AppState& state) {
  if (!settingsStateChanged(state)) {
    return;
  }

  M5.Lcd.fillScreen(COLOR_BACKGROUND);
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  M5.Lcd.drawString("settings", 12, 12);
  M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  M5.Lcd.drawRightString(
    String(state.selectedSettingsOption + 1) + "/" + SETTINGS_OPTION_COUNT,
    228,
    12,
    1);
  M5.Lcd.drawFastHLine(12, 36, 216, COLOR_DIM);

  uint8_t firstVisibleOption = 0;
  if (state.selectedSettingsOption >= SETTINGS_VISIBLE_ROWS) {
    firstVisibleOption = state.selectedSettingsOption - SETTINGS_VISIBLE_ROWS + 1;
  }

  for (uint8_t row = 0; row < SETTINGS_VISIBLE_ROWS; ++row) {
    uint8_t option = firstVisibleOption + row;
    if (option >= SETTINGS_OPTION_COUNT) {
      break;
    }

    drawSettingRow(
      SETTINGS_ROW_START_Y + row * SETTINGS_ROW_HEIGHT,
      state.selectedSettingsOption == option,
      settingLabel(option),
      settingValue(state, option));
  }

  lastDrawnState = state;
  hasLastDrawnState = true;
}

void DisplayView::drawDisconnected(const AppState& state) {
  if (hasLastDrawnState && !lastDrawnState.connected &&
      !lastDrawnState.settingsOpen &&
      lastDrawnState.timeText == state.timeText &&
      lastDrawnState.bleClientConnected == state.bleClientConnected &&
      lastDrawnState.bleWriteCount == state.bleWriteCount &&
      lastDrawnState.bleLineCount == state.bleLineCount &&
      lastDrawnState.bleEnabled == state.bleEnabled &&
      lastDrawnState.brightnessIndex == state.brightnessIndex) {
    return;
  }

  M5.Lcd.fillScreen(COLOR_BACKGROUND);
  M5.Lcd.drawRoundRect(10, 10, 220, 104, 4, COLOR_DIM);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(COLOR_RED, COLOR_BACKGROUND);
  M5.Lcd.drawString("Disconnected", 120, 48);
  M5.Lcd.setTextSize(1);
  if (!state.bleEnabled) {
    M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
    M5.Lcd.drawString("BLE off", 120, 76);
  } else if (state.bleClientConnected) {
    M5.Lcd.setTextColor(COLOR_GREEN, COLOR_BACKGROUND);
    M5.Lcd.drawString(
      String("BLE linked W:") + state.bleWriteCount + " L:" + state.bleLineCount,
      120,
      76);
  } else {
    M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
    M5.Lcd.drawString("Waiting for valid PC data", 120, 76);
  }
  M5.Lcd.setTextDatum(TL_DATUM);
  drawFooter(state);

  lastDrawnState = state;
  hasLastDrawnState = true;
}

void DisplayView::setBrightnessByIndex(uint8_t index) {
  uint8_t safeIndex = index;
  if (safeIndex >= FirmwareConfig::BRIGHTNESS_LEVEL_COUNT) {
    safeIndex = FirmwareConfig::BRIGHTNESS_LEVEL_COUNT - 1;
  }

  M5.Axp.ScreenBreath(FirmwareConfig::BRIGHTNESS_LEVELS[safeIndex]);
}

void DisplayView::sleepScreen() {
  M5.Axp.ScreenSwitch(false);
}

void DisplayView::wakeScreen(uint8_t brightnessIndex) {
  M5.Axp.ScreenSwitch(true);
  setBrightnessByIndex(brightnessIndex);
  hasLastDrawnState = false;
}

void DisplayView::setInverted(bool inverted) {
  if (screenInverted == inverted) {
    return;
  }

  screenInverted = inverted;
  applyRotation();
  M5.Lcd.fillScreen(COLOR_BACKGROUND);
  hasLastDrawnState = false;
}

bool DisplayView::isInverted() const {
  return screenInverted;
}

void DisplayView::applyRotation() {
  M5.Lcd.setRotation(screenInverted ? ROTATION_LANDSCAPE_INVERTED : ROTATION_LANDSCAPE);
  M5.Lcd.setTextDatum(TL_DATUM);
}

void DisplayView::drawLayout() {
  M5.Lcd.fillScreen(COLOR_BACKGROUND);
  M5.Lcd.drawRoundRect(8, 8, 224, 94, 4, COLOR_DIM);
  M5.Lcd.drawFastVLine(120, 18, 64, COLOR_DIM);
}

void DisplayView::drawMetricBlock(int x, int y, const char* label, int percent) {
  M5.Lcd.fillRect(x, y, 96, 46, COLOR_BACKGROUND);
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  M5.Lcd.drawString(label, x, y);

  M5.Lcd.setTextSize(4);
  M5.Lcd.setTextColor(colorForPercent(percent), COLOR_BACKGROUND);
  M5.Lcd.drawRightString(String(percent), x + 76, y + 18, 1);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString("%", x + 80, y + 30);
}

void DisplayView::drawProgressBar(int x, int y, int w, int h, int percent, uint16_t color) {
  int fillWidth = (w * percent) / 100;
  M5.Lcd.drawRoundRect(x, y, w, h, 3, COLOR_DIM);
  M5.Lcd.fillRect(x + 2, y + 2, w - 4, h - 4, COLOR_PANEL);
  if (fillWidth > 4) {
    M5.Lcd.fillRect(x + 2, y + 2, fillWidth - 4, h - 4, color);
  }
}

void DisplayView::drawFooter(const AppState& state) {
  M5.Lcd.fillRect(0, 108, 240, 27, COLOR_BACKGROUND);
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextSize(1);

  if (state.connected) {
    M5.Lcd.setTextColor(COLOR_GREEN, COLOR_BACKGROUND);
    M5.Lcd.drawString("PC Connected", 12, 116);
  } else {
    M5.Lcd.setTextColor(COLOR_RED, COLOR_BACKGROUND);
    M5.Lcd.drawString("PC Disconnected", 12, 116);
  }

  M5.Lcd.setTextColor(COLOR_MUTED, COLOR_BACKGROUND);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawRightString(state.timeText, 228, 112, 1);
}

void DisplayView::drawSettingRow(int y, bool selected, const char* label, const String& value) {
  uint16_t textColor = selected ? COLOR_TEXT : COLOR_MUTED;
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(selected ? COLOR_GREEN : COLOR_DIM, COLOR_BACKGROUND);
  M5.Lcd.drawString(selected ? ">" : " ", 14, y);
  M5.Lcd.setTextColor(textColor, COLOR_BACKGROUND);
  M5.Lcd.drawString(label, 34, y);

  if (value.length() > 0) {
    M5.Lcd.drawRightString(value, 224, y, 1);
  }
}

uint16_t DisplayView::colorForPercent(int percent) {
  if (percent >= 85) {
    return COLOR_RED;
  }

  if (percent >= 60) {
    return COLOR_YELLOW;
  }

  return COLOR_GREEN;
}

bool DisplayView::stateChanged(const AppState& state) const {
  if (!hasLastDrawnState) {
    return true;
  }

  return lastDrawnState.connected != state.connected ||
         lastDrawnState.metrics.cpuPercent != state.metrics.cpuPercent ||
         lastDrawnState.metrics.memoryPercent != state.metrics.memoryPercent ||
         lastDrawnState.timeText != state.timeText ||
         lastDrawnState.settingsOpen != state.settingsOpen;
}

bool DisplayView::settingsStateChanged(const AppState& state) const {
  if (!hasLastDrawnState) {
    return true;
  }

  return !lastDrawnState.settingsOpen ||
         lastDrawnState.selectedSettingsOption != state.selectedSettingsOption ||
         lastDrawnState.brightnessIndex != state.brightnessIndex ||
         lastDrawnState.bleEnabled != state.bleEnabled ||
         lastDrawnState.autoRotateEnabled != state.autoRotateEnabled ||
         lastDrawnState.batteryPercentKnown != state.batteryPercentKnown ||
         lastDrawnState.batteryPercent != state.batteryPercent;
}
