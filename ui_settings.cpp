#include "ui_settings.h"
#include "ui.h"
#include "state.h"
#include "config.h"
#include "prefs.h"
#include "wifi_setup.h"
#include "discovery.h"

// Simplified UX while we debug the rotation/touch alignment:
//   any tap         → cycle rotation 0..7 (saved to NVS)
//   long-press      → return to hero (handled in ui.cpp::ui_handle_long_press)
//
// The cycle prints the new rotation big in the middle of the screen so
// it's legible regardless of which orientation the panel ends up in.

void ui_settings_draw() {
  auto& g = ui_gfx();
  g.fillScreen(TFT_BLACK);   // settings screen has no strip; clear all
  g.setTextColor(TFT_WHITE, TFT_BLACK);
  g.setTextSize(2);
  g.setCursor(8, FY(4, 16));
  g.print("Settings");

  g.setTextSize(3);
  g.setTextColor(TFT_CYAN, TFT_BLACK);
  g.setCursor(8, FY(60, 24));
  g.printf("Rotation: %u", prefs_rotation());

  g.setTextSize(1);
  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(8, FY(110, 8));
  g.print("Tap anywhere to cycle rotation.");
  g.setCursor(8, FY(124, 8));
  g.print("Long-press to return to dashboard.");
}

void ui_settings_handle_touch(int16_t /*x*/, int16_t /*y*/) {
  // With LovyanGFX's pre-built CYD config, all 8 rotations produce
  // clean output — the user picks whichever physical orientation they
  // prefer. 0/2 are portrait, 1/3 landscape, 4..7 mirrored variants.
  uint8_t next = (prefs_rotation() + 1) & 0x07;
  prefs_set_rotation(next);
  ui_gfx().setRotation(next);
  ui_gfx().fillScreen(TFT_BLACK);
  state_bump_version();
}
