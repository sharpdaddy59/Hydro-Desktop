#include "state.h"
#include <cstring>
#include <cmath>

DeviceRecord    g_devices[MAX_DEVICES];
std::atomic<uint8_t> g_device_count{0};
std::atomic<uint32_t> g_state_version{0};
SemaphoreHandle_t    g_devices_mutex = nullptr;

void state_bump_version() {
  g_state_version.fetch_add(1, std::memory_order_relaxed);
}

void state_init() {
  g_devices_mutex = xSemaphoreCreateMutex();
  for (auto& d : g_devices) {
    d.hostname[0]   = '\0';
    d.alias[0]      = '\0';
    d.fw_version[0] = '\0';
    d.last_ip       = IPAddress();
    d.mac[0]        = '\0';
    d.device_kind.store((uint8_t)DeviceKind::HTTP);
    d.water_temp.store(NAN);
    d.air_temp.store(NAN);
    d.humidity.store(NAN);
    d.light.store(NAN);
    d.rssi.store(0);
    d.uptime_s.store(0);
    d.battery_pct.store(-1);
    d.last_ok_ms.store(0);
    d.consecutive_fails.store(0);
    d.stale.store(true);
    d.mode_air.store((uint8_t)SensorMode::REAL);
    d.mode_water.store((uint8_t)SensorMode::REAL);
    d.mode_light.store((uint8_t)SensorMode::REAL);
    d.temp_unit.store((uint8_t)TempUnit::CELSIUS);
    d.has_data.store(false);
  }
}

int state_find_by_hostname(const char* hostname) {
  uint8_t n = g_device_count.load();
  for (uint8_t i = 0; i < n; i++) {
    if (strcmp(g_devices[i].hostname, hostname) == 0) return i;
  }
  return -1;
}

int state_insert(const char* hostname) {
  if (xSemaphoreTake(g_devices_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return -1;

  int existing = state_find_by_hostname(hostname);
  if (existing >= 0) { xSemaphoreGive(g_devices_mutex); return existing; }

  uint8_t n = g_device_count.load();
  if (n >= MAX_DEVICES) { xSemaphoreGive(g_devices_mutex); return -1; }

  DeviceRecord& d = g_devices[n];
  strncpy(d.hostname, hostname, sizeof(d.hostname) - 1);
  d.hostname[sizeof(d.hostname) - 1] = '\0';
  g_device_count.store(n + 1);

  xSemaphoreGive(g_devices_mutex);
  state_bump_version();
  return n;
}

// Insert a BLE-heard device. Mirrors state_insert but also stores the MAC
// and marks the kind, all under the mutex before g_device_count is bumped
// — so the lock-free UI reader never sees a half-populated record. The
// BLE scanner uses a synthesized hostname (derived from the MAC) as the
// stable lookup key, so repeated calls are idempotent.
int state_insert_ble(const char* hostname, const char* mac) {
  if (xSemaphoreTake(g_devices_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return -1;

  int existing = state_find_by_hostname(hostname);
  if (existing >= 0) { xSemaphoreGive(g_devices_mutex); return existing; }

  uint8_t n = g_device_count.load();
  if (n >= MAX_DEVICES) { xSemaphoreGive(g_devices_mutex); return -1; }

  DeviceRecord& d = g_devices[n];
  strncpy(d.hostname, hostname, sizeof(d.hostname) - 1);
  d.hostname[sizeof(d.hostname) - 1] = '\0';
  strncpy(d.mac, mac, sizeof(d.mac) - 1);
  d.mac[sizeof(d.mac) - 1] = '\0';
  d.device_kind.store((uint8_t)DeviceKind::BLE);
  g_device_count.store(n + 1);

  xSemaphoreGive(g_devices_mutex);
  state_bump_version();
  return n;
}
