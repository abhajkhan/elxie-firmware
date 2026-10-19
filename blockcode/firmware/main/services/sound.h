// Sound service: non-blocking note sequencer on a passive buzzer.
#pragma once
#include <stdint.h>

#include "hal/platform.h"

namespace sfx {
enum : uint8_t { BEEP = 1, HAPPY = 2, SAD = 3, ALARM = 4, LEVELUP = 5,
                 // internal (used by pet reactions / system), not in the block menu
                 EXCITED = 20, SCARED = 21, LOVE = 22, ANGRY = 23, SLEEPY = 24, BOOT = 25, ERROR = 26, CONNECT = 27 };
}

class Sound {
 public:
  void begin();
  uint32_t playNote(float midi, uint32_t ms);
  uint32_t playEffect(uint8_t id);
  uint32_t playTone(float hz, uint32_t ms);
  uint32_t playMelody(uint8_t id);  // 1 birthday, 2 twinkle, 3 victory, 4 siren
  void stop();
  bool done(uint32_t token) const { return token != token_ || (count_ == 0 && !playing_); }
  void setTempo(float bpm) { tempo_ = constrain(bpm, 20.0f, 400.0f); }
  float tempo() const { return tempo_; }
  uint32_t beatsToMs(float beats) const { return (uint32_t)(constrain(beats, 0.0f, 64.0f) * 60000.0f / tempo_); }
  void update();

 private:
  struct Note { uint16_t hz; uint16_t ms; };
  void push(uint16_t hz, uint16_t ms);
  void output(uint16_t hz);
  static const uint8_t kQueue = 64;
  Note q_[kQueue];
  uint8_t head_ = 0, count_ = 0;
  bool playing_ = false;
  uint32_t noteEnd_ = 0;
  uint32_t token_ = 0;
  float tempo_ = 120;
  uint16_t current_ = 0;
};

extern Sound sound;
