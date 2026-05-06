#include "ui_grid.h"
#include "ui.h"
#include "ui_detail.h"
#include "state.h"
#include "config.h"
#include "prefs.h"
#include <cmath>

// 2x2 grid in 320x240 landscape -> each tile is 160x120.
static constexpr int TILE_W = TFT_W / 2;
static constexpr int TILE_H = TFT_H / 2;

static int8_t tile_at(int16_t x, int16_t y) {
  if (x < 0 || y < 0 || x >= TFT_W || y >= TFT_H) return -1;
  int col = x / TILE_W;
  int row = y / TILE_H;
  return (int8_t)(row * 2 + col);
}

static uint32_t tile_color(const DeviceRecord& d) {
  if (!d.has_data.load() || d.stale.load())                    return 0x4208;        // dark gray
  if (d.sim_air.load() || d.sim_water.load() || d.sim_light.load()) return TFT_YELLOW;
  return TFT_DARKGREEN;
}

static void draw_tile(uint8_t idx, const DeviceRecord& d, bool present) {
  int x = (idx % 2) * TILE_W;
  int y = (idx / 2) * TILE_H;
  auto& g = ui_gfx();

  g.fillRect(x + 1, y + 1, TILE_W - 2, TILE_H - 2, TFT_BLACK);
  g.drawRect(x, y, TILE_W, TILE_H, TFT_DARKGREY);

  if (!present) {
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.setTextSize(1);
    g.setCursor(x + 8, y + TILE_H / 2 - 4);
    g.print("(no device)");
    return;
  }

  // Status dot + label
  g.fillCircle(x + TILE_W - 12, y + 12, 4, tile_color(d));
  g.setTextColor(TFT_WHITE, TFT_BLACK);
  g.setTextSize(2);
  const char* alias = prefs_alias_for(d.hostname);
  g.setCursor(x + 6, y + 6);
  g.print(*alias ? alias : d.hostname);

  // Readings
  g.setTextSize(1);
  g.setCursor(x + 6, y + 32);
  float w = d.water_temp.load();
  float a = d.air_temp.load();
  float h = d.humidity.load();
  float l = d.light.load();
  if (!isnan(w)) g.printf("Water %.1fC\n",  w); else g.print("Water  --\n");
  g.setCursor(x + 6, y + 48);
  if (!isnan(a)) g.printf("Air   %.1fC\n",  a); else g.print("Air    --\n");
  g.setCursor(x + 6, y + 64);
  if (!isnan(h)) g.printf("Humid %.0f%%\n", h); else g.print("Humid  --\n");
  g.setCursor(x + 6, y + 80);
  if (!isnan(l)) g.printf("Light %.0f\n",   l); else g.print("Light  --\n");
}

void ui_grid_draw() {
  uint8_t n = g_device_count.load();
  for (uint8_t i = 0; i < 4; i++) {
    bool present = i < n;
    draw_tile(i, present ? g_devices[i] : g_devices[0], present);
  }
}

void ui_grid_handle_touch(int16_t x, int16_t y) {
  int8_t t = tile_at(x, y);
  if (t < 0 || t >= (int8_t)g_device_count.load()) return;
  ui_detail_set_device(t);
  ui_set_screen(UI_SCREEN_DETAIL);
}
