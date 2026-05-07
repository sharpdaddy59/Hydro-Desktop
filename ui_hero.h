// ui_hero.h — single-device "hero" view with status strip on top.
//
// Top 30 px: one colored dot per known device (green=fresh, yellow=sim,
// gray=stale/no-data); the focused device's dot has a white highlight ring.
// Tap a dot to focus + pause cycling.
//
// Below: large readings for the focused device.
// Tap the hero area to toggle pause; long-press opens Settings (handled
// upstream in ui.cpp).

#pragma once

#include <Arduino.h>

void ui_hero_draw();
void ui_hero_handle_touch(int16_t x, int16_t y);

// Tick the auto-cycle timer. Called from ui_loop on every iteration so
// the cycle advances even when the renderer is short-circuited by the
// dirty-version check. Bumps g_state_version when the focus changes.
void ui_hero_tick();
