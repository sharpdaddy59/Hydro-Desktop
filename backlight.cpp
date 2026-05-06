#include "backlight.h"
#include "config.h"
#include "prefs.h"
#include <Arduino.h>

static uint8_t  s_duty       = BL_MAX_DUTY;
static uint32_t s_last_check = 0;
static float    s_ema        = 2000;

void backlight_begin() {
  // ESP32 Arduino Core 3.x LedC API: pin-based, channels are managed
  // internally. ledcAttach binds the pin to a freshly-allocated channel
  // at the requested frequency/resolution; subsequent ledcWrite calls
  // address the pin directly.
  ledcAttach(TFT_BL, TFT_BL_PWM_FREQ, TFT_BL_PWM_BITS);
  ledcWrite(TFT_BL, BL_MAX_DUTY);
  pinMode(LDR_PIN, INPUT);
}

void backlight_set_duty(uint8_t d) {
  s_duty = d;
  ledcWrite(TFT_BL, d);
}

static uint8_t duty_for_ldr(uint16_t raw) {
  // Inverse mapping: brighter room (higher ADC) -> higher backlight duty.
  // The CYD's LDR is wired so dark = low ADC, light = high ADC.
  if (raw <= BL_LDR_DARK)   return BL_MIN_DUTY;
  if (raw >= BL_LDR_BRIGHT) return BL_MAX_DUTY;
  uint32_t span = BL_LDR_BRIGHT - BL_LDR_DARK;
  uint32_t pos  = raw - BL_LDR_DARK;
  return BL_MIN_DUTY + (uint8_t)((BL_MAX_DUTY - BL_MIN_DUTY) * pos / span);
}

void backlight_loop() {
  uint32_t now = millis();
  if (now - s_last_check < 500) return;
  s_last_check = now;

  switch (prefs_brightness_mode()) {
    case BRIGHTNESS_FULL: backlight_set_duty(BL_MAX_DUTY); return;
    case BRIGHTNESS_DIM:  backlight_set_duty(BL_MIN_DUTY); return;
    case BRIGHTNESS_AUTO: break;
  }

  uint16_t raw = analogRead(LDR_PIN);
  s_ema = s_ema * 0.85f + (float)raw * 0.15f;
  uint8_t target = duty_for_ldr((uint16_t)s_ema);
  if (target != s_duty) backlight_set_duty(target);
}
