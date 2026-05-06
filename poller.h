// poller.h — round-robin HTTP poll of every known device.
//
// One FreeRTOS task. Iterates g_devices, GETs /sensors (fast cadence)
// and /status (slow cadence), parses with ArduinoJson, writes results
// into atomics on the corresponding DeviceRecord.
//
// On consecutive failures past STALE_AFTER_MISSES the record's stale
// flag flips and ui_grid renders the tile in gray.

#pragma once

void poller_begin();
