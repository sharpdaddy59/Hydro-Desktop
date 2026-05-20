#include "http_server.h"
#include "config.h"
#include "state.h"
#include "prefs.h"
#include "wifi_setup.h"
#include "discovery.h"
#include "device_id.h"
#include "web_assets.h"
#include "ota.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <cmath>

static WebServer s_server(80);

static void send_json(int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  s_server.send(code, "application/json", out);
}

// Standard reply for the SPA's write endpoints — it fetch()es these and
// only checks the HTTP status, so a tiny JSON body is all that's needed.
static void send_ok() {
  s_server.send(200, "application/json", "{\"ok\":true}");
}

// GET / — the settings single-page app. Served straight from PROGMEM as
// pre-gzipped bytes (see web_assets.h, generated from web/index.html).
static void handle_root() {
  s_server.sendHeader("Content-Encoding", "gzip");
  s_server.sendHeader("Cache-Control", "no-cache");
  s_server.send_P(200, "text/html", (PGM_P)WEB_INDEX_HTML_GZ,
                  WEB_INDEX_HTML_GZ_LEN);
}

// Emit a float as JSON null if NaN (cores3-hydro semantics: a null
// value means "stale or sensor disabled, no meaningful reading").
static void emit_float_or_null(JsonObject& o, const char* k, float v) {
  if (isnan(v)) o[k] = nullptr;
  else          o[k] = v;
}

static void handle_devices_get() {
  // Static (not stack) — at 8 devices this document is several KB and
  // the WebServer handlers run on the main loop task's modest stack.
  // to<JsonArray>() clears it, so there's no stale carry-over.
  static StaticJsonDocument<8192> doc;
  JsonArray arr = doc.to<JsonArray>();
  uint8_t n = g_device_count.load();
  for (uint8_t i = 0; i < n; i++) {
    const DeviceRecord& d = g_devices[i];
    JsonObject o = arr.createNestedObject();
    o["hostname"]   = d.hostname;
    // prefs_alias_for() returns a pointer into a shared static buffer
    // that the next call overwrites — wrap in String so ArduinoJson
    // copies the value now instead of storing the soon-stale pointer.
    o["alias"]      = String(prefs_alias_for(d.hostname));
    o["ip"]         = d.last_ip.toString();
    emit_float_or_null(o, "water_temp", d.water_temp.load());
    emit_float_or_null(o, "air_temp",   d.air_temp.load());
    emit_float_or_null(o, "humidity",   d.humidity.load());
    emit_float_or_null(o, "light",      d.light.load());
    o["temperature_units"] =
        ((TempUnit)d.temp_unit.load() == TempUnit::FAHRENHEIT)
            ? "fahrenheit" : "celsius";
    o["rssi"]       = d.rssi.load();
    o["uptime_s"]   = d.uptime_s.load();
    o["stale"]      = (bool)d.stale.load();
    o["fails"]      = d.consecutive_fails.load();
    JsonObject status = o.createNestedObject("status");
    status["air"]   = sensor_mode_label((SensorMode)d.mode_air.load());
    status["water"] = sensor_mode_label((SensorMode)d.mode_water.load());
    status["light"] = sensor_mode_label((SensorMode)d.mode_light.load());
  }
  send_json(200, doc);
}

static void handle_devices_post() {
  if (!s_server.hasArg("hostname")) {
    s_server.send(400, "text/plain", "need hostname");
    return;
  }
  String h = s_server.arg("hostname");
  h.trim();
  if (!h.length()) {
    s_server.send(400, "text/plain", "empty hostname");
    return;
  }
  prefs_add_manual_host(h.c_str());
  state_insert(h.c_str());
  send_ok();
}

// Remove a manually-added host from the persisted list. The device's
// existing record stays in g_devices until the next reboot — the array
// is append-only by design (the UI task reads it lockless), so removal
// only stops the host being re-seeded on subsequent boots. A host that
// is also mDNS-visible will simply be rediscovered.
static void handle_devices_remove() {
  if (!s_server.hasArg("hostname")) {
    s_server.send(400, "text/plain", "need hostname");
    return;
  }
  prefs_remove_manual_host(s_server.arg("hostname").c_str());
  send_ok();
}

