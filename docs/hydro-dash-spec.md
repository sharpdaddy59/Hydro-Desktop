# hydro-dash — design spec

Desktop dashboard for monitoring multiple cores3-hydro devices on a LAN.
Polls each unit's `/sensors` and `/status` endpoints, renders a glanceable
2×2 tile grid on a Sunton ESP32-2432S028R (CYD).

This spec is the source of truth for design decisions. If behavior diverges
from the spec, update the spec.

## Changelog

- **v0.1.9:** Reclaim flash for future feature growth. v0.1.8 ran at
  99% of the default partition's 1.31 MB app slot, with no room for
  the planned device-side configuration UI. The build now passes
  `--board-options PartitionScheme=min_spiffs` to `arduino-cli` in
  both `build.ps1` and the GitHub Actions release workflow,
  promoting the app partition to 1.9 MB (~600 KB headroom). OTA
  support is retained even though no OTA code is in use today — the
  scheme keeps both `app0` and `app1` partitions so a future OTA
  path is possible without another partition migration. NVS lives
  at `0x9000` in every standard arduino-esp32 scheme, so saved WiFi
  credentials, brightness mode, aliases, and manual hosts survive
  the swap without a factory reset. Also deleted `ui_detail.cpp/.h`
  and the `UI_SCREEN_DETAIL` enumerator — the per-device drilldown
  screen has been unreachable since the v0.1.1 hero-view rewrite
  (no code path set it as the active screen).
- **v0.1.8:** In-place hero redraws. After v0.1.7 culled spurious
  version bumps, the dashboard still flashed on legitimate value
  changes — RSSI drift ±1-2 dBm per poll, sub-decimal sensor
  movement — because every redraw began with a 320×210 fillRect to
  black before repainting. `draw_strip` and `draw_hero` now take a
  `full_redraw` flag computed in `ui_hero_draw` from focus / pause /
  device-count change. On a page transition (focused device
  advances, user taps to pause, a device appears or disappears) the
  full clear-and-repaint still happens — that's the page-change
  flash the user accepts. On any other version bump the renderer
  redraws each element in place: `setTextColor(fg, bg)` overwrites
  prior glyphs cleanly, and a narrow per-row right-band clear
  handles right-aligned values that may shrink in width
  (e.g. `100.5C` → `9.5C`). The hostname and device name skip
  redraw entirely on in-place refreshes since they're invariant for
  the focused device. Net effect: live values update silently
  between cycle transitions, matching the architectural model of "a
  display task reading atomic state, redrawing the same page until
  it's time to advance."
- **v0.1.7:** Cull spurious hero redraws. `poll_sensors` previously
  bumped `g_state_version` unconditionally on every successful poll,
  so a stable 2-device LAN flashed every ~7.5 s (POLL_INTERVAL_MS /
  N_devices) between the legitimate 6 s auto-cycle transitions —
  visible because `draw_hero` clears the 320×210 hero region to
  black before repainting, with no off-screen sprite. The poller now
  compares each incoming field to the current atomic value via small
  `set_*_if_changed` helpers and bumps the UI version only if
  anything visible moved. NaN→NaN is treated as no-change so a
  disabled-upstream sensor (value is null every poll) doesn't
  oscillate. The `has_data` and `stale` transitions are folded into
  the same `changed` flag via `atomic::exchange`. Bookkeeping fields
  that don't drive rendering (`last_ok_ms`, `consecutive_fails`)
  intentionally do not contribute. No change to the auto-cycle
  bump, the stale-trip bump, or the screen-change full clear —
  those are legitimate redraws and stay.
