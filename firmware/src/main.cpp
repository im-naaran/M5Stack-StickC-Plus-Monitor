#include <Arduino.h>
#include <M5StickCPlus.h>

#include "AppState.h"
#include "DisplayView.h"
#include "FirmwareConfig.h"
#include "Protocol.h"
#include "SerialReceiver.h"

namespace {
AppState appState;
SerialReceiver serialReceiver;
DisplayView displayView;

void handleSerialInput();
void handleConnectionTimeout();
void handleButtons();
void applyMetrics(const ParseResult& result);
void cycleBrightness();
bool buttonBWasPressed();
}

void setup() {
  M5.begin();
  pinMode(BUTTON_B_PIN, INPUT);
  serialReceiver.begin(FirmwareConfig::SERIAL_BAUD_RATE);
  displayView.begin();
  displayView.drawBoot();
}

void loop() {
  M5.update();
  handleSerialInput();
  handleConnectionTimeout();
  handleButtons();
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
  appState.metrics = result.metrics;
  if (result.hasTime) {
    appState.timeText = result.timeText;
  }
  appState.connected = true;
  appState.lastUpdateMs = millis();
}

void cycleBrightness() {
  appState.brightnessIndex =
    (appState.brightnessIndex + 1) % FirmwareConfig::BRIGHTNESS_LEVEL_COUNT;
  displayView.setBrightnessByIndex(appState.brightnessIndex);
}
}
