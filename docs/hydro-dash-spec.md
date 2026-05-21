# hydro-dash — design spec

Desktop dashboard for monitoring multiple cores3-hydro devices on a LAN.
Polls each unit's `/sensors` and `/status` endpoints, renders a glanceable
2×2 tile grid on a Sunton ESP32-2432S028R (CYD).

This spec is the source of truth for design decisions. If behavior diverges
from the spec, update the spec.

## Changelog

- **v0.1.17:** SD-log tuning — monthly files, 5-minute interval, faster
  SD clock. The CSV log now rotates **monthly** (`/hydro-YYYY-MM.csv`)
  rather than daily: for the insert-and-forget archival use, 12 files a
  year keeps the `/logs` web list complete and quick — no realistic
  truncation. The log interval goes from 1 to 5 minutes — water/air
  temp, humidity and light move slowly, so 5-minute samples capture the
  trend and cut file size 5× (the live screen is unaffected — it still
  refreshes at the 15 s poll rate). And `SD_SPI_FREQ_HZ` rises 4 →
  20 MHz: the conservative 4 MHz was leftover from when the SD card
  shared the VSPI bus with the touchscreen, and at ~0.4 MB/s it made a
  month's download a tens-of-seconds dashboard freeze. A monthly file is
  now ~3 MB and downloads in a couple of seconds. Three `config.h`
  constants and one `strftime` format — no code-path changes.
- **v0.1.16:** Daily SD-log rotation + browser log download. The CSV log
  now rotates **per local day** — `sdlog.cpp` writes to
  `/hydro-YYYY-MM-DD.csv` (the date from the configured timezone), each
  day's file getting its own header row; `/hydro-log.csv` is kept as the
  pre-NTP fallback (a no-internet LAN logs there exactly as before). Two
  new endpoints let the log be pulled without removing the card:
  `GET /logs` lists the CSV files on the card (name + size) and
  `GET /logs/download?file=NAME` streams one as a download. The `file`
  param is untrusted — `sdlog_is_log_filename()` is a strict whitelist
  (no separators, no `..`, `hydro-` prefix, `.csv` suffix) applied before
  any `SD.open`. The settings SPA's Logging section lists the files as
  download links. SD/FS knowledge stays in `sdlog.cpp`
  (`sdlog_list_files` / `sdlog_open_for_read`); `http_server.cpp` only
  calls those. The download briefly freezes the dashboard during the
  transfer (synchronous `WebServer`, same trade-off as the OTA upload).
- **v0.1.15:** microSD CSV data logging. New module `sdlog.cpp` appends
  one CSV row per device to `/hydro-log.csv` on the microSD card every
  `SD_LOG_INTERVAL_MS` (default 60 s) — pull the card to analyze the data
  in a spreadsheet. The card is optional: with no card (or a failed
  mount) `sdlog_begin()` disables logging cleanly and the dashboard is
  unaffected; pulling the card mid-run is detected on the next append and
  self-disables. Logging is driven from `loop()` (no task, no locking) —
  the SD card owns the VSPI bus outright now that the touchscreen is
  gone. The CSV carries `timestamp` (ISO-8601, empty until NTP syncs),
  `uptime_s`, and each device's readings; a NaN reading is an empty
  cell. The timestamp's timezone is picked from a dropdown of common
  zones in the settings SPA (each maps to a POSIX TZ string, so DST is
  automatic) — default UTC (trailing `Z`), otherwise local time with a
  `±HH:MM` offset. NTP is started non-blocking — a no-internet LAN keeps
  the empty-timestamp / uptime-seconds fallback.
  `GET /config` gains a read-only `sdlog` status; the settings SPA shows
  it. Logging *records*, it never interprets — no code path reads the
  CSV back — so it stays consistent with the dashboard's stateless
  stance. `SD` / `FS` / `SPI` ship with the esp32 core (no library
  change). Single append-only file for now; daily rotation is open work.
