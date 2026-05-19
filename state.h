// state.h — shared device records + atomic flags.
//
// Convention copies cores3-hydro: producers (poller, discovery) write
// into atomics; consumers (ui) read without locking. Mutation of the
// device array (insertion, reordering) is guarded by g_devices_mutex.

#pragma once

#include <atomic>
#include <cstring>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

// Per-sensor mode mirrored from cores3-hydro's /sensors `status` object
// (v0.7.0+). Integer values match the upstream enum byte-for-byte so a
// drop-in re-use of the wire format works without translation. OFF (not
// DISABLED) for the same reason as upstream: dodges ESP-IDF's
// `#define DISABLED 0x00` GPIO-mode macro.
enum class SensorMode : uint8_t {
  REAL      = 0,
  SIMULATED = 1,
  OFF       = 2,
};

// Temperature unit the upstream device is currently reporting in.
// Per-device because each cores3-hydro can be configured independently.
enum class TempUnit : uint8_t {
  CELSIUS    = 0,
  FAHRENHEIT = 1,
};

inline SensorMode sensor_mode_from_str(const char* s) {
  if (!s) return SensorMode::REAL;
  if (strcmp(s, "simulated") == 0) return SensorMode::SIMULATED;
  if (strcmp(s, "disabled")  == 0) return SensorMode::OFF;
  return SensorMode::REAL;
}

inline const char* sensor_mode_label(SensorMode m) {
  switch (m) {
    case SensorMode::SIMULATED: return "simulated";
    case SensorMode::OFF:       return "disabled";
    case SensorMode::REAL:
    default:                    return "real";
  }
}

inline const char* temp_unit_suffix(TempUnit u) {
  return (u == TempUnit::FAHRENHEIT) ? "F" : "C";
}

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

  // Per-sensor source-of-truth mode (mirrored from /sensors `status`).
  // Stored as uint8_t so the atomic is trivially lock-free; cast through
  // SensorMode at use sites.
  std::atomic<uint8_t> mode_air;
  std::atomic<uint8_t> mode_water;
  std::atomic<uint8_t> mode_light;

  // Temperature unit the upstream device is currently emitting in (from
  // /sensors `temperature_units`). Stored as uint8_t so the atomic is
  // lock-free; cast to TempUnit at use sites. Defaults to CELSIUS.
  std::atomic<uint8_t> temp_unit;

  // True once any data has been received successfully.
  std::atomic<bool> has_data;
};

// Globals — defined in state.cpp (or an early translation unit).
extern DeviceRecord    g_devices[MAX_DEVICES];
extern std::atomic<uint8_t> g_device_count;
extern SemaphoreHandle_t    g_devices_mutex;

// Bumped whenever something display-relevant changes (new poll data,
// stale-flag flip, device added, focus/pause/screen change). The UI
// loop skips a redraw when the version hasn't moved since last frame —
// this is what kills idle flicker without per-element dirty-tracking.
extern std::atomic<uint32_t> g_state_version;

// Helpers (state.cpp)
void state_init();
int  state_find_by_hostname(const char* hostname);   // -1 if not found
int  state_insert(const char* hostname);             // -1 if MAX_DEVICES reached
void state_bump_version();                            // invalidate UI cache
