#include "http_server.h"
#include "config.h"
#include "state.h"
#include "prefs.h"
#include "wifi_setup.h"
#include "discovery.h"
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
  String body = "<!doctype html><meta charset=utf-8><title>hydro-dash</title>"
                "<h1>hydro-dash " FW_VERSION "</h1>"
                "<p>Devices: ";
  body += g_device_count.load();
  body += "</p><ul>"
          "<li><a href=/devices>/devices</a></li>"
          "<li><a href=/status>/status</a></li>"
          "</ul>";
  s_server.send(200, "text/html", body);
}

static void handle_devices_get() {
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.to<JsonArray>();
  uint8_t n = g_device_count.load();
  for (uint8_t i = 0; i < n; i++) {
    const DeviceRecord& d = g_devices[i];
    JsonObject o = arr.createNestedObject();
    o["hostname"]   = d.hostname;
    o["alias"]      = prefs_alias_for(d.hostname);
    o["ip"]         = d.last_ip.toString();
    o["water_temp"] = d.water_temp.load();
    o["air_temp"]   = d.air_temp.load();
    o["humidity"]   = d.humidity.load();
    o["light"]      = d.light.load();
    o["rssi"]       = d.rssi.load();
    o["uptime_s"]   = d.uptime_s.load();
    o["stale"]      = (bool)d.stale.load();
    o["fails"]      = d.consecutive_fails.load();
  }
  send_json(200, doc);
}

static void handle_devices_post() {
  if (!s_server.hasArg("hostname")) { s_server.send(400, "text/plain", "need hostname"); return; }
  String h = s_server.arg("hostname");
  prefs_add_manual_host(h.c_str());
  state_insert(h.c_str());
  s_server.send(200, "text/plain", "ok");
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
    s_server.send(200, "text/plain", "resetting WiFi");
    delay(500);
    wifi_setup_reset_and_reboot();
  });
  s_server.on("/rebrowse", HTTP_POST, []() {
    discovery_force_rebrowse();
    s_server.send(200, "text/plain", "ok");
  });
  s_server.begin();
}

void http_server_loop() { s_server.handleClient(); }
