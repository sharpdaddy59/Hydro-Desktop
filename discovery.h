// discovery.h — mDNS browse for cores3-hydro-*.local.
//
// Periodically queries the LAN for HTTP services, filters by hostname
// prefix MDNS_FILTER_PREFIX, and inserts new devices into g_devices.
// Manually-added hosts from prefs are merged into the same array on
// boot.
//
// IPs are cached on the DeviceRecord so a flaky mDNS responder doesn't
// blank the dashboard between browses — the poller falls back to the
// cached IP if hostname resolution fails.

#pragma once

void discovery_begin();
void discovery_force_rebrowse();   // settings screen: "Re-scan now"
