#pragma once

#include <Arduino.h>
#include "AppState.h"

class SettingsStore {
public:
  void load(AppState& state);
  void saveBrightnessIndex(uint8_t brightnessIndex);
  void saveBleEnabled(bool enabled);
  void saveAutoRotateEnabled(bool enabled);

private:
  uint8_t normalizeBrightnessIndex(uint8_t brightnessIndex) const;
};
