#include "discovery.h"
#include "config.h"
#include "state.h"
#include "prefs.h"
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

static volatile bool s_force = false;

static void seed_manual_hosts() {
  uint8_t n = prefs_manual_host_count();
  for (uint8_t i = 0; i < n; i++) {
    state_insert(prefs_manual_host(i));
  }
}

// GET http://<ip>/sensors with a short timeout; treat the device as a
// hydro if the response looks like one. Cheap content sniff (looking for
// the "water_temp" field name) avoids paying for full JSON parsing in
// the discovery path.
static bool probe_is_hydro(const IPAddress& ip) {
  HTTPClient http;
  http.setTimeout(2000);
  String url = String("http://") + ip.toString() + "/sensors";
  if (!http.begin(url)) return false;
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  String body = http.getString();
  http.end();
  return body.indexOf("water_temp") >= 0;
}

static void browse_once() {
  // mDNS-SD service browse for every HTTP server on the LAN. We can't
  // filter by hostname prefix anymore (devices are user-renameable), so
  // we probe each candidate's /sensors endpoint and accept any device
  // whose response contains the expected hydro field shape.
  int n = MDNS.queryService("http", "tcp");
  for (int i = 0; i < n; i++) {
    String host = MDNS.hostname(i);
    IPAddress ip = MDNS.address(i);
    if (state_find_by_hostname(host.c_str()) >= 0) continue;  // already known
    if (!probe_is_hydro(ip)) continue;
    int idx = state_insert(host.c_str());
    if (idx >= 0) g_devices[idx].last_ip = ip;
  }
}

static void task_discovery(void* /*arg*/) {
  // Wait for WiFi to settle before the first browse — mDNS responder
  // isn't reachable until the IP is up.
  for (;;) {
    browse_once();
    uint32_t deadline = millis() + DISCOVERY_INTERVAL_MS;
    while (millis() < deadline) {
      if (s_force) { s_force = false; break; }
      vTaskDelay(pdMS_TO_TICKS(200));
    }
  }
}

void discovery_begin() {
  if (!MDNS.begin("hydro-dash")) {
    // Non-fatal — we just won't be able to browse.
  }
  seed_manual_hosts();
  xTaskCreatePinnedToCore(task_discovery, "discovery", 4096, nullptr, 1, nullptr, 0);
}

void discovery_force_rebrowse() { s_force = true; }
