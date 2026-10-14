#pragma once

#include <Arduino.h>

constexpr size_t MAX_ADV_BYTES = 31;
constexpr size_t MAX_RAW_BYTES = MAX_ADV_BYTES * 2;

struct Snapshot {
  char deviceMac[18]{};
  uint8_t addressType = 0;
  uint8_t advType = 0;
  uint8_t advLength = 0;
  uint8_t responseLength = 0;
  uint8_t advData[MAX_ADV_BYTES]{};
  uint8_t responseData[MAX_ADV_BYTES]{};
  uint8_t rawLength = 0;
  uint8_t rawData[MAX_RAW_BYTES]{};
  uint8_t inferredFromSample = 0;
  uint64_t updatedEpoch = 0;  // 0 means clock was not set.
  uint32_t updatedUptimeMs = 0;
};

bool storageBegin();
bool storageLoad(Snapshot& value);
bool storageSave(const Snapshot& value);
bool storageClear();
bool builtInSampleEnabled();
bool appendScanLog(const Snapshot& value, int rssi, uint64_t epoch);
void printScanLog(Stream& output);
bool validAdData(const uint8_t* data, size_t length);
bool findAdValue(const uint8_t* data, size_t length, uint8_t type,
                 uint8_t* output, uint8_t& outputLength);
String hexString(const uint8_t* data, size_t length);

