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
