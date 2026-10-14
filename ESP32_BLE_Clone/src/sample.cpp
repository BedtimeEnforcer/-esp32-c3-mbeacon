#include "sample.h"

#include <NimBLEDevice.h>
#include <cstring>

bool loadBuiltInSample(Snapshot& value) {
  static const uint8_t adv[] = {
      0x02, 0x01, 0x06, 0x1A, 0xFF, 0x4C, 0x00, 0x02, 0x15, 0xFD,
      0xA5, 0x06, 0x93, 0xA4, 0xE2, 0x4F, 0xB1, 0xAF, 0xCF, 0xC6,
      0xEB, 0x07, 0x64, 0x78, 0x25, 0x27, 0xD9, 0x48, 0xE3, 0xC5};
  static const uint8_t rsp[] = {
      0x02, 0x0A, 0x00, 0x03, 0x03, 0xA5, 0xFD, 0x0E, 0x16, 0xA5,
      0xFD, 0x64, 0x27, 0xD9, 0x48, 0xE3, 0xC3, 0x00, 0x00, 0x1B,
      0x95, 0x0F, 0x08, 0x09, 0x4D, 0x42, 0x65, 0x61, 0x63, 0x6F,
      0x6E};
  static_assert(sizeof(adv) == 30 && sizeof(rsp) == 31, "bad sample split");
  value = Snapshot{};
  memcpy(value.deviceMac, "C3:00:00:1B:95:0F", sizeof(value.deviceMac));
  value.addressType = BLE_ADDR_RANDOM;  // inferred from address bits
  value.advType = BLE_HCI_ADV_RPT_EVTYPE_SCAN_IND;  // inferred from scan response
  value.advLength = sizeof(adv);
  value.responseLength = sizeof(rsp);
  memcpy(value.advData, adv, sizeof(adv));
  memcpy(value.responseData, rsp, sizeof(rsp));
  memcpy(value.rawData, adv, sizeof(adv));
  memcpy(value.rawData + sizeof(adv), rsp, sizeof(rsp));
  value.rawLength = sizeof(adv) + sizeof(rsp);
  value.inferredFromSample = 1;
  return validAdData(value.advData, value.advLength) &&
         validAdData(value.responseData, value.responseLength);
}

