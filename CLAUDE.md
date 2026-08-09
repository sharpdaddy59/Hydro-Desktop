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
| LDR (auto-dim) | GPIO 34 |
| RGB LED (active LOW) | R 4, G 16, B 17 |

Reference: https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/

## Architecture pointers

- **Boot order** (`hydro-dash.ino`): `state_init` → `prefs_load` →
  `backlight_begin` → `ui_begin` → `wifi_setup_begin` (WiFiManager) →
  `discovery_begin` → `poller_begin` → `ble_scanner_begin` →
  `http_server_begin`.
- **Concurrency:** producers (`poller`, `discovery`) write atomics;
  consumer (`ui`) reads without locking. `g_devices_mutex` only guards
  inserts into the device array.
- **HTTP server:** synchronous `WebServer` polled from `loop()` via
  `http_server_loop()`. Same convention as cores3-hydro.
- **NVS** is split into per-feature namespaces: `dash-ui` (brightness,
  rotation, cycle dwell, temperature unit), `dash-hosts` (manual host
  list), `dash-alias` (per-device display names). WiFiManager owns its
  own keys separately.
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
3. **VSPI belongs to the microSD card.** `sdlog.cpp` mounts the SD card
   on VSPI (its own GPIOs); the bus is uncontended since the touchscreen
   was removed in v0.1.14. SD access runs from `loop()` (`sdlog_loop`) —
   no task, no locking.
4. **GPIO 35 is input-only.** Don't try to drive it as an output.
5. **WiFiManager blocks** in `wifi_setup_begin()` until creds are
   submitted or `AP_TIMEOUT_S` expires (default 180 s). On timeout we
   reboot — there's nothing useful for an offline dashboard to do.
6. **`FY()` in `ui.h` is currently an identity passthrough** — it was
   added to flip Y when an earlier panel config inverted the canvas Y
   axis. The locked-in config doesn't need flipping, but the hook is
   left in place so a future panel-config change can reinstate the
   flip without touching every draw call.
7. **GitHub Actions Node runtime.** The release workflow's actions are
   pinned to their Node-24 majors (`checkout@v5`, `cache@v5`,
   `action-gh-release@v3`) — except `arduino/setup-arduino-cli@v2`,
   which has no Node-24 release yet. GitHub force-runs any remaining
   Node-20 action on the Node 24 runtime from 2026-06-02; bump
   `setup-arduino-cli` once Arduino ships a Node-24 version.

## Conventions for new work

- **New polled field from cores3-hydro:** add an atomic to
  `DeviceRecord` in `state.h`, parse it in `poller.cpp::poll_sensors`
  or `poll_status`, render it where appropriate. **Don't break the
  contract direction** — the hydro firmware's `/sensors` shape is the
  source of truth; if a field is missing here, it's missing on the
  device side, not the other way round.
- **New NVS-persisted state:** mirror existing namespaces in `prefs.cpp`.
  Separate `Preferences` namespace, load in `prefs_load`, save inline
  on change. An *additive* key in an existing namespace (a new getter
  with a sensible default) needs no `PREFS_SCHEMA` bump — only bump
  when the meaning of an existing value changes.
- **Web settings UI:** edit `web/index.html`, then regenerate
  `web_assets.h` via `tools/gen-web-assets.ps1`. The build embeds the
  committed header and never reads the HTML directly — an edit that
  isn't regenerated ships nothing. New settings reach the SPA through
  the `/config` JSON endpoints in `http_server.cpp`.
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
  LovyanGFX + WiFiManager + NimBLE-Arduino is the surface area; keep it
  small.

## Recent state

- **v0.1.18 (current):** Web-configurable SD-log interval. The
  compile-time `SD_LOG_INTERVAL_MS` became a `dash-ui` pref `logmin`
  (minutes, default `SD_LOG_INTERVAL_DEFAULT_MIN` = 5, additive key —
  no schema bump). `sdlog_loop()` re-reads the pref every pass so a
  change applies without a reboot. Set via `POST /config/loginterval`
  (form arg `minutes=N`, clamped [1, 1440]); `GET /config` reports
  `log_interval_min`; the SPA's Logging card gains a "Log interval"
  dropdown. Backported from govee-dash's v0.2.0 implementation.
- **v0.1.17:** SD-log tuning. Rotation switched from daily to
  **monthly** files (`/hydro-YYYY-MM.csv` — `current_log_path` formats
  `%Y-%m`); the log interval went 1 min → 5 min (`SD_LOG_INTERVAL_MS`);
  and `SD_SPI_FREQ_HZ` rose 4 → 20 MHz (the SD card has VSPI to itself
  now that touch is gone). A monthly file is ~3 MB and downloads in
  ~2–3 s. Three `config.h` constants and one `strftime` format — no
  code-path changes.