- **v0.1.6:** Track cores3-hydro v0.7.0's `/sensors` and `/status` wire
  format. The boolean `simulated` object is gone — replaced by a `status`
  object whose values are per-sensor strings: `"real"`, `"simulated"`,
  or `"disabled"`. Per-reading values may now be JSON null (stale-on-
  upstream or sensor explicitly disabled); the poller maps null to NaN
  so the hero/detail screens render `--` as before. New
  `temperature_units` field (`"celsius"` or `"fahrenheit"`) is tracked
  per-device and surfaced as the C/F suffix in both screens, so each
  cores3-hydro can be configured independently and the dashboard
  follows. `state.h` gains `SensorMode` (REAL/SIMULATED/OFF, mirroring
  upstream's enum) and `TempUnit`; `sim_air/water/light` booleans were
  replaced by `mode_air/water/light` (atomic uint8). Hero rendering now
  shows `OFF` for disabled sensors in dim grey rather than the
  ambiguous `--`. `/status` field rename `uptime_s` → `uptime` is
  accepted in addition to the old name so a half-upgraded LAN keeps
  working until every cores3-hydro is on v0.7.0. The dashboard's own
  management `/devices` JSON gains a per-device `status` object plus
  `temperature_units` and emits JSON null for missing readings, so
  downstream consumers see the same shape as cores3-hydro.
- **v0.1.5:** Auto-dim polarity fix. The CYD's LDR is wired with the
  pull-up high (R10 1MΩ to 3V3, LDR to GND, GPIO 34 between them), so
  bright light produces a LOW raw ADC value and dark produces a HIGH
  one — opposite to what `backlight.cpp` previously assumed. The old
  mapping was running the dimmer backwards: dimming the screen in lit
  rooms and brightening it in dark ones. Polarity flipped in
  `duty_for_ldr()`, thresholds rebased to the empirically-measured
  range (`BL_LDR_BRIGHT=150`, `BL_LDR_DARK=550` at 6 dB attenuation),
  and ADC attenuation pinned to 6 dB (0 dB compresses the operating
  range into half-scale; 11 dB pushes the bright end below the ADC's
  lower dead-zone and reads zero). Added a standalone diagnostic at
  `docs/cyd-ldr-test/` that probes GPIO 34 across all four
  attenuations with a back-to-back burst and rolling stats — useful
  for sanity-checking the LDR on a fresh CYD or recalibrating the
  thresholds for a board with different LDR characteristics.
- **v0.1.4:** Per-reading colour tinting on the hero view — green for
  fresh+real, yellow for sim, dim grey for stale (matches the strip
  dot's existing semantics, applied per row using each sensor's own
  sim flag). Air and Humidity share `sim_air` since the DHT20 reports
  both. Two LovyanGFX-specific fixes: switched colour variables from
  `uint32_t` to `uint16_t` so values dispatch through the RGB565 path
  instead of being reinterpreted as RGB888 bytes (TFT_GREEN was
  rendering as red); and set `rgb_order = true` on the panel so the
  CYD's BGR-wired LCD sees correct R/B channels (TFT_YELLOW was
  rendering as cyan/blue). `build.ps1` auto-detect now falls back to
  "the only unrecognised serial port" when arduino-cli's USB-ID
  database doesn't tag a device as ESP32 — useful for CH340-based
  CYDs that show up as Unknown.
- **v0.1.3:** Settings screen now cycles brightness mode
  (auto → full → dim → auto) instead of rotation. The rotation cycle
  was producing unreadable intermediate states that made navigation
  hard, and on this CYD only rotation 4 is usable anyway — pinned
  via PREFS_SCHEMA in `prefs.cpp`. The brightness modes were already
  honored by `backlight.cpp::backlight_loop`; this commit just wires
  up a UI to switch between them.
- **v0.1.2:** Per-MAC unique hostname (`hydro-dash-<last4mac>`) used for
  mDNS, the WiFiManager AP SSID, and shown in the hero footer so
  multiple CYDs are distinguishable on the same LAN. New `device_id.cpp`
  / `.h` owns the computation. Hero view's "Humid" label expanded to
  "Humidity". README rewritten to describe the actual hero view (no
  longer the 2×2 grid that was in the original scaffold).
