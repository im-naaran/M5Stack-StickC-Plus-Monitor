#include "SettingsStore.h"

#include <Preferences.h>
#include "FirmwareConfig.h"

namespace {
const char* const SETTINGS_NAMESPACE = "m5mon";
const char* const KEY_SCHEMA = "schema";
const char* const KEY_BRIGHTNESS = "bright";
const char* const KEY_BLE = "ble";
const char* const KEY_ROTATE = "rotate";
const uint32_t CURRENT_SCHEMA_VERSION = 1;
}

void SettingsStore::load(AppState& state) {
  Preferences prefs;
  if (!prefs.begin(SETTINGS_NAMESPACE, false)) {
    state.brightnessIndex = normalizeBrightnessIndex(state.brightnessIndex);
    return;
  }

  uint32_t schemaVersion = prefs.getUInt(KEY_SCHEMA, 0);
  if (schemaVersion > CURRENT_SCHEMA_VERSION) {
    prefs.clear();
    prefs.putUInt(KEY_SCHEMA, CURRENT_SCHEMA_VERSION);
    prefs.end();
    state.brightnessIndex = normalizeBrightnessIndex(state.brightnessIndex);
    return;
  }

  uint8_t brightnessIndex =
    prefs.getUChar(KEY_BRIGHTNESS, FirmwareConfig::DEFAULT_BRIGHTNESS_INDEX);
  state.brightnessIndex = normalizeBrightnessIndex(brightnessIndex);
  state.bleEnabled = prefs.getBool(KEY_BLE, true);
  state.autoRotateEnabled = prefs.getBool(KEY_ROTATE, true);

  prefs.putUInt(KEY_SCHEMA, CURRENT_SCHEMA_VERSION);
  if (state.brightnessIndex != brightnessIndex) {
    prefs.putUChar(KEY_BRIGHTNESS, state.brightnessIndex);
  }
  prefs.end();
}

void SettingsStore::saveBrightnessIndex(uint8_t brightnessIndex) {
  Preferences prefs;
  if (!prefs.begin(SETTINGS_NAMESPACE, false)) {
    return;
  }

  prefs.putUInt(KEY_SCHEMA, CURRENT_SCHEMA_VERSION);
  prefs.putUChar(KEY_BRIGHTNESS, normalizeBrightnessIndex(brightnessIndex));
  prefs.end();
}

void SettingsStore::saveBleEnabled(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(SETTINGS_NAMESPACE, false)) {
    return;
  }

  prefs.putUInt(KEY_SCHEMA, CURRENT_SCHEMA_VERSION);
  prefs.putBool(KEY_BLE, enabled);
  prefs.end();
}

void SettingsStore::saveAutoRotateEnabled(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(SETTINGS_NAMESPACE, false)) {
    return;
  }

  prefs.putUInt(KEY_SCHEMA, CURRENT_SCHEMA_VERSION);
  prefs.putBool(KEY_ROTATE, enabled);
  prefs.end();
}

uint8_t SettingsStore::normalizeBrightnessIndex(uint8_t brightnessIndex) const {
  if (brightnessIndex >= FirmwareConfig::BRIGHTNESS_LEVEL_COUNT) {
    return FirmwareConfig::DEFAULT_BRIGHTNESS_INDEX;
  }

  return brightnessIndex;
}
