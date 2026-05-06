// ui_detail.h — drilldown view for a single device.
//
// Shows everything in the grid tile plus uptime, RSSI, FW version,
// battery, IP, and per-sensor sim flags. Back button returns to grid.

#pragma once

#include <Arduino.h>

void ui_detail_set_device(int8_t device_idx);
void ui_detail_draw();
void ui_detail_handle_touch(int16_t x, int16_t y);
