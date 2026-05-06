// ui_grid.h — main 2x2 tile dashboard.
//
// One tile per device: alias (or hostname), water/air/humidity/light
// readings, status dot (green = fresh, gray = stale, yellow = sim).
// Tap a tile -> push UI_SCREEN_DETAIL with that device index.

#pragma once

#include <Arduino.h>

void ui_grid_draw();
void ui_grid_handle_touch(int16_t x, int16_t y);
