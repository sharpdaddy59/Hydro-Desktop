#include "touch.h"
#include "ui.h"
#include "config.h"
#include <Arduino.h>

// Wrapper around LovyanGFX's built-in XPT2046 driver. The instance lives
// on the LGFX object configured in ui.cpp.
//
// Tap dispatch: a press, then release, with travel < ~12 px = tap.
// Long press: held > 600 ms without significant travel.

static bool     s_was_pressed   = false;
static int16_t  s_press_x       = 0;
static int16_t  s_press_y       = 0;
static uint32_t s_press_started = 0;
static bool     s_long_fired    = false;

void touch_begin() {
  // LovyanGFX's panel.init() (called from ui_begin) brings up the touch
  // controller as well. Nothing extra needed here yet — placeholder for
  // future calibration handoff from prefs_load_touch_cal().
}

bool touch_is_pressed() {
  uint16_t x, y;
  return ui_gfx().getTouch(&x, &y);
}

bool touch_read_raw(int16_t& x, int16_t& y) {
  uint16_t rx, ry;
  bool ok = ui_gfx().getTouch(&rx, &ry);
  x = (int16_t)rx;
  y = (int16_t)ry;
  return ok;
}

void touch_loop() {
  int16_t x, y;
  bool pressed = touch_read_raw(x, y);

  if (pressed && !s_was_pressed) {
    s_press_x = x;
    s_press_y = y;
    s_press_started = millis();
    s_long_fired = false;
  } else if (pressed && s_was_pressed) {
    if (!s_long_fired && millis() - s_press_started > 600 &&
        abs(x - s_press_x) < 12 && abs(y - s_press_y) < 12) {
      ui_handle_long_press(x, y);
      s_long_fired = true;
    }
  } else if (!pressed && s_was_pressed) {
    if (!s_long_fired &&
        abs(x - s_press_x) < 12 && abs(y - s_press_y) < 12) {
      ui_handle_touch(s_press_x, s_press_y);
    }
  }
  s_was_pressed = pressed;
}
