# CLAUDE.md — hydro-dash project notes for Claude Code

Desktop dashboard firmware for the Sunton ESP32-2432S028R (CYD). Polls
multiple cores3-hydro devices on the LAN over their HTTP API and renders
a glanceable 2×2 tile grid. Stateless — never alerts, never decides,
mirroring the hydro firmware's stance.

**Authoritative design doc:** `docs/hydro-dash-spec.md`. Read it for any
non-trivial work. **Sister project:** `../cores3-hydro` — this dashboard
is its consumer; conventions are deliberately mirrored.

## Build / flash / monitor

PowerShell, from the project root:

```powershell
.\build.ps1                  # compile only
.\build.ps1 -Upload          # compile + auto-detect port + flash
.\build.ps1 -Upload -Monitor # ... + serial @ 115200
.\build.ps1 -Strict          # warnings=all
```

One-time setup: `.\setup.ps1` installs arduino-cli, the mainstream
`esp32:esp32` core, and required libraries (LovyanGFX, WiFiManager,
ArduinoJson).

**Why mainstream esp32:esp32 not M5Stack's fork:** the CYD is a plain
WROOM-32 board with no OPI PSRAM. The PSRAM-init fragility that pushed
cores3-hydro to M5Stack's fork doesn't apply here, and the mainstream
core has better long-term library compatibility.

## Hardware map

See `docs/hydro-dash-spec.md` for the full pinout table. Critical pins
duplicated here for fast lookup:

| Subsystem | Pin / detail |
|-----------|--------------|
| ILI9341 TFT (HSPI) | MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, BL 21 |
| XPT2046 touch (VSPI) | MOSI 32, MISO 39, SCLK 25, CS 33, IRQ 36 |
| LDR (auto-dim) | GPIO 34 |
| RGB LED (active LOW) | R 4, G 16, B 17 |

Reference: https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/

## Architecture pointers

- **Boot order** (`hydro-dash.ino`): `state_init` → `prefs_load` →
  `backlight_begin` → `ui_begin` → `touch_begin` → `wifi_setup_begin`
  (WiFiManager) → `discovery_begin` → `poller_begin` →
  `http_server_begin`.
- **Concurrency:** producers (`poller`, `discovery`) write atomics;
  consumer (`ui`) reads without locking. `g_devices_mutex` only guards
  inserts into the device array.
- **HTTP server:** synchronous `WebServer` polled from `loop()` via
  `http_server_loop()`. Same convention as cores3-hydro.
- **NVS** is split into per-feature namespaces: `dash-ui` (brightness,
  rotation), `dash-hosts` (manual host list), `dash-alias` (per-device
  display names), `dash-touch` (XPT2046 calibration). WiFiManager owns
  its own keys separately.
- **mDNS:** `MDNS.queryService("http", "tcp")` periodically; results
  filtered by hostname prefix `cores3-hydro-`. Resolved IPs are cached
  on the DeviceRecord so the poller can skip mDNS resolution per
  request.

## Critical gotchas

1. **CYD-S028R LovyanGFX panel config is fiddly and non-obvious.** The
   working combination, found empirically, is in `ui.cpp::LGFX_CYD`:
   `panel_width=320, panel_height=240` (swapped from chip-native 240×320),
   `offset_y=80`, and `setRotation(4)`. Don't switch to `LGFX_AUTODETECT`
   — its runtime probe gives a white screen on this board. Don't change
   the swap or offset without re-running `cyd-rotation-test` (the
   sibling diagnostic sketch I deleted; recreate as needed). The schema
   bump in `prefs.cpp::PREFS_SCHEMA` is what forces the right rotation
   on existing units after a config change.
2. **GPIO 21 is shared.** Backlight and the P3 expansion header both
   use it. If you ever wire something to P3 pin 4, the panel goes dark.
3. **VSPI is shared with the SD slot.** The current scaffold doesn't use
   SD, so no contention. If SD is ever added, the LovyanGFX touch driver
   needs `bus_shared = true` and explicit lock management.
4. **GPIO 35 is input-only.** Don't try to drive it as an output.
5. **Touch calibration ships with placeholder values** in `ui.cpp`. A
   fresh unit will be visibly off-axis on first press until the
   recalibration flow lands. Numbers stored via
   `prefs_save_touch_cal`; settings-screen handler is a stub.
6. **WiFiManager blocks** in `wifi_setup_begin()` until creds are
   submitted or `AP_TIMEOUT_S` expires (default 180 s). On timeout we
   reboot — there's nothing useful for an offline dashboard to do.
7. **`FY()` in `ui.h` is currently an identity passthrough** — it was
   added to flip Y when an earlier panel config inverted the canvas Y
   axis. The locked-in config doesn't need flipping, but the hook is
   left in place so a future panel-config change can reinstate the
   flip without touching every draw call.
