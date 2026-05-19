#include "ui_hero.h"
#include "ui.h"
#include "ui_detail.h"
#include "state.h"
#include "config.h"
#include "prefs.h"
#include "device_id.h"
#include <cmath>

// Layout. Name and readings both render at text size 3 (~24 px tall);
// stride accounts for the larger name baseline. Horizontal positions are
// computed against ui_gfx().width() at draw time so the layout adapts
// automatically to whatever rotation the user picked (some rotations
// produce a 320-wide framebuffer, some 240-wide).
static constexpr int STRIP_H        = 30;
static constexpr int STRIP_DOT_Y    = 14;
static constexpr int HERO_TOP       = STRIP_H + 4;
static constexpr int HERO_NAME_Y    = HERO_TOP + 2;
static constexpr int HERO_LINE0_Y   = HERO_TOP + 38;    // gap below name
static constexpr int HERO_LINE_DY   = 28;
static constexpr int RIGHT_PAD      = 8;

// Cycle behavior
static constexpr uint32_t CYCLE_INTERVAL_MS = 6000;

static int8_t   s_focused        = 0;
static bool     s_paused         = false;
static uint32_t s_last_cycle_ms  = 0;

// Snapshot of the screen-state inputs that determine whether the last
// rendered frame is "the same page" or "a new page." Initialised to
// sentinels that force a full first-frame draw. Updated by
// ui_hero_draw() AFTER drawing — until then `full_redraw` compares the
// live values against these. See the comment block in ui_hero_draw()
// for why we redraw in place between page transitions instead of
// re-clearing the whole region every time.
static int8_t s_last_drawn_focused = -1;
static int8_t s_last_drawn_paused  = -1;
static int8_t s_last_drawn_count   = -1;

// uint16_t (not uint32_t) so LovyanGFX's color path treats values as
// RGB565. With uint32_t the converter dispatches to its RGB888
// overload and the bytes get reinterpreted (TFT_GREEN ends up red).
static uint16_t status_color(const DeviceRecord& d) {
  if (!d.has_data.load() || d.stale.load()) return 0x4208;       // dim gray
  // Any sensor in SIMULATED → yellow. OFF (disabled) is treated as a
  // healthy state — the user explicitly turned the sensor off, so it
  // shouldn't drag the whole device down to yellow.
  SensorMode a = (SensorMode)d.mode_air.load();
  SensorMode w = (SensorMode)d.mode_water.load();
  SensorMode l = (SensorMode)d.mode_light.load();
  if (a == SensorMode::SIMULATED || w == SensorMode::SIMULATED ||
      l == SensorMode::SIMULATED) return TFT_YELLOW;
  return TFT_GREEN;
}

static void advance_focus() {
  uint8_t n = g_device_count.load();
  if (n == 0) return;
  s_focused = (s_focused + 1) % n;
}

static void draw_strip(bool full_redraw) {
  auto& g = ui_gfx();
  int W = g.width();
  uint8_t n = g_device_count.load();

  // Full clear only on a page transition (focus/pause/count change).
  // On in-place updates the dots overwrite their prior pixels via
  // fillCircle; the PAUSED text can't have changed (pause is part of
  // the full_redraw trigger) so it remains correct on the framebuffer.
  // All Y values via FY() since the canvas Y axis is inverted.
  if (full_redraw) {
    g.fillRect(0, FY(0, STRIP_H), W, STRIP_H, TFT_BLACK);
    g.drawFastHLine(0, FY(STRIP_H - 1), W, 0x4208);
  }

  if (n == 0) {
    if (full_redraw) {
      g.setTextSize(1);
      g.setTextColor(TFT_DARKGREY, TFT_BLACK);
      g.setCursor(8, FY(10, 8));      // size-1 font ~ 8px tall
      g.print("waiting for devices...");
    }
    return;
  }

  // Evenly-space N dots horizontally. Radius shrinks for larger N.
  int radius  = (n <= 4) ? 10 : 8;
  int spacing = W / (n + 1);
  for (uint8_t i = 0; i < n; i++) {
    int cx = spacing * (i + 1);
    int cy = FY(STRIP_DOT_Y);
    g.fillCircle(cx, cy, radius, status_color(g_devices[i]));
    if (i == s_focused) g.drawCircle(cx, cy, radius + 2, TFT_WHITE);
  }

  if (full_redraw && s_paused) {
    g.setTextSize(1);
    g.setTextColor(TFT_YELLOW, TFT_BLACK);
    g.setCursor(W - 56, FY(4, 8));
    g.print("PAUSED");
  }
}

