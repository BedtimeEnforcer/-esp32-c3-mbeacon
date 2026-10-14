#pragma once

#include <NimBLEDevice.h>

#include "storage.h"

class BleAdvertiser {
 public:
  bool start(const Snapshot& snapshot);
  void stop();
  bool active() const;
  const char* lastError() const { return lastError_; }

 private:
  NimBLEAdvertising* advertising_ = nullptr;
  const char* lastError_ = "none";
};

