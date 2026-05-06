// wifi_setup.h — WiFiManager AP-mode onboarding.
//
// No camera on this board, so the QR-code path used by cores3-hydro
// doesn't apply. Standard captive portal: if no credentials are saved,
// the device opens AP_SSID; the user joins it from a phone, the portal
// auto-launches, they enter their LAN credentials, the device saves
// them and reboots into client mode.
//
// Settings -> "Reset WiFi" wipes creds and reboots into AP mode.

#pragma once

void wifi_setup_begin();
void wifi_setup_reset_and_reboot();
bool wifi_setup_in_ap_mode();
