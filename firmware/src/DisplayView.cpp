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
}

void DisplayView::begin() {
  applyRotation();
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  M5.Lcd.fillScreen(COLOR_BACKGROUND);
  setBrightnessByIndex(FirmwareConfig::DEFAULT_BRIGHTNESS_INDEX);
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
  if (!state.connected) {
    drawDisconnected(state);
    return;
  }

  if (!stateChanged(state)) {
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
}

void DisplayView::drawDisconnected(const AppState& state) {
  if (hasLastDrawnState && !lastDrawnState.connected &&
      lastDrawnState.timeText == state.timeText &&
      lastDrawnState.bleClientConnected == state.bleClientConnected &&
      lastDrawnState.bleWriteCount == state.bleWriteCount &&
      lastDrawnState.bleLineCount == state.bleLineCount &&
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
  if (state.bleClientConnected) {
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

void DisplayView::setInverted(bool inverted) {
  if (screenInverted == inverted) {
    return;
  }

  screenInverted = inverted;
  applyRotation();
  M5.Lcd.fillScreen(COLOR_BACKGROUND);
  hasLastDrawnState = false;
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
         lastDrawnState.bleClientConnected != state.bleClientConnected ||
         lastDrawnState.bleWriteCount != state.bleWriteCount ||
         lastDrawnState.bleLineCount != state.bleLineCount ||
         lastDrawnState.brightnessIndex != state.brightnessIndex;
}
