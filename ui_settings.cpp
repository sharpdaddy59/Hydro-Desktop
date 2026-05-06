#include "ui_settings.h"
#include "ui.h"
#include "config.h"
#include "prefs.h"
#include "wifi_setup.h"
#include "discovery.h"

// TODO: real layout. For now: a couple of placeholder rows + back tap.

void ui_settings_draw() {
  auto& g = ui_gfx();
  g.setTextColor(TFT_WHITE, TFT_BLACK);
  g.setTextSize(2);
  g.setCursor(8, 4);
  g.print("Settings");

  g.setTextSize(1);
  int y = 36;
  auto line = [&](const char* s) { g.setCursor(8, y); g.print(s); y += 16; };
  line("(stub) Brightness mode");
  line("(stub) Screen rotation");
  line("(stub) Re-scan mDNS");
  line("(stub) Reset WiFi");
  line("(stub) Recalibrate touch");

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(8, TFT_H - 16);
  g.print("Tap anywhere to return");
}

void ui_settings_handle_touch(int16_t /*x*/, int16_t /*y*/) {
  ui_set_screen(UI_SCREEN_GRID);
}
