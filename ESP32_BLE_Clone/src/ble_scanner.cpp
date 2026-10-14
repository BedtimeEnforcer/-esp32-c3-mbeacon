#include "ble_scanner.h"

#include <cstring>

#include "config.h"

bool BleScanner::begin() {
  queue_ = xQueueCreate(4, sizeof(ScanObservation));
  scan_ = NimBLEDevice::getScan();
  if (!queue_ || !scan_) return false;
  scan_->setScanCallbacks(this, true);
  scan_->setActiveScan(true);
  scan_->setInterval(SCAN_INTERVAL_MS);
  scan_->setWindow(SCAN_WINDOW_MS);
  scan_->setDuplicateFilter(0);
  scan_->setMaxResults(0);
  return true;
}

bool BleScanner::start() {
  if (queue_) xQueueReset(queue_);
  return scan_ && scan_->start(0);
}

void BleScanner::stop() {
  if (scan_ && scan_->isScanning()) scan_->stop();
}

bool BleScanner::active() const {
  return scan_ && scan_->isScanning();
}

bool BleScanner::pop(ScanObservation& observation) {
  return queue_ && xQueueReceive(queue_, &observation, 0) == pdTRUE;
}

void BleScanner::onResult(const NimBLEAdvertisedDevice* device) {
  if (!device || String(TARGET_MAC).length() != 17) return;
  const String address(device->getAddress().toString().c_str());
  if (!address.equalsIgnoreCase(TARGET_MAC)) return;

  const auto& payload = device->getPayload();
  const size_t advLength = device->getAdvLength();
  if (advLength > MAX_ADV_BYTES || payload.size() < advLength ||
      payload.size() - advLength > MAX_ADV_BYTES) return;

  ScanObservation report{};
  address.toCharArray(report.snapshot.deviceMac, sizeof(report.snapshot.deviceMac));
  report.snapshot.addressType = device->getAddressType();
  report.snapshot.advType = device->getAdvType();
  report.snapshot.advLength = advLength;
  report.snapshot.responseLength = payload.size() - advLength;
  if (advLength) memcpy(report.snapshot.advData, payload.data(), advLength);
  if (report.snapshot.responseLength) {
    memcpy(report.snapshot.responseData, payload.data() + advLength,
           report.snapshot.responseLength);
  }
  if (!validAdData(report.snapshot.advData, advLength) ||
      !validAdData(report.snapshot.responseData, report.snapshot.responseLength)) return;
  report.rssi = device->getRSSI();
  // A full queue drops the oldest report while keeping the latest value.
  if (xQueueSend(queue_, &report, 0) != pdTRUE) {
    ScanObservation discarded;
    xQueueReceive(queue_, &discarded, 0);
    xQueueSend(queue_, &report, 0);
  }
}

