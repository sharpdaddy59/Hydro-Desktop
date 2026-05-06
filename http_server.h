// http_server.h — optional management API.
//
// Routes:
//   GET  /         simple status page
//   GET  /devices  JSON snapshot of g_devices
//   POST /devices  add a manual host  (body: hostname=...)
//   DELETE /devices  remove a manual host (body: hostname=...)
//   GET  /status   uptime, RSSI, FW_VERSION, heap
//   POST /wifi/reset  wipe WiFi creds and reboot
//   POST /rebrowse    force mDNS rebrowse
//
// Convention copies cores3-hydro: synchronous WebServer polled from
// loop() via http_server_loop(). LAN-trusted, no auth.

#pragma once

void http_server_begin();
void http_server_loop();
