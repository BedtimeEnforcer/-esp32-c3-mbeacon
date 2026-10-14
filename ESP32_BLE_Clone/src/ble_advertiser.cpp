#include "ble_advertiser.h"

#include "config.h"

bool BleAdvertiser::start(const Snapshot& snapshot) {
  lastError_ = "none";
  if (!validAdData(snapshot.advData, snapshot.advLength) ||
      !validAdData(snapshot.responseData, snapshot.responseLength) ||
      snapshot.advLength == 0) {
    lastError_ = "invalid saved advertisement bytes";
    return false;
  }

  NimBLEAddress target(snapshot.deviceMac, BLE_ADDR_RANDOM);
  if (snapshot.addressType != BLE_ADDR_RANDOM || !target.isStatic()) {
    lastError_ = "target address is not static random; same-MAC replay unavailable";
    return false;
  }
  if (snapshot.advType != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
      snapshot.advType != BLE_HCI_ADV_RPT_EVTYPE_SCAN_IND &&
      snapshot.advType != BLE_HCI_ADV_RPT_EVTYPE_NONCONN_IND) {
    lastError_ = "unsupported advertisement type (directed or extended)";
    return false;
  }
  if (snapshot.advType == BLE_HCI_ADV_RPT_EVTYPE_NONCONN_IND &&
      snapshot.responseLength != 0) {
    lastError_ = "non-connectable advertisement cannot carry scan response";
    return false;
  }

  advertising_ = NimBLEDevice::getAdvertising();
  if (!advertising_ || !advertising_->reset()) {
    lastError_ = "NimBLE advertising reset failed";
    return false;
  }

  if (!NimBLEDevice::setOwnAddr(target) ||
      !NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM)) {
    lastError_ = "NimBLE rejected target static random address";
    return false;
  }

  // These setters may edit the internal AD Flags. Call them before loading the
  // captured bytes so they cannot silently rewrite a saved payload.
  const bool nonConnectable = snapshot.advType != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND;
  const bool nonScannable = snapshot.advType == BLE_HCI_ADV_RPT_EVTYPE_NONCONN_IND;
  if (!advertising_->setConnectableMode(nonConnectable ? BLE_GAP_CONN_MODE_NON
                                                       : BLE_GAP_CONN_MODE_UND) ||
      !advertising_->setDiscoverableMode(nonScannable ? BLE_GAP_DISC_MODE_NON
                                                     : BLE_GAP_DISC_MODE_GEN)) {
    lastError_ = "NimBLE rejected advertisement type";
    return false;
  }
  advertising_->enableScanResponse(snapshot.responseLength != 0);
  advertising_->setAdvertisingInterval((ADV_INTERVAL_MS * 1000UL) / 625);

  NimBLEAdvertisementData advData;
  if (!advData.addData(snapshot.advData, snapshot.advLength) ||
      !advertising_->setAdvertisementData(advData)) {
    lastError_ = "advertisement bytes rejected (check 31-byte limit)";
    return false;
  }

  if (snapshot.responseLength) {
    NimBLEAdvertisementData response;
    if (!response.addData(snapshot.responseData, snapshot.responseLength) ||
        !advertising_->setScanResponseData(response)) {
      lastError_ = "scan response bytes rejected (check 31-byte limit)";
      return false;
    }
  }
  if (!advertising_->start()) {
    lastError_ = "NimBLE controller failed to start advertising";
    return false;
  }
  return true;
}

void BleAdvertiser::stop() {
  if (advertising_ && advertising_->isAdvertising()) advertising_->stop();
}

bool BleAdvertiser::active() const {
  return advertising_ && advertising_->isAdvertising();
}