static void handle_status() {
  StaticJsonDocument<256> doc;
  doc["fw_version"] = FW_VERSION;
  doc["uptime_s"]   = (uint32_t)(millis() / 1000);
  doc["heap_free"]  = ESP.getFreeHeap();
  doc["devices"]    = g_device_count.load();
  send_json(200, doc);
}

// GET /config — settings snapshot for the SPA. manual_hosts lets the
// page decide which device rows get a "remove" button.
static void handle_config_get() {
  StaticJsonDocument<1024> doc;
  doc["fw_version"] = FW_VERSION;
  doc["hostname"]   = device_hostname();

  const char* bl = "auto";
  switch (prefs_brightness_mode()) {
    case BRIGHTNESS_FULL: bl = "full"; break;
    case BRIGHTNESS_DIM:  bl = "dim";  break;
    case BRIGHTNESS_AUTO: break;
  }
  doc["brightness"]    = bl;
  doc["cycle_seconds"] = prefs_cycle_seconds();
  doc["cycle_max"]     = CYCLE_SECONDS_MAX;

  JsonArray mh = doc.createNestedArray("manual_hosts");
  uint8_t n = prefs_manual_host_count();
  for (uint8_t i = 0; i < n; i++) mh.add(prefs_manual_host(i));

  send_json(200, doc);
}

// POST /config/brightness — form arg mode=auto|full|dim. backlight_loop
// re-reads the mode every 500 ms, so the change applies on its own.
static void handle_config_brightness() {
  if (!s_server.hasArg("mode")) {
    s_server.send(400, "text/plain", "need mode");
    return;
  }
  String m = s_server.arg("mode");
  if      (m == "auto") prefs_set_brightness_mode(BRIGHTNESS_AUTO);
  else if (m == "full") prefs_set_brightness_mode(BRIGHTNESS_FULL);
  else if (m == "dim")  prefs_set_brightness_mode(BRIGHTNESS_DIM);
  else { s_server.send(400, "text/plain", "bad mode"); return; }
  send_ok();
}

// POST /config/cycle — form arg seconds=N. 0 disables auto-cycling;
// prefs_set_cycle_seconds clamps the upper bound. ui_hero_tick picks up
// the new dwell on its next tick.
static void handle_config_cycle() {
  if (!s_server.hasArg("seconds")) {
    s_server.send(400, "text/plain", "need seconds");
    return;
  }
  long v = s_server.arg("seconds").toInt();
  if (v < 0) v = 0;
  if (v > CYCLE_SECONDS_MAX) v = CYCLE_SECONDS_MAX;
  prefs_set_cycle_seconds((uint8_t)v);
  send_ok();
}

// POST /config/alias — form args hostname=... & alias=... (empty alias
// clears it). The hero view renders alias-or-hostname, so bump the UI
// version to force a redraw with the new name.
static void handle_config_alias() {
  if (!s_server.hasArg("hostname")) {
    s_server.send(400, "text/plain", "need hostname");
    return;
  }
  String h = s_server.arg("hostname");
  String a = s_server.hasArg("alias") ? s_server.arg("alias") : String();
  a.trim();
  prefs_set_alias(h.c_str(), a.c_str());
  state_bump_version();
  send_ok();
}

void http_server_begin() {
  s_server.on("/",                  HTTP_GET,  handle_root);
  s_server.on("/devices",           HTTP_GET,  handle_devices_get);
  s_server.on("/devices",           HTTP_POST, handle_devices_post);
  s_server.on("/devices/remove",    HTTP_POST, handle_devices_remove);
  s_server.on("/status",            HTTP_GET,  handle_status);
  s_server.on("/config",            HTTP_GET,  handle_config_get);
  s_server.on("/config/brightness", HTTP_POST, handle_config_brightness);
  s_server.on("/config/cycle",      HTTP_POST, handle_config_cycle);
  s_server.on("/config/alias",      HTTP_POST, handle_config_alias);
  s_server.on("/wifi/reset", HTTP_POST, []() {
    s_server.send(200, "text/plain",
                  "Resetting WiFi - rebooting into the setup access point.");
    delay(500);
    wifi_setup_reset_and_reboot();
  });
  s_server.on("/rebrowse", HTTP_POST, []() {
    discovery_force_rebrowse();
    send_ok();
  });
  // POST /ota/upload — browser-driven firmware update. Owned by ota.cpp.
  ota_register(s_server);
  s_server.begin();
}

void http_server_loop() { s_server.handleClient(); }