- **v0.1.16:** Daily SD-log rotation + browser log download.
  `sdlog.cpp` now writes one file per local day — `/hydro-YYYY-MM-DD.csv`
  (`current_log_path` computes it from the configured-timezone local
  date, so a file's day boundary matches its rows' timestamps).
  `/hydro-log.csv` remains the pre-NTP fallback. The boot-time file open
  moved into `sdlog_loop()` (the header is written per new day's file).
  New `GET /logs` (JSON file list) and `GET /logs/download?file=`
  (streams a CSV as a download) — the `file` param is gated by
  `sdlog_is_log_filename()`, a strict path-traversal whitelist applied
  before any `SD.open`. SD/FS access stays behind `sdlog.cpp`
  (`sdlog_list_files`, `sdlog_open_for_read`); `http_server.cpp` calls
  those, no `<SD.h>`. The settings SPA lists the files as download
  links. The download blocks the loop for the transfer (like OTA).
- **v0.1.15:** microSD CSV data logging. New module
  `sdlog.cpp` / `.h` appends one CSV row per device to `/hydro-log.csv`
  every `SD_LOG_INTERVAL_MS` (default 60 s). Optional — `sdlog_begin()`
  disables cleanly when no card is present, and `sdlog_loop()`
  self-disables if the card is pulled (the next append open fails).
  Driven from `loop()` — no task, no locking; `g_devices` atomics are
  read lock-free as the UI does. The SD card owns the VSPI bus outright
  (touch is gone). CSV columns: ISO-8601 `timestamp` (empty until NTP
  syncs), `uptime_s`, plus each device's readings (NaN → empty cell).
  The timestamp timezone is a `dash-ui` pref `tz` (additive POSIX TZ
  string — no schema bump) set from a zone dropdown in the SPA via
  `POST /config/timezone`; `sdlog` applies it with `configTzTime` /
  `setenv`, default UTC. `GET /config` gains read-only `sdlog` status +
  `timezone`, shown in the settings SPA. `SD` / `FS` / `SPI` ship with the esp32 core — no
  library change. Logging records, never interprets — consistent with
  the stateless stance.
- **v0.1.14:** Touchscreen removed. The XPT2046 touch panel
  and the on-device settings screen are gone — the dashboard is
  display-only, configured entirely from the web console. Touch was the
  flakiest subsystem (placeholder calibration, stub recalibration,
  off-axis on first press); the settings screen only cycled brightness,
  which the web console already does. Deleted `touch.cpp/.h` and
  `ui_settings.cpp/.h`; `ui.cpp`'s screen state machine collapsed to the
  single hero view (`UiScreen` enum, `ui_set_screen`, `ui_handle_touch`,
  `ui_handle_long_press` all gone); `ui_hero` lost its pause /
  focus-by-touch state and now always auto-cycles. Removed the
  `dash-touch` NVS namespace and `prefs_load/save_touch_cal` (no
  `PREFS_SCHEMA` bump — separate namespace; orphaned data is harmless).
  This frees the VSPI controller, which the microSD card needs.
- **v0.1.13:** Temperature unit preference + trimmed Govee
  tiles. New dashboard-wide temp display unit (Auto / °C / °F) — NVS
  pref `units` in `dash-ui` (additive key, no `PREFS_SCHEMA` bump),
  `prefs_temp_unit()`, set via `POST /config/units` and the settings
  SPA's Display section. `ui_hero.cpp` converts air/water temperatures
  at render time (`convert_temp` / `display_temp_unit`); `Auto` keeps
  the original mirror-the-device behavior. Conversion is display-only —
  doesn't touch the stateless-mirror stance. BLE (Govee) tiles now
  render a trimmed three-row layout (Air / Humidity / Battery, centered)
  instead of the HTTP four-row layout — the `reading()` lambda now takes
  an explicit `y` and `draw_hero` branches on `device_kind`. Battery %
  comes from the H5075 advertisement. The web device list still shows
  each device's *reported* unit (it's a raw-data view); the unit pref
  only affects the on-screen hero tiles. Also fixed a settings-SPA bug
  latent since v0.1.10 — the 5 s `/devices` poll rebuilt the device
  list and discarded in-progress display-name edits; `loadDevices()`
  now skips the rebuild while an alias `<input>` is focused.
