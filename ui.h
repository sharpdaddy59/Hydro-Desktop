// ui.h — top-level screen state machine + LovyanGFX owner.
//
// The UI never does I/O. It reads from g_devices atomics and renders.
// Touch events from touch.cpp call ui_handle_touch().

#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

enum UiScreen : uint8_t {
  UI_SCREEN_GRID = 0,
  UI_SCREEN_DETAIL,
  UI_SCREEN_SETTINGS,
};

void ui_begin();
void ui_loop();   // call every frame from loop()

void ui_set_screen(UiScreen s);
UiScreen ui_current_screen();

// Set/clear a transient status line shown over the current screen.
// Used during boot ("Connecting to WiFi...") and on errors.
void ui_set_status(const char* msg);

// Forwarded by touch.cpp. Coordinates in display-rotated space.
void ui_handle_touch(int16_t x, int16_t y);
void ui_handle_long_press(int16_t x, int16_t y);

// Sub-screens draw into the same canvas via this accessor. The concrete
// LGFX subclass (panel-specific config) lives in ui.cpp; consumers see
// the LGFX_Device base which carries all the drawing primitives.
lgfx::LGFX_Device& ui_gfx();
