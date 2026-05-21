// ble_scanner.cpp — passive BLE scan for Govee H5075 sensors.
//
// Built against NimBLE-Arduino 2.x (pinned in setup.ps1). NimBLE 2.x is
// the line compatible with arduino-esp32 3.x / ESP-IDF 5.x — the 1.4.x
// line targets IDF 4.x and its esp_bt_controller_init() aborts at boot on
// a 3.x core. The scan API here (NimBLEScanCallbacks, const onResult,
// setScanCallbacks, getResults) is 2.x-specific.

#include "ble_scanner.h"
#include "config.h"
#include "state.h"
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cmath>
#include <string>

// ---------------------------------------------------------------------------
// Debug logging — off by default. Set BLE_DEBUG to 1 to log every scan
// window and every decoded Govee advert (raw manufacturer-data hex +
// decoded values) to serial @ 115200. Useful for re-deriving the decode
// against a new H5075 firmware revision, or when adding another model.
// ---------------------------------------------------------------------------
#define BLE_DEBUG 0

#if BLE_DEBUG
  #define BLE_LOGF(...) Serial.printf(__VA_ARGS__)
// Plain counters — incremented only from the NimBLE host task (onResult),
// read+reset from the scan task. A rare off-by-one in the summary is
// harmless; not worth an atomic for throwaway debug scaffolding.
static uint32_t s_dbg_total_adverts = 0;   // all adverts seen this window
static uint32_t s_dbg_govee_adverts = 0;   // company-ID matches this window
static void dbg_print_hex(const std::string& s) {
  for (size_t i = 0; i < s.size(); i++) Serial.printf("%02X", (uint8_t)s[i]);
}
#else
  #define BLE_LOGF(...) ((void)0)
#endif

// ---------------------------------------------------------------------------
// Govee H5075 advertisement decode
// ---------------------------------------------------------------------------

struct GoveeReading {
  float temp_c;
  float humidity;
  int   battery_pct;
  bool  ok;
};

// Decode the H5075's manufacturer-specific payload (the 2-byte company ID
// must ALREADY be stripped). Layout: byte 0 is a leading byte; bytes 1-3
// are a 24-bit big-endian packed value whose top bit is the temperature
// sign; byte 4 is battery percent. temp_c = ±mag/10000, humidity =
// (mag % 1000) / 10. Offsets per Home Assistant's `govee-ble` library and
// the Theengs decoder.
//
// VERIFY AGAINST A REAL DEVICE: Govee has shipped multiple H5075 firmware
// revisions. The sanity gate below drops an implausible decode (wrong
// company ID / wrong offsets) rather than painting garbage on the hero
// screen — but a silently-wrong offset would just show nothing. Compare
// the decoded values against the sensor's own LCD on first bring-up.
static GoveeReading decode_h5075(const uint8_t* p, size_t len) {
  GoveeReading r{NAN, NAN, -1, false};
  if (len < 5) return r;

  uint32_t packed   = ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
  bool     negative = (packed & 0x800000u) != 0;
  uint32_t mag      = packed & 0x7FFFFFu;

  r.temp_c      = (negative ? -1.0f : 1.0f) * (mag / 10000.0f);
  r.humidity    = (mag % 1000) / 10.0f;
  r.battery_pct = p[4];

  // Sanity gate — reject nonsense so a wrong decode is dropped, not shown.
  if (r.temp_c   < -40.0f || r.temp_c   > 80.0f)  return r;
  if (r.humidity <   0.0f || r.humidity > 100.0f) return r;
  if (r.battery_pct < 0   || r.battery_pct > 100) return r;

  r.ok = true;
  return r;
}

// ---------------------------------------------------------------------------
// Pending-results buffer — the NimBLE callback (host task) stashes here;
// the scan task drains it. Mirrors discovery.cpp's collect-then-insert
// pattern so the short callback never touches g_devices_mutex.
// ---------------------------------------------------------------------------

struct PendingSensor {
  uint8_t      mac[6];     // canonical (display) octet order
  GoveeReading reading;
  int          rssi;
  bool         used;
};

static PendingSensor     s_pending[MAX_DEVICES];
static SemaphoreHandle_t s_pending_mutex = nullptr;

// NimBLE stores BLE addresses little-endian (over-the-air order); flip to
// the human-readable big-endian octet order so the synthesized hostname
// and the displayed MAC string agree.
static void canon_mac(const NimBLEAddress& addr, uint8_t out[6]) {
  const uint8_t* val = addr.getVal();
  for (int i = 0; i < 6; i++) out[i] = val[5 - i];
}

class GoveeScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* adv) override {
    // Runs on the NimBLE host task — keep it short, no g_devices_mutex.
#if BLE_DEBUG
    s_dbg_total_adverts++;
#endif
    if (!adv->haveManufacturerData()) return;
    std::string md = adv->getManufacturerData();
    if (md.size() < 8) return;   // H5075: 2-byte company ID + 6-byte payload

    uint16_t cid = (uint8_t)md[0] | ((uint16_t)(uint8_t)md[1] << 8);

#if BLE_DEBUG
    // Safety net: an advert whose name starts "GV" (looks like a Govee)
    // but whose company ID does NOT match our constant — surfaces a wrong
    // GOVEE_H5075_COMPANY_ID in the log instead of it silently producing
    // zero results.
    if (cid != GOVEE_H5075_COMPANY_ID && adv->haveName() &&
        strncmp(adv->getName().c_str(), "GV", 2) == 0) {
      BLE_LOGF("[ble] ?? name=%s company=0x%04X expected=0x%04X hex=",
               adv->getName().c_str(), cid, GOVEE_H5075_COMPANY_ID);
      dbg_print_hex(md);
      BLE_LOGF("\n");
    }
#endif

    if (cid != GOVEE_H5075_COMPANY_ID) return;

    // Opportunistic name guard. The H5075's local name may live in a scan
    // response we never see during a passive scan, so a MISSING name is
    // fine — but a present, non-matching name is a different Govee product.
    if (adv->haveName() &&
        strncmp(adv->getName().c_str(), BLE_NAME_PREFIX,
                strlen(BLE_NAME_PREFIX)) != 0) {
      return;
    }

    GoveeReading r = decode_h5075((const uint8_t*)md.data() + 2, md.size() - 2);

    uint8_t canon[6];
    canon_mac(adv->getAddress(), canon);

#if BLE_DEBUG
    s_dbg_govee_adverts++;
    BLE_LOGF("[ble] Govee mac=%02X:%02X:%02X:%02X:%02X:%02X rssi=%d name=%s hex=",
             canon[0], canon[1], canon[2], canon[3], canon[4], canon[5],
             adv->getRSSI(),
             adv->haveName() ? adv->getName().c_str() : "(none)");
    dbg_print_hex(md);
    BLE_LOGF("\n[ble]   -> temp=%.2fC hum=%.1f%% batt=%d ok=%d\n",
             r.temp_c, r.humidity, r.battery_pct, r.ok ? 1 : 0);
#endif

    if (!r.ok) return;

