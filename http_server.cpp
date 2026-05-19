#include "http_server.h"
#include "config.h"
#include "state.h"
#include "prefs.h"
#include "wifi_setup.h"
#include "discovery.h"
#include "device_id.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <cmath>

static WebServer s_server(80);

static void send_json(int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  s_server.send(code, "application/json", out);
}

static void handle_root() {
  const char* host = device_hostname();
  String body;
  body.reserve(1500);
  body  = F("<!doctype html><meta charset=utf-8><title>");
  body += host;
  body += F("</title>"
            "<style>"
            "body{font-family:sans-serif;max-width:480px;margin:1.5em auto;padding:0 1em;}"
            "h1{margin-bottom:.1em}h2{margin-top:1.5em}"
            "form{display:inline}"
            "button,input{font-size:1em;padding:.4em .8em;margin:.3em 0}"
            "</style>"
            "<h1>");
  body += host;
  body += F("</h1><p>Firmware " FW_VERSION ". Devices known: ");
  body += g_device_count.load();
  body += F(".</p>"
            "<h2>Inspect</h2>"
            "<ul>"
            "<li><a href=/devices>/devices</a> &mdash; JSON device list</li>"
            "<li><a href=/status>/status</a> &mdash; firmware, uptime, heap</li>"
            "</ul>"
            "<h2>Actions</h2>"
            "<form method=POST action=/rebrowse>"
              "<button type=submit>Re-scan mDNS</button>"
            "</form> "
            "<form method=POST action=/wifi/reset onsubmit=\"return confirm('Wipe WiFi credentials and reboot?');\">"
              "<button type=submit>Reset WiFi</button>"
            "</form>"
            "<h2>Add a manual host</h2>"
            "<form method=POST action=/devices>"
              "<input name=hostname placeholder=hydro-greenhouse-1>"
              "<button type=submit>Add</button>"
            "</form>");
  s_server.send(200, "text/html", body);
}

// Emit a float as JSON null if NaN (cores3-hydro semantics: a null
// value means "stale or sensor disabled, no meaningful reading").
static void emit_float_or_null(JsonObject& o, const char* k, float v) {
  if (isnan(v)) o[k] = nullptr;
  else          o[k] = v;
}

static void handle_devices_get() {
  StaticJsonDocument<3072> doc;
  JsonArray arr = doc.to<JsonArray>();
  uint8_t n = g_device_count.load();
  for (uint8_t i = 0; i < n; i++) {
    const DeviceRecord& d = g_devices[i];
    JsonObject o = arr.createNestedObject();
    o["hostname"]   = d.hostname;
    o["alias"]      = prefs_alias_for(d.hostname);
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

// Small "you POSTed something, here's a Back link + auto-redirect"
// landing page used by the action endpoints.
static void send_back_html(const char* msg) {
  String body;
  body.reserve(400);
  body  = F("<!doctype html><meta charset=utf-8>"
            "<meta http-equiv=refresh content=\"2;url=/\">"
            "<title>");
  body += msg;
  body += F("</title>"
            "<body style=\"font-family:sans-serif;text-align:center;margin-top:3em\">"
            "<p>");
  body += msg;
  body += F("</p><p><a href=/>&larr; Back to home</a></p>");
  s_server.send(200, "text/html", body);
}

static void handle_devices_post() {
  if (!s_server.hasArg("hostname")) {
    s_server.send(400, "text/plain", "need hostname");
    return;
  }
  String h = s_server.arg("hostname");
  prefs_add_manual_host(h.c_str());
  state_insert(h.c_str());
  send_back_html("Manual host added");
}

static void handle_status() {
  StaticJsonDocument<256> doc;
  doc["fw_version"] = FW_VERSION;
  doc["uptime_s"]   = (uint32_t)(millis() / 1000);
  doc["heap_free"]  = ESP.getFreeHeap();
  doc["devices"]    = g_device_count.load();
  send_json(200, doc);
}

void http_server_begin() {
  s_server.on("/",         HTTP_GET,    handle_root);
  s_server.on("/devices",  HTTP_GET,    handle_devices_get);
  s_server.on("/devices",  HTTP_POST,   handle_devices_post);
  s_server.on("/status",   HTTP_GET,    handle_status);
  s_server.on("/wifi/reset", HTTP_POST, []() {
    String body;
    body.reserve(400);
    body  = F("<!doctype html><meta charset=utf-8>"
              "<title>Resetting WiFi</title>"
              "<body style=\"font-family:sans-serif;text-align:center;margin-top:3em\">"
              "<p>Wiping WiFi credentials and rebooting. The device will "
              "come back online in setup-AP mode named "
              "<code>");
    body += device_hostname();
    body += F("-setup</code>.</p>");
    s_server.send(200, "text/html", body);
    delay(500);
    wifi_setup_reset_and_reboot();
  });
  s_server.on("/rebrowse", HTTP_POST, []() {
    discovery_force_rebrowse();
    send_back_html("mDNS re-scan triggered");
  });
  s_server.begin();
}

void http_server_loop() { s_server.handleClient(); }
