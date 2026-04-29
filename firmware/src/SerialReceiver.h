#pragma once

#include <Arduino.h>

class SerialReceiver {
public:
  void begin(unsigned long baudRate);
  bool readLine(String& outLine);
  void clear();

private:
  String buffer;
  static const size_t MAX_LINE_LENGTH = 64;
};
