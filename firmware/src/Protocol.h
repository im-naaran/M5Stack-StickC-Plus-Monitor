#pragma once

#include <Arduino.h>
#include "AppState.h"

struct ParseResult {
  bool ok = false;
  MetricsState metrics;
  bool hasCpu = false;
  bool hasMemory = false;
  bool hasTimestamp = false;
  uint32_t timestampSeconds = 0;
  bool hasTimezone = false;
  int timezoneOffsetHours = FirmwareConfig::DEFAULT_TIMEZONE_OFFSET_HOURS;
};

ParseResult parseMetricsLine(const String& line);
int clampPercent(int value);