    if (xSemaphoreTake(s_pending_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return;
    int slot = -1;
    for (int i = 0; i < MAX_DEVICES; i++) {
      if (s_pending[i].used && memcmp(s_pending[i].mac, canon, 6) == 0) {
        slot = i;  break;
      }
    }
    if (slot < 0) {
      for (int i = 0; i < MAX_DEVICES; i++) {
        if (!s_pending[i].used) { slot = i; break; }
      }
    }
    if (slot >= 0) {
      memcpy(s_pending[slot].mac, canon, 6);
      s_pending[slot].reading = r;
      s_pending[slot].rssi    = adv->getRSSI();
      s_pending[slot].used    = true;
    }
    xSemaphoreGive(s_pending_mutex);
  }
};

// ---------------------------------------------------------------------------
// Scan task — drain pending results into g_devices, age stale BLE tiles.
// ---------------------------------------------------------------------------

// "govee-a3f2c1" from the last 3 MAC octets — 16.7M-space uniqueness,
// ample for a home LAN, and stable across reboots (the H5075's public
// address is fixed). Used as the find-or-insert key, so repeated sightings
// map to the same DeviceRecord.
static void synth_hostname(const uint8_t* canon, char* out, size_t outlen) {
  snprintf(out, outlen, "govee-%02x%02x%02x", canon[3], canon[4], canon[5]);
}

static void apply_pending() {
  PendingSensor snap[MAX_DEVICES];
  if (xSemaphoreTake(s_pending_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
  memcpy(snap, s_pending, sizeof(snap));
  for (auto& p : s_pending) p.used = false;   // reset for the next window
  xSemaphoreGive(s_pending_mutex);

  for (auto& p : snap) {
    if (!p.used) continue;

    char host[20], mac[18];
    synth_hostname(p.mac, host, sizeof(host));
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             p.mac[0], p.mac[1], p.mac[2], p.mac[3], p.mac[4], p.mac[5]);

    int idx = state_insert_ble(host, mac);    // idempotent: existing index on repeat
    if (idx < 0) {                            // MAX_DEVICES full
      BLE_LOGF("[ble] %s dropped — device table full (MAX_DEVICES)\n", host);
      continue;
    }
    DeviceRecord& d = g_devices[idx];

    // Write through set_*_if_changed so the UI version only bumps when a
    // value actually moved — the H5075 re-broadcasts an unchanged reading
    // every couple of seconds and that must not flash the hero screen.
    bool changed = false;
    changed |= set_float_if_changed(d.air_temp,    p.reading.temp_c);
    changed |= set_float_if_changed(d.humidity,    p.reading.humidity);
    changed |= set_if_changed(d.rssi,        p.rssi);
    changed |= set_if_changed(d.battery_pct, p.reading.battery_pct);
    // BLE sensors carry only air temp + humidity. Water/Light render as
    // OFF; the modes are static but set_if_changed makes repeats free.
    changed |= set_if_changed(d.mode_air,   (uint8_t)SensorMode::REAL);
    changed |= set_if_changed(d.mode_water, (uint8_t)SensorMode::OFF);
    changed |= set_if_changed(d.mode_light, (uint8_t)SensorMode::OFF);
    changed |= set_if_changed(d.temp_unit,  (uint8_t)TempUnit::CELSIUS);
    d.water_temp.store(NAN);
    d.light.store(NAN);

    d.last_ok_ms.store(millis());             // drives BLE staleness
    if (!d.has_data.exchange(true)) changed = true;
    if (d.stale.exchange(false))    changed = true;

    if (changed) state_bump_version();

    BLE_LOGF("[ble] %s idx=%d air=%.2fC hum=%.1f%% batt=%d rssi=%d%s\n",
             host, idx, p.reading.temp_c, p.reading.humidity,
             p.reading.battery_pct, p.rssi, changed ? " (changed)" : "");
  }
}

// The HTTP poller drives staleness for HTTP devices but never sees BLE
// ones. This is their staleness owner: a BLE tile unheard for longer than
// BLE_STALE_AFTER_MS goes gray, exactly like a stale HTTP tile (the hero's
// status_color() logic is shared, so no UI change is needed).
static void age_ble_devices() {
  uint32_t now = millis();
  uint8_t  n   = g_device_count.load();
  for (uint8_t i = 0; i < n; i++) {
    DeviceRecord& d = g_devices[i];
    if ((DeviceKind)d.device_kind.load() != DeviceKind::BLE) continue;
    if (!d.has_data.load()) continue;
    if (now - d.last_ok_ms.load() > BLE_STALE_AFTER_MS) {
      if (!d.stale.exchange(true)) state_bump_version();  // bump on the 0->1 edge only
    }
  }
}

static void task_ble_scan(void* /*arg*/) {
  NimBLEScan* scan = NimBLEDevice::getScan();
  for (;;) {
    // Blocking scan window; callbacks fire on the NimBLE host task.
    // getResults(durationMs, isContinue) starts the scan, blocks, returns.
    scan->getResults((uint32_t)BLE_SCAN_DURATION_S * 1000, false);
    scan->clearResults();                     // free the result cache
    apply_pending();
    age_ble_devices();
#if BLE_DEBUG
    BLE_LOGF("[ble] window done: %u advert(s) seen, %u Govee match(es)\n",
             (unsigned)s_dbg_total_adverts, (unsigned)s_dbg_govee_adverts);
    s_dbg_total_adverts = 0;
    s_dbg_govee_adverts = 0;
#endif
    vTaskDelay(pdMS_TO_TICKS(BLE_SCAN_GAP_MS));  // WiFi-coexistence breather
  }
}

void ble_scanner_begin() {
  s_pending_mutex = xSemaphoreCreateMutex();
  for (auto& p : s_pending) p.used = false;

  NimBLEDevice::init("");                     // empty name — scan-only, never advertises
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new GoveeScanCallbacks(), /*wantDuplicates=*/true);
  scan->setActiveScan(false);                 // passive: never transmits scan requests
  // Interval/window (milliseconds in NimBLE 2.x) keep the controller's
  // scan duty cycle low (window < interval) so BLE listening does not
  // starve the WiFi poller.
  scan->setInterval(BLE_SCAN_INTERVAL_MS);
  scan->setWindow(BLE_SCAN_WINDOW_MS);

  // Core 0 alongside discovery; the latency-sensitive UI render loop owns
  // core 1. 4 KB stack matches discovery — bump if it overflows in test.
  xTaskCreatePinnedToCore(task_ble_scan, "ble_scan", 4096, nullptr, 1, nullptr, 0);
  BLE_LOGF("[ble] scanner started — passive scan for %s sensors\n", BLE_NAME_PREFIX);
}
