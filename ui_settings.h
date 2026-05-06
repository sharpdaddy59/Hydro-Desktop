// ui_settings.h — settings screen.
//
// Toggles: brightness mode (auto / full / dim), screen rotation.
// Buttons: reset WiFi credentials, force mDNS rebrowse, recalibrate touch.
// Long-press on grid -> opens settings.

#pragma once

#include <Arduino.h>

void ui_settings_draw();
void ui_settings_handle_touch(int16_t x, int16_t y);
