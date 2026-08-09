#include "http_server.h"
#include "config.h"
#include "state.h"
#include "prefs.h"
#include "wifi_setup.h"
#include "discovery.h"
#include "device_id.h"
#include "web_assets.h"
#include "ota.h"
#include "sdlog.h"
#include <WebServer.h>
#include <FS.h>
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
    // "http" = polled cores3-hydro; "ble" = passively-heard Govee H5075.
    // For "ble" rows, ip/uptime_s/fails are not meaningful.
    o["kind"]       = ((DeviceKind)d.device_kind.load() == DeviceKind::BLE)
                          ? "ble" : "http";
    o["mac"]        = d.mac;          // populated for BLE devices, "" for HTTP
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

  const char* tu = "auto";
  switch (prefs_temp_unit()) {
    case TEMP_UNIT_CELSIUS:    tu = "celsius";    break;
    case TEMP_UNIT_FAHRENHEIT: tu = "fahrenheit"; break;
    case TEMP_UNIT_AUTO: break;
  }
  doc["temp_unit"]        = tu;
  doc["sdlog"]            = sdlog_active() ? "logging" : "no card";
  doc["timezone"]         = prefs_timezone();
  doc["log_interval_min"] = prefs_log_interval_min();

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

// POST /config/units — form arg unit=auto|celsius|fahrenheit. The hero
// view resolves the display unit on every redraw, so bump the UI version
// to apply the change immediately.
static void handle_config_units() {
  if (!s_server.hasArg("unit")) {
    s_server.send(400, "text/plain", "need unit");
    return;
  }
  String u = s_server.arg("unit");
  if      (u == "auto")       prefs_set_temp_unit(TEMP_UNIT_AUTO);
  else if (u == "celsius")    prefs_set_temp_unit(TEMP_UNIT_CELSIUS);
  else if (u == "fahrenheit") prefs_set_temp_unit(TEMP_UNIT_FAHRENHEIT);
  else { s_server.send(400, "text/plain", "bad unit"); return; }
  state_bump_version();
  send_ok();
}

// POST /config/timezone — form arg tz=<POSIX TZ string> ("" = UTC). The
// value comes from the SPA's fixed zone dropdown; applied immediately so
// the next logged CSV row picks it up.
static void handle_config_timezone() {
  String tz = s_server.hasArg("tz") ? s_server.arg("tz") : String();
  prefs_set_timezone(tz.c_str());
  sdlog_apply_timezone();
  send_ok();
}

// POST /config/loginterval — form arg minutes=N. sdlog_loop re-reads the
// pref every pass, so the new cadence applies without a reboot.
static void handle_config_loginterval() {
  if (!s_server.hasArg("minutes")) {
    s_server.send(400, "text/plain", "need minutes");
    return;
  }
  long v = s_server.arg("minutes").toInt();
  if (v < 1) v = 1;
  if (v > LOG_INTERVAL_MAX_MIN) v = LOG_INTERVAL_MAX_MIN;
  prefs_set_log_interval_min((uint16_t)v);
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

// GET /logs — JSON list of the CSV log files on the SD card (name + size).
struct LogListCtx { JsonArray* arr; int count; };
static void log_list_cb(const char* name, uint32_t size, void* ctx) {
  LogListCtx* c = (LogListCtx*)ctx;
  if (c->count >= 180) return;          // bound the JSON document
  JsonObject o = c->arr->createNestedObject();
  o["name"] = String(name);             // copy now — `name` is transient
  o["size"] = size;
  c->count++;
}
static void handle_logs_get() {
  static StaticJsonDocument<8192> doc;
  JsonArray arr = doc.to<JsonArray>();
  LogListCtx ctx{ &arr, 0 };
  sdlog_list_files(log_list_cb, &ctx);   // empty array = no card / no files
  send_json(200, doc);
}

// GET /logs/download?file=NAME — stream one log file as a CSV download.
// `file` is untrusted: sdlog_is_log_filename() is the path-traversal gate
// and runs before any SD access.
static void handle_logs_download() {
  if (!s_server.hasArg("file")) {
    s_server.send(400, "text/plain", "need file");
    return;
  }
  String name = s_server.arg("file");
  if (!sdlog_is_log_filename(name.c_str())) {
    s_server.send(400, "text/plain", "bad file");
    return;
  }
  File f = sdlog_open_for_read(name.c_str());
  if (!f) {
    s_server.send(404, "text/plain", "not found");
    return;
  }
  s_server.sendHeader("Content-Disposition",
                      "attachment; filename=\"" + name + "\"");
  s_server.streamFile(f, "text/csv");    // blocks the loop for the transfer
  f.close();
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
  s_server.on("/config/units",      HTTP_POST, handle_config_units);
  s_server.on("/config/timezone",   HTTP_POST, handle_config_timezone);
  s_server.on("/config/loginterval", HTTP_POST, handle_config_loginterval);
  s_server.on("/config/alias",      HTTP_POST, handle_config_alias);
  s_server.on("/logs",              HTTP_GET,  handle_logs_get);
  s_server.on("/logs/download",     HTTP_GET,  handle_logs_download);
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
