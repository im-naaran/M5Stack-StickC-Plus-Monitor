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
  if (normalized.charAt(0) == '-' || normalized.charAt(0) == '+') {
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

static bool parseTimestampSeconds(const String& value, uint32_t& outValue) {
  String normalized = value;
  normalized.trim();

  if (normalized.length() == 0) {
    return false;
  }

  uint64_t parsed = 0;
  for (int i = 0; i < normalized.length(); ++i) {
    char ch = normalized.charAt(i);
    if (!isDigit(ch)) {
      return false;
    }

    parsed = parsed * 10 + static_cast<uint64_t>(ch - '0');
    if (parsed > 4294967295ULL) {
      return false;
    }
  }

  outValue = static_cast<uint32_t>(parsed);
  return true;
}

static bool parseTimezoneOffsetHours(const String& value, int& outValue) {
  int parsedValue = 0;
  if (!parseInteger(value, parsedValue)) {
    return false;
  }

  if (parsedValue < FirmwareConfig::MIN_TIMEZONE_OFFSET_HOURS ||
      parsedValue > FirmwareConfig::MAX_TIMEZONE_OFFSET_HOURS) {
    return false;
  }

  outValue = parsedValue;
  return true;
}

ParseResult parseMetricsLine(const String& line) {
  ParseResult result;
  String normalized = line;
  normalized.trim();

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
          uint32_t timestampSeconds = 0;
          if (parseTimestampSeconds(value, timestampSeconds)) {
            result.timestampSeconds = timestampSeconds;
            result.hasTimestamp = true;
          }
        } else if (key == "Z") {
          int timezoneOffsetHours = 0;
          if (parseTimezoneOffsetHours(value, timezoneOffsetHours)) {
            result.timezoneOffsetHours = timezoneOffsetHours;
            result.hasTimezone = true;
          }
        } else {
          int parsedValue = 0;
          if (parseInteger(value, parsedValue)) {
            if (key == "C") {
              result.metrics.cpuPercent = clampPercent(parsedValue);
              result.hasCpu = true;
            } else if (key == "M") {
              result.metrics.memoryPercent = clampPercent(parsedValue);
              result.hasMemory = true;
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

  result.ok = result.hasCpu || result.hasMemory || result.hasTimestamp || result.hasTimezone;
  return result;
}