static void draw_hero(bool full_redraw) {
  auto& g = ui_gfx();
  int W = g.width();
  int H = g.height();
  uint8_t n = g_device_count.load();

  // Full clear only on a page transition (focus / pause / count
  // change). In-place updates redraw each element over its prior
  // pixels via setTextColor(fg, bg) — the bg fills each glyph's
  // bounding box, overwriting the prior glyph without a black flash.
  // Variable-width right-aligned values (e.g. "100.5C" → "9.5C") get
  // a narrow per-row right-band clear inline below.
  if (full_redraw) {
    g.fillRect(0, FY(STRIP_H, H - STRIP_H), W, H - STRIP_H, TFT_BLACK);
  }

  if (n == 0) return;
  if (s_focused >= n) s_focused = 0;
  const DeviceRecord& d = g_devices[s_focused];

  // Name (alias falls back to hostname). Only drawn on full_redraw —
  // outside a page transition, the name is invariant for the focused
  // device, so it's already on the framebuffer from the prior frame.
  // setTextWrap(false) keeps long names on one line (truncated at the
  // right edge) rather than wrapping into the readings region below.
  if (full_redraw) {
    g.setTextColor(TFT_WHITE, TFT_BLACK);
    g.setTextSize(3);
    g.setTextWrap(false);
    const int NAME_H = 24;
    g.setCursor(8, FY(HERO_NAME_Y, NAME_H));
    const char* alias = prefs_alias_for(d.hostname);
    g.print(*alias ? alias : d.hostname);
    g.setTextWrap(true);
  }

  // Readings — text size 3. Label left-aligned at x=8; value right-
  // aligned to the panel's right edge. Each row's color reflects its
  // own state (matches the strip dot's color semantics):
  //   green  = fresh, real data
  //   yellow = fresh, but this sensor is in sim mode upstream
  //   gray   = device is stale (overrides per-sensor sim state)
  g.setTextSize(3);
  const int LINE_H        = 24;
  // Wide enough for any plausible value at size 3 — "888.8 F" is ~96 px
  // and we want headroom; 140 px also covers the few-pixel kerning
  // wobble between digits.
  const int VALUE_BAND_W  = 140;
  bool stale = d.stale.load();
  const char* tsuf = temp_unit_suffix((TempUnit)d.temp_unit.load());

  auto reading = [&](int line, const char* label, float v, const char* unit,
                     SensorMode mode) {
    int y = HERO_LINE0_Y + line * HERO_LINE_DY;
    bool disabled  = (mode == SensorMode::OFF);
    bool simulated = (mode == SensorMode::SIMULATED);
    // uint16_t (not uint32_t!) so LovyanGFX treats the value as
    // RGB565. With uint32_t it dispatches to the RGB888 overload and
    // the bytes get reinterpreted, which is why TFT_GREEN was coming
    // out red.
    uint16_t color = (stale || disabled) ? 0x7BEF
                   : simulated           ? TFT_YELLOW
                   :                       TFT_GREEN;

    // In-place redraws: clear the right-aligned value band so a value
    // that shrunk in width doesn't leave leading character pixels
    // behind. On full_redraw the whole region is already black.
    if (!full_redraw) {
      g.fillRect(W - RIGHT_PAD - VALUE_BAND_W, FY(y, LINE_H),
                 VALUE_BAND_W, LINE_H, (uint16_t)TFT_BLACK);
    }

    g.setTextColor(color, (uint16_t)TFT_BLACK);
    g.setCursor(8, FY(y, LINE_H));
    g.print(label);

    char buf[16];
    if (disabled)                     snprintf(buf, sizeof(buf), "OFF");
    else if (isnan(v))                snprintf(buf, sizeof(buf), "--");
    else if (strcmp(unit, "%") == 0)  snprintf(buf, sizeof(buf), "%.0f%s", v, unit);
    else                              snprintf(buf, sizeof(buf), "%.1f%s", v, unit);
    int tw = g.textWidth(buf);
    g.setCursor(W - RIGHT_PAD - tw, FY(y, LINE_H));
    g.print(buf);
  };
  // Air and Humidity share mode_air — same DHT20 sensor on the hydro side.
  SensorMode m_air   = (SensorMode)d.mode_air.load();
  SensorMode m_water = (SensorMode)d.mode_water.load();
  SensorMode m_light = (SensorMode)d.mode_light.load();
  reading(0, "Water",    d.water_temp.load(), tsuf, m_water);
  reading(1, "Air",      d.air_temp.load(),   tsuf, m_air);
  reading(2, "Humidity", d.humidity.load(),   "%",  m_air);
  reading(3, "Light",    d.light.load(),      "",   m_light);

  // Footer: status details up top, hostname on its own line at the
  // bottom (size 2 so it's legible from desk distance) — multiple CYDs
  // are distinguishable at a glance from across the room.
  const int FOOTER_LINE_H = 16;
  int hostname_y = H - FOOTER_LINE_H - 4;
  int status_y   = hostname_y - FOOTER_LINE_H - 2;

  // Status line ("1/2   RSSI -50") width varies with RSSI digits and
  // the X/N device index — clear the row band before redrawing on
  // in-place updates.
  if (!full_redraw) {
    g.fillRect(0, FY(status_y, FOOTER_LINE_H), W, FOOTER_LINE_H,
               (uint16_t)TFT_BLACK);
  }
  g.setTextSize(2);
  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(8, FY(status_y, FOOTER_LINE_H));
  g.printf("%u/%u   RSSI %d", (unsigned)(s_focused + 1), (unsigned)n,
           d.rssi.load());

  // Hostname is this dashboard's own per-MAC name — invariant after
  // boot. Only redraw on full_redraw (when the clear erased it).
  if (full_redraw) {
    g.setTextSize(2);
    g.setTextColor(TFT_WHITE, TFT_BLACK);
    g.setCursor(8, FY(hostname_y, FOOTER_LINE_H));
    g.print(device_hostname());
  }
}

