// prefs.h — NVS-backed user preferences.
//
// Stores: brightness mode, screen rotation, auto-cycle dwell time,
// temperature unit, SD-log timezone, manually-added hosts (in case mDNS
// browse is unreliable on the LAN), per-device aliases.

#pragma once

#include <Arduino.h>

enum BrightnessMode : uint8_t {
  BRIGHTNESS_AUTO = 0,
  BRIGHTNESS_FULL = 1,
  BRIGHTNESS_DIM  = 2,
};

// Dashboard-wide temperature display unit. AUTO mirrors whatever unit each
// device reports (the original behavior); CELSIUS / FAHRENHEIT convert
// every tile's temperatures to that unit at render time.
enum TempUnitPref : uint8_t {
  TEMP_UNIT_AUTO       = 0,
  TEMP_UNIT_CELSIUS    = 1,
  TEMP_UNIT_FAHRENHEIT = 2,
};

void prefs_load();
void prefs_save();

BrightnessMode prefs_brightness_mode();
void           prefs_set_brightness_mode(BrightnessMode m);

uint8_t prefs_rotation();   // 0..3 (LovyanGFX setRotation)
void    prefs_set_rotation(uint8_t r);

// Auto-cycle dwell — seconds the hero view shows each device before
// advancing to the next. 0 disables auto-cycling (the view holds on
// one device until tapped). prefs_set_cycle_seconds clamps to
// [0, CYCLE_SECONDS_MAX].
static constexpr uint8_t CYCLE_SECONDS_MAX     = 60;
static constexpr uint8_t CYCLE_SECONDS_DEFAULT = 6;
uint8_t prefs_cycle_seconds();
void    prefs_set_cycle_seconds(uint8_t s);

// Temperature display unit — see TempUnitPref. Dashboard-wide.
TempUnitPref prefs_temp_unit();
void         prefs_set_temp_unit(TempUnitPref u);

// POSIX TZ string for the SD-log CSV timestamp ("" = UTC). The value is
// a POSIX TZ string (e.g. "EST5EDT,M3.2.0,M11.1.0") so DST is automatic.
const char* prefs_timezone();
void        prefs_set_timezone(const char* tz);

// SD-log append cadence in minutes. sdlog_loop() re-reads this every
// pass, so a change takes effect without a reboot. Clamped to
// [1, LOG_INTERVAL_MAX_MIN]; default SD_LOG_INTERVAL_DEFAULT_MIN.
static constexpr uint16_t LOG_INTERVAL_MAX_MIN = 1440;   // one day
uint16_t prefs_log_interval_min();
void     prefs_set_log_interval_min(uint16_t m);

// Manual host list — hosts the user added by hand (for LANs where mDNS
// browse misses devices). Discovery merges these into the device array.
uint8_t      prefs_manual_host_count();
const char*  prefs_manual_host(uint8_t i);
bool         prefs_add_manual_host(const char* hostname);
bool         prefs_remove_manual_host(const char* hostname);

// Per-device alias ("Tomatoes" instead of cores3-hydro-a3f2).
const char* prefs_alias_for(const char* hostname);  // "" if none
void        prefs_set_alias(const char* hostname, const char* alias);
