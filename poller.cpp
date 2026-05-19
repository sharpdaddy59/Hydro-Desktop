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

// Read a numeric field that may be JSON null (cores3-hydro emits null
// when a reading is stale or the sensor is disabled). Maps null/missing
// to NaN so the UI's existing isnan() branch renders "--".
static float json_float_or_nan(JsonVariantConst v) {
  if (v.isNull()) return NAN;
  return v.as<float>();
}

// Store v into dst only if it differs from the current value. Returns
// true iff a write happened — callers OR this into a `changed` flag and
// only bump the UI version when something visible actually moved.
// Without this, a poll that returned identical readings still triggered
// a full hero redraw (POLL_INTERVAL_MS / N_devices apart), which the
// user saw as a periodic flash between cycle transitions.
template <typename T>
static bool set_if_changed(std::atomic<T>& dst, T v) {
  T old = dst.load();
  if (old == v) return false;
  dst.store(v);
  return true;
}

// NaN-aware float variant: treat NaN→NaN as no-change. Without this a
// disabled-upstream sensor (value is null → NaN every poll) would
// register as a change on every comparison since NaN != NaN per IEEE 754.
static bool set_float_if_changed(std::atomic<float>& dst, float v) {
  float old = dst.load();
  if ((isnan(old) && isnan(v)) || old == v) return false;
  dst.store(v);
  return true;
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

  // Track whether any displayed field actually changed. The UI version
  // is bumped only when something visible moved, so a poll that yielded
  // identical readings no longer triggers a redraw between the 6 s
  // auto-cycle transitions.
  bool changed = false;

  // Per-reading null is the v0.7.0+ contract for "stale or disabled".
  // Map to NaN so the UI's existing isnan() rendering still works.
  changed |= set_float_if_changed(d.water_temp, json_float_or_nan(doc["water_temp"]));
  changed |= set_float_if_changed(d.air_temp,   json_float_or_nan(doc["air_temp"]));
  changed |= set_float_if_changed(d.humidity,   json_float_or_nan(doc["humidity"]));
  changed |= set_float_if_changed(d.light,      json_float_or_nan(doc["light"]));
  if (doc.containsKey("rssi")) {
    changed |= set_if_changed(d.rssi, doc["rssi"].as<int>());
  }

  // Temperature unit. Upstream emits "celsius" or "fahrenheit"; absent
  // field defaults to celsius (the legacy assumption).
  {
    const char* tu = doc["temperature_units"] | "celsius";
    uint8_t new_unit = (uint8_t)(strcmp(tu, "fahrenheit") == 0
                                   ? TempUnit::FAHRENHEIT
                                   : TempUnit::CELSIUS);
    changed |= set_if_changed(d.temp_unit, new_unit);
  }

  // Per-sensor mode object. Replaced the pre-v0.7.0 boolean `simulated`
  // object — values are "real" | "simulated" | "disabled" strings.
  if (doc.containsKey("status")) {
    JsonObjectConst st = doc["status"].as<JsonObjectConst>();
    changed |= set_if_changed(d.mode_air,
                              (uint8_t)sensor_mode_from_str(st["air"]   | (const char*)nullptr));
    changed |= set_if_changed(d.mode_water,
                              (uint8_t)sensor_mode_from_str(st["water"] | (const char*)nullptr));
    changed |= set_if_changed(d.mode_light,
                              (uint8_t)sensor_mode_from_str(st["light"] | (const char*)nullptr));
  }

  // Visual transitions that the strip-dot colour depends on: first-ever
  // successful poll (has_data 0→1), and stale-recovery (stale 1→0).
  // exchange() returns the previous value so we can detect the edge in
  // a single atomic op.
  if (!d.has_data.exchange(true)) changed = true;
  if (d.stale.exchange(false))    changed = true;

  // Bookkeeping fields that don't drive the hero render — these update
  // every poll regardless and intentionally do NOT contribute to
  // `changed`.
  d.last_ok_ms.store(millis());
  d.consecutive_fails.store(0);

  if (changed) state_bump_version();
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

  // Upstream renamed `uptime_s` → `uptime` in v0.7.0; field-name only,
  // still seconds. Accept either so a mixed-version LAN keeps working
  // until every cores3-hydro is upgraded.
  if (doc.containsKey("uptime"))      d.uptime_s.store(doc["uptime"].as<uint32_t>());
  else if (doc.containsKey("uptime_s")) d.uptime_s.store(doc["uptime_s"].as<uint32_t>());
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
        if (f == STALE_AFTER_MISSES) {
          d.stale.store(true);
          state_bump_version();   // tile color flips; redraw the strip
        }
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
