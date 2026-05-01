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
void applyMetrics(const ParseResult& result);
void updateClock();
void updateDisplayOrientation();
bool readDisplayOrientation(bool& inverted);
float configuredOrientationAxis(uint8_t axis, float accX, float accY, float accZ);
void refreshClockText(bool force);
String formatClockText(uint32_t epochSeconds, int timezoneOffsetHours);
void cycleBrightness();
bool buttonBWasPressed();
}

void setup() {
  M5.begin();
  imuAvailable = M5.Imu.Init() == 0;
  pinMode(BUTTON_B_PIN, INPUT);
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
  if (buttonBWasPressed()) {
    cycleBrightness();
  }
}

bool buttonBWasPressed() {
  static bool lastRawPressed = false;
  static bool stablePressed = false;
  static unsigned long lastRawChangeMs = 0;

  bool rawPressed = digitalRead(BUTTON_B_PIN) == LOW;
  unsigned long now = millis();

  if (rawPressed != lastRawPressed) {
    lastRawPressed = rawPressed;
    lastRawChangeMs = now;
  }

  if (now - lastRawChangeMs < FirmwareConfig::BUTTON_DEBOUNCE_MS) {
    return false;
  }

  if (rawPressed != stablePressed) {
    stablePressed = rawPressed;
    return stablePressed;
  }

  return false;
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

void cycleBrightness() {
  appState.brightnessIndex =
    (appState.brightnessIndex + 1) % FirmwareConfig::BRIGHTNESS_LEVEL_COUNT;
  displayView.setBrightnessByIndex(appState.brightnessIndex);
}
}
