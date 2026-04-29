#include "Protocol.h"

int clampPercent(int value) {
  if (value < 0) {
    return 0;
  }

  if (value > 100) {
    return 100;
  }

  return value;
}

static bool parseInteger(const String& value, int& outValue) {
  String normalized = value;
  normalized.trim();

  if (normalized.length() == 0) {
    return false;
  }

  int start = 0;
  if (normalized.charAt(0) == '-') {
    if (normalized.length() == 1) {
      return false;
    }
    start = 1;
  }

  for (int i = start; i < normalized.length(); ++i) {
    if (!isDigit(normalized.charAt(i))) {
      return false;
    }
  }

  outValue = normalized.toInt();
  return true;
}

static bool parseTimeText(const String& value, String& outValue) {
  String normalized = value;
  normalized.trim();

  if (normalized.length() != 4) {
    return false;
  }

  for (int i = 0; i < normalized.length(); ++i) {
    if (!isDigit(normalized.charAt(i))) {
      return false;
    }
  }

  int hour = normalized.substring(0, 2).toInt();
  int minute = normalized.substring(2, 4).toInt();

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    return false;
  }

  outValue = normalized.substring(0, 2) + ":" + normalized.substring(2, 4);
  return true;
}

ParseResult parseMetricsLine(const String& line) {
  ParseResult result;
  String normalized = line;
  normalized.trim();

  bool hasCpu = false;
  bool hasMemory = false;

  int start = 0;
  while (start <= normalized.length()) {
    int separator = normalized.indexOf('|', start);
    String token = separator >= 0
      ? normalized.substring(start, separator)
      : normalized.substring(start);
    token.trim();

    if (token.length() > 0) {
      int colon = token.indexOf(':');
      if (colon > 0) {
        String key = token.substring(0, colon);
        String value = token.substring(colon + 1);
        key.trim();
        value.trim();

        if (key == "T") {
          String parsedTime;
          if (parseTimeText(value, parsedTime)) {
            result.timeText = parsedTime;
            result.hasTime = true;
          }
        } else {
          int parsedValue = 0;
          if (parseInteger(value, parsedValue)) {
            if (key == "C") {
              result.metrics.cpuPercent = clampPercent(parsedValue);
              hasCpu = true;
            } else if (key == "M") {
              result.metrics.memoryPercent = clampPercent(parsedValue);
              hasMemory = true;
            }
          }
        }
      }
    }

    if (separator < 0) {
      break;
    }
    start = separator + 1;
  }

  result.ok = hasCpu && hasMemory;
  return result;
}
