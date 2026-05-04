#include <Arduino.h>
#include <M5StickCPlus.h>
#include <math.h>

#include "AppState.h"
#include "BleReceiver.h"
#include "DisplayView.h"
#include "FirmwareConfig.h"
#include "Protocol.h"
#include "SerialReceiver.h"

namespace {
AppState appState;
SerialReceiver serialReceiver;
BleReceiver bleReceiver;
DisplayView displayView;
bool imuAvailable = false;
bool displayInverted = false;
bool hasPendingOrientation = false;
bool pendingOrientationInverted = false;
unsigned long lastOrientationSampleMs = 0;
unsigned long pendingOrientationSinceMs = 0;
bool orientationBaselineReady = false;
float orientationBaselineAxis1 = 0.0f;
float orientationBaselineAxis2 = 0.0f;

void handleSerialInput();
void handleBleInput();
void handleConnectionTimeout();
void updateBleDiagnostics();
void handleButtons();
void updateBatteryState(bool force);
void applyMetrics(const ParseResult& result);
void updateClock();
void updateDisplayOrientation();
bool readDisplayOrientation(bool& inverted);
float configuredOrientationAxis(uint8_t axis, float accX, float accY, float accZ);
void refreshClockText(bool force);
String formatClockText(uint32_t epochSeconds, int timezoneOffsetHours);
void enterSettings();
void selectNextSettingsOption();
void applySelectedSettingsOption();
void cycleSettingsBrightness();
uint8_t estimateBatteryPercent(float voltage);
}

void setup() {
  M5.begin();
  imuAvailable = M5.Imu.Init() == 0;
  serialReceiver.begin(FirmwareConfig::SERIAL_BAUD_RATE);
  bleReceiver.begin();
  displayView.begin();
  displayView.drawBoot();
}

void loop() {
  M5.update();
  handleSerialInput();
  handleBleInput();
  updateBleDiagnostics();
  updateClock();
  updateBatteryState(false);
  handleConnectionTimeout();
  handleButtons();
  updateDisplayOrientation();
  displayView.draw(appState);
  delay(FirmwareConfig::LOOP_DELAY_MS);
}

