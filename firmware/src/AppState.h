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
