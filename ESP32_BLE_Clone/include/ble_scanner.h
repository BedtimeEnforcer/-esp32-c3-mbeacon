#pragma once

#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "storage.h"

struct ScanObservation {
  Snapshot snapshot;
  int8_t rssi = 0;
};

class BleScanner : public NimBLEScanCallbacks {
 public:
  bool begin();
  bool start();
  void stop();
  bool pop(ScanObservation& observation);
  bool active() const;

 private:
  void onResult(const NimBLEAdvertisedDevice* device) override;
  NimBLEScan* scan_ = nullptr;
  QueueHandle_t queue_ = nullptr;
};

