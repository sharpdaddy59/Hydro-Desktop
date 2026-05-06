#include "poller.h"
#include "config.h"
#include "state.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

// Build "http://<host>.local/<path>" or "http://<ip>/<path>" depending on
// whether the cached IP is usable. Falling back to .local survives router
// reboots; falling back to cached IP survives flaky mDNS.
static String url_for(const DeviceRecord& d, const char* path) {
  if (d.last_ip != IPAddress() && d.last_ip != IPAddress(0,0,0,0)) {
    return String("http://") + d.last_ip.toString() + path;
  }
  return String("http://") + d.hostname + ".local" + path;
}

static bool poll_sensors(DeviceRecord& d) {
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(url_for(d, "/sensors"))) return false;
  int code = http.GET();
  if (code != 200) { http.end(); return false; }

  StaticJsonDocument<768> doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) return false;

  if (doc.containsKey("water_temp")) d.water_temp.store(doc["water_temp"].as<float>());
  if (doc.containsKey("air_temp"))   d.air_temp.store(doc["air_temp"].as<float>());
  if (doc.containsKey("humidity"))   d.humidity.store(doc["humidity"].as<float>());
  if (doc.containsKey("light"))      d.light.store(doc["light"].as<float>());
  if (doc.containsKey("rssi"))       d.rssi.store(doc["rssi"].as<int>());

  if (doc.containsKey("simulated")) {
    JsonObject sim = doc["simulated"];
    d.sim_air.store(sim["air"]   | false);
    d.sim_water.store(sim["water"] | false);
    d.sim_light.store(sim["light"] | false);
  }

  d.last_ok_ms.store(millis());
  d.consecutive_fails.store(0);
  d.stale.store(false);
  d.has_data.store(true);
  return true;
}

static bool poll_status(DeviceRecord& d) {
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(url_for(d, "/status"))) return false;
  int code = http.GET();
  if (code != 200) { http.end(); return false; }

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, http.getStream())) { http.end(); return false; }
  http.end();

  if (doc.containsKey("uptime_s"))    d.uptime_s.store(doc["uptime_s"].as<uint32_t>());
  if (doc.containsKey("battery_pct")) d.battery_pct.store(doc["battery_pct"].as<int>());
  if (doc.containsKey("fw_version")) {
    const char* v = doc["fw_version"];
    strncpy(d.fw_version, v ? v : "", sizeof(d.fw_version) - 1);
    d.fw_version[sizeof(d.fw_version) - 1] = '\0';
  }
  return true;
}

static void task_poller(void* /*arg*/) {
  uint32_t last_status_run[MAX_DEVICES] = {0};

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    uint8_t n = g_device_count.load();
    for (uint8_t i = 0; i < n; i++) {
      DeviceRecord& d = g_devices[i];
      if (!poll_sensors(d)) {
        uint16_t f = d.consecutive_fails.load() + 1;
        d.consecutive_fails.store(f);
        if (f >= STALE_AFTER_MISSES) d.stale.store(true);
      }
      if (millis() - last_status_run[i] > STATUS_INTERVAL_MS) {
        poll_status(d);
        last_status_run[i] = millis();
      }
      vTaskDelay(pdMS_TO_TICKS(50));   // small breath between devices
    }
    vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
  }
}

void poller_begin() {
  xTaskCreatePinnedToCore(task_poller, "poller", 8192, nullptr, 1, nullptr, 1);
}
