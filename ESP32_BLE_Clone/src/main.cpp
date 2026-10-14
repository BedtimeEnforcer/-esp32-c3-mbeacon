#include <Arduino.h>
#include <NimBLEDevice.h>
#include <cstring>
#if ARDUINO_USB_CDC_ON_BOOT
#include <HWCDC.h>
#endif

#include "ble_advertiser.h"
#include "ble_scanner.h"
#include "button.h"
#include "config.h"
#include "sample.h"
#include "storage.h"

namespace {
enum class Mode { Simulate, Scan, Stopped };
Mode mode = Mode::Stopped;
BleScanner scanner;
BleAdvertiser advertiser;
Button button;
Snapshot saved{};
bool hasSaved = false;
String command;
uint64_t clockEpochAtSet = 0;
uint32_t clockMillisAtSet = 0;
uint32_t lastLoggedAt = 0;
uint32_t lastDebugAt = 0;
uint32_t lastLedAt = 0;
bool ledOn = false;
enum class SuccessLight { None, On, Off };
SuccessLight successLight = SuccessLight::None;
uint32_t successLightAt = 0;
uint32_t lastScanRetryAt = 0;
uint32_t lastAdvertiseFailureAt = 0;

uint64_t currentEpoch() {
  return clockEpochAtSet ? clockEpochAtSet + (millis() - clockMillisAtSet) / 1000 : 0;
}

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, (on != LED_ACTIVE_LOW) ? HIGH : LOW);
}

// The supplied MBeacon sample contains a complete legacy ADV and scan
// response. Do not replace a complete NVS snapshot with an ADV-only report.
bool completeTargetReport(const Snapshot& s) {
  if (!s.advLength || !s.responseLength) return false;
  uint8_t manufacturer[MAX_ADV_BYTES]{};
  uint8_t service[MAX_ADV_BYTES]{};
  uint8_t name[MAX_ADV_BYTES]{};
  uint8_t manufacturerLength = 0;
  uint8_t serviceLength = 0;
  uint8_t nameLength = 0;
  return findAdValue(s.advData, s.advLength, 0xFF, manufacturer,
                     manufacturerLength) &&
         manufacturerLength == 25 &&
         memcmp(manufacturer, "\x4C\x00\x02\x15", 4) == 0 &&
         findAdValue(s.responseData, s.responseLength, 0x16, service,
                     serviceLength) &&
         serviceLength == 13 && service[0] == 0xA5 && service[1] == 0xFD &&
         findAdValue(s.responseData, s.responseLength, 0x09, name,
                     nameLength) &&
         nameLength == 7 && memcmp(name, "MBeacon", 7) == 0;
}

void startSuccessLight() {
  successLight = SuccessLight::On;
  successLightAt = millis();
  setLed(true);
  Serial.println("LEARN SUCCESS: D4 solid for 3 seconds; advertising started");
}

void updateLed(uint32_t now) {
  if (successLight == SuccessLight::On) {
    if (now - successLightAt >= LEARNED_LED_ON_MS) {
      successLight = SuccessLight::Off;
      successLightAt = now;
      setLed(false);
    }
    return;
  }
  if (successLight == SuccessLight::Off) {
    if (now - successLightAt >= LEARNED_LED_OFF_MS) {
      successLight = SuccessLight::None;
      lastLedAt = now;
      setLed(false);
    }
    return;
  }
  const uint32_t period = mode == Mode::Scan ? SCAN_LED_TOGGLE_MS :
                          mode == Mode::Simulate ? SIMULATE_LED_TOGGLE_MS : 0;
  if (!period) setLed(false);
  else if (now - lastLedAt >= period) {
    lastLedAt = now;
    setLed(!ledOn);
  }
}

void printAdFields(const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length;) {
    const uint8_t fieldLength = data[i];
    const uint8_t type = data[i + 1];
    if (type == 0xFF) Serial.printf("Manufacturer Data: %s\n", hexString(data + i + 2, fieldLength - 1).c_str());
    if (type == 0x16 || type == 0x20 || type == 0x21) {
      Serial.printf("Service Data (0x%02X): %s\n", type,
                    hexString(data + i + 2, fieldLength - 1).c_str());
    }
    if (type == 0x02 || type == 0x03 || type == 0x04 || type == 0x05 ||
        type == 0x06 || type == 0x07) {
      Serial.printf("Service UUID (AD 0x%02X, LE bytes): %s\n", type,
                    hexString(data + i + 2, fieldLength - 1).c_str());
    }
    i += fieldLength + 1;
  }
}

void printObservation(const ScanObservation& observation) {
  const Snapshot& s = observation.snapshot;
  Serial.printf("MAC: %s (addr_type=%u)\nRSSI: %d\nAdvertisement Type: 0x%02X\n",
                s.deviceMac, observation.rssi, s.advType);
  printAdFields(s.advData, s.advLength);
  printAdFields(s.responseData, s.responseLength);
  Serial.printf("Raw Advertisement Packet (AD bytes): %s\n",
                hexString(s.advData, s.advLength).c_str());
  Serial.printf("Raw Scan Response (AD bytes): %s\n",
                hexString(s.responseData, s.responseLength).c_str());
}

