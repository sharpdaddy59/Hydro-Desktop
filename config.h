// config.h — central tunables for hydro-dash.
//
// Pinout matches the Sunton ESP32-2432S028R (CYD) per
// https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/
//
// If you have a different revision (S028C capacitive, or one of the
// silently-different clone variants), the pin map below is the place to fix.

#pragma once

#define FW_VERSION       "0.1.11"

// ---------------------------------------------------------------------------
// Display (ILI9341, HSPI bus, 240x320 portrait native -> rotated to 320x240)
// ---------------------------------------------------------------------------
#define TFT_MOSI         13
#define TFT_MISO         12
#define TFT_SCLK         14
#define TFT_CS           15
#define TFT_DC           2
#define TFT_RST          -1   // tied to ESP32 reset; software reset only
#define TFT_BL           21   // backlight, active HIGH, PWM-capable
#define TFT_BL_PWM_CH    0
#define TFT_BL_PWM_FREQ  5000
#define TFT_BL_PWM_BITS  8

#define TFT_W            320
#define TFT_H            240

// ---------------------------------------------------------------------------
// Touch (XPT2046, separate VSPI bus — shares VSPI with SD if SD ever used)
// ---------------------------------------------------------------------------
#define TOUCH_MOSI       32
#define TOUCH_MISO       39
#define TOUCH_SCLK       25
#define TOUCH_CS         33
#define TOUCH_IRQ        36

// ---------------------------------------------------------------------------
// On-board sensors / indicators
// ---------------------------------------------------------------------------
#define LDR_PIN          34   // ADC1, input-only
#define SPEAKER_PIN      26   // not used by default; reserved
#define LED_R_PIN        4    // active LOW
#define LED_G_PIN        16
#define LED_B_PIN        17

// ---------------------------------------------------------------------------
// App behavior
// ---------------------------------------------------------------------------
#define MAX_DEVICES              8     // tile grid is 2x2; storage for more
#define POLL_INTERVAL_MS         15000 // per-device /sensors poll cadence
#define STATUS_INTERVAL_MS       60000 // per-device /status poll cadence
#define DISCOVERY_INTERVAL_MS    60000 // mDNS browse cadence
#define HTTP_TIMEOUT_MS          3000
#define STALE_AFTER_MISSES       3     // gray a tile after N consecutive failures

#define MDNS_FILTER_PREFIX       "cores3-hydro-"

// AP mode (WiFiManager fallback when no creds saved). The SSID is
// built at runtime as "<device_hostname>-setup" so multiple units being
// onboarded simultaneously don't show identical networks.
#define AP_PASSWORD              ""    // open AP; user only sees it during setup
#define AP_TIMEOUT_S             180

// Backlight auto-dim
//
// CYD wiring (per the Sunton schematic): R10 1MΩ pull-up to 3V3, LDR
// between GPIO 34 and GND. So bright light drops the LDR's resistance,
// pulls the tap toward GND, and produces a LOW raw ADC value. Dark
// produces a HIGH raw value. Constants below are named for the room
// condition, not the raw direction — so BL_LDR_BRIGHT < BL_LDR_DARK
// numerically.
//
// Values measured at ADC_6db attenuation through the enclosure cover:
// indoor-lit ≈ 170 raw, dim ≈ 300, lights-out ≈ 500. Headroom added
// at both ends so brighter-than-tested and darker-than-tested clamp
// cleanly to MAX/MIN duty.
#define BL_MAX_DUTY              255
#define BL_MIN_DUTY              30
#define BL_LDR_BRIGHT            150   // raw ADC when room is bright (low because of CYD wiring)
#define BL_LDR_DARK              550   // raw ADC when room is dark (high because of CYD wiring)
