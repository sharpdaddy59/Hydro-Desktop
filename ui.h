// ui.h — hero-view render loop + LovyanGFX owner.
//
// The UI never does I/O. It reads from g_devices atomics and renders.
// The dashboard is display-only — there is no on-device input.

#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

void ui_begin();
void ui_loop();   // call every frame from loop()

// Set/clear a transient status line shown over the hero view.
// Used during boot ("Connecting to WiFi...") and on errors.
void ui_set_status(const char* msg);

// The hero view draws into the canvas via this accessor. The concrete
// LGFX subclass (panel-specific config) lives in ui.cpp; consumers see
// the LGFX_Device base which carries all the drawing primitives.
lgfx::LGFX_Device& ui_gfx();

// FY: passthrough. Earlier panel-config experiments suggested the CYD
// rotation-4 canvas Y axis was inverted from the user's view, so we
// added this hook to flip Y for layout. After locking in the panel-swap
// + offset_y=80 config, it turns out the Y axis on a fresh boot reads
// top-down naturally, so FY is now an identity. Left in place so we can
// re-introduce a flip without touching every draw call if the panel
// config ever changes again.
inline int FY(int user_y, int /*element_h*/ = 1) {
  return user_y;
}