void changeMode(Mode next) {
  scanner.stop();
  advertiser.stop();
  successLight = SuccessLight::None;
  setLed(false);
  lastLedAt = millis();
  mode = next;
  if (next == Mode::Scan) {
    lastDebugAt = 0;
    lastLoggedAt = 0;
    lastScanRetryAt = millis();
    if (String(TARGET_MAC).equalsIgnoreCase("xx:xx:xx:xx:xx:xx"))
      Serial.println("CONFIG REQUIRED: set TARGET_MAC in config.h");
    if (!scanner.start()) Serial.println("SCAN ERROR: failed to start");
    else Serial.println("MODE: SCAN");
  } else if (next == Mode::Simulate) {
    if (!hasSaved) Serial.println("MODE: SIMULATE, no payload; press button or send SCAN/SAMPLE");
    else if (!advertiser.start(saved)) {
      Serial.printf("ADVERTISING ERROR: %s\n", advertiser.lastError());
      mode = Mode::Stopped;
      setLed(false);
    }
    else {
      Serial.println("MODE: SIMULATE, address=target static random");
      if (saved.advType == 0) Serial.println("NOTE: connectable flag copied, but no GATT service is implemented");
    }
  } else Serial.println("MODE: STOPPED");
}

bool learn(const ScanObservation& observation) {
  Snapshot next = observation.snapshot;
  const uint32_t now = millis();
  const bool debugDue = lastDebugAt == 0 || now - lastDebugAt >= SCAN_LOG_MIN_INTERVAL_MS;
  if (debugDue) {
    printObservation(observation);
    lastDebugAt = now;
  }

  if (!completeTargetReport(next)) {
    if (debugDue) Serial.println("WAITING: target report lacks complete MBeacon ADV + scan response");
    return false;
  }
  NimBLEAddress address(next.deviceMac, BLE_ADDR_RANDOM);
  if (next.addressType != BLE_ADDR_RANDOM || !address.isStatic()) {
    if (debugDue) Serial.println("UNSUPPORTED: target address is not static random; same-MAC replay unavailable");
    return false;
  }
  next.rawLength = next.advLength + next.responseLength;
  memcpy(next.rawData, next.advData, next.advLength);
  memcpy(next.rawData + next.advLength, next.responseData, next.responseLength);
  const bool changed = !hasSaved || saved.inferredFromSample ||
                       saved.rawLength != next.rawLength ||
                       saved.advLength != next.advLength ||
                       saved.responseLength != next.responseLength ||
                       memcmp(saved.rawData, next.rawData, next.rawLength) != 0 ||
                       saved.addressType != next.addressType ||
                       saved.advType != next.advType;
  if (changed && !debugDue) {
    printObservation(observation);
    lastDebugAt = now;
  }
  if (changed || now - lastLoggedAt >= SCAN_LOG_MIN_INTERVAL_MS) {
    if (!appendScanLog(next, observation.rssi, currentEpoch()))
      Serial.println("WARN: scan_log.txt write failed");
    lastLoggedAt = now;
  }
  if (!changed) {
    Serial.println("CAPTURED: valid target data unchanged; saved snapshot ready");
    return true;
  }
  next.updatedEpoch = currentEpoch();
  next.updatedUptimeMs = now;
  Serial.printf("OLD: %s\nNEW: %s\n",
                hasSaved ? hexString(saved.rawData, saved.rawLength).c_str() : "<empty>",
                hexString(next.rawData, next.rawLength).c_str());
  if (storageSave(next)) {
    Snapshot verified;
    if (storageLoad(verified) && verified.rawLength == next.rawLength &&
        verified.advLength == next.advLength &&
        verified.responseLength == next.responseLength &&
        verified.addressType == next.addressType &&
        verified.advType == next.advType && !verified.inferredFromSample &&
        memcmp(verified.rawData, next.rawData, next.rawLength) == 0) {
      saved = verified;
      hasSaved = true;
      Serial.println("UPDATED: NVS read-back verified");
      return true;
    }
    Serial.println("ERROR: NVS read-back failed; keep scanning");
  } else Serial.println("ERROR: NVS write failed; keep scanning");
  return false;
}

