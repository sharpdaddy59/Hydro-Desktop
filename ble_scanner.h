// ble_scanner.h — passive BLE advertisement scan for Govee H5075
// temperature/humidity sensors.
//
// Scan-only: no pairing, no GATT connect. The H5075 broadcasts its
// temperature and humidity in the manufacturer-specific data of its BLE
// advertisement; this module listens, decodes, and surfaces each sensor
// as a DeviceKind::BLE entry in g_devices — rendered by the hero screen
// exactly like a polled cores3-hydro device (Water/Light show OFF).
//
// One FreeRTOS task, pinned to core 0 alongside discovery, with a low
// scan duty cycle so WiFi is not starved (the WROOM-32 shares one radio).

#pragma once

// Brings up NimBLE (scan-only) and spawns the scan task. Call after
// wifi_setup_begin() so BLE init does not contend with WiFiManager's
// AP-mode radio use during onboarding.
void ble_scanner_begin();
