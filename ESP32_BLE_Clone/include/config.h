#pragma once

// Set this to the address printed by the target's scan report.
#define TARGET_MAC "C3:00:00:1B:95:0F"

// Required AD field before accepting a scan result. The complete ADV and
// scan-response bytes are compared and persisted as raw_data.
#define RAW_AD_TYPE 0xFF

// LuatOS ESP32C3-CORE on-board BOOT key and D4 LED. Press BOOT only after
// startup; holding it during reset enters firmware download mode.
// The documented external-flash board uses DIO, freeing GPIO12 for D4.
#define BUTTON_PIN 9
#define LED_PIN 12
#define LED_ACTIVE_LOW false

#define ADV_INTERVAL_MS 250
#define SCAN_INTERVAL_MS 100
#define SCAN_WINDOW_MS 90
#define SCAN_LOG_MIN_INTERVAL_MS 5000
#define SCAN_LOG_MAX_BYTES 65536
#define SCAN_LED_TOGGLE_MS 150
#define SIMULATE_LED_TOGGLE_MS 700
#define LEARNED_LED_ON_MS 3000
#define LEARNED_LED_OFF_MS 200
#define ADVERTISING_RETRY_MS 5000

