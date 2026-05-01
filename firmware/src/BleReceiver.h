#pragma once

#include <Arduino.h>
#include <string>
#include "FirmwareConfig.h"

class BleReceiver {
public:
  void begin();
  bool readLine(String& outLine);
  bool isClientConnected() const;
  uint32_t getWriteCount() const;
  uint32_t getLineCount() const;

  void setClientConnected(bool connected);
  void handleWrite(const std::string& value);

private:
  void enqueueLine(const String& line);
  void clearWriteBuffer();

  portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;
  char pendingLines[FirmwareConfig::BLE_LINE_QUEUE_CAPACITY]
                   [FirmwareConfig::METRICS_LINE_MAX_LENGTH + 1] = {};
  uint8_t readIndex = 0;
  uint8_t writeIndex = 0;
  uint8_t pendingCount = 0;
  volatile bool clientConnected = false;
  volatile uint32_t writeCount = 0;
  volatile uint32_t lineCount = 0;
  String writeBuffer;
};