8. **GitHub Actions Node 20 deprecation warning** is the same as in
   cores3-hydro — non-blocking, will resolve when the action authors
   ship Node 24 majors.

## Conventions for new work

- **New screen:** add `ui_<name>.{cpp,h}` mirroring `ui_hero` /
  `ui_settings`. Wire in via `ui.cpp`'s `ui_loop` switch and
  `ui_set_screen`.
- **New polled field from cores3-hydro:** add an atomic to
  `DeviceRecord` in `state.h`, parse it in `poller.cpp::poll_sensors`
  or `poll_status`, render it where appropriate. **Don't break the
  contract direction** — the hydro firmware's `/sensors` shape is the
  source of truth; if a field is missing here, it's missing on the
  device side, not the other way round.
- **New NVS-persisted state:** mirror existing namespaces in `prefs.cpp`.
  Separate `Preferences` namespace, load in `prefs_load`, save inline
  on change.
- **User-facing changes:** bump `FW_VERSION` in `config.h`, add a note
  to the changelog at the top of `docs/hydro-dash-spec.md`.
- **Spec doc is the source of truth** for design decisions. If behavior
  diverges, update the spec.

## Don'ts

- Don't add alerting. The dashboard is stateless by design — same
  stance as cores3-hydro. The upstream agent does interpretation.
- Don't hardcode WiFi credentials. WiFiManager AP-mode onboarding is
  the one true path.
- Don't `Wire1.end()` style stunts here — there's no sibling I²C bus
  to worry about, but resist the temptation to manually init SPI buses
  that LovyanGFX is already managing.
- Don't add per-device authentication assumptions to the management
  HTTP API; the dashboard is LAN-trusted by design.
- Don't bump `FW_VERSION` without updating the spec changelog.
- Don't drag in an OS abstraction layer. ArduinoJson + HTTPClient +
  LovyanGFX + WiFiManager is the surface area; keep it small.

## Recent state

- **v0.1.9 (current):** Reclaim flash via partition swap. v0.1.8
  was at 99% of the default partition's 1.31 MB app slot. Build now
  passes `--board-options PartitionScheme=min_spiffs` to
  `arduino-cli` in `build.ps1` and the release workflow, promoting
  the app to 1.9 MB (~600 KB headroom for the planned device-side
  config UI). OTA layout preserved. NVS offset is identical across
  standard schemes, so saved credentials survive the swap. Also
  deleted `ui_detail.cpp/.h` and the `UI_SCREEN_DETAIL` enumerator
  — unreachable since the v0.1.1 hero-view rewrite. **The build's
  partition scheme is now part of the build contract** — when
  flashing manually with `arduino-cli upload`, pass
  `--board-options PartitionScheme=min_spiffs` or `build.ps1` will
  fail to find the output binary.
- **v0.1.8:** In-place hero redraws. v0.1.7's bump-cull
  stopped no-op redraws, but legitimate value changes (RSSI drift,
  sub-decimal sensor noise) still triggered full clear-then-paint
  → still visibly flashed. `draw_strip` and `draw_hero` now take a
  `full_redraw` flag; full `fillRect(...TFT_BLACK)` only happens on
  page transitions (focus / pause / device-count change). In-place
  refreshes redraw text over prior glyphs via
  `setTextColor(fg, bg)` and use a narrow per-row right-band clear
  to handle right-aligned values that may shrink. Hostname and
  device name skip redraw on in-place refreshes (invariant).
- **v0.1.7:** Cull spurious hero redraws. `poll_sensors`
  previously called `state_bump_version()` unconditionally on every
  successful poll, so a steady-state 2-device LAN flashed every
  ~7.5 s purely from poll cadence — independent of the 6 s
  auto-cycle. The poller now compares each incoming field to the
  current atomic via `set_if_changed` / `set_float_if_changed`
  helpers (NaN-aware, so disabled-upstream sensors stay quiet) and
  bumps only when something visible moved. `has_data` and `stale`
  edge transitions are folded in via `atomic::exchange`. The hero's
  own `fillRect`-then-redraw pattern wasn't touched — once spurious
  bumps are gone, the only redraws are legitimate page transitions
  (auto-cycle / focus change / stale-trip).
- **v0.1.6:** Track cores3-hydro v0.7.0's `/sensors` +
  `/status` wire format. Boolean `simulated` object → string-valued
  `status` object (`"real"` | `"simulated"` | `"disabled"`); readings
  may now be JSON null (mapped to NaN). `state.h` gains `SensorMode`
  (REAL/SIMULATED/OFF, mirroring upstream) and `TempUnit`; the old
  `sim_air/water/light` booleans on `DeviceRecord` were replaced by
  `mode_air/water/light` (atomic uint8). Temperature unit is now
  per-device — hero/detail screens render the C or F suffix from the
  upstream device's current setting via `temp_unit_suffix()`. `OFF`
  text replaces `--` on the hero screen for explicitly-disabled
  sensors so users can tell "I turned this off" from "no data yet".
  `/status` field rename `uptime_s` → `uptime` is accepted in addition
  to the old name during the upgrade window. The dashboard's own
  `/devices` JSON now emits the same shape (per-device `status`
  object, `temperature_units`, null for missing readings).
