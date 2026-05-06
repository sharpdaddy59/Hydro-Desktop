#include "wifi_setup.h"
#include "config.h"
#include "ui.h"
#include <WiFi.h>
#include <WiFiManager.h>

static bool s_in_ap = false;

void wifi_setup_begin() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(AP_TIMEOUT_S);
  wm.setAPCallback([](WiFiManager* /*m*/) {
    s_in_ap = true;
    ui_set_status("AP: " AP_SSID);
  });

  // autoConnect either reuses saved creds or opens an AP and blocks
  // until the user submits credentials. After timeout it returns false
  // and we reboot — there's nothing useful for a dashboard to do offline.
  bool ok = wm.autoConnect(AP_SSID);
  if (!ok) {
    ui_set_status("WiFi failed; rebooting");
    delay(2000);
    ESP.restart();
  }
  s_in_ap = false;
}

void wifi_setup_reset_and_reboot() {
  WiFiManager wm;
  wm.resetSettings();
  delay(200);
  ESP.restart();
}

bool wifi_setup_in_ap_mode() { return s_in_ap; }