void printInfo() {
  Serial.printf("MODE: %s\nTARGET_MAC: %s\nRAW_AD_TYPE: 0x%02X\n",
                mode == Mode::Scan ? "SCAN" : mode == Mode::Simulate ? "SIMULATE" : "STOPPED",
                TARGET_MAC, RAW_AD_TYPE);
  if (!hasSaved) {
    Serial.println("SAVED: none");
    return;
  }
  Serial.printf("SAVED MAC: %s\nADDR TYPE: %u\nADV TYPE: 0x%02X\n",
                saved.deviceMac, saved.addressType, saved.advType);
  NimBLEAddress savedAddress(saved.deviceMac, BLE_ADDR_RANDOM);
  if (saved.addressType != BLE_ADDR_RANDOM || !savedAddress.isStatic())
    Serial.println("WARNING: saved address cannot be replayed as the same MAC");
  if (saved.inferredFromSample)
    Serial.println("SOURCE: built-in capture; address/PDU types are inferred");
  Serial.printf("RAW LENGTH: %u\nRAW DATA: %s\nADV DATA: %s\nSCAN RSP: %s\n",
                saved.rawLength, hexString(saved.rawData, saved.rawLength).c_str(),
                hexString(saved.advData, saved.advLength).c_str(),
                hexString(saved.responseData, saved.responseLength).c_str());
  Serial.printf("UPDATED EPOCH: %llu (0=unset)\nUPDATED UPTIME_MS: %lu\n",
                static_cast<unsigned long long>(saved.updatedEpoch),
                static_cast<unsigned long>(saved.updatedUptimeMs));
}

void processCommand(String line) {
  line.trim();
  line.toUpperCase();
  if (line == "SCAN") changeMode(Mode::Scan);
  else if (line == "START") changeMode(Mode::Simulate);
  else if (line == "STOP") changeMode(Mode::Stopped);
  else if (line == "INFO") printInfo();
  else if (line == "LOG") printScanLog(Serial);
  else if (line == "SAMPLE") {
    Snapshot example;
    if (loadBuiltInSample(example) && storageSave(example)) {
      saved = example;
      hasSaved = true;
      Serial.println("SAMPLE RESTORED: MBeacon capture");
      changeMode(Mode::Simulate);
    } else Serial.println("ERROR: sample restore failed");
  }
  else if (line == "CLEAR") {
    changeMode(Mode::Stopped);
    if (storageClear()) {
      saved = Snapshot{};
      hasSaved = false;
      Serial.println("CLEARED: snapshot and scan log");
    } else Serial.println("ERROR: clear failed");
  } else if (line.startsWith("TIME ")) {
    const uint64_t epoch = strtoull(line.c_str() + 5, nullptr, 10);
    if (epoch < 1600000000ULL) Serial.println("ERROR: use TIME <Unix seconds>");
    else {
      clockEpochAtSet = epoch;
      clockMillisAtSet = millis();
      Serial.printf("CLOCK: %llu\n", static_cast<unsigned long long>(epoch));
    }
  } else Serial.println("COMMANDS: SCAN START STOP INFO LOG SAMPLE CLEAR TIME <Unix seconds>");
}
}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
  button.begin();
  delay(300);
  Serial.println("ESP32 BLE Clone boot");
  if (!storageBegin()) Serial.println("ERROR: NVS unavailable");
  hasSaved = storageLoad(saved);
  if (hasSaved && !String(saved.deviceMac).equalsIgnoreCase(TARGET_MAC)) {
    Serial.println("SAVED MAC differs from TARGET_MAC; old snapshot will not be advertised");
    hasSaved = false;
  }
  if (!hasSaved && builtInSampleEnabled() &&
      String(TARGET_MAC).equalsIgnoreCase("C3:00:00:1B:95:0F")) {
    Snapshot example;
    if (loadBuiltInSample(example) && storageSave(example)) {
      saved = example;
      hasSaved = true;
      Serial.println("PRELOADED: supplied MBeacon capture (address/PDU types inferred)");
    }
  }
  if (!NimBLEDevice::init("BLE Clone")) {
    Serial.println("ERROR: NimBLE init failed");
    return;
  }
  if (!scanner.begin()) Serial.println("ERROR: scanner init failed");
  changeMode(Mode::Simulate);
  printInfo();
}

void loop() {
  if (button.pressed()) changeMode(mode == Mode::Scan ? Mode::Simulate : Mode::Scan);
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (command.length()) {
        processCommand(command);
        command = "";
      }
    } else if (command.length() < 80) command += c;
  }
  ScanObservation observation;
  while (mode == Mode::Scan && scanner.pop(observation)) {
    if (lastAdvertiseFailureAt &&
        millis() - lastAdvertiseFailureAt < ADVERTISING_RETRY_MS) continue;
    if (!learn(observation)) continue;
    changeMode(Mode::Simulate);
    if (advertiser.active()) startSuccessLight();
    else {
      Serial.printf("ERROR: advertising unavailable; scanning again in %u ms\n",
                    ADVERTISING_RETRY_MS);
      changeMode(Mode::Scan);
      lastAdvertiseFailureAt = millis();
    }
  }
  const uint32_t now = millis();
  if (mode == Mode::Scan && !scanner.active() &&
      now - lastScanRetryAt >= 2000) {
    lastScanRetryAt = now;
    if (!scanner.start()) Serial.println("SCAN ERROR: retry failed");
  }
  updateLed(now);
  delay(5);
}

