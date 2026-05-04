#include "BleReceiver.h"

#include <BLEAdvertising.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <cstring>

#include "FirmwareConfig.h"

namespace {
BleReceiver* activeReceiver = nullptr;

class MetricsCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic* characteristic) override {
    if (!activeReceiver) {
      return;
    }

    activeReceiver->handleWrite(characteristic->getValue());
  }
};

class MonitorServerCallbacks : public BLEServerCallbacks {
public:
  void onConnect(BLEServer*) override {
    if (activeReceiver) {
      activeReceiver->setClientConnected(true);
    }
  }

  void onDisconnect(BLEServer*) override {
    if (activeReceiver) {
      activeReceiver->setClientConnected(false);
      if (activeReceiver->isEnabled()) {
        BLEDevice::startAdvertising();
      }
    }
  }
};
}

void BleReceiver::begin() {
  if (enabled) {
    return;
  }

  activeReceiver = this;
  clearWriteBuffer();
  BLEDevice::init(FirmwareConfig::BLE_DEVICE_NAME);

  server = BLEDevice::createServer();
  server->setCallbacks(new MonitorServerCallbacks());

  BLEService* service = server->createService(FirmwareConfig::BLE_SERVICE_UUID);
  BLECharacteristic* metricsCharacteristic = service->createCharacteristic(
    FirmwareConfig::BLE_METRICS_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  metricsCharacteristic->setCallbacks(new MetricsCharacteristicCallbacks());

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(FirmwareConfig::BLE_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinInterval(FirmwareConfig::BLE_ADVERTISING_MIN_INTERVAL_UNITS);
  advertising->setMaxInterval(FirmwareConfig::BLE_ADVERTISING_MAX_INTERVAL_UNITS);
  BLEDevice::startAdvertising();
  enabled = true;
}

void BleReceiver::setEnabled(bool newEnabled) {
  if (newEnabled) {
    begin();
    return;
  }

  if (!enabled) {
    return;
  }

  enabled = false;
  BLEDevice::stopAdvertising();
  if (server && server->getConnectedCount() > 0) {
    server->disconnect(server->getConnId());
  }

  setClientConnected(false);
  clearQueue();
  BLEDevice::deinit(false);
  server = nullptr;
}

bool BleReceiver::isEnabled() const {
  return enabled;
}

bool BleReceiver::readLine(String& outLine) {
  portENTER_CRITICAL(&queueMux);
  if (pendingCount == 0) {
    portEXIT_CRITICAL(&queueMux);
    return false;
  }

  char line[FirmwareConfig::METRICS_LINE_MAX_LENGTH + 1];
  strncpy(line, pendingLines[readIndex], sizeof(line));
  line[FirmwareConfig::METRICS_LINE_MAX_LENGTH] = '\0';
  readIndex = (readIndex + 1) % FirmwareConfig::BLE_LINE_QUEUE_CAPACITY;
  pendingCount--;
  portEXIT_CRITICAL(&queueMux);

  outLine = String(line);
  outLine.trim();
  return outLine.length() > 0;
}

bool BleReceiver::isClientConnected() const {
  return enabled && clientConnected;
}

uint32_t BleReceiver::getWriteCount() const {
  return writeCount;
}

uint32_t BleReceiver::getLineCount() const {
  return lineCount;
}

void BleReceiver::setClientConnected(bool connected) {
  clientConnected = connected;
  if (!connected) {
    clearWriteBuffer();
  }
}

void BleReceiver::handleWrite(const std::string& value) {
  if (!enabled) {
    return;
  }

  writeCount++;

  for (size_t i = 0; i < value.length(); ++i) {
    char ch = value[i];

    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      if (writeBufferLen == 0) {
        clearWriteBuffer();
        continue;
      }

      writeBuffer[writeBufferLen] = '\0';
      String line = writeBuffer;
      clearWriteBuffer();
      line.trim();
      if (line.length() > 0) {
        enqueueLine(line);
      }
      continue;
    }

    if (writeBufferLen >= FirmwareConfig::METRICS_LINE_MAX_LENGTH) {
      clearWriteBuffer();
      continue;
    }

    writeBuffer[writeBufferLen++] = ch;
    writeBuffer[writeBufferLen] = '\0';
  }
}

void BleReceiver::enqueueLine(const String& line) {
  char lineBuffer[FirmwareConfig::METRICS_LINE_MAX_LENGTH + 1];
  line.substring(0, FirmwareConfig::METRICS_LINE_MAX_LENGTH)
    .toCharArray(lineBuffer, sizeof(lineBuffer));

  portENTER_CRITICAL(&queueMux);
  if (pendingCount >= FirmwareConfig::BLE_LINE_QUEUE_CAPACITY) {
    readIndex = (readIndex + 1) % FirmwareConfig::BLE_LINE_QUEUE_CAPACITY;
    pendingCount--;
  }

  strncpy(
    pendingLines[writeIndex],
    lineBuffer,
    FirmwareConfig::METRICS_LINE_MAX_LENGTH + 1);
  pendingLines[writeIndex][FirmwareConfig::METRICS_LINE_MAX_LENGTH] = '\0';
  writeIndex = (writeIndex + 1) % FirmwareConfig::BLE_LINE_QUEUE_CAPACITY;
  pendingCount++;
  lineCount++;
  portEXIT_CRITICAL(&queueMux);
}

void BleReceiver::clearQueue() {
  portENTER_CRITICAL(&queueMux);
  readIndex = 0;
  writeIndex = 0;
  pendingCount = 0;
  portEXIT_CRITICAL(&queueMux);
}

void BleReceiver::clearWriteBuffer() {
  writeBufferLen = 0;
  writeBuffer[0] = '\0';
}
