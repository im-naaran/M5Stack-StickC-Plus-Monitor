#include <Arduino.h>
#include <M5StickCPlus.h>
#include <math.h>

#include "AppState.h"
#include "BleReceiver.h"
#include "DisplayView.h"
#include "FirmwareConfig.h"
#include "Protocol.h"
#include "SerialReceiver.h"
#include "SettingsStore.h"

namespace {
AppState appState;
SerialReceiver serialReceiver;
BleReceiver bleReceiver;
DisplayView displayView;
SettingsStore settingsStore;
bool imuAvailable = false;
bool hasPendingOrientation = false;
bool pendingOrientationInverted = false;
unsigned long lastOrientationSampleMs = 0;
unsigned long pendingOrientationSinceMs = 0;
unsigned long disconnectedScreenAwakeSinceMs = 0;
bool disconnectedBrightnessDimmed = false;
bool buttonBLongPressHandled = false;
bool orientationBaselineReady = false;
float orientationBaselineAxis1 = 0.0f;
float orientationBaselineAxis2 = 0.0f;

void handleSerialInput();
void handleBleInput();
void handleConnectionTimeout();
void updateBleDiagnostics();
void handleButtons();
void updateScreenPower();
void sleepScreen();
void wakeScreen();
unsigned long loopDelayMs();
void updateBatteryState(bool force);
void applyMetrics(const ParseResult& result);
void updateDisplayOrientation();
bool readDisplayOrientation(bool& inverted);
float configuredOrientationAxis(uint8_t axis, float accX, float accY, float accZ);
void refreshClockText(bool force);
String formatClockText(uint32_t epochSeconds, int timezoneOffsetHours);
void enterSettings();
void selectNextSettingsOption();
void applySelectedSettingsOption();
void cycleSettingsBrightness();
void toggleBle();
void toggleAutoRotate();
uint8_t estimateBatteryPercent(float voltage);
}

void setup() {
  setCpuFrequencyMhz(FirmwareConfig::CPU_FREQUENCY_MHZ);
  M5.begin();
  imuAvailable = M5.Imu.Init() == 0;
  settingsStore.load(appState);
  serialReceiver.begin(FirmwareConfig::SERIAL_BAUD_RATE);
  if (appState.bleEnabled) {
    bleReceiver.begin();
  }
  displayView.begin();
  displayView.setBrightnessByIndex(appState.brightnessIndex);
  displayView.drawBoot();
}

void loop() {
  M5.update();
  handleSerialInput();
  handleBleInput();
  updateBleDiagnostics();
  refreshClockText(false);
  updateBatteryState(false);
  handleConnectionTimeout();
  handleButtons();
  updateScreenPower();

  if (!appState.screenSleeping) {
    updateDisplayOrientation();
    displayView.draw(appState);
  }

  delay(loopDelayMs());
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
  if (!appState.bleEnabled) {
    return;
  }

  String line;
  while (bleReceiver.readLine(line)) {
    ParseResult result = parseMetricsLine(line);
    if (result.ok) {
      applyMetrics(result);
    }
  }
}

void updateBleDiagnostics() {
  appState.bleEnabled = bleReceiver.isEnabled();
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
  bool buttonBLongPressed = M5.BtnB.pressedFor(FirmwareConfig::BUTTON_LONG_PRESS_MS);
  bool buttonBShortReleased = M5.BtnB.wasReleased();
  bool buttonAReleased = M5.BtnA.wasReleased();

  if (appState.screenSleeping &&
      (M5.BtnA.isPressed() ||
       M5.BtnB.isPressed() ||
       buttonAReleased ||
       buttonBShortReleased ||
       buttonBLongPressed)) {
    wakeScreen();
    return;
  }

  if (buttonBLongPressed && !buttonBLongPressHandled) {
    buttonBLongPressHandled = true;
    if (!appState.settingsOpen) {
      enterSettings();
    }
    return;
  }

  if (buttonBShortReleased && buttonBLongPressHandled) {
    buttonBLongPressHandled = false;
    return;
  }

  if (appState.settingsOpen && buttonBShortReleased) {
    selectNextSettingsOption();
  }

  if (appState.settingsOpen && buttonAReleased) {
    applySelectedSettingsOption();
  }
}

void updateScreenPower() {
  unsigned long now = millis();

  if (appState.connected || appState.settingsOpen) {
    disconnectedScreenAwakeSinceMs = 0;
    if (disconnectedBrightnessDimmed) {
      displayView.setBrightnessByIndex(appState.brightnessIndex);
      disconnectedBrightnessDimmed = false;
    }
    wakeScreen();
    return;
  }

  if (disconnectedScreenAwakeSinceMs == 0) {
    disconnectedScreenAwakeSinceMs = now;
    return;
  }

  if (!appState.screenSleeping &&
      !disconnectedBrightnessDimmed &&
      now - disconnectedScreenAwakeSinceMs >= FirmwareConfig::DISCONNECTED_SCREEN_DIM_MS) {
    displayView.setBrightnessByIndex(
      FirmwareConfig::DISCONNECTED_SCREEN_DIM_BRIGHTNESS_INDEX);
    disconnectedBrightnessDimmed = true;
  }

  if (!appState.screenSleeping &&
      now - disconnectedScreenAwakeSinceMs >= FirmwareConfig::DISCONNECTED_SCREEN_SLEEP_MS) {
    sleepScreen();
  }
}

void sleepScreen() {
  appState.screenSleeping = true;
  displayView.sleepScreen();
  hasPendingOrientation = false;
}

void wakeScreen() {
  if (!appState.screenSleeping) {
    return;
  }

  appState.screenSleeping = false;
  disconnectedScreenAwakeSinceMs = millis();
  disconnectedBrightnessDimmed = false;
  displayView.wakeScreen(appState.brightnessIndex);
}

unsigned long loopDelayMs() {
  return appState.screenSleeping ?
    FirmwareConfig::SCREEN_SLEEP_LOOP_DELAY_MS :
    FirmwareConfig::LOOP_DELAY_MS;
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
  wakeScreen();
}

void updateDisplayOrientation() {
  if (!imuAvailable || !appState.autoRotateEnabled) {
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

  if (detectedInverted == displayView.isInverted()) {
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

  hasPendingOrientation = false;
  displayView.setInverted(detectedInverted);
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
  if (disconnectedBrightnessDimmed) {
    displayView.setBrightnessByIndex(appState.brightnessIndex);
    disconnectedBrightnessDimmed = false;
  }

  appState.settingsOpen = true;
  appState.selectedSettingsOption = SETTINGS_OPTION_BATTERY;
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
    case SETTINGS_OPTION_BLE:
      toggleBle();
      break;
    case SETTINGS_OPTION_ROTATE:
      toggleAutoRotate();
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
  settingsStore.saveBrightnessIndex(appState.brightnessIndex);
}

void toggleBle() {
  appState.bleEnabled = !appState.bleEnabled;
  bleReceiver.setEnabled(appState.bleEnabled);
  updateBleDiagnostics();
  settingsStore.saveBleEnabled(appState.bleEnabled);
}

void toggleAutoRotate() {
  appState.autoRotateEnabled = !appState.autoRotateEnabled;
  hasPendingOrientation = false;
  settingsStore.saveAutoRotateEnabled(appState.autoRotateEnabled);
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
