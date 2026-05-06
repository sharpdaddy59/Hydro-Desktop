// state.h — shared device records + atomic flags.
//
// Convention copies cores3-hydro: producers (poller, discovery) write
// into atomics; consumers (ui) read without locking. Mutation of the
// device array (insertion, reordering) is guarded by g_devices_mutex.

#pragma once

#include <atomic>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

struct DeviceRecord {
  // Identity
  char hostname[48];          // e.g. "cores3-hydro-a3f2"
  char alias[32];             // user-friendly name from prefs (optional)
  IPAddress last_ip;          // cached resolution; 0.0.0.0 if unknown

  // Latest /sensors readings (NaN if never received).
  std::atomic<float> water_temp;
  std::atomic<float> air_temp;
  std::atomic<float> humidity;
  std::atomic<float> light;
  std::atomic<int>   rssi;

  // Latest /status fields.
  std::atomic<uint32_t> uptime_s;
  std::atomic<int>      battery_pct;
  char                  fw_version[16];

  // Health
  std::atomic<uint32_t> last_ok_ms;        // millis() of last successful poll
  std::atomic<uint16_t> consecutive_fails;
  std::atomic<bool>     stale;             // true once consecutive_fails >= STALE_AFTER_MISSES

  // Per-sensor sim flags (mirrored from /sensors so the UI can tint).
  std::atomic<bool> sim_air;
  std::atomic<bool> sim_water;
  std::atomic<bool> sim_light;

  // True once any data has been received successfully.
  std::atomic<bool> has_data;
};

// Globals — defined in state.cpp (or an early translation unit).
extern DeviceRecord    g_devices[MAX_DEVICES];
extern std::atomic<uint8_t> g_device_count;
extern SemaphoreHandle_t    g_devices_mutex;

// Helpers (state.cpp)
void state_init();
int  state_find_by_hostname(const char* hostname);   // -1 if not found
int  state_insert(const char* hostname);             // -1 if MAX_DEVICES reached