- **v0.1.14:** Touchscreen removed. The XPT2046 touch panel and the
  on-device settings screen are deleted — the dashboard is now
  display-only, configured entirely from the web settings console.
  Touch on the CYD was the firmware's flakiest subsystem (placeholder
  calibration, a stub recalibration flow, off-axis on first press), and
  everything the settings screen offered (brightness mode) is on the web
  console. `touch.cpp/.h` and `ui_settings.cpp/.h` are gone; the UI
  screen state machine collapses to the single hero view, which now
  always auto-cycles (no on-device focus/pause). The `dash-touch` NVS
  namespace and the touch-calibration prefs are removed — no
  `PREFS_SCHEMA` bump (separate namespace; orphaned data on existing
  units is harmless). Removing touch also frees the VSPI SPI controller,
  which the microSD card needs.
- **v0.1.13:** Temperature unit preference + tidied Govee tiles.
  A new dashboard-wide temperature display unit — **Auto / °C / °F** —
  is settable from the web settings console (segmented control in the
  Display section, `POST /config/units`, surfaced in `GET /config`).
  `Auto` keeps the original behavior (each tile shows whatever unit its
  device reports); `°C` / `°F` convert every tile's temperatures (air
  and water) at render time in `ui_hero.cpp`. Stored as the additive
  `dash-ui` NVS key `units` — no `PREFS_SCHEMA` bump, units flashed
  before it default to `Auto`. Conversion is display-only — the
  dashboard still mirrors each device's reported reading; it just
  presents it in the chosen unit. Separately, BLE (Govee H5075) tiles
  now render a trimmed three-row layout — **Air, Humidity, Battery** —
  instead of the four-row HTTP layout with its misleading `Water OFF` /
  `Light OFF` rows. Battery percent comes from the H5075 advertisement
  (already captured since v0.1.12, now shown); the rows are vertically
  centered in the readings area. Also fixed a settings-SPA bug latent
  since v0.1.10: the 5 s `/devices` poll rebuilt the device list from
  scratch, so an in-progress display-name edit was discarded before it
  could be saved — the rebuild now skips while an alias field is focused.
- **v0.1.12:** Passive BLE sensor support — Govee H5075. New module
  `ble_scanner.cpp` runs a low-duty-cycle passive BLE advertisement scan
  (NimBLE-Arduino, scan-only — no pairing, no GATT connect) and surfaces
  each Govee H5075 temperature/humidity sensor as an ordinary device in
  the hero rotation, alongside the HTTP-polled cores3-hydro units. This
  deliberately widens the dashboard's role: it is no longer a pure
  consumer — it now also sources readings. The H5075's
  manufacturer-specific advertisement carries a 24-bit big-endian packed
  temp/humidity value plus a battery byte; `ble_scanner.cpp::decode_h5075`
  decodes it (offsets per Home Assistant's `govee-ble` library and the
  Theengs decoder — **verify against a real device**; a sanity gate drops
  an implausible decode rather than painting garbage). `DeviceRecord`
  gains a `device_kind` discriminator (HTTP/BLE) and a `mac` field; the
  poller skips BLE entries (they have no IP), and the BLE scan task owns
  their staleness — a tile that has not broadcast within
  `BLE_STALE_AFTER_MS` (90 s) goes gray. BLE sensors render Water and
  Light as `OFF`, Air and Humidity as live values. The scanner uses a
  ~30% radio duty cycle (`BLE_SCAN_WINDOW_MS` < `BLE_SCAN_INTERVAL_MS`)
  with a gap between windows so it does not starve WiFi on the WROOM-32's
  shared 2.4 GHz radio; its task is pinned to core 0 alongside discovery.
  `setup.ps1` gains `NimBLE-Arduino` (pinned to 2.x — the line compatible
  with the arduino-esp32 3.x core; 1.4.x targets IDF 4.x and aborts at
  boot). The BT controller is the feature's main RAM cost on
  the no-PSRAM CYD. `/devices` JSON rows now carry `kind` and `mac`.
- **v0.1.11:** Browser-driven firmware updates + JSON endpoint links.
  The settings SPA gains a Firmware section — pick a `.bin`, watch a
  progress bar, the device verifies the image and reboots into it —
  and an API section linking the live JSON endpoints (`/devices`,
  `/config`, `/status`) so they are discoverable. New module
  `ota.cpp` registers `POST /ota/upload`: a multipart upload fed
  straight to the `Update` library. Unlike cores3-hydro, which
  buffers the whole image in PSRAM before flashing, the CYD has no
  PSRAM — so each multipart chunk streams directly into the inactive
  OTA app partition. Still safe: `Update` writes only the spare
  partition and flips the boot pointer at `end()`, so an interrupted
  or invalid upload leaves the running firmware bootable and
  untouched. The synchronous `WebServer` blocks for the upload's
  duration, which freezes the on-screen UI until the reboot — fine
  for a ~10–30 s operation. The OTA partition layout is the one
  v0.1.9's `min_spiffs` swap preserved (two ~1.9 MB app slots). The
  SPA pauses its device-list poll during an upload so it does not
  compete with the transfer on the single-threaded server.
