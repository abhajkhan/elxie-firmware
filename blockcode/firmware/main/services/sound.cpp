#include "sound.h"

#include <math.h>

#include <initializer_list>

#include "../hal/platform.h"
#include "board_config.h"

Sound sound;

static uint16_t midiToHz(float n) {
  if (n <= 0) return 0;
  return (uint16_t)(440.0f * powf(2.0f, (n - 69.0f) / 12.0f) + 0.5f);
}

void Sound::begin() {
  hal::pwmTimer(LEDC_TIMER_BUZZER, 1000, BUZZER_PWM_BITS);
  hal::pwmAttach(PIN_BUZZER, LEDC_CH_BUZZER, LEDC_TIMER_BUZZER);
  current_ = 1;
  output(0);
}

void Sound::output(uint16_t hz) {
  if (hz == current_) return;
  current_ = hz;
  if (hz < 20) {  // silence
    hal::pwmWrite(LEDC_CH_BUZZER, 0);
    return;
  }
  hal::pwmFreq(LEDC_TIMER_BUZZER, hz);
  hal::pwmWrite(LEDC_CH_BUZZER, 1u << (BUZZER_PWM_BITS - 1));  // 50 % duty = loudest square wave
}

void Sound::push(uint16_t hz, uint16_t ms) {
  if (count_ >= kQueue) return;
  q_[(head_ + count_) % kQueue] = {hz, ms};
  count_++;
}

void Sound::stop() {
  count_ = 0;
  playing_ = false;
  output(0);
  token_++;
}

uint32_t Sound::playNote(float midi, uint32_t ms) {
  stop();
  push(midiToHz(midi), (uint16_t)min<uint32_t>(ms, 60000));
  return token_;
}

uint32_t Sound::playEffect(uint8_t id) {
  stop();
  auto seq = [&](std::initializer_list<uint8_t> notes, uint16_t ms) {
    for (uint8_t n : notes) push(midiToHz(n), ms);
  };
  switch (id) {
    case sfx::BEEP: push(1000, 120); break;
    case sfx::HAPPY: seq({72, 76, 79, 84}, 90); break;
    case sfx::SAD: seq({67, 64}, 250); push(midiToHz(60), 500); break;
    case sfx::ALARM: for (int i = 0; i < 3; ++i) { push(880, 150); push(660, 150); } break;
    case sfx::LEVELUP: seq({72, 76, 79, 84, 88, 91}, 70); push(midiToHz(96), 220); break;
    case sfx::EXCITED: seq({76, 79, 76, 79, 84}, 60); break;
    case sfx::SCARED: seq({84, 78, 72}, 80); push(0, 60); push(midiToHz(90), 60); break;
    case sfx::LOVE: seq({72, 76}, 150); push(midiToHz(79), 300); break;
    case sfx::ANGRY: for (int i = 0; i < 3; ++i) { push(180, 90); push(0, 40); } break;
    case sfx::SLEEPY: seq({67, 62, 55}, 300); break;
    case sfx::BOOT: seq({60, 67, 72}, 80); break;
    case sfx::ERROR: push(300, 200); push(0, 60); push(200, 400); break;
    case sfx::CONNECT: seq({79, 84}, 70); break;
    default: push(1000, 60); break;
  }
  return token_;
}

uint32_t Sound::playTone(float hz, uint32_t ms) {
  stop();
  push((uint16_t)constrain(hz, 0.0f, 9000.0f), (uint16_t)min<uint32_t>(ms, 60000));
  return token_;
}

uint32_t Sound::playMelody(uint8_t id) {
  stop();
  const uint16_t b = (uint16_t)beatsToMs(1);  // follows "set tempo"
  auto n = [&](uint8_t midi, float beats) { push(midiToHz(midi), (uint16_t)(b * beats)); };
  switch (id) {
    case 1:  // Happy Birthday (traditional)
      n(60, .75f); n(60, .25f); n(62, 1); n(60, 1); n(65, 1); n(64, 2);
      n(60, .75f); n(60, .25f); n(62, 1); n(60, 1); n(67, 1); n(65, 2);
      n(60, .75f); n(60, .25f); n(72, 1); n(69, 1); n(65, 1); n(64, 1); n(62, 2);
      n(70, .75f); n(70, .25f); n(69, 1); n(65, 1); n(67, 1); n(65, 2);
      break;
    case 2:  // Twinkle Twinkle (traditional)
      for (uint8_t m : {60, 60, 67, 67, 69, 69}) n(m, 1);
      n(67, 2);
      for (uint8_t m : {65, 65, 64, 64, 62, 62}) n(m, 1);
      n(60, 2);
      break;
    case 3:  // victory fanfare
      n(67, .33f); n(67, .33f); n(67, .33f); n(72, 1.5f); n(0, .25f); n(71, .5f); n(72, 2);
      break;
    case 4:  // siren
      for (int i = 0; i < 4; ++i) { push(700, 300); push(1000, 300); }
      break;
    default:
      n(72, .5f);
      break;
  }
  return token_;
}

void Sound::update() {
  uint32_t now = millis();
  if (playing_ && (int32_t)(now - noteEnd_) < 0) {
    // 12 ms of silence at the end of each note keeps repeated notes distinct
    if ((int32_t)(noteEnd_ - now) <= 12) output(0);
    return;
  }
  playing_ = false;
  if (count_ == 0) {
    output(0);
    return;
  }
  Note n = q_[head_];
  head_ = (head_ + 1) % kQueue;
  count_--;
  output(n.hz);
  noteEnd_ = now + n.ms;
  playing_ = true;
}
