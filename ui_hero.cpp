#include "ui_hero.h"
#include "ui.h"
#include "ui_detail.h"
#include "state.h"
#include "config.h"
#include "prefs.h"
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

static uint32_t status_color(const DeviceRecord& d) {
  if (!d.has_data.load() || d.stale.load())                      return 0x4208;       // dim gray
  if (d.sim_air.load() || d.sim_water.load() || d.sim_light.load()) return TFT_YELLOW;
  return TFT_GREEN;
}

static void advance_focus() {
  uint8_t n = g_device_count.load();
  if (n == 0) return;
  s_focused = (s_focused + 1) % n;
}

static void draw_strip() {
  auto& g = ui_gfx();
  int W = g.width();
  uint8_t n = g_device_count.load();

  // Clear the strip (covers stale "PAUSED" / changing dot positions).
  // All Y values via FY() since the canvas Y axis is inverted.
  g.fillRect(0, FY(0, STRIP_H), W, STRIP_H, TFT_BLACK);
  g.drawFastHLine(0, FY(STRIP_H - 1), W, 0x4208);

  if (n == 0) {
    g.setTextSize(1);
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.setCursor(8, FY(10, 8));      // size-1 font ~ 8px tall
    g.print("waiting for devices...");
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

  if (s_paused) {
    g.setTextSize(1);
    g.setTextColor(TFT_YELLOW, TFT_BLACK);
    g.setCursor(W - 56, FY(4, 8));
    g.print("PAUSED");
  }
}

static void draw_hero() {
  auto& g = ui_gfx();
  int W = g.width();
  int H = g.height();
  uint8_t n = g_device_count.load();

  // Clear the hero region (logical y=STRIP_H..H, height H-STRIP_H).
  g.fillRect(0, FY(STRIP_H, H - STRIP_H), W, H - STRIP_H, TFT_BLACK);

  if (n == 0) return;
  if (s_focused >= n) s_focused = 0;
  const DeviceRecord& d = g_devices[s_focused];

  // Name (alias falls back to hostname). Size 3 to match the readings.
  // setTextWrap(false) keeps long names on one line (truncated at the
  // right edge) rather than wrapping into the readings region below.
  g.setTextColor(TFT_WHITE, TFT_BLACK);
  g.setTextSize(3);
  g.setTextWrap(false);
  const int NAME_H = 24;
  g.setCursor(8, FY(HERO_NAME_Y, NAME_H));
  const char* alias = prefs_alias_for(d.hostname);
  g.print(*alias ? alias : d.hostname);
  g.setTextWrap(true);

  // Readings — text size 3. Label left-aligned at x=8; value right-
  // aligned to the panel's right edge.
  g.setTextSize(3);
  const int LINE_H = 24;
  uint32_t fg = d.stale.load() ? 0x7BEF : TFT_WHITE;
  g.setTextColor(fg, TFT_BLACK);

  auto reading = [&](int line, const char* label, float v, const char* unit) {
    int y = HERO_LINE0_Y + line * HERO_LINE_DY;
    g.setCursor(8, FY(y, LINE_H));
    g.print(label);

    char buf[16];
    if (isnan(v))                     snprintf(buf, sizeof(buf), "--");
    else if (strcmp(unit, "%") == 0)  snprintf(buf, sizeof(buf), "%.0f%s", v, unit);
    else                              snprintf(buf, sizeof(buf), "%.1f%s", v, unit);
    int tw = g.textWidth(buf);
    g.setCursor(W - RIGHT_PAD - tw, FY(y, LINE_H));
    g.print(buf);
  };
  reading(0, "Water", d.water_temp.load(), "C");
  reading(1, "Air",   d.air_temp.load(),   "C");
  reading(2, "Humid", d.humidity.load(),   "%");
  reading(3, "Light", d.light.load(),      "");

  // Footer: device count, RSSI, sim flags. Logical y=H-12 puts it 12 px
  // from the user's bottom edge.
  g.setTextSize(1);
  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(8, FY(H - 12, 8));
  g.printf("%u/%u  RSSI %d  fails:%u",
           (unsigned)(s_focused + 1), (unsigned)n,
           d.rssi.load(), (unsigned)d.consecutive_fails.load());
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
  draw_strip();
  draw_hero();
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
