# Govee H5075 — BLE advertisement format

hydro-dash reads Govee H5075 temperature/humidity sensors by passively
listening to their Bluetooth Low Energy advertisements — no pairing, no
connection. This documents the wire format and how the firmware decodes
it. Implementation lives in [`ble_scanner.cpp`](../ble_scanner.cpp); the
tunables are in [`config.h`](../config.h).

## Why passive scanning

The H5075 broadcasts its current reading in the manufacturer-specific
data of its BLE advertisement, roughly every 2 seconds. hydro-dash runs a
**passive scan** — it never transmits scan requests and never connects or
pairs. It just listens.

- Every H5075 in range is picked up simultaneously; each becomes its own
  dashboard tile.
- A passive scan is cheap on the WROOM-32's shared 2.4 GHz radio. The
  `BLE_SCAN_*` constants in `config.h` keep the scan duty cycle low
  (window < interval) so BLE listening does not starve the WiFi poller.
- Trade-off: a passive scan does **not** receive scan-response packets.
  If a sensor were to put its name only in the scan response, that name
  would be invisible — so the firmware filters on the manufacturer-data
  **company ID**, not the device name. The `GVH5075` name prefix is only
  an opportunistic secondary check when a name happens to be present.

## The advertisement

The H5075's manufacturer-specific data (advertising data type `0xFF`) is
8 bytes:

```
byte:   0   1   2   3   4   5   6   7
        88  EC  00  ──packed──  BB  ──
        └company┘  └temp/hum─┘  battery
           ID
```

| Bytes | Field | Notes |
|-------|-------|-------|
| 0–1 | Company ID | `88 EC` → `0xEC88` little-endian. Keyed on empirically — see [Source & verification](#source--verification). |
| 2 | Leading byte | Not used by the decode (commonly `0x00`). |
| 3–5 | Packed temp/humidity | 24-bit **big-endian**. |
| 6 | Battery | Percent, `0`–`100`. |
| 7 | Trailing byte | Not used. |

`ble_scanner.cpp::decode_h5075` is handed the payload with the 2-byte
company ID **already stripped**, so inside that function `p[0]` is byte 2
above, `p[1..3]` the packed value, and `p[4]` the battery byte.

## Decoding temperature & humidity

The 24-bit packed value (bytes 3–5) carries both readings at once:

```
packed   = (byte3 << 16) | (byte4 << 8) | byte5     // big-endian
negative = packed & 0x800000                        // top bit = temp sign
mag      = packed & 0x7FFFFF                         // low 23 bits
temp_c   = (negative ? -1 : +1) * mag / 10000.0
humidity = (mag % 1000) / 10.0
battery  = byte6
```

Temperature is always Celsius on the wire. Conversion to °F — when the
user has chosen that display unit — happens at render time in
`ui_hero.cpp`, not here.

### Worked example

Manufacturer data `88 EC 00 03 6A 4C 5A 00`:

- Company ID `0xEC88` ✓
- `packed = 0x036A4C = 224332`; top bit clear → positive
- `temp_c  = 224332 / 10000` = **22.43 °C**
- `humidity = (224332 mod 1000) / 10 = 332 / 10` = **33.2 %**
- `battery = 0x5A` = **90 %**

## Sanity gate

`decode_h5075` rejects an implausible result — temperature outside
−40…80 °C, humidity outside 0…100 %, or battery outside 0…100 — and the
advertisement is dropped. So a wrong company ID or wrong byte offsets
**fail safe** (the tile simply shows no data) instead of painting garbage
on the screen.

## How hydro-dash uses it

- Each H5075 becomes a `DeviceRecord` with `device_kind == BLE`, keyed by
  a synthesized hostname `govee-<last 3 MAC octets>` (stable across
  reboots — the H5075 uses a fixed public address).
- The HTTP poller skips BLE devices; the BLE scan task owns their
  staleness — a sensor unheard for `BLE_STALE_AFTER_MS` (default 90 s)
  greys out, exactly like a stale HTTP device.
- BLE tiles render a trimmed three-row layout — Air, Humidity, Battery —
  with no Water/Light rows.
- Library: NimBLE-Arduino 2.x (scan-only).

## Source & verification

The byte layout above is the community-reverse-engineered H5075 format,
cross-referenced from:

- Home Assistant's `govee-ble` library — `govee_ble/parser.py`.
- The Theengs decoder — `Theengs/decoder`, the `GVH5075` device entry.

`0xEC88` is the value observed in the company-ID field; it is **not** a
Bluetooth SIG-assigned Govee identifier — the firmware keys on it
empirically. The constants live in `config.h` as `GOVEE_H5075_COMPANY_ID`
and `BLE_NAME_PREFIX`. First confirmed working against a physical H5075
in May 2026.

If a future H5075 firmware revision changes the layout, the `[ble]` debug
logging in `ble_scanner.cpp` (set `BLE_DEBUG` to `1`) dumps the raw
advertisement hex for every Govee packet so the offsets can be
re-derived.

## Adding another model

The Govee H5074, H5102, H5179 and the various ThermoPro sensors each
broadcast in their own layout. To support one:

1. Add its company ID and/or name prefix to `config.h`.
2. Add a per-model decoder alongside `decode_h5075`.
3. Dispatch on the company ID (and payload length) in the scan callback.

The `DeviceRecord` + `state_insert_ble` path is model-agnostic — only the
decode step differs.
