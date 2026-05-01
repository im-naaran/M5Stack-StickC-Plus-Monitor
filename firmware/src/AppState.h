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
  bool timeSynced = false;
  uint32_t syncedEpochSeconds = 0;
  unsigned long timeSyncMs = 0;
  unsigned long lastClockRefreshMs = 0;
  int timezoneOffsetHours = FirmwareConfig::DEFAULT_TIMEZONE_OFFSET_HOURS;
  bool connected = false;
  unsigned long lastUpdateMs = 0;
  bool bleClientConnected = false;
  uint32_t bleWriteCount = 0;
  uint32_t bleLineCount = 0;
  uint8_t brightnessIndex = FirmwareConfig::DEFAULT_BRIGHTNESS_INDEX;
};