- **v0.1.10:** Web settings console + configurable auto-cycle dwell.
  The HTTP server now serves a single-page settings app at `/`
  (gzip-compressed, embedded in PROGMEM via `web_assets.h` —
  regenerate from `web/index.html` with `tools/gen-web-assets.ps1`),
  replacing the old server-rendered status page. New JSON API:
  `GET /config` plus `POST /config/{brightness,cycle,alias}` and
  `POST /devices/remove`. The page lets the user rename devices, add
  and remove manual hosts, switch backlight mode, and set how long
  each device is shown before the hero view advances — previously a
  compile-time `CYCLE_INTERVAL_MS = 6000` constant, now the `dash-ui`
  pref `cycle` (0–60 s; `0` disables auto-cycling, holding the view
  on one device until tapped). `cycle` is an additive NVS key, so it
  needs no `PREFS_SCHEMA` bump — units flashed before it existed read
  the 6 s default. The dashboard stays a stateless consumer: the web
  UI changes only the dashboard's own settings, never the upstream
  cores3-hydro devices. Removing a manual host stops it being
  re-seeded but leaves its record on screen until the next reboot —
  the device array is append-only by design so the lockless UI reader
  stays safe.
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
PCB-retention posts, both USB cutouts, an optional microSD
card-access slot, an integrated stylus channel, removable kickstand,
and a wall-mount keyhole.



| Subsystem | Pin / detail |
|-----------|--------------|
| ILI9341 TFT (HSPI) | MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, RST -1, BL 21 |
| microSD (VSPI) | MOSI 23, MISO 19, SCLK 18, CS 5 |
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
- VSPI carries the microSD card (CSV logging, added v0.1.15); it has the
  bus to itself since the touchscreen was removed in v0.1.14.

## Boot order (`hydro-dash.ino`)

1. `Serial.begin(115200)`
2. `state_init()` — allocate the device-table mutex.
3. `prefs_load()` — pull NVS prefs (brightness, rotation, manual hosts).
4. `backlight_begin()` — PWM on TFT_BL, LDR pin mode.
5. `ui_begin()` — LovyanGFX init, fill black, set rotation from prefs.
6. `wifi_setup_begin()` — `WiFiManager.autoConnect`; opens `hydro-dash-setup`
   AP if no creds; reboots after `AP_TIMEOUT_S` failure.
7. `discovery_begin()` — `MDNS.begin`, seed manual hosts from prefs,
   spawn the browse task.
8. `poller_begin()` — spawn the per-device polling task on core 1.
9. `ble_scanner_begin()` — spawn the passive BLE scan task (Govee H5075).
10. `http_server_begin()` — bind `/`, `/devices`, `/status`, etc.

`loop()` pumps `ui_loop`, `backlight_loop`, and `http_server_loop` at
~10 ms cadence.

## Concurrency model

Same convention as cores3-hydro: producers write atomics, consumers read
without locking. The only mutex is `g_devices_mutex` which guards
inserts into `g_devices[]` (since hostname strings need to be copied
under a lock to prevent a partial-write race during a concurrent browse
+ manual-add).

Tasks:

| Task | Core | Stack | Purpose |
|------|------|-------|---------|
| loop (Arduino) | 1 | default | UI render, backlight, HTTP server |
| `discovery` | 0 | 4 KB | mDNS browse every `DISCOVERY_INTERVAL_MS` |
| `poller` | 1 | 8 KB | round-robin per-device polling |
| `ble_scan` | 0 | 4 KB | passive BLE advertisement scan (Govee H5075) |

## Discovery

`discovery_begin()` calls `MDNS.queryService("http", "tcp")` periodically
and filters results whose hostname starts with `cores3-hydro-`. New
hostnames are inserted into `g_devices[]` via `state_insert()`. Resolved
IPs are cached on the `DeviceRecord` so the poller can fall back to IP
when mDNS resolution fails on a specific request.

