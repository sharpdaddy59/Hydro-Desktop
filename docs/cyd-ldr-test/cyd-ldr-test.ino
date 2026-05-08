// cyd-ldr-test.ino — one-shot LDR diagnostic for the Sunton CYD.
//
// Standalone sketch. Boots without WiFi / HTTP / poller. Mirrors the
// locked-in LovyanGFX panel config from the main project (320x240,
// offset_y=80, rgb_order=true, rotation 4) so the readout is actually
// readable on this board.
//
// What it shows on every cycle (~500 ms):
//   * Big current raw ADC value (red if 0, green otherwise).
//   * eFuse-calibrated millivolts via analogReadMilliVolts().
//   * Burst of 8 back-to-back reads — visualises the high-impedance
//     S/H "ramp" if it's present (first sample low, later ones climb).
//   * Sparkline of the last ~50 samples.
//   * Rolling min / max / mean over that window.
//   * Cycles through all four ADC attenuations (0 / 2.5 / 6 / 11 dB)
//     every 4 s so you can see whether the value clips at one setting.
//   * Control read on GPIO 35 (unconnected, input-only) — sanity check
//     that ADC1 itself is alive.

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX_CYD : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI       _bus;
public:
  LGFX_CYD() {
    {
      auto c = _bus.config();
      c.spi_host    = HSPI_HOST;
      c.spi_mode    = 0;
      c.freq_write  = 55000000;
      c.freq_read   = 20000000;
      c.spi_3wire   = false;
      c.use_lock    = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk    = 14;
      c.pin_mosi    = 13;
      c.pin_miso    = 12;
      c.pin_dc      = 2;
      _bus.config(c);
      _panel.setBus(&_bus);
    }
    {
      auto c = _panel.config();
      c.pin_cs           = 15;
      c.pin_rst          = -1;
      c.pin_busy         = -1;
      c.panel_width      = 320;
      c.panel_height     = 240;
      c.offset_x         = 0;
      c.offset_y         = 80;
      c.offset_rotation  = 0;
      c.dummy_read_pixel = 8;
      c.dummy_read_bits  = 1;
      c.readable         = true;
      c.invert           = false;
      c.rgb_order        = true;
      c.dlen_16bit       = false;
      c.bus_shared       = false;
      _panel.config(c);
    }
    setPanel(&_panel);
  }
};

static LGFX_CYD tft;

#define LDR_PIN     34
#define CONTROL_PIN 35
#define TFT_BL      21

constexpr int BURST_N  = 8;
constexpr int WINDOW_N = 64;

static const adc_attenuation_t ATTENS[4]    = { ADC_0db, ADC_2_5db, ADC_6db, ADC_11db };
static const char*             ATTEN_NAME[4] = { "0dB", "2.5dB", "6dB", "11dB" };
static int       g_atten_idx       = 3;
static uint32_t  g_last_atten_swap = 0;

static int g_window[WINDOW_N];
static int g_w_idx  = 0;
static int g_w_fill = 0;

static void backlight_on() {
  ledcAttach(TFT_BL, 5000, 8);
  ledcWrite(TFT_BL, 255);
}

static void read_burst(int* out, int n) {
  for (int i = 0; i < n; i++) out[i] = analogRead(LDR_PIN);
}

static void draw_sparkline(int x, int y, int w, int h, const int* vals, int n) {
  tft.drawRect(x, y, w, h, TFT_DARKGREY);
  if (n < 2) return;
  for (int i = 1; i < n; i++) {
    int x0 = x + ((i - 1) * (w - 1)) / (n - 1);
    int x1 = x + (i       * (w - 1)) / (n - 1);
    int y0 = y + h - 1 - ((vals[i - 1] * (h - 2)) / 4095);
    int y1 = y + h - 1 - ((vals[i]     * (h - 2)) / 4095);
    tft.drawLine(x0, y0, x1, y1, TFT_GREEN);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("cyd-ldr-test starting");

  pinMode(LDR_PIN, INPUT);
  pinMode(CONTROL_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(LDR_PIN,     ATTENS[g_atten_idx]);
  analogSetPinAttenuation(CONTROL_PIN, ADC_11db);

  tft.init();
  tft.setRotation(4);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  backlight_on();
}

void loop() {
  uint32_t now = millis();

  if (now - g_last_atten_swap > 4000) {
    g_last_atten_swap = now;
    g_atten_idx = (g_atten_idx + 1) % 4;
    analogSetPinAttenuation(LDR_PIN, ATTENS[g_atten_idx]);
  }

  int burst[BURST_N];
  read_burst(burst, BURST_N);
  int raw = burst[BURST_N - 1];
  int mv  = analogReadMilliVolts(LDR_PIN);
  int ctl = analogRead(CONTROL_PIN);

  g_window[g_w_idx] = raw;
  g_w_idx = (g_w_idx + 1) % WINDOW_N;
  if (g_w_fill < WINDOW_N) g_w_fill++;
  long sum = 0;
  int wmin = 4096, wmax = -1;
  for (int i = 0; i < g_w_fill; i++) {
    sum += g_window[i];
    if (g_window[i] < wmin) wmin = g_window[i];
    if (g_window[i] > wmax) wmax = g_window[i];
  }
  int mean = g_w_fill ? (int)(sum / g_w_fill) : 0;

  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(4, 4);
  tft.print("LDR diag  GPIO 34");

  tft.setTextSize(5);
  tft.setTextColor(raw == 0 ? TFT_RED : TFT_GREEN, TFT_BLACK);
  tft.setCursor(4, 28);
  char buf[16];
  snprintf(buf, sizeof(buf), "%4d", raw);
  tft.print(buf);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(160, 32);
  tft.printf("%4d mV", mv);
  tft.setCursor(160, 56);
  tft.printf("att %s", ATTEN_NAME[g_atten_idx]);

  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(4, 92);
  tft.print("burst:");
  for (int i = 0; i < BURST_N; i++) {
    tft.setCursor(48 + i * 34, 92);
    tft.printf("%4d", burst[i]);
  }

  draw_sparkline(4, 108, 312, 50, g_window, g_w_fill);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4, 168);
  tft.printf("min %4d  max %4d", wmin, wmax);
  tft.setCursor(4, 192);
  tft.printf("mean %4d", mean);

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(4, 224);
  tft.printf("ctrl GPIO35: %4d  (floating, expect noisy mid-scale)", ctl);

  Serial.printf("att=%-5s burst=", ATTEN_NAME[g_atten_idx]);
  for (int i = 0; i < BURST_N; i++) Serial.printf("%4d ", burst[i]);
  Serial.printf("mv=%4d min=%4d max=%4d mean=%4d ctrl35=%4d\n",
                mv, wmin, wmax, mean, ctl);

  delay(500);
}