- **v0.1.5:** Auto-dim polarity fix. The CYD's LDR is wired
  with R10 (1MΩ) pulling GPIO 34 up to 3V3 and the LDR pulling it down
  to GND — so bright light = low raw ADC, dark = high. `backlight.cpp`
  previously had the comparison backwards, which meant the dimmer was
  brightening dark rooms and dimming lit ones. Polarity flipped,
  thresholds rebased to the measured range (`BL_LDR_BRIGHT=150`,
  `BL_LDR_DARK=550` at ADC_6db), attenuation pinned to 6 dB (0 dB
  saturates near-bright; 11 dB falls into the ADC dead-zone). Added
  `docs/cyd-ldr-test/` standalone diagnostic for re-calibrating on a
  different board.
- **v0.1.4:** Per-reading hero-view colour tinting and two
  underlying LovyanGFX colour fixes. Hero readings are now green
  (fresh+real), yellow (sim), or dim grey (stale), matching the strip
  dot per row. Two bugs that surfaced during this work: setTextColor
  with `uint32_t` dispatches to the RGB888 overload and reinterprets
  the bytes (TFT_GREEN came out red — fix is to use `uint16_t`);
  CYD's LCD is BGR-wired and needs `rgb_order = true` so MADCTL.BGR
  is set (otherwise R/B swap and TFT_YELLOW renders as cyan).
  `build.ps1` auto-detect now falls back to a single unrecognised
  serial port — handles CH340-based CYDs whose USB IDs aren't in
  arduino-cli's board database.
- **v0.1.3:** settings screen cycles brightness mode
  (auto / full / dim) instead of rotation. The rotation cycle made
  the settings screen unreadable in transit and only rotation 4 is
  ever useful on this CYD anyway, so it's pinned via `PREFS_SCHEMA`.
  Backlight modes were already implemented in `backlight.cpp`; this
  release just adds the UI to switch between them.
- **v0.1.2:** per-MAC unique hostname so multiple CYDs are
  distinguishable on the same LAN. `device_id.cpp` / `.h` computes
  `hydro-dash-<last4mac>` once at boot; used for `MDNS.begin`, the
  WiFiManager AP SSID (now `<hostname>-setup`), and shown in the hero
  footer. Hero "Humid" label expanded to "Humidity". README rewritten
  to describe the actual hero view (the original scaffold's 2×2 grid
  description was stale).
- **v0.1.1:** working dashboard end-to-end. Hero-view layout
  (one device at a time with auto-cycle and a status-dot strip on top)
  replaced the original 2×2 grid because text-size-1 in 160×120 tiles
  was unreadable from desk distance. Discovery dropped the
  `cores3-hydro-` hostname-prefix filter and now probes any LAN HTTP
  service for a hydro-shaped `/sensors` response, since the user can
  rename units. Render path throttled to 500 ms and skipped entirely
  unless `g_state_version` advanced — kills the idle flicker. **Panel
  config locked to the empirically-determined values** (see gotcha
  #1 above): `panel_width=320, panel_height=240, offset_y=80, rotation 4`,
  `PREFS_SCHEMA=6`. Settings screen is now any-tap-cycles-rotation,
  long-press-returns; touch tap-on-release bug fixed in `touch.cpp`.
- **v0.1.0:** initial scaffold. Boot orchestration, LovyanGFX panel
  config for the CYD (ILI9341 HSPI + XPT2046 VSPI + PWM backlight),
  stub UI screens, mDNS discovery + per-device HTTP polling tasks,
  WiFiManager AP onboarding, NVS prefs, management HTTP API, GitHub
  Actions release workflow. Open work tracked at the bottom of
  `docs/hydro-dash-spec.md`.

## Where to look first

- `hydro-dash.ino` — boot orchestration, FreeRTOS task spawn
- `docs/hydro-dash-spec.md` — full design doc + screen flow + open work
- `config.h` — central tunables (intervals, pins, version)
- `state.h` / `state.cpp` — DeviceRecord, atomics, mutex
- `enclosure/` — parametric OpenSCAD case (snap-fit, no fasteners)
- `ui.cpp` — LovyanGFX panel config (pin numbers come from `config.h`)
- `poller.cpp` — HTTP polling task; the `/sensors` contract lives here
- `discovery.cpp` — mDNS browse + manual-host seeding