Manual hosts (added via `POST /devices`) are
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

The dashboard is display-only — there is no on-device input. It shows a
single screen, the hero view: one device at a time, auto-cycling through
`g_devices[]` on the `prefs_cycle_seconds` timer (`0` holds on the first
device). All configuration is done from the web console (see HTTP API).

- **Status strip** (top 30 px): one colored dot per known device — green
  (fresh, all sensors REAL), yellow (any sensor SIMULATED upstream), gray
  (stale / no data). The focused device's dot has a white ring.
- **Hero readings:** the focused device's name and large readings —
  Water / Air / Humidity / Light for cores3-hydro units, Air / Humidity /
  Battery for Govee BLE sensors. A disabled (OFF) sensor renders `OFF` in
  dim grey.
- **Footer:** device index, RSSI, and this dashboard's own hostname.

## HTTP API

The dashboard itself exposes a small management API on port 80. LAN-trusted,
no auth.

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/` | GET | Settings single-page app (gzipped, served from PROGMEM) |
| `/devices` | GET | JSON snapshot of every known device (each row has `kind`: `http`\|`ble`, and `mac` for BLE rows) |
| `/devices` | POST | Add a manual host (form: `hostname=...`) |
| `/devices/remove` | POST | Drop a manual host from the persisted list (form: `hostname=...`) |
| `/status` | GET | FW version, uptime, heap, device count |
| `/config` | GET | Brightness, auto-cycle dwell, temperature unit, SD-logging status, log timezone, manual hosts |
| `/config/brightness` | POST | Set backlight mode (form: `mode=auto\|full\|dim`) |
| `/config/cycle` | POST | Set auto-cycle dwell seconds (form: `seconds=N`, 0–60) |
| `/config/units` | POST | Set temperature display unit (form: `unit=auto\|celsius\|fahrenheit`) |
| `/config/timezone` | POST | Set the SD-log timestamp timezone (form: `tz=<POSIX TZ string>`) |
| `/config/alias` | POST | Set/clear a device display name (form: `hostname=...&alias=...`) |
| `/wifi/reset` | POST | Wipe creds and reboot to AP mode |
| `/rebrowse` | POST | Force an mDNS rebrowse |
| `/ota/upload` | POST | Multipart firmware-image upload; verifies, then reboots |
| `/logs` | GET | JSON list of SD log files (name + size) |
| `/logs/download` | GET | Download one log file as CSV (query: `file=NAME`, traversal-validated) |

## NVS namespaces

Each is single-purpose so a wipe-one-thing user action doesn't hit unrelated state.

| Namespace | Holds |
|-----------|-------|
| `dash-ui` | brightness mode, rotation, auto-cycle dwell seconds, temperature unit |
| `dash-hosts` | manually-added cores3-hydro hostnames |
| `dash-alias` | per-hostname display alias |

WiFiManager owns its own NVS keys and is intentionally not surfaced here.

## Open work (post-scaffold)

- **Stale-but-visible rendering** — keep last-known readings on screen
  in dimmed text rather than blanking the tile.
- **Sparklines** — small history graphs in detail view (optional;
  needs a server-side aggregator if we want anything > a few minutes,
  or could read back the SD CSV log).
- **SD log: optional on/off toggle** — logging is on whenever a card is
  present. A `dash-ui` NVS toggle + a web control could disable it with
  a card inserted (additive key — no `PREFS_SCHEMA` bump).
- **Screen rotation auto-detect** via the accelerometer? CYD doesn't
  have one — skip.
- **Remove a dead BLE sensor without a reboot.** `g_devices` is
  append-only by design (the UI task reads it lockless), so a Govee
  sensor that is unplugged, relocated out of range, or whose battery
  dies stays as a permanently-stale tile until reboot — and keeps
  consuming one of the `MAX_DEVICES` slots.
- **More BLE sensor models** (Govee H5074/H5102/H5179, ThermoPro …).
  `decode_h5075` is model-specific; generalizing means a decoder
  dispatch keyed on company ID + payload length/shape. See
  `docs/govee-ble.md` for the H5075 wire format and the steps to add a
  model.
