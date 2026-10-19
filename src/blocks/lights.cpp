#include "lights.h"

#include <math.h>

#include "blocks_config.h"
#include "esp_log.h"
#include "platform.h"
#include "esp_random.h"

#include <Adafruit_NeoPixel.h>

Lights lights;
static Adafruit_NeoPixel* s_strip = nullptr;
static const int kCount = NEOPIXEL_COUNT < 64 ? NEOPIXEL_COUNT : 64;

void Lights::begin(Adafruit_NeoPixel* strip) {
  s_strip = strip;
  setBrightness(NEOPIXEL_DEFAULT_BRIGHTNESS * 100.0f / NEOPIXEL_MAX_BRIGHTNESS);
  off();
}

void Lights::setAll(uint32_t rgb) {
  effect_ = 0;
  for (int i = 0; i < kCount; ++i) px_[i] = rgb & 0xFFFFFF;
  dirty_ = true;
}

void Lights::setPixel(int index1, uint32_t rgb) {
  effect_ = 0;
  if (index1 < 1 || index1 > kCount) return;
  px_[index1 - 1] = rgb & 0xFFFFFF;
  dirty_ = true;
}

void Lights::effect(uint8_t e, uint32_t rgb) {
  effect_ = e;
  effectColor_ = rgb & 0xFFFFFF;
  effectStart_ = millis();
  dirty_ = true;
}

void Lights::setBrightness(float pct) {
  // Block 100 % = the hard cap (battery + eyes): scale, don't clip, so every step still changes.
  brightness_ = (uint8_t)constrain(pct, 0.0f, 100.0f);
  dirty_ = true;
}

void Lights::off() { setAll(0); }

void Lights::bar(float value, float maxValue, uint32_t rgb) {
  effect_ = 0;
  float frac = maxValue > 0 ? value / maxValue : 0;
  frac = constrain(frac, 0.0f, 1.0f);
  const int lit = (int)lroundf(frac * kCount);
  for (int i = 0; i < kCount; ++i) px_[i] = i < lit ? (rgb & 0xFFFFFF) : 0;
  dirty_ = true;
}

static const uint8_t kLeftGroup[] = LIGHTS_LEFT_GROUP;
static const uint8_t kRightGroup[] = LIGHTS_RIGHT_GROUP;
static bool inGroup(const uint8_t* g, int n, int i) {
  for (int k = 0; k < n; ++k)
    if (g[k] == i) return true;
  return false;
}

static uint32_t scale(uint32_t c, uint32_t k) {  // k 0..255
  return (((c >> 16 & 0xFF) * k / 255) << 16) | (((c >> 8 & 0xFF) * k / 255) << 8) | ((c & 0xFF) * k / 255);
}

static uint32_t hsv(uint16_t hue, uint8_t sat, uint8_t val) {  // hue 0..65535
  uint32_t h = (uint32_t)hue * 6;
  uint8_t sector = (uint8_t)(h >> 16), f = (uint8_t)(h >> 8);
  uint8_t p = (uint8_t)(val * (255 - sat) / 255);
  uint8_t q = (uint8_t)(val * (255 - sat * f / 255) / 255);
  uint8_t t = (uint8_t)(val * (255 - sat * (255 - f) / 255) / 255);
  uint8_t r, g, b;
  switch (sector) {
    case 0: r = val; g = t; b = p; break;
    case 1: r = q; g = val; b = p; break;
    case 2: r = p; g = val; b = t; break;
    case 3: r = p; g = q; b = val; break;
    case 4: r = t; g = p; b = val; break;
    default: r = val; g = p; b = q; break;
  }
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void Lights::update() {
  if (!s_strip) return;
  uint32_t now = millis();
  if (effect_ == 0 && !dirty_) return;
  if (effect_ != 0 && now - lastShow_ < 20) return;  // animate at 50 Hz
  uint32_t t = now - effectStart_;
  const uint32_t level = (uint32_t)brightness_ * NEOPIXEL_MAX_BRIGHTNESS * 255 / 10000;  // 0..~153
  for (int i = 0; i < kCount; ++i) {
    uint32_t c = px_[i];
    switch (effect_) {
      case 1:  // rainbow
        c = hsv((uint16_t)(t * 20 + i * 65536 / kCount), 255, 255);
        break;
      case 2: {  // pulse
        float k = 0.5f - 0.5f * cosf(t * 0.004f);
        c = scale(effectColor_, (uint32_t)(20 + k * 235));
        break;
      }
      case 3:  // blink 2 Hz
        c = ((t / 250) % 2) ? 0 : effectColor_;
        break;
      case 4:  // chase
        c = ((int)((t / 60) % kCount) == i) ? effectColor_ : scale(effectColor_, 20);
        break;
      case 5: {  // police: halves flash red / blue
        const bool phase = (t / 150) % 2;
        c = (i < kCount / 2) == phase ? 0xFF0000 : 0x0030FF;
        if ((t / 600) % 2) c = scale(c, 60);
        break;
      }
      case 6: {  // fire: warm flicker
        const uint32_t r = esp_random();
        const uint32_t g = 40 + (r & 0x3F);
        c = (255u << 16) | (g << 8);
        c = scale(c, 120 + ((r >> 8) & 0x7F));
        break;
      }
      case 7:  // left turn signal (amber blink on the left LEDs)
      case 8:  // right turn signal
        c = inGroup(effect_ == 7 ? kLeftGroup : kRightGroup, 3, i) && ((t / 350) % 2) == 0 ? 0xFF8C00 : px_[i];
        break;
    }
    c = scale(c, level);
    s_strip->setPixelColor(i, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
  }
  s_strip->show();
  lastShow_ = now;
  dirty_ = false;
}
