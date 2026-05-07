#include "ui_settings.h"
#include "ui.h"
#include "state.h"
#include "config.h"
#include "prefs.h"
#include "wifi_setup.h"
#include "discovery.h"

// Settings UX:
//   any tap   → cycle brightness mode (auto → full → dim → auto)
//   long-press → return to hero
//
// Auto mode honors the on-board LDR via backlight.cpp. On units where
// the LDR isn't reading (some CYD revisions seem to leave the part
// unpopulated or wired differently), Auto behaves like Dim — switch
// to Full as a workaround.

static const char* mode_label(BrightnessMode m) {
  switch (m) {
    case BRIGHTNESS_AUTO: return "Auto";
    case BRIGHTNESS_FULL: return "Full";
    case BRIGHTNESS_DIM:  return "Dim";
  }
  return "?";
}

static const char* mode_hint(BrightnessMode m) {
  switch (m) {
    case BRIGHTNESS_AUTO: return "follows ambient light (LDR)";
    case BRIGHTNESS_FULL: return "always 100%";
    case BRIGHTNESS_DIM:  return "always minimum";
  }
  return "";
}

void ui_settings_draw() {
  auto& g = ui_gfx();
  g.fillScreen(TFT_BLACK);

  g.setTextColor(TFT_WHITE, TFT_BLACK);
  g.setTextSize(2);
  g.setCursor(8, FY(4, 16));
  g.print("Settings");

  g.setTextSize(3);
  g.setTextColor(TFT_CYAN, TFT_BLACK);
  g.setCursor(8, FY(56, 24));
  g.printf("Brightness: %s", mode_label(prefs_brightness_mode()));

  g.setTextSize(2);
  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(8, FY(108, 16));
  g.print(mode_hint(prefs_brightness_mode()));

  g.setTextSize(1);
  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(8, FY(200, 8));
  g.print("Tap anywhere to cycle brightness.");
  g.setCursor(8, FY(214, 8));
  g.print("Long-press to return to dashboard.");
}

void ui_settings_handle_touch(int16_t /*x*/, int16_t /*y*/) {
  BrightnessMode cur = prefs_brightness_mode();
  BrightnessMode next;
  switch (cur) {
    case BRIGHTNESS_AUTO: next = BRIGHTNESS_FULL; break;
    case BRIGHTNESS_FULL: next = BRIGHTNESS_DIM;  break;
    case BRIGHTNESS_DIM:  next = BRIGHTNESS_AUTO; break;
    default:              next = BRIGHTNESS_AUTO; break;
  }
  prefs_set_brightness_mode(next);
  state_bump_version();
}
