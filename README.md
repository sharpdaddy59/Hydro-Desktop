# hydro-dash

Desktop dashboard firmware for the Sunton ESP32-2432S028R (CYD). Polls
multiple [cores3-hydro](https://github.com/sharpdaddy59/cores3-hydro)
devices on the LAN over their JSON HTTP API and renders a glanceable
2×2 tile grid of current readings.

The dashboard is **stateless and never alerts** — same stance as the
hydro firmware itself. Interpretation is the upstream agent's job; this
just shows what's true right now.

## Features

- 2×2 tile grid: one device per tile, water/air/humidity/light at a glance
- Tap a tile for a detail view (uptime, RSSI, FW version, battery, IP)
- Long-press the grid for settings (brightness, rotation, WiFi reset, re-scan)
- **mDNS discovery** for `cores3-hydro-*.local` services on the LAN
- **Manual host fallback** for routers with flaky mDNS — host list is
  persisted in NVS and merged with discovery results
- **WiFiManager AP onboarding** — no QR code, no app; join the
  `hydro-dash-setup` network from a phone, captive portal does the rest
- **LDR-driven backlight auto-dim** — won't burn in on a desk
- Small management API on port 80 (`/devices`, `/status`, `/wifi/reset`)

## Hardware

- **Sunton ESP32-2432S028R** (the "Cheap Yellow Display"): ESP32-WROOM-32,
  2.8" 320×240 ILI9341, XPT2046 resistive touch, on-board LDR, RGB LED,
  microSD, two USB ports.
- That's it. No additional sensors needed — readings come from the
  cores3-hydro devices over the LAN.

> **Other CYD revisions:** the `S028C` (capacitive) and various clone
> variants have different pin maps. Update [`config.h`](config.h) to
> match yours; pin numbers are in one place.

## Quick start

PowerShell on Windows:

```powershell
git clone https://github.com/<your-user>/hydro-dash.git
cd hydro-dash

# One-time setup: installs arduino-cli, esp32:esp32 core, and libraries.
.\setup.ps1

# Plug in the CYD over USB, then:
.\build.ps1 -Upload -Monitor
```

On first boot the device opens a `hydro-dash-setup` WiFi network. Join
it from a phone, the captive portal opens, enter your LAN credentials,
the device reboots into client mode and starts browsing for hydros.

## HTTP API

The dashboard exposes a small management API on port 80. LAN-trusted, no auth.

| Endpoint | Methods | Description |
|----------|---------|-------------|
| `/` | GET | Status page |
| `/devices` | GET, POST | List known devices / add a manual host |
| `/status` | GET | FW version, uptime, heap, device count |
| `/wifi/reset` | POST | Wipe stored credentials and reboot |
| `/rebrowse` | POST | Force an mDNS rebrowse |

## Project layout

```
hydro-dash/
├── hydro-dash.ino          Sketch entry, FreeRTOS task spawn
├── config.h                Pins, intervals, FW_VERSION, max devices
├── state.{cpp,h}           DeviceRecord struct, atomic globals, mutex
├── prefs.{cpp,h}           NVS: brightness, rotation, manual hosts, aliases
├── backlight.{cpp,h}       LDR-driven PWM auto-dim
├── ui.{cpp,h}              LovyanGFX panel config + screen state machine
├── ui_grid.{cpp,h}         2×2 tile dashboard
├── ui_detail.{cpp,h}       Per-device drilldown
├── ui_settings.{cpp,h}     Settings screen (stub)
├── touch.{cpp,h}           XPT2046 tap/long-press dispatch
├── wifi_setup.{cpp,h}      WiFiManager AP-mode onboarding
├── discovery.{cpp,h}       mDNS browse for cores3-hydro-*
├── poller.{cpp,h}          HTTP polling task
├── http_server.{cpp,h}     Management API
├── build.ps1, setup.ps1    arduino-cli wrappers
├── docs/hydro-dash-spec.md Full design + screen flow
└── CLAUDE.md               Notes for Claude Code agents
```

## Status

v0.1.0 — initial scaffold. Compiles, boots, brings up the panel.
First-boot touch calibration and the settings screen are still stubs;
see the open-work list at the bottom of [`docs/hydro-dash-spec.md`](docs/hydro-dash-spec.md).

## License

MIT — see [`LICENSE`](LICENSE).
