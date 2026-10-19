// Remote for the block engine. The ESP-NOW radio belongs to the game code's
// EspNowManager; this class only reads its latest packet (the ESP8266 remote's
// 7-byte packet: raw joystick ADC + 3 buttons) and converts it for blocks.
#pragma once
#include <stdint.h>

class RemoteLink {
 public:
  void begin();
  void update();
  bool connected() const { return connected_; }
  int8_t joyX() const { return connected_ ? x_ : 0; }
  int8_t joyY() const { return connected_ ? y_ : 0; }
  bool button(uint8_t bit) const { return connected_ && (buttons_ & (1u << bit)); }
  uint16_t buttons() const { return connected_ ? buttons_ : 0; }
  void buzz(uint16_t ms, uint8_t strength = 80);

 private:
  bool connected_ = false;
  int8_t x_ = 0, y_ = 0;
  uint32_t alertOffAt_ = 0;
  uint16_t buttons_ = 0;
};

extern RemoteLink remote;
