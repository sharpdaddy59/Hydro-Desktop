// ui.cpp — LovyanGFX panel + screen state machine.
//
// Hand-rolled panel config for the Sunton ESP32-2432S028R. We tried
// LovyanGFX's autodetect path; on this specific board it produced a
// blank screen (the runtime probe doesn't always latch onto the right
// chip variant). Hand-rolling is more reliable — pins live in config.h.

#define LGFX_USE_V1
#include "ui.h"
#include "ui_hero.h"
#include "ui_settings.h"
#include "state.h"
#include "config.h"
#include "prefs.h"

class LGFX_CYD : public lgfx::LGFX_Device {
  // Backlight is owned by backlight.cpp (LedC PWM driven by the LDR
  // auto-dim policy), so no Light_* instance is configured here.
  lgfx::Panel_ILI9341   _panel;
  lgfx::Bus_SPI         _bus;
  lgfx::Touch_XPT2046   _touch;

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
      c.pin_sclk    = TFT_SCLK;
      c.pin_mosi    = TFT_MOSI;
      c.pin_miso    = TFT_MISO;
      c.pin_dc      = TFT_DC;
      _bus.config(c);
      _panel.setBus(&_bus);
    }
    {
      auto c = _panel.config();
      c.pin_cs           = TFT_CS;
      c.pin_rst          = TFT_RST;
      c.pin_busy         = -1;
      // CYD-S028R panel mounting quirk: telling LovyanGFX the panel is
      // natively 320x240 makes width()/height() report landscape values
      // (so layout fills the user's view). offset_y=80 compensates for
      // the chip's GRAM-row alignment in this rotation. Empirically
      // determined via the cyd-rotation-test sketch.
      c.panel_width      = 320;
      c.panel_height     = 240;
      c.offset_x         = 0;
      c.offset_y         = 80;
      c.offset_rotation  = 0;
      c.dummy_read_pixel = 8;
      c.dummy_read_bits  = 1;
      c.readable         = true;
      c.invert           = false;
      c.rgb_order        = true;   // CYD wires the LCD as BGR; this enables MADCTL.BGR so RGB565 colors render correctly (yellow stays yellow, red stays red, etc.)
      c.dlen_16bit       = false;
      c.bus_shared       = false;
      _panel.config(c);
    }
    {
      auto c = _touch.config();
      c.x_min      = 300;
      c.x_max      = 3900;
      c.y_min      = 300;
      c.y_max      = 3900;
      c.pin_int    = TOUCH_IRQ;
      c.bus_shared = false;
      c.offset_rotation = 0;
      c.spi_host   = VSPI_HOST;
      c.freq       = 1000000;
      c.pin_sclk   = TOUCH_SCLK;
      c.pin_mosi   = TOUCH_MOSI;
      c.pin_miso   = TOUCH_MISO;
      c.pin_cs     = TOUCH_CS;
      _touch.config(c);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};

static LGFX_CYD          s_gfx;
static UiScreen          s_screen = UI_SCREEN_HERO;
static char              s_status[64] = {0};

lgfx::LGFX_Device& ui_gfx() { return s_gfx; }

void ui_begin() {
  s_gfx.init();
  s_gfx.setRotation(prefs_rotation());
  s_gfx.fillScreen(TFT_BLACK);
  s_gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  s_gfx.setTextSize(2);
}

void ui_set_screen(UiScreen s) {
  s_screen = s;
  s_gfx.fillScreen(TFT_BLACK);
  state_bump_version();
}

UiScreen ui_current_screen() { return s_screen; }

void ui_set_status(const char* msg) {
  strncpy(s_status, msg ? msg : "", sizeof(s_status) - 1);
  s_status[sizeof(s_status) - 1] = '\0';
  state_bump_version();
}

void ui_loop() {
  // Per-screen pre-render hooks (cycle timers, animations) need to run
  // every iteration so they're not held off by the dirty-version skip
  // below. Hero auto-cycle is the only one currently.
  if (s_screen == UI_SCREEN_HERO) ui_hero_tick();

  // Skip the render entirely if nothing display-relevant has changed
  // since the last successful draw. This is what keeps the screen still
  // between data updates instead of flickering through clear-then-redraw
  // cycles at 2 Hz.
  static uint32_t last_drawn_version = (uint32_t)-1;
  uint32_t v = g_state_version.load(std::memory_order_relaxed);
  if (v == last_drawn_version) return;
  last_drawn_version = v;

  switch (s_screen) {
    case UI_SCREEN_HERO:     ui_hero_draw();     break;
    case UI_SCREEN_SETTINGS: ui_settings_draw(); break;
  }

  if (s_status[0]) {
    s_gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
    s_gfx.setCursor(8, FY(s_gfx.height() - 24, 8));
    s_gfx.print(s_status);
  }
}

void ui_handle_touch(int16_t x, int16_t y) {
  switch (s_screen) {
    case UI_SCREEN_HERO:     ui_hero_handle_touch(x, y);     break;
    case UI_SCREEN_SETTINGS: ui_settings_handle_touch(x, y); break;
  }
}

void ui_handle_long_press(int16_t /*x*/, int16_t /*y*/) {
  // Long-press toggles between hero and settings.
  if (s_screen == UI_SCREEN_HERO)          ui_set_screen(UI_SCREEN_SETTINGS);
  else if (s_screen == UI_SCREEN_SETTINGS) ui_set_screen(UI_SCREEN_HERO);
}
