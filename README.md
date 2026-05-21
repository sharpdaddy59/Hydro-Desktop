# hydro-dash

Desktop dashboard firmware for the Sunton ESP32-2432S028R (CYD). Polls
multiple [cores3-hydro](https://github.com/sharpdaddy59/cores3-hydro)
devices on the LAN over their JSON HTTP API and renders a single
device at a time on a glanceable hero view, auto-cycling through them.

The dashboard is **stateless and never alerts** — same stance as the
hydro firmware itself. Interpretation is the upstream agent's job; this
just shows what's true right now.

## Features

- **Hero view** — one device at a time, big text-size-3 readings
  readable from desk distance: water / air / humidity / light for
  cores3-hydro units, air / humidity / battery for Govee BLE sensors.
  Cycles through devices on a timer (6 s each by default; adjustable
  3–60 s or disabled entirely from the web settings console).
  Temperatures display in °C, °F, or each device's own unit (Auto).
- **Status-dot strip** at the top, one colored dot per known device:
  green = fresh, yellow = sim mode, gray = stale or no data. Tap a dot
  to focus that device + pause cycling. Tap the hero pane to toggle
  pause.
- **Long-press** opens Settings — cycles backlight mode
  (Auto / Full / Dim) on each tap. Auto follows the on-board LDR;
  Full and Dim pin the backlight at max or min.
- **Per-MAC unique hostname** like `hydro-dash-a3f2.local` — multiple
  CYDs on the same LAN don't collide. The hostname is shown in the
  hero footer so you can identify which physical unit you're looking at.
- **Auto-discovery** — mDNS-browses every `_http._tcp` service on the
  LAN and probes `/sensors` for the cores3-hydro shape. Works with any
  hostname your hydros are using (the dashboard doesn't assume the
  default `cores3-hydro-*` prefix).
- **Manual host fallback** for routers with flaky mDNS — host list is
  persisted in NVS and merged with discovery results.
- **Govee BLE sensors** — passively scans for Govee H5075
  temperature/humidity sensors and shows each as its own tile next to
  the polled cores3-hydro units. No pairing or setup — it just listens
  for their BLE advertisements. See
  [`docs/govee-ble.md`](docs/govee-ble.md) for the wire format.
- **WiFiManager AP onboarding** — first boot opens a per-device AP
  named `<hostname>-setup`; join from a phone, captive portal does the
  rest.
- **LDR-driven backlight auto-dim** — won't burn in on a desk.
- **Web settings console** on port 80 — open `http://<hostname>.local/`
  from any browser on the LAN to rename devices, add/remove manual
  hosts, switch backlight mode, tune the auto-cycle dwell time, choose
  the temperature unit (°C / °F / Auto), and apply **firmware updates**
  (upload a `.bin`; the device verifies the image and reboots into it).
  A matching JSON API backs it — see the table below.

## Hardware

- **Sunton ESP32-2432S028R** (the "Cheap Yellow Display"): ESP32-WROOM-32,
  2.8" 320×240 ILI9341, XPT2046 resistive touch, on-board LDR, RGB LED,
  microSD, two USB ports.
- No extra wiring needed. cores3-hydro readings arrive over the LAN;
  Govee H5075 sensors in BLE range are picked up automatically. The
  on-board LDR (backlight auto-dim) is the only sensor on the board
  itself.

> **Other CYD revisions:** the `S028C` (capacitive) and various clone
> variants have different pin maps and may need different LovyanGFX
> panel-config values. Update [`config.h`](config.h) and the panel
> config in [`ui.cpp`](ui.cpp) to match yours. The current values
> (`panel_width=320, panel_height=240, offset_y=80, rotation 4`) were
> dialed in empirically against the user's S028R units; see
> [`docs/hydro-dash-spec.md`](docs/hydro-dash-spec.md) for the story.

## Quick start

PowerShell on Windows:

```powershell
git clone https://github.com/sharpdaddy59/Hydro-Desktop.git hydro-dash
cd hydro-dash

# One-time setup: installs arduino-cli, esp32:esp32 core, and libraries.
.\setup.ps1

# Plug in the CYD over USB, then:
.\build.ps1 -Upload -Monitor
```

On first boot the device opens a `hydro-dash-<last4mac>-setup` WiFi
network. Join it from a phone, the captive portal opens, enter your LAN
credentials, the device reboots into client mode and starts browsing for
hydros.

## HTTP API

The dashboard exposes a small management API on port 80. LAN-trusted, no auth.

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Settings single-page app |
| `/devices` | GET | JSON snapshot of every device (`kind` http/ble, `mac` for BLE) |
| `/devices` | POST | Add a manual host (`hostname=...`) |
| `/devices/remove` | POST | Drop a manual host from the persisted list (`hostname=...`) |
| `/status` | GET | FW version, uptime, free heap, device count |
| `/config` | GET | Brightness, auto-cycle dwell, temperature unit, manual hosts |
| `/config/brightness` | POST | Backlight mode (`mode=auto\|full\|dim`) |
| `/config/cycle` | POST | Auto-cycle dwell seconds (`seconds=N`, 0–60) |
| `/config/units` | POST | Temperature unit (`unit=auto\|celsius\|fahrenheit`) |
| `/config/alias` | POST | Set/clear a device display name (`hostname=...&alias=...`) |
| `/rebrowse` | POST | Force an mDNS rebrowse |
| `/wifi/reset` | POST | Wipe stored credentials and reboot to AP mode |
| `/ota/upload` | POST | Multipart firmware-image upload; verifies, then reboots |

## Project layout

```
hydro-dash/
├── hydro-dash.ino          Sketch entry, FreeRTOS task spawn
├── config.h                Pins, intervals, FW_VERSION, BLE tunables
├── state.{cpp,h}           DeviceRecord struct, atomic globals, mutex
├── device_id.{cpp,h}       Per-MAC unique hostname helper
├── prefs.{cpp,h}           NVS: brightness, rotation, cycle, units, hosts, aliases
├── backlight.{cpp,h}       LDR-driven PWM auto-dim
├── ui.{cpp,h}              LovyanGFX panel config + screen state machine
├── ui_hero.{cpp,h}         Hero view + status-dot strip
├── ui_settings.{cpp,h}     Settings screen (brightness mode cycle)
├── touch.{cpp,h}           XPT2046 tap/long-press dispatch
├── wifi_setup.{cpp,h}      WiFiManager AP-mode onboarding
├── discovery.{cpp,h}       mDNS browse + /sensors probe
├── poller.{cpp,h}          HTTP polling task
├── ble_scanner.{cpp,h}     Passive BLE scan for Govee H5075 sensors
├── http_server.{cpp,h}     Management API + settings SPA
├── ota.{cpp,h}             Browser-driven firmware update
├── web/index.html          Settings SPA source
├── web_assets.h            Gzipped SPA embedded in PROGMEM (generated)
├── tools/                  gen-web-assets.ps1 — regenerates web_assets.h
├── build.ps1, setup.ps1    arduino-cli wrappers
├── enclosure/              Parametric OpenSCAD snap-fit case + STLs
├── docs/hydro-dash-spec.md Full design + screen flow + changelog
├── docs/govee-ble.md       Govee H5075 BLE advertisement format
├── docs/cyd-ldr-test/      Standalone LDR diagnostic sketch
└── CLAUDE.md               Notes for Claude Code agents
```

## Status

Working end-to-end on the Sunton S028R. Current version is in
[`config.h`](config.h) (`FW_VERSION`); per-release notes and the
open-work list live in
[`docs/hydro-dash-spec.md`](docs/hydro-dash-spec.md).

## License

MIT — see [`LICENSE`](LICENSE).
