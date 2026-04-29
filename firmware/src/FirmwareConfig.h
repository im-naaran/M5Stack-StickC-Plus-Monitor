#pragma once

#include <Arduino.h>

namespace FirmwareConfig {
static const unsigned long SERIAL_BAUD_RATE = 115200;
static const unsigned long DISCONNECT_TIMEOUT_MS = 5000;
static const unsigned long BUTTON_DEBOUNCE_MS = 25;
static const unsigned long LOOP_DELAY_MS = 10;

static const uint8_t DEFAULT_BRIGHTNESS_INDEX = 1;
static const uint8_t BRIGHTNESS_LEVELS[] = { 20, 60, 100 };
static const size_t BRIGHTNESS_LEVEL_COUNT =
  sizeof(BRIGHTNESS_LEVELS) / sizeof(BRIGHTNESS_LEVELS[0]);
}
