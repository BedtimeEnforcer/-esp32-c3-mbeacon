#include "storage.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <cstring>

#include "config.h"

namespace {
constexpr uint32_t kMagic = 0x424C4531;
constexpr uint16_t kVersion = 3;
Preferences prefs;
bool fsReady = false;

struct StoredSnapshot {
  uint32_t magic;
  uint16_t version;
  Snapshot value;
};
}  // namespace

bool validAdData(const uint8_t* data, size_t length) {
  if (length > MAX_ADV_BYTES) return false;
  for (size_t i = 0; i < length;) {
    const uint8_t fieldLength = data[i];
    if (fieldLength == 0 || i + fieldLength + 1 > length) return false;
    i += fieldLength + 1;
  }
  return true;
}

bool findAdValue(const uint8_t* data, size_t length, uint8_t type,
                 uint8_t* output, uint8_t& outputLength) {
  if (!validAdData(data, length)) return false;
  for (size_t i = 0; i < length;) {
    const uint8_t fieldLength = data[i];
    if (data[i + 1] == type) {
      outputLength = fieldLength - 1;
      memcpy(output, data + i + 2, outputLength);
      return true;
    }
    i += fieldLength + 1;
  }
  return false;
}

String hexString(const uint8_t* data, size_t length) {
  const char digits[] = "0123456789ABCDEF";
  String result;
  result.reserve(length * 2);
  for (size_t i = 0; i < length; ++i) {
    result += digits[data[i] >> 4];
    result += digits[data[i] & 0x0F];
  }
  return result;
}

bool storageBegin() {
  fsReady = LittleFS.begin(true);
  return prefs.begin("bleclone", false);
}

bool storageLoad(Snapshot& value) {
  if (prefs.getBytesLength("snapshot") != sizeof(StoredSnapshot)) return false;
  StoredSnapshot stored{};
  if (prefs.getBytes("snapshot", &stored, sizeof(stored)) != sizeof(stored)) return false;
  if (stored.magic != kMagic || stored.version != kVersion) return false;
  const Snapshot& v = stored.value;
  if (v.deviceMac[17] != '\0' || v.advLength > MAX_ADV_BYTES ||
      v.responseLength > MAX_ADV_BYTES || v.rawLength == 0 ||
      v.rawLength > MAX_RAW_BYTES ||
      v.rawLength != v.advLength + v.responseLength ||
      !validAdData(v.advData, v.advLength) ||
      !validAdData(v.responseData, v.responseLength)) return false;
  value = v;
  return true;
}

bool storageSave(const Snapshot& value) {
  StoredSnapshot stored{};
  stored.magic = kMagic;
  stored.version = kVersion;
  stored.value = value;
  return prefs.putBytes("snapshot", &stored, sizeof(stored)) == sizeof(stored);
}

bool storageClear() {
  bool ok = prefs.clear();
  ok = prefs.putBool("skipSample", true) && ok;
  if (fsReady && LittleFS.exists("/scan_log.txt")) ok = LittleFS.remove("/scan_log.txt") && ok;
  return ok;
}

bool builtInSampleEnabled() {
  return !prefs.getBool("skipSample", false);
}

bool appendScanLog(const Snapshot& value, int rssi, uint64_t epoch) {
  if (!fsReady) return false;
  if (LittleFS.exists("/scan_log.txt")) {
    File existing = LittleFS.open("/scan_log.txt", "r");
    const size_t size = existing ? existing.size() : 0;
    existing.close();
    if (size >= SCAN_LOG_MAX_BYTES) LittleFS.remove("/scan_log.txt");
  }
  File file = LittleFS.open("/scan_log.txt", "a");
  if (!file) return false;
  file.printf("epoch=%llu uptime_ms=%lu mac=%s rssi=%d type=0x%02X adv=%s rsp=%s\n",
              static_cast<unsigned long long>(epoch),
              static_cast<unsigned long>(millis()), value.deviceMac, rssi,
              value.advType, hexString(value.advData, value.advLength).c_str(),
              hexString(value.responseData, value.responseLength).c_str());
  file.close();
  return true;
}

void printScanLog(Stream& output) {
  if (!fsReady || !LittleFS.exists("/scan_log.txt")) {
    output.println("LOG: empty or LittleFS unavailable");
    return;
  }
  File file = LittleFS.open("/scan_log.txt", "r");
  if (!file) {
    output.println("LOG: open failed");
    return;
  }
  while (file.available()) output.write(file.read());
  file.close();
}

