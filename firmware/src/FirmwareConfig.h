#pragma once

#include <Arduino.h>

namespace FirmwareConfig {
static const unsigned long SERIAL_BAUD_RATE = 115200;
static const uint32_t CPU_FREQUENCY_MHZ = 80;
static const char* const BLE_DEVICE_NAME = "M5Monitor-Plus";
static const char* const BLE_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* const BLE_METRICS_CHARACTERISTIC_UUID =
  "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static const uint16_t BLE_ADVERTISING_MIN_INTERVAL_UNITS = 800;
static const uint16_t BLE_ADVERTISING_MAX_INTERVAL_UNITS = 1600;
static const unsigned long DISCONNECT_TIMEOUT_MS = 5000;
static const unsigned long BUTTON_LONG_PRESS_MS = 3000;
static const unsigned long LOOP_DELAY_MS = 20;
static const unsigned long SCREEN_SLEEP_LOOP_DELAY_MS = 200;
static const unsigned long CLOCK_REFRESH_INTERVAL_MS = 1000;
static const unsigned long BATTERY_REFRESH_INTERVAL_MS = 5000;
static const unsigned long MAIN_DISPLAY_REFRESH_INTERVAL_MS = 250;
static const unsigned long DISCONNECTED_SCREEN_DIM_MS = 20000;
static const unsigned long DISCONNECTED_SCREEN_SLEEP_MS = 60000;
static const uint8_t DISCONNECTED_SCREEN_DIM_BRIGHTNESS_INDEX = 0;
static const int DEFAULT_TIMEZONE_OFFSET_HOURS = 8;
static const int MIN_TIMEZONE_OFFSET_HOURS = -12;
static const int MAX_TIMEZONE_OFFSET_HOURS = 14;
static const int SECONDS_PER_HOUR = 3600;
static const int SECONDS_PER_DAY = 86400;
static const size_t METRICS_LINE_MAX_LENGTH = 64;
static const uint8_t BLE_LINE_QUEUE_CAPACITY = 4;

static const uint8_t ORIENTATION_AXIS_X = 0;
static const uint8_t ORIENTATION_AXIS_Y = 1;
static const uint8_t ORIENTATION_AXIS_Z = 2;
static const uint8_t DISPLAY_ORIENTATION_PLANE_AXIS_1 = ORIENTATION_AXIS_X;
static const uint8_t DISPLAY_ORIENTATION_PLANE_AXIS_2 = ORIENTATION_AXIS_Y;
static const float DISPLAY_ORIENTATION_VECTOR_MIN_G = 0.35f;
static const float DISPLAY_ORIENTATION_DOT_THRESHOLD = 0.45f;
static const unsigned long ORIENTATION_SAMPLE_INTERVAL_MS = 80;
static const unsigned long ORIENTATION_STABLE_MS = 240;

static const uint8_t DEFAULT_BRIGHTNESS_INDEX = 2;
static const uint8_t BRIGHTNESS_LEVELS[] = { 20, 40, 60, 80, 100 };
static const size_t BRIGHTNESS_LEVEL_COUNT =
  sizeof(BRIGHTNESS_LEVELS) / sizeof(BRIGHTNESS_LEVELS[0]);

static const float BATTERY_EMPTY_VOLTAGE = 3.20f;
static const float BATTERY_FULL_VOLTAGE = 4.20f;
}
