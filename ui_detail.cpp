#include "ui_detail.h"
#include "ui.h"
#include "state.h"
#include "config.h"
#include <cmath>

static int8_t s_idx = -1;

void ui_detail_set_device(int8_t device_idx) { s_idx = device_idx; }

void ui_detail_draw() {
  auto& g = ui_gfx();
  if (s_idx < 0 || s_idx >= (int8_t)g_device_count.load()) {
    ui_set_screen(UI_SCREEN_HERO);
    return;
  }
  const DeviceRecord& d = g_devices[s_idx];

  g.fillScreen(TFT_BLACK);
  g.setTextColor(TFT_WHITE, TFT_BLACK);
  g.setTextSize(2);
  g.setCursor(8, FY(4, 16));
  g.print(d.hostname);

  g.setTextSize(1);
  int y = 32;
  auto line = [&](const char* fmt, auto v) {
    g.setCursor(8, FY(y, 8)); g.printf(fmt, v); y += 14;
  };
  const char* tsuf = temp_unit_suffix((TempUnit)d.temp_unit.load());
  g.setCursor(8, FY(y, 8));
  g.printf("Water temp: %.2f %s", d.water_temp.load(), tsuf); y += 14;
  g.setCursor(8, FY(y, 8));
  g.printf("Air temp:   %.2f %s", d.air_temp.load(), tsuf);   y += 14;
  line("Humidity:   %.0f %%",  d.humidity.load());
  line("Light:      %.0f",     d.light.load());
  line("RSSI:       %d dBm",   d.rssi.load());
  line("Battery:    %d %%",    d.battery_pct.load());
  line("Uptime:     %u s",     d.uptime_s.load());
  line("Misses:     %u",       (unsigned)d.consecutive_fails.load());
  g.setCursor(8, FY(y, 8));
  g.printf("Sensors:    A:%s W:%s L:%s",
           sensor_mode_label((SensorMode)d.mode_air.load()),
           sensor_mode_label((SensorMode)d.mode_water.load()),
           sensor_mode_label((SensorMode)d.mode_light.load()));
  y += 14;

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(8, FY(g.height() - 16, 8));
  g.print("Tap anywhere to return");
}

void ui_detail_handle_touch(int16_t /*x*/, int16_t /*y*/) {
  ui_set_screen(UI_SCREEN_HERO);
}
