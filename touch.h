// touch.h — XPT2046 resistive touch driver wrapper.
//
// Owns the secondary VSPI bus. Reads the touch chip on IRQ-low (or
// polled every ~20 ms), debounces, applies the calibration matrix from
// prefs, and dispatches tap / long-press events to ui.cpp.
//
// First-boot flow: if no calibration is stored, ui pushes a 4-corner
// calibration screen before reaching the grid.

#pragma once

#include <Arduino.h>

void touch_begin();
void touch_loop();

// Returns true if a touch is currently active. Useful for the
// calibration screen's "press and hold" gesture.
bool touch_is_pressed();
bool touch_read_raw(int16_t& x, int16_t& y);