void ui_hero_tick() {
  if (s_paused) return;
  if (g_device_count.load() < 2) return;  // nothing to cycle to
  if (millis() - s_last_cycle_ms <= CYCLE_INTERVAL_MS) return;
  advance_focus();
  s_last_cycle_ms = millis();
  state_bump_version();
}

void ui_hero_draw() {
  // A "page transition" is any change in which device is shown, the
  // pause state (the PAUSED text appears/disappears), or the device
  // count (dot layout / "waiting" text). Anything else — RSSI drift,
  // sensor decimal-place changes, sim-mode toggles upstream — is an
  // in-place refresh that should be visually silent.
  int8_t now_count = (int8_t)g_device_count.load();
  bool full_redraw = (s_focused          != s_last_drawn_focused) ||
                     ((int8_t)s_paused   != s_last_drawn_paused)  ||
                     (now_count          != s_last_drawn_count);

  draw_strip(full_redraw);
  draw_hero(full_redraw);

  s_last_drawn_focused = s_focused;
  s_last_drawn_paused  = (int8_t)s_paused;
  s_last_drawn_count   = now_count;
}

void ui_hero_handle_touch(int16_t x, int16_t y) {
  // Touch coords come back in canvas Y, which now matches our logical Y
  // (FY is an identity passthrough). Hit-region checks below work as-is.
  uint8_t n = g_device_count.load();

  if (y < STRIP_H && n > 0) {
    // Map x to a dot index. Tolerate sloppy taps — anything closer to
    // dot i than to its neighbors counts as i.
    int W = ui_gfx().width();
    int spacing = W / (n + 1);
    int idx = (x + spacing / 2) / spacing - 1;
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    s_focused = (int8_t)idx;
    s_paused  = true;
    s_last_cycle_ms = millis();
    state_bump_version();
    return;
  }

  // Hero area: toggle pause. Reset the cycle timer so an unpause doesn't
  // immediately advance off the device the user wanted to read.
  s_paused = !s_paused;
  s_last_cycle_ms = millis();
  state_bump_version();
}
