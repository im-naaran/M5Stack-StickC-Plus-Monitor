#pragma once

#include <Arduino.h>
#include "FirmwareConfig.h"

struct MetricsState {
  int cpuPercent = 0;
  int memoryPercent = 0;
};

enum SettingsOption {
  SETTINGS_OPTION_BRIGHTNESS = 0,
  SETTINGS_OPTION_BATTERY = 1,
  SETTINGS_OPTION_EXIT = 2,
  SETTINGS_OPTION_COUNT = 3,
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
  bool settingsOpen = false;
  uint8_t selectedSettingsOption = SETTINGS_OPTION_BRIGHTNESS;
  bool batteryPercentKnown = false;
  uint8_t batteryPercent = 0;
  unsigned long lastBatteryRefreshMs = 0;
};
