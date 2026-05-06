#include "discovery.h"
#include "config.h"
#include "state.h"
#include "prefs.h"
#include <ESPmDNS.h>
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

static void browse_once() {
  // mDNS-SD service browse for all HTTP servers on the LAN, then filter
  // by hostname prefix. queryService is blocking but bounded.
  int n = MDNS.queryService("http", "tcp");
  for (int i = 0; i < n; i++) {
    String host = MDNS.hostname(i);
    if (!host.startsWith(MDNS_FILTER_PREFIX)) continue;
    int idx = state_insert(host.c_str());
    if (idx >= 0) g_devices[idx].last_ip = MDNS.address(i);
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
