// NeoPixel lights: solid colors per pixel plus animated effects.
#pragma once
#include <stdint.h>

class Lights {
 public:
  void begin();
  void setAll(uint32_t rgb);
  void setPixel(int index1, uint32_t rgb);  // 1-based, as kids count
  void effect(uint8_t effect, uint32_t rgb);
  void setBrightness(float pct);
  void off();
  void bar(float value, float max, uint32_t rgb);  // first N LEDs lit, like a level meter
  void stopEffect() { effect_ = 0; dirty_ = true; }  // back to the solid colors
  void update();
  uint32_t pixel(int i) const { return (i >= 0 && i < kMax) ? px_[i] : 0; }

 private:
  static const int kMax = 64;
  uint32_t px_[kMax] = {0};
  uint8_t effect_ = 0;
  uint32_t effectColor_ = 0;
  uint32_t effectStart_ = 0;
  uint8_t brightness_ = 40;
  bool dirty_ = true;
  uint32_t lastShow_ = 0;
};

extern Lights lights;
