// ui_hero.h — single-device "hero" view with status strip on top.
//
// Top 30 px: one colored dot per known device (green=fresh, yellow=sim,
// gray=stale/no-data); the focused device's dot has a white highlight ring.
//
// Below: large readings for the focused device. The view auto-cycles
// through devices on a timer (prefs_cycle_seconds; 0 holds on device 0).

#pragma once

#include <Arduino.h>

void ui_hero_draw();

// Tick the auto-cycle timer. Called from ui_loop on every iteration so
// the cycle advances even when the renderer is short-circuited by the
// dirty-version check. Bumps g_state_version when the focus changes.
void ui_hero_tick();
