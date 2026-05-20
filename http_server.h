// http_server.h — management API + settings web app.
//
// Routes:
//   GET  /                   settings single-page app (gzipped, PROGMEM)
//   GET  /devices            JSON snapshot of g_devices
//   POST /devices            add a manual host        (body: hostname=...)
//   POST /devices/remove     drop a manual host       (body: hostname=...)
//   GET  /status             uptime, FW_VERSION, heap, device count
//   GET  /config             brightness, cycle dwell, manual-host list
//   POST /config/brightness  set backlight mode       (body: mode=...)
//   POST /config/cycle       set auto-cycle seconds   (body: seconds=...)
//   POST /config/alias       set/clear a display name (body: hostname,alias)
//   POST /wifi/reset         wipe WiFi creds and reboot
//   POST /rebrowse           force an mDNS rebrowse
//   POST /ota/upload         firmware update          (see ota.cpp)
//
// Convention copies cores3-hydro: synchronous WebServer polled from
// loop() via http_server_loop(). LAN-trusted, no auth.

#pragma once

void http_server_begin();
void http_server_loop();