- **v0.1.1:** Replaced 2×2 grid with single-device "hero" view +
  auto-cycle + status-dot strip (4 tiles at 160×120 with size-1 font
  was unreadable from desk distance). Dropped the `cores3-hydro-`
  hostname-prefix filter in discovery — now probes `/sensors` shape
  on any LAN HTTP service, so user-renamed hydros are found. Render
  path now version-gated (skips draw unless `g_state_version`
  advanced) — kills idle flicker. **Panel config locked to the
  empirically-determined CYD-S028R values:** `panel_width=320`,
  `panel_height=240`, `offset_y=80`, `setRotation(4)`. Settings
  screen reduced to any-tap-cycles-rotation, long-press-returns;
  touch tap-on-release bug fixed in `touch.cpp`.
- **v0.1.0:** initial scaffold — boot orchestration, LovyanGFX panel
  config for the CYD, stub UI screens, mDNS discovery + HTTP polling
  tasks, WiFiManager AP onboarding, NVS-backed prefs (manual hosts,
  brightness, rotation, touch calibration), management HTTP API.

## Hardware

For the enclosure (3D-printed, parametric OpenSCAD source), see
[`enclosure/`](../enclosure/). It's a snap-fit two-piece case with
PCB-retention posts, both USB cutouts, an integrated stylus channel,
removable kickstand, and a wall-mount keyhole.



| Subsystem | Pin / detail |
|-----------|--------------|
| ILI9341 TFT (HSPI) | MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, RST -1, BL 21 |
| XPT2046 touch (VSPI) | MOSI 32, MISO 39, SCLK 25, CS 33, IRQ 36 |
| LDR (auto-dim) | GPIO 34 (ADC1, input-only) |
| Speaker | GPIO 26 (reserved, unused) |
| RGB LED (active LOW) | R 4, G 16, B 17 |
| BOOT button | GPIO 0 |
| P3 expansion | GND, GPIO 35 (in only), GPIO 22, GPIO 21 (BL!) |
| CN1 expansion | GND, GPIO 22, GPIO 27, 3V3 |

**Conflicts to watch:**
- GPIO 21 is shared between backlight and the P3 expansion header. Don't
  use P3-21 if you need the panel lit.
- GPIO 35 is input-only — fine for sensors, never an output.
- VSPI is shared between the touch controller and (if ever wired) the
  microSD slot. The current scaffold doesn't use SD, so no contention.

## Boot order (`hydro-dash.ino`)

1. `Serial.begin(115200)`
2. `state_init()` — allocate the device-table mutex.
3. `prefs_load()` — pull NVS prefs (brightness, rotation, manual hosts).
4. `backlight_begin()` — PWM on TFT_BL, LDR pin mode.
5. `ui_begin()` — LovyanGFX init, fill black, set rotation from prefs.
6. `touch_begin()` — passes through to LovyanGFX's XPT2046 driver.
7. `wifi_setup_begin()` — `WiFiManager.autoConnect`; opens `hydro-dash-setup`
   AP if no creds; reboots after `AP_TIMEOUT_S` failure.
8. `discovery_begin()` — `MDNS.begin`, seed manual hosts from prefs,
   spawn the browse task.
9. `poller_begin()` — spawn the per-device polling task on core 1.
10. `http_server_begin()` — bind `/`, `/devices`, `/status`, etc.

`loop()` pumps `ui_loop`, `touch_loop`, `backlight_loop`, and
`http_server_loop` at ~10 ms cadence.

## Concurrency model

Same convention as cores3-hydro: producers write atomics, consumers read
without locking. The only mutex is `g_devices_mutex` which guards
inserts into `g_devices[]` (since hostname strings need to be copied
under a lock to prevent a partial-write race during a concurrent browse
+ manual-add).

Tasks:

