#pragma once

#include <Arduino.h>
#include "AppState.h"

class DisplayView {
public:
  void begin();
  void drawBoot();
  void draw(const AppState& state);
  void setBrightnessByIndex(uint8_t index);
  void sleepScreen();
  void wakeScreen(uint8_t brightnessIndex);
  void setInverted(bool inverted);
  bool isInverted() const;

private:
  void applyRotation();
  void drawDisconnected(const AppState& state);
  void drawSettings(const AppState& state);
  void drawLayout();
  void drawMetricBlock(int x, int y, const char* label, int percent);
  void drawProgressBar(int x, int y, int w, int h, int percent, uint16_t color);
  void drawFooter(const AppState& state);
  void drawSettingRow(int y, bool selected, const char* label, const String& value);
  uint16_t colorForPercent(int percent);
  bool stateChanged(const AppState& state) const;
  bool settingsStateChanged(const AppState& state) const;

  AppState lastDrawnState;
  bool hasLastDrawnState = false;
  bool screenInverted = false;
  unsigned long lastMainDrawMs = 0;
};