namespace {

void handleSerialInput() {
  String line;
  while (serialReceiver.readLine(line)) {
    ParseResult result = parseMetricsLine(line);
    if (result.ok) {
      applyMetrics(result);
    }
  }
}

void handleBleInput() {
  String line;
  while (bleReceiver.readLine(line)) {
    ParseResult result = parseMetricsLine(line);
    if (result.ok) {
      applyMetrics(result);
    }
  }
}

void updateBleDiagnostics() {
  appState.bleClientConnected = bleReceiver.isClientConnected();
  appState.bleWriteCount = bleReceiver.getWriteCount();
  appState.bleLineCount = bleReceiver.getLineCount();
}

void handleConnectionTimeout() {
  if (!appState.connected) {
    return;
  }

  if (millis() - appState.lastUpdateMs > FirmwareConfig::DISCONNECT_TIMEOUT_MS) {
    appState.connected = false;
  }
}

void handleButtons() {
  bool buttonBLongReleased = M5.BtnB.wasReleasefor(FirmwareConfig::BUTTON_LONG_PRESS_MS);
  bool buttonBShortReleased = M5.BtnB.wasReleased();

  if (buttonBLongReleased) {
    enterSettings();
    return;
  }

  if (appState.settingsOpen && buttonBShortReleased) {
    selectNextSettingsOption();
  }

  if (appState.settingsOpen && M5.BtnA.wasReleased()) {
    applySelectedSettingsOption();
  }
}

void updateBatteryState(bool force) {
  unsigned long now = millis();
  if (!force &&
      appState.lastBatteryRefreshMs != 0 &&
      now - appState.lastBatteryRefreshMs < FirmwareConfig::BATTERY_REFRESH_INTERVAL_MS) {
    return;
  }

  float voltage = M5.Axp.GetBatVoltage();
  appState.lastBatteryRefreshMs = now;

  if (voltage < 2.50f || voltage > 4.50f) {
    appState.batteryPercentKnown = false;
    return;
  }

  appState.batteryPercent = estimateBatteryPercent(voltage);
  appState.batteryPercentKnown = true;
}

void applyMetrics(const ParseResult& result) {
  if (result.hasCpu) {
    appState.metrics.cpuPercent = result.metrics.cpuPercent;
  }

  if (result.hasMemory) {
    appState.metrics.memoryPercent = result.metrics.memoryPercent;
  }

  if (result.hasTimezone) {
    appState.timezoneOffsetHours = result.timezoneOffsetHours;
  }

  if (result.hasTimestamp) {
    appState.syncedEpochSeconds = result.timestampSeconds;
    appState.timeSyncMs = millis();
    appState.timeSynced = true;
    refreshClockText(true);
  } else if (result.hasTimezone) {
    refreshClockText(true);
  }

  appState.connected = true;
  appState.lastUpdateMs = millis();
}

void updateClock() {
  refreshClockText(false);
}

void updateDisplayOrientation() {
  if (!imuAvailable) {
    return;
  }

  unsigned long now = millis();
  if (lastOrientationSampleMs != 0 &&
      now - lastOrientationSampleMs < FirmwareConfig::ORIENTATION_SAMPLE_INTERVAL_MS) {
    return;
  }
  lastOrientationSampleMs = now;

  bool detectedInverted = false;
  if (!readDisplayOrientation(detectedInverted)) {
    return;
  }

  if (detectedInverted == displayInverted) {
    hasPendingOrientation = false;
    return;
  }

  if (!hasPendingOrientation || pendingOrientationInverted != detectedInverted) {
    pendingOrientationInverted = detectedInverted;
    pendingOrientationSinceMs = now;
    hasPendingOrientation = true;
    return;
  }

  if (now - pendingOrientationSinceMs < FirmwareConfig::ORIENTATION_STABLE_MS) {
    return;
  }

  displayInverted = detectedInverted;
  hasPendingOrientation = false;
  displayView.setInverted(displayInverted);
}

bool readDisplayOrientation(bool& inverted) {
  float accX = 0.0f;
  float accY = 0.0f;
  float accZ = 0.0f;
  M5.Imu.getAccelData(&accX, &accY, &accZ);

  float axis1 = configuredOrientationAxis(
    FirmwareConfig::DISPLAY_ORIENTATION_PLANE_AXIS_1, accX, accY, accZ);
  float axis2 = configuredOrientationAxis(
    FirmwareConfig::DISPLAY_ORIENTATION_PLANE_AXIS_2, accX, accY, accZ);
  float magnitude = sqrtf(axis1 * axis1 + axis2 * axis2);

  if (magnitude < FirmwareConfig::DISPLAY_ORIENTATION_VECTOR_MIN_G) {
    return false;
  }

  axis1 /= magnitude;
  axis2 /= magnitude;

  if (!orientationBaselineReady) {
    orientationBaselineAxis1 = axis1;
    orientationBaselineAxis2 = axis2;
    orientationBaselineReady = true;
    inverted = false;
    return true;
  }

  float dot = orientationBaselineAxis1 * axis1 + orientationBaselineAxis2 * axis2;

  if (dot > FirmwareConfig::DISPLAY_ORIENTATION_DOT_THRESHOLD) {
    inverted = false;
    return true;
  }

  if (dot < -FirmwareConfig::DISPLAY_ORIENTATION_DOT_THRESHOLD) {
    inverted = true;
    return true;
  }

  return false;
}

float configuredOrientationAxis(uint8_t axis, float accX, float accY, float accZ) {
  switch (axis) {
    case FirmwareConfig::ORIENTATION_AXIS_X:
      return accX;
    case FirmwareConfig::ORIENTATION_AXIS_Z:
      return accZ;
    case FirmwareConfig::ORIENTATION_AXIS_Y:
    default:
      return accY;
  }
}

void refreshClockText(bool force) {
  if (!appState.timeSynced) {
    return;
  }

  unsigned long now = millis();
  if (!force &&
      appState.lastClockRefreshMs != 0 &&
      now - appState.lastClockRefreshMs < FirmwareConfig::CLOCK_REFRESH_INTERVAL_MS) {
    return;
  }

  uint32_t elapsedSeconds = static_cast<uint32_t>((now - appState.timeSyncMs) / 1000);
  uint32_t currentEpochSeconds = appState.syncedEpochSeconds + elapsedSeconds;
  appState.timeText = formatClockText(currentEpochSeconds, appState.timezoneOffsetHours);
  appState.lastClockRefreshMs = now;
}

String formatClockText(uint32_t epochSeconds, int timezoneOffsetHours) {
  int64_t localSeconds =
    static_cast<int64_t>(epochSeconds) +
    static_cast<int64_t>(timezoneOffsetHours) * FirmwareConfig::SECONDS_PER_HOUR;
  int secondsOfDay = static_cast<int>(localSeconds % FirmwareConfig::SECONDS_PER_DAY);

  if (secondsOfDay < 0) {
    secondsOfDay += FirmwareConfig::SECONDS_PER_DAY;
  }

  int hours = secondsOfDay / FirmwareConfig::SECONDS_PER_HOUR;
  int minutes = (secondsOfDay % FirmwareConfig::SECONDS_PER_HOUR) / 60;
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", hours, minutes);
  return String(buffer);
}

void enterSettings() {
  appState.settingsOpen = true;
  appState.selectedSettingsOption = SETTINGS_OPTION_BRIGHTNESS;
  updateBatteryState(true);
}

void selectNextSettingsOption() {
  appState.selectedSettingsOption =
    (appState.selectedSettingsOption + 1) % SETTINGS_OPTION_COUNT;
}

void applySelectedSettingsOption() {
  switch (appState.selectedSettingsOption) {
    case SETTINGS_OPTION_BRIGHTNESS:
      cycleSettingsBrightness();
      break;
    case SETTINGS_OPTION_EXIT:
      appState.settingsOpen = false;
      break;
    case SETTINGS_OPTION_BATTERY:
    default:
      break;
  }
}

void cycleSettingsBrightness() {
  appState.brightnessIndex =
    (appState.brightnessIndex + 1) % FirmwareConfig::BRIGHTNESS_LEVEL_COUNT;
  displayView.setBrightnessByIndex(appState.brightnessIndex);
}

uint8_t estimateBatteryPercent(float voltage) {
  float ratio =
    (voltage - FirmwareConfig::BATTERY_EMPTY_VOLTAGE) /
    (FirmwareConfig::BATTERY_FULL_VOLTAGE - FirmwareConfig::BATTERY_EMPTY_VOLTAGE);

  if (ratio < 0.0f) {
    ratio = 0.0f;
  } else if (ratio > 1.0f) {
    ratio = 1.0f;
  }

  return static_cast<uint8_t>(roundf(ratio * 100.0f));
}
}
