// hydro-dash.ino — entry point.
//
// Desktop dashboard for cores3-hydro devices. Polls each unit's /sensors
// and /status over LAN HTTP, renders a glanceable grid on the CYD display.
//
// Hardware: Sunton ESP32-2432S028R (CYD). Pin map in config.h, full
// design in docs/hydro-dash-spec.md.
//
// Boot sequence is intentionally explicit — same convention as cores3-hydro.

#include "config.h"
#include "state.h"
#include "prefs.h"
#include "backlight.h"
#include "ui.h"
#include "touch.h"
#include "wifi_setup.h"
#include "discovery.h"
#include "poller.h"
#include "ble_scanner.h"
#include "http_server.h"

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.printf("[boot] hydro-dash %s\n", FW_VERSION);

  // Allocate the device-table mutex before anything that reads/writes it.
  state_init();

  // NVS first — UI prefs and saved manual hosts inform later steps.
  prefs_load();

  // Backlight + display before WiFi so the user sees a "connecting" screen
  // during onboarding rather than a dark panel.
  backlight_begin();
  ui_begin();
  ui_set_status("Connecting to WiFi...");

  touch_begin();

  // Blocks until WiFi up. WiFiManager opens an AP if no creds saved.
  wifi_setup_begin();

  // mDNS browse + per-device polling tasks.
  discovery_begin();
  poller_begin();

  // Passive BLE advertisement scan for Govee H5075 sensors. After WiFi so
  // BLE init doesn't contend with WiFiManager's AP-mode radio use.
  ble_scanner_begin();

  // Optional management API — /devices, /wifi/reset, /status.
  http_server_begin();

  ui_set_status("");  // clear the connecting message
}

void loop() {
  ui_loop();
  touch_loop();
  backlight_loop();
  http_server_loop();
  delay(10);
}
