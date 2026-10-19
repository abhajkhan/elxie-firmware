// Remote link over ESP-NOW (control link from the framework doc).
// Packet formats below are the contract with the remote's firmware; if your
// remote already speaks another format, adapt parse() only.
#pragma once
#include <stdint.h>

struct __attribute__((packed)) RemoteInputPacket {  // remote -> robot, ~50 Hz
  uint8_t magic;    // 0xE1
  uint8_t type;     // 1
  int8_t joyX;      // -100..100 (right +)
  int8_t joyY;      // -100..100 (up +)
  uint16_t buttons; // bit per button, see REMOTE_BTN_* in board_config.h
  uint8_t seq;
};

struct __attribute__((packed)) RemoteHapticPacket {  // robot -> remote
  uint8_t magic;    // 0xE1
  uint8_t type;     // 2
  uint16_t ms;
  uint8_t strength; // 0..100
};

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
  uint16_t buttons_ = 0;
};

extern RemoteLink remote;