| Task | Core | Stack | Purpose |
|------|------|-------|---------|
| loop (Arduino) | 1 | default | UI render, touch dispatch, backlight, HTTP server |
| `discovery` | 0 | 4 KB | mDNS browse every `DISCOVERY_INTERVAL_MS` |
| `poller` | 1 | 8 KB | round-robin per-device polling |

## Discovery

`discovery_begin()` calls `MDNS.queryService("http", "tcp")` periodically
and filters results whose hostname starts with `cores3-hydro-`. New
hostnames are inserted into `g_devices[]` via `state_insert()`. Resolved
IPs are cached on the `DeviceRecord` so the poller can fall back to IP
when mDNS resolution fails on a specific request.

Manual hosts (added via `POST /devices` or the settings screen) are
seeded from NVS at boot, so they survive reboots and are tried even when
mDNS is unhappy.

## Polling

The `poller` task round-robins through `g_devices[]`:

- `GET /sensors` every `POLL_INTERVAL_MS` (default 15 s)
- `GET /status` every `STATUS_INTERVAL_MS` (default 60 s)

Failure handling:
- A single failure increments `consecutive_fails`.
- After `STALE_AFTER_MISSES` (default 3), `stale` flips true and the
  tile renders gray. Last-known readings remain visible (dimmed, in a
  later UI iteration).
- On any successful `/sensors`, `consecutive_fails` resets and `stale`
  clears.

The dashboard never alerts. Mirroring the cores3-hydro firmware's stance
("stateless data source — never alerts, never decides"), interpretation
is the upstream agent's responsibility. The dashboard just shows what's
true right now.

## Screen flow

```
GRID --tap tile-->     DETAIL  --tap-->  GRID
GRID --long press-->   SETTINGS --tap--> GRID
```

- **GRID:** 2×2 tiles. Each: alias-or-hostname, water/air/humidity/light,
  status dot (green = fresh + all sensors REAL, gray = stale, yellow =
  any sensor in SIMULATED mode upstream). A sensor in the OFF/disabled
  mode is treated as a healthy state for the dot — the user explicitly
  turned it off — and the per-row reading renders `OFF` in dim grey.
- **DETAIL:** all readings + uptime, RSSI, battery, FW version, IP,
  failure count.
- **SETTINGS** (stub): brightness mode toggle, screen rotation, re-scan
  mDNS button, reset WiFi, recalibrate touch.

Tile geometry: 320×240 / 2 = 160×120 per tile in landscape rotation.

## HTTP API

The dashboard itself exposes a small management API on port 80. LAN-trusted,
no auth.

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/` | GET | HTML status page |
| `/devices` | GET | JSON snapshot of every known device |
| `/devices` | POST | Add a manual host (form: `hostname=...`) |
| `/status` | GET | FW version, uptime, heap, device count |
| `/wifi/reset` | POST | Wipe creds and reboot to AP mode |
| `/rebrowse` | POST | Force an mDNS rebrowse |

## NVS namespaces

Each is single-purpose so a wipe-one-thing user action doesn't hit unrelated state.

| Namespace | Holds |
|-----------|-------|
| `dash-ui` | brightness mode, rotation |
| `dash-hosts` | manually-added cores3-hydro hostnames |
| `dash-alias` | per-hostname display alias |
| `dash-touch` | XPT2046 calibration matrix |

WiFiManager owns its own NVS keys and is intentionally not surfaced here.

## Open work (post-scaffold)

- **First-boot touch calibration screen.** Right now `ui.cpp` ships with
  placeholder XPT2046 calibration; first-press behavior on a fresh unit
  will be visibly off-axis. Add a 4-corner press flow that writes via
  `prefs_save_touch_cal`.
- **Flesh out settings screen.** Currently stub rows.
- **Stale-but-visible rendering** — keep last-known readings on screen
  in dimmed text rather than blanking the tile.
- **Sparklines** — small history graphs in detail view (optional;
  needs a server-side aggregator if we want anything > a few minutes).
- **Screen rotation auto-detect** via the accelerometer? CYD doesn't
  have one — skip.