- **v0.1.12:** Passive BLE sensor support — Govee H5075. New
  module `ble_scanner.cpp` / `.h` runs a low-duty-cycle passive BLE
  advertisement scan via NimBLE-Arduino (scan-only — no pairing, no GATT
  connect) and surfaces each Govee H5075 temp/humidity sensor as an
  ordinary `DeviceRecord` in the hero rotation, next to the HTTP-polled
  cores3-hydro units. This is a deliberate, user-approved widening of the
  dashboard's role — it now *sources* readings, not just consumes them.
  `DeviceRecord` gains a `device_kind` discriminator (HTTP/BLE) and a
  `mac` field; `state_insert_ble()` is the BLE insert path (existing
  `state_insert()` untouched, so discovery/http_server need no edits).
  The poller skips `DeviceKind::BLE` entries — they have no IP — and the
  BLE scan task owns their staleness (`age_ble_devices` grays a tile
  unheard for `BLE_STALE_AFTER_MS`). The `set_if_changed` /
  `set_float_if_changed` helpers moved from `poller.cpp` statics to
  `inline` in `state.h` since both producers now need them. The H5075
  decoder (`decode_h5075`) reads a 24-bit big-endian packed temp/humidity
  value + battery byte from the manufacturer-specific advertisement;
  **offsets and the `0xEC88` company ID need real-device verification** —
  a sanity gate drops an implausible decode. BLE sensors render Water and
  Light as `OFF`. Scan duty cycle is ~30% (`BLE_SCAN_WINDOW_MS` <
  `BLE_SCAN_INTERVAL_MS`) with a gap between windows so BLE listening
  doesn't starve WiFi on the WROOM-32's shared radio; the task is pinned
  to core 0 alongside discovery. `NimBLE-Arduino` is pinned to 2.5.0 in
  `setup.ps1` — the 2.x line is the one compatible with the arduino-esp32
  3.x core (IDF 5.x); 1.4.x compiles but aborts at boot in
  `esp_bt_controller_init()`. The BT controller is
  the feature's main RAM cost on the no-PSRAM CYD; measure free heap via
  `/status`. `/devices` JSON rows gained `kind` and `mac`. Append-only
  `g_devices` means a dead BLE sensor can't be removed without a reboot
  (tracked in the spec's open work).
- **v0.1.11:** Browser-driven firmware updates. New module
  `ota.cpp` / `ota.h` registers `POST /ota/upload` — a multipart
  firmware upload streamed straight to the `Update` library.
  `ota_register(s_server)` is called from `http_server_begin()`. The
  CYD has no PSRAM, so unlike cores3-hydro (which buffers the whole
  image in PSRAM, freeing camera framebuffers to make room) each
  chunk streams directly into the inactive OTA app partition;
  `Update` flips the boot pointer only on a verified `end(true)`, so
  an interrupted or invalid upload is a safe no-op. The settings SPA
  gains a Firmware section (file picker + progress bar, `XMLHttp`
  upload for progress events) and an API section linking the GET
  JSON endpoints. The SPA pauses its device-list poll during an
  upload — the synchronous `WebServer` serves one request at a time
  and `handleClient()` blocks for the whole transfer, which also
  freezes the on-screen UI (drawn from `loop()`) until the reboot.
- **v0.1.10:** Web settings console + configurable
  auto-cycle dwell. `/` now serves a gzip-compressed single-page
  settings app (embedded in PROGMEM via `web_assets.h`, generated
  from `web/index.html` by `tools/gen-web-assets.ps1`) instead of the
  old server-rendered status page. New write API: `GET /config`,
  `POST /config/{brightness,cycle,alias}`, `POST /devices/remove`.
  The hero auto-cycle interval — formerly the compile-time
  `CYCLE_INTERVAL_MS = 6000` in `ui_hero.cpp` — is now the `dash-ui`
  NVS pref `cycle` (`prefs_cycle_seconds`, 0–60 s, `0` = no cycling).
  Additive key, no `PREFS_SCHEMA` bump — existing units read the 6 s
  default. Scope held to the dashboard's own settings; no upstream
  cores3-hydro control. Removing a manual host only un-seeds it — the
  record persists in `g_devices` until reboot because the array is
  append-only (the UI task reads it lockless). Also fixed a latent
  `/devices` bug: per-device `alias` stored the shared
  `prefs_alias_for` static-buffer pointer, so every row reported the
  last device's alias.
- **v0.1.9:** Reclaim flash via partition swap. v0.1.8
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
- `ble_scanner.cpp` — passive BLE scan for Govee H5075 sensors;
  wire format documented in `docs/govee-ble.md`
- `http_server.cpp` / `web/index.html` — management HTTP API + settings SPA
- `ota.cpp` — browser-driven firmware update (`POST /ota/upload`)
- `sdlog.cpp` — microSD CSV data logging (monthly `/hydro-YYYY-MM.csv`)
