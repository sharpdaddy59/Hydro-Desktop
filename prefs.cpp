#include "prefs.h"
#include <Preferences.h>
#include <vector>
#include <string>

// Each NVS namespace is single-purpose so wiping one doesn't disturb others.
static Preferences s_ui;       // namespace "dash-ui"
static Preferences s_hosts;    // namespace "dash-hosts"
static Preferences s_aliases;  // namespace "dash-alias"
static Preferences s_touch;    // namespace "dash-touch"

// Bump this whenever the meaning of stored prefs changes in a way that
// existing values would be invalid (e.g., we change the panel config in
// ui.cpp and the old rotation value would produce garbled output).
// On boot, mismatched schema triggers a one-time reset to safe defaults.
static constexpr uint8_t PREFS_SCHEMA = 6;

static BrightnessMode s_mode  = BRIGHTNESS_AUTO;
static uint8_t        s_rot   = 4;            // CYD landscape (panel swap + offset_y=80 in ui.cpp)
static uint8_t        s_cycle = CYCLE_SECONDS_DEFAULT;  // auto-cycle dwell, seconds
static TempUnitPref   s_units = TEMP_UNIT_AUTO;         // temperature display unit
static std::vector<std::string> s_manual_hosts;

static void load_manual_hosts() {
  s_manual_hosts.clear();
  s_hosts.begin("dash-hosts", true);
  uint8_t n = s_hosts.getUChar("count", 0);
  for (uint8_t i = 0; i < n; i++) {
    char key[8];
    snprintf(key, sizeof(key), "h%u", i);
    String h = s_hosts.getString(key, "");
    if (h.length()) s_manual_hosts.push_back(h.c_str());
  }
  s_hosts.end();
}

static void save_manual_hosts() {
  s_hosts.begin("dash-hosts", false);
  s_hosts.clear();
  s_hosts.putUChar("count", (uint8_t)s_manual_hosts.size());
  for (size_t i = 0; i < s_manual_hosts.size(); i++) {
    char key[8];
    snprintf(key, sizeof(key), "h%u", (unsigned)i);
    s_hosts.putString(key, s_manual_hosts[i].c_str());
  }
  s_hosts.end();
}

void prefs_load() {
  s_ui.begin("dash-ui", true);
  uint8_t schema = s_ui.getUChar("schema", 0);
  s_ui.end();

  if (schema != PREFS_SCHEMA) {
    // First boot of this build (or a schema-incompatible upgrade).
    // Reset to safe defaults and write the new schema number.
    s_mode  = BRIGHTNESS_AUTO;
    s_rot   = 4;
    s_cycle = CYCLE_SECONDS_DEFAULT;
    s_units = TEMP_UNIT_AUTO;
    s_ui.begin("dash-ui", false);
    s_ui.putUChar("mode",   (uint8_t)s_mode);
    s_ui.putUChar("rot",    s_rot);
    s_ui.putUChar("cycle",  s_cycle);
    s_ui.putUChar("units",  (uint8_t)s_units);
    s_ui.putUChar("schema", PREFS_SCHEMA);
    s_ui.end();
  } else {
    s_ui.begin("dash-ui", true);
    s_mode = (BrightnessMode)s_ui.getUChar("mode", BRIGHTNESS_AUTO);
    s_rot  = s_ui.getUChar("rot", 4);
    // "cycle" is an additive key (v0.1.10). Units flashed before it
    // existed simply read the default here — no schema bump needed.
    s_cycle = s_ui.getUChar("cycle", CYCLE_SECONDS_DEFAULT);
    // "units" is an additive key (v0.1.13) — same story, default AUTO.
    s_units = (TempUnitPref)s_ui.getUChar("units", TEMP_UNIT_AUTO);
    s_ui.end();
  }
  load_manual_hosts();
}

void prefs_save() {
  s_ui.begin("dash-ui", false);
  s_ui.putUChar("mode",  (uint8_t)s_mode);
  s_ui.putUChar("rot",   s_rot);
  s_ui.putUChar("cycle", s_cycle);
  s_ui.putUChar("units", (uint8_t)s_units);
  s_ui.end();
}

BrightnessMode prefs_brightness_mode()           { return s_mode; }
void           prefs_set_brightness_mode(BrightnessMode m) { s_mode = m; prefs_save(); }
uint8_t        prefs_rotation()                  { return s_rot;  }
void           prefs_set_rotation(uint8_t r)     { s_rot = r;  prefs_save(); }

uint8_t prefs_cycle_seconds() { return s_cycle; }
void    prefs_set_cycle_seconds(uint8_t s) {
  if (s > CYCLE_SECONDS_MAX) s = CYCLE_SECONDS_MAX;
  s_cycle = s;
  prefs_save();
}

TempUnitPref prefs_temp_unit() { return s_units; }
void         prefs_set_temp_unit(TempUnitPref u) {
  if (u > TEMP_UNIT_FAHRENHEIT) u = TEMP_UNIT_AUTO;
  s_units = u;
  prefs_save();
}

uint8_t prefs_manual_host_count() { return (uint8_t)s_manual_hosts.size(); }
const char* prefs_manual_host(uint8_t i) {
  if (i >= s_manual_hosts.size()) return "";
  return s_manual_hosts[i].c_str();
}
bool prefs_add_manual_host(const char* hostname) {
  for (auto& h : s_manual_hosts) if (h == hostname) return false;
  s_manual_hosts.emplace_back(hostname);
  save_manual_hosts();
  return true;
}
bool prefs_remove_manual_host(const char* hostname) {
  for (auto it = s_manual_hosts.begin(); it != s_manual_hosts.end(); ++it) {
    if (*it == hostname) { s_manual_hosts.erase(it); save_manual_hosts(); return true; }
  }
  return false;
}

const char* prefs_alias_for(const char* hostname) {
  static String s_buf;  // returned pointer is valid until the next call
  s_aliases.begin("dash-alias", true);
  s_buf = s_aliases.getString(hostname, "");
  s_aliases.end();
  return s_buf.c_str();
}
void prefs_set_alias(const char* hostname, const char* alias) {
  s_aliases.begin("dash-alias", false);
  if (alias && *alias) s_aliases.putString(hostname, alias);
  else                 s_aliases.remove(hostname);
  s_aliases.end();
}

bool prefs_load_touch_cal(TouchCal& out) {
  s_touch.begin("dash-touch", true);
  bool ok = s_touch.isKey("xmin");
  if (ok) {
    out.x_min = s_touch.getShort("xmin", 300);
    out.x_max = s_touch.getShort("xmax", 3900);
    out.y_min = s_touch.getShort("ymin", 300);
    out.y_max = s_touch.getShort("ymax", 3900);
  }
  s_touch.end();
  return ok;
}
void prefs_save_touch_cal(const TouchCal& cal) {
  s_touch.begin("dash-touch", false);
  s_touch.putShort("xmin", cal.x_min);
  s_touch.putShort("xmax", cal.x_max);
  s_touch.putShort("ymin", cal.y_min);
  s_touch.putShort("ymax", cal.y_max);
  s_touch.end();
}
