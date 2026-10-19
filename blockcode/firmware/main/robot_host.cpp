#include "robot_host.h"

#include <math.h>

#include "esp_log.h"
#include "esp_random.h"
#include "hal/platform.h"

#include "board_config.h"
#include "hal/sensor_hub.h"
#include "services/face.h"
#include "services/lights.h"
#include "services/motion.h"
#include "services/remote_link.h"
#include "services/sound.h"
#include "services/storage.h"

using namespace elx;
namespace en = elx::enums;

RobotHost robotHost;

static const uint32_t REACT_MS = 1200;

static uint32_t toColor(float v) { return v <= 0 ? 0 : ((uint32_t)v) & 0xFFFFFF; }
static int arg(float v) { return (int)lroundf(v); }

static uint8_t moodFace(uint8_t m) {
  switch (m) {
    case en::emotion::HAPPY: return en::face::HAPPY;
    case en::emotion::SAD: return en::face::SAD;
    case en::emotion::EXCITED: return en::face::HAPPY;
    case en::emotion::SCARED: return en::face::SURPRISED;
    case en::emotion::ANGRY: return en::face::ANGRY;
    case en::emotion::SLEEPY: return en::face::SLEEPY;
    case en::emotion::LOVE: return en::face::LOVE;
    default: return en::face::NEUTRAL;
  }
}

static uint8_t remoteBit(int b) {
  switch (b) {
    case en::button::A: return REMOTE_BTN_A;
    case en::button::B: return REMOTE_BTN_B;
    case en::button::STICK: return REMOTE_BTN_STICK;
    default: return 15;  // unused bit -> never pressed
  }
}

static float wrap180(float d) {
  while (d > 180.0f) d -= 360.0f;
  while (d < -180.0f) d += 360.0f;
  return d;
}

bool RobotHost::syscallSupported(uint8_t id) {
  if (id == SYS_SOUND_LEVEL && PIN_MIC < 0) return false;  // no microphone on this board
  return (SYSCALL_INFO[id].flags & SYSF_V1) != 0;
}
bool RobotHost::eventSupported(uint8_t ev) {
  if (ev == EV_LOUD && PIN_MIC < 0) return false;
  return ev <= EV_BATTERY_LOW;
}

uint32_t RobotHost::nowMs() { return millis(); }
uint32_t RobotHost::random32() { return esp_random(); }

// ------------------------------------------------------------------ actions
uint32_t RobotHost::startAction(uint32_t ms) {
  actionEnd_ = millis() + ms;
  return ++actionTok_;
}

void RobotHost::endAction() {
  const ActionKind k = action_;
  action_ = A_NONE;
  if (k == A_REACT || k == A_ROUTINE) {
    lights.stopEffect();
    face.look(en::look::CENTER);
    face.show(moodFace(mood_));
    if (k == A_ROUTINE) motion.stop();
  } else if (k == A_CALIBRATE) {
    sensors.endIrCalibration();
    storage::saveBlob("/ircal.bin", &sensors.calibration(), sizeof(IrCalibration));
    face.show(en::face::HAPPY);
    sound.playEffect(sfx::LEVELUP);
  }
}

uint32_t RobotHost::react(uint8_t e) {
  uint8_t fx = sfx::BEEP, effect = 2;
  uint32_t color = 0xFFFFFF;
  switch (e) {
    case en::emotion::HAPPY:   fx = sfx::HAPPY;   color = 0xFFC000; break;
    case en::emotion::SAD:     fx = sfx::SAD;     color = 0x0040FF; break;
    case en::emotion::EXCITED: fx = sfx::EXCITED; effect = 1;       break;
    case en::emotion::SCARED:  fx = sfx::SCARED;  color = 0x8000FF; effect = 3; break;
    case en::emotion::ANGRY:   fx = sfx::ANGRY;   color = 0xFF0000; effect = 3; break;
    case en::emotion::SLEEPY:  fx = sfx::SLEEPY;  color = 0x301060; break;
    case en::emotion::LOVE:    fx = sfx::LOVE;    color = 0xFF2080; break;
    default:               fx = sfx::BEEP;    color = 0x00C0FF; break;
  }
  if (action_ != A_NONE) endAction();
  face.show(moodFace(e));
  if (e == en::emotion::EXCITED) face.look(en::look::UP);
  sound.playEffect(fx);
  lights.effect(effect, color);
  action_ = A_REACT;
  return startAction(REACT_MS);
}

void RobotHost::applyStep(const Step& st) {
  if (st.left || st.right) motion.setMotors(st.left, st.right);
  else motion.stop();
  if (st.face != 0xFF) face.show(st.face);
  if (st.look != 0xFF) face.look(st.look);
  if (st.sfx) sound.playEffect(st.sfx);
  if (st.effect != 0xFF) {
    if (st.effect == 0) lights.setAll(st.color);
    else lights.effect(st.effect, st.color);
  }
}

uint32_t RobotHost::startRoutine(const Step* steps, uint8_t count) {
  if (action_ != A_NONE) endAction();
  routine_ = steps;
  routineLen_ = count;
  routineIdx_ = 0;
  action_ = A_ROUTINE;
  uint32_t total = 0;
  for (uint8_t i = 0; i < count; ++i) total += steps[i].ms;
  applyStep(steps[0]);
  const uint32_t tok = startAction(total);
  actionEnd_ = millis() + steps[0].ms;  // per-step deadline while running
  return tok;
}

namespace F = en::face;
namespace LK = en::look;
static const uint8_t K = 0xFF;
// wiggle dance
static const RobotHost::Step kDance1[] = {
    {250, -45, 45, F::HAPPY, LK::LEFT, sfx::HAPPY, 1, 0}, {250, 45, -45, K, LK::RIGHT, 0, K, 0},
    {250, -45, 45, K, LK::LEFT, 0, K, 0}, {250, 45, -45, K, LK::RIGHT, 0, K, 0},
    {250, -45, 45, K, LK::LEFT, 0, K, 0}, {250, 45, -45, K, LK::RIGHT, 0, K, 0},
    {600, 0, 0, F::LOVE, LK::CENTER, sfx::LEVELUP, 4, 0xFF2080}};
// spin party
static const RobotHost::Step kDance2[] = {
    {1200, -55, 55, F::HAPPY, LK::UP, sfx::EXCITED, 5, 0}, {1200, 55, -55, F::SURPRISED, K, sfx::EXCITED, K, 0},
    {700, 0, 0, F::HAPPY, LK::CENTER, sfx::LEVELUP, 1, 0}};
// shuffle
static const RobotHost::Step kDance3[] = {
    {300, 40, 40, F::HAPPY, LK::LEFT, sfx::BEEP, 3, 0x00C0FF}, {300, -40, -40, K, LK::RIGHT, 0, K, 0},
    {300, 40, 40, K, LK::LEFT, sfx::BEEP, K, 0}, {300, -40, -40, K, LK::RIGHT, 0, K, 0},
    {300, 40, 40, K, LK::LEFT, sfx::BEEP, K, 0}, {300, -40, -40, K, LK::RIGHT, 0, K, 0},
    {600, 0, 0, F::LOVE, LK::CENTER, sfx::HAPPY, 1, 0}};
static const RobotHost::Step kHello[] = {
    {300, 0, 0, F::HAPPY, LK::UP, sfx::CONNECT, 2, 0x00C0FF}, {220, -35, 35, K, K, 0, K, 0},
    {220, 35, -35, K, K, 0, K, 0}, {220, -35, 35, K, K, 0, K, 0}, {220, 35, -35, K, K, 0, K, 0},
    {600, 0, 0, F::HAPPY, LK::CENTER, sfx::HAPPY, K, 0}};
static const RobotHost::Step kSleepy[] = {
    {800, 0, 0, F::NEUTRAL, LK::DOWN, sfx::SLEEPY, 2, 0x301060}, {800, 0, 0, F::SLEEPY, K, 0, K, 0},
    {1600, 0, 0, F::SLEEPY, LK::CENTER, 0, K, 0}};
#define ROUTINE(a) a, (uint8_t)(sizeof(a) / sizeof(a[0]))

static uint32_t hsvColor(uint16_t hue) {  // full saturation/brightness, hue 0..359
  const uint8_t sector = (uint8_t)(hue / 60), f = (uint8_t)((hue % 60) * 255 / 60);
  uint8_t r = 0, g = 0, b = 0;
  switch (sector) {
    case 0: r = 255; g = f; break;
    case 1: r = 255 - f; g = 255; break;
    case 2: g = 255; b = f; break;
    case 3: g = 255 - f; b = 255; break;
    case 4: r = f; b = 255; break;
    default: r = 255; b = 255 - f; break;
  }
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void RobotHost::update() {
  const uint32_t now = millis();
  const SensorState& s = sensors.state();

  // actions
  if (action_ != A_NONE && (int32_t)(now - actionEnd_) >= 0) {
    if (action_ == A_ROUTINE && routineIdx_ + 1 < routineLen_) {
      ++routineIdx_;
      applyStep(routine_[routineIdx_]);
      actionEnd_ = now + routine_[routineIdx_].ms;
    } else {
      endAction();
    }
  }

  // touch gestures: count, double tap
  const bool t = s.touch[0];
  if (t && !lastTouch_) {
    touchDownAt_ = now;
    touchCount_++;
  }
  if (!t && lastTouch_) {
    if (now - touchDownAt_ < 400) {  // a tap
      if (lastTapAt_ && now - lastTapAt_ < TOUCH_DOUBLE_GAP_MS + 400) {
        doubleTapUntil_ = now + 120;
        lastTapAt_ = 0;
      } else {
        lastTapAt_ = now;
      }
    }
  }
  lastTouch_ = t;

  // picked up = every bottom sensor sees no floor for a moment
  const bool lifted = sensors.allEdges();
  if (lifted != pickedUp_) {
    if (!pickChangeSince_) pickChangeSince_ = now;
    if (now - pickChangeSince_ >= PICKUP_DEBOUNCE_MS) {
      pickedUp_ = lifted;
      pickChangeSince_ = 0;
    }
  } else {
    pickChangeSince_ = 0;
  }
}

bool RobotHost::syscall(uint8_t id, const float* a, uint8_t argc, float* r, Wait* w, const Program& prog) {
  (void)argc;
  const SensorState& s = sensors.state();
  switch (id) {
    // ---------------------------------------------------------------- motion
    case SYS_DRIVE_TIMED:
      *w = {WaitKind::MOTION, motion.driveTimed(arg(a[0]) != en::drive::BACKWARD, a[1], a[2])};
      return true;
    case SYS_TURN_DEGREES:
      *w = {WaitKind::MOTION, motion.turn(arg(a[0]) != en::turn::RIGHT, a[1])};
      return true;
    case SYS_DRIVE: motion.drive(arg(a[0]) != en::drive::BACKWARD, a[1]); return true;
    case SYS_SET_MOTORS: motion.setMotors(a[0], a[1]); return true;
    case SYS_STOP_MOTORS: motion.stop(); return true;
    case SYS_SPIN: *w = {WaitKind::MOTION, motion.spin(arg(a[0]) != en::turn::RIGHT, a[1])}; return true;
    case SYS_DRIVE_ARC: *w = {WaitKind::MOTION, motion.arc(arg(a[0]) != en::turn::RIGHT, a[1], a[2])}; return true;
    case SYS_MOVE_UNTIL:
      *w = {WaitKind::MOTION, motion.moveUntil(arg(a[0]) != en::drive::BACKWARD, a[1], (uint8_t)constrain(arg(a[2]), 1, 3))};
      return true;
    case SYS_FOLLOW_LINE: *w = {WaitKind::MOTION, motion.followLine(a[0], a[1])}; return true;
    case SYS_SET_STOP_MODE: motion.setBrakeOnStop(arg(a[0]) != en::stopmode::COAST); return true;
    case SYS_JOYSTICK_DRIVE: *w = {WaitKind::MOTION, motion.joystickDrive(a[0])}; return true;
    case SYS_TURN_TO_HEADING: {
      // Measure the compass once, then let the gyro do the turn (motors disturb the compass).
      float delta = wrap180(a[0] - s.heading);  // + = clockwise = right
      *w = {WaitKind::MOTION, motion.turn(delta < 0, fabsf(delta))};
      return true;
    }

    // ---------------------------------------------------------------- face
    case SYS_FACE_SHOW: face.show((uint8_t)constrain(arg(a[0]), 0, 7)); return true;
    case SYS_FACE_LOOK: face.look((uint8_t)constrain(arg(a[0]), 0, 4)); return true;
    case SYS_FACE_BLINK: face.blink(); return true;
    case SYS_SHOW_TEXT: {
      const char* str = programString(prog, arg(a[0]));
      if (!str) return false;
      face.text(str);
      return true;
    }
    case SYS_SHOW_NUMBER: face.number(a[0]); return true;
    case SYS_FACE_CLEAR: face.clear(); return true;
    case SYS_SHOW_ICON: face.icon((uint8_t)constrain(arg(a[0]), 0, 11)); return true;
    case SYS_FACE_WINK: *w = {WaitKind::TIME, millis() + face.wink()}; return true;
    case SYS_FACE_ANIM: *w = {WaitKind::TIME, millis() + face.animate((uint8_t)constrain(arg(a[0]), 1, 3))}; return true;
    case SYS_DRAW_PIXEL: face.drawPixel(arg(a[0]), arg(a[1])); return true;
    case SYS_DRAW_LINE: face.drawLine(arg(a[0]), arg(a[1]), arg(a[2]), arg(a[3])); return true;
    case SYS_DRAW_RECT: face.drawRect(arg(a[0]), arg(a[1]), arg(a[2]), arg(a[3]), a[4] != 0); return true;
    case SYS_DRAW_CIRCLE: face.drawCircle(arg(a[0]), arg(a[1]), arg(a[2]), a[3] != 0); return true;

    // ---------------------------------------------------------------- lights
    case SYS_LIGHTS_ALL: lights.setAll(toColor(a[0])); return true;
    case SYS_LIGHT_SET: lights.setPixel(arg(a[0]), toColor(a[1])); return true;
    case SYS_LIGHT_EFFECT: lights.effect((uint8_t)constrain(arg(a[0]), 0, 8), toColor(a[1])); return true;
    case SYS_RGB: {
      const uint32_t rr = (uint32_t)constrain(arg(a[0]), 0, 255), gg = (uint32_t)constrain(arg(a[1]), 0, 255),
                     bb = (uint32_t)constrain(arg(a[2]), 0, 255);
      *r = (float)((rr << 16) | (gg << 8) | bb);
      return true;
    }
    case SYS_RANDOM_COLOR: *r = (float)hsvColor((uint16_t)(esp_random() % 360)); return true;
    case SYS_LIGHT_BAR: lights.bar(a[0], a[1], toColor(a[2])); return true;
    case SYS_LIGHTS_BRIGHTNESS: lights.setBrightness(a[0]); return true;
    case SYS_LIGHTS_OFF: lights.off(); return true;

    // ---------------------------------------------------------------- sound
    case SYS_PLAY_NOTE: *w = {WaitKind::SOUND, sound.playNote(a[0], sound.beatsToMs(a[1]))}; return true;
    case SYS_PLAY_SOUND: {
      int id2 = arg(a[0]);
      *w = {WaitKind::SOUND, sound.playEffect((uint8_t)((id2 >= 1 && id2 <= 5) ? id2 : sfx::BEEP))};
      return true;
    }
    case SYS_REST: *w = {WaitKind::TIME, (uint32_t)(millis() + sound.beatsToMs(a[0]))}; return true;
    case SYS_SET_TEMPO: sound.setTempo(a[0]); return true;
    case SYS_PLAY_TONE: *w = {WaitKind::SOUND, sound.playTone(a[0], (uint32_t)(constrain(a[1], 0.0f, 60.0f) * 1000.0f))}; return true;
    case SYS_STOP_SOUND: sound.stop(); return true;
    case SYS_PLAY_MELODY: *w = {WaitKind::SOUND, sound.playMelody((uint8_t)arg(a[0]))}; return true;

    // ---------------------------------------------------------------- sensors
    case SYS_DISTANCE: *r = roundf(s.distanceCm * 10) / 10; return true;
    case SYS_LINE_VALUE: {
      int i = arg(a[0]);
      *r = (i >= 1 && i <= 8) ? s.line[i - 1] : 0;
      return true;
    }
    case SYS_LINE_POSITION: *r = roundf(s.linePosition); return true;
    case SYS_LINE_DETECTED: *r = sensors.lineInZone((uint8_t)arg(a[0])); return true;
    case SYS_EDGE_DETECTED: *r = sensors.edgeAt((uint8_t)arg(a[0])); return true;
    case SYS_FLOOR_DEPTH: {
      int c = arg(a[0]);
      if (c >= 1 && c <= 4) *r = s.depth[c - 1];
      else *r = max(max(s.depth[0], s.depth[1]), max(s.depth[2], s.depth[3]));
      return true;
    }
    case SYS_TILT: *r = roundf(arg(a[0]) == en::axis::ROLL ? s.roll : s.pitch); return true;
    case SYS_IS_SHAKEN: *r = s.shaken; return true;
    case SYS_IS_TOUCHED: *r = sensors.touched((uint8_t)arg(a[0])); return true;
    case SYS_SOUND_LEVEL: *r = s.soundLevel; return true;
    case SYS_TIMER: *r = roundf((millis() - timerStart_) / 100.0f) / 10.0f; return true;
    case SYS_RESET_TIMER: timerStart_ = millis(); return true;
    case SYS_HEADING: *r = roundf(s.heading) >= 360 ? 0 : roundf(s.heading); return true;
    case SYS_MAGNET_NEAR: *r = s.magOk && s.magStrength >= MAGNET_NEAR_LEVEL; return true;
    case SYS_MAGNET_STRENGTH: *r = s.magOk ? s.magStrength : 0; return true;
    case SYS_OBJECT_NEAR: *r = sensors.objectNear((uint8_t)constrain(arg(a[0]), 0, 4)); return true;
    case SYS_LINE_ALL: {
      int on = 0;
      for (int i = 0; i < 8; ++i) on += sensors.lineAt(i) ? 1 : 0;
      *r = arg(a[0]) == en::lineall::WHITE ? (on == 0) : (on == 8);
      return true;
    }
    case SYS_PICKED_UP: *r = pickedUp_; return true;
    case SYS_UPSIDE_DOWN: *r = s.imuOk && s.accelZ < -0.5f; return true;
    case SYS_TURNED_ANGLE: *r = roundf(-(s.yaw - angleZero_)); return true;  // + = turned right (clockwise)
    case SYS_RESET_ANGLE: angleZero_ = s.yaw; return true;
    case SYS_FACING: {
      const float target = (float)((constrain(arg(a[0]), 1, 4) - 1) * 90);
      *r = fabsf(wrap180(s.heading - target)) <= 45.0f;
      return true;
    }
    case SYS_TOUCH_COUNT: *r = touchCount_; return true;
    case SYS_BATTERY_LEVEL: *r = s.batteryPct; return true;
    case SYS_CALIBRATE_LINE: {
      if (action_ != A_NONE) endAction();
      sensors.beginIrCalibration();
      face.message("Calibrating", "slide over line + floor");
      action_ = A_CALIBRATE;
      *w = {WaitKind::ACTION, startAction((uint32_t)(constrain(a[0], 1.0f, 30.0f) * 1000.0f))};
      return true;
    }

    // ---------------------------------------------------------------- remote
    case SYS_JOYSTICK: *r = arg(a[0]) == en::axis::Y ? remote.joyY() : remote.joyX(); return true;
    case SYS_REMOTE_BUTTON: *r = remote.button(remoteBit(arg(a[0]))); return true;
    case SYS_REMOTE_BUZZ: remote.buzz((uint16_t)constrain(arg(a[0]), 0, 2000)); return true;

    // ---------------------------------------------------------------- pet
    case SYS_REACT: *w = {WaitKind::ACTION, react((uint8_t)arg(a[0]))}; return true;
    case SYS_SET_MOOD:
      mood_ = (uint8_t)constrain(arg(a[0]), 0, 7);
      if (action_ == A_NONE) face.show(moodFace(mood_));
      return true;
    case SYS_DANCE: {
      const int n = arg(a[0]);
      *w = {WaitKind::ACTION, n == 2 ? startRoutine(ROUTINE(kDance2)) : n == 3 ? startRoutine(ROUTINE(kDance3)) : startRoutine(ROUTINE(kDance1))};
      return true;
    }
    case SYS_SAY_HELLO: *w = {WaitKind::ACTION, startRoutine(ROUTINE(kHello))}; return true;
    case SYS_ACT_SLEEPY: *w = {WaitKind::ACTION, startRoutine(ROUTINE(kSleepy))}; return true;
    case SYS_MOOD: *r = mood_; return true;

    default:
      return false;  // validator should have rejected it
  }
}

bool RobotHost::waitDone(const Wait& w) {
  switch (w.kind) {
    case WaitKind::MOTION: return motion.done(w.value);
    case WaitKind::SOUND: return sound.done(w.value);
    case WaitKind::ACTION: return w.value != actionTok_ || action_ == A_NONE;
    default: return true;
  }
}

bool RobotHost::eventCondition(uint8_t ev, uint8_t p) {
  const SensorState& s = sensors.state();
  switch (ev) {
    case EV_TOUCHED: return sensors.touched(p);
    case EV_OBSTACLE: return s.distanceCm < (p ? p : 10);
    case EV_LINE: return sensors.lineInZone(p);
    case EV_EDGE: return sensors.edgeAt(p);
    case EV_TILTED:
      switch (p) {
        case en::tilt::FORWARD: return s.pitch > TILT_EVENT_DEG;
        case en::tilt::BACKWARD: return s.pitch < -TILT_EVENT_DEG;
        case en::tilt::LEFT: return s.roll < -TILT_EVENT_DEG;
        case en::tilt::RIGHT: return s.roll > TILT_EVENT_DEG;
        default: return fabsf(s.pitch) > TILT_EVENT_DEG || fabsf(s.roll) > TILT_EVENT_DEG;
      }
    case EV_SHAKEN: return s.shaken;
    case EV_REMOTE_BUTTON: return remote.button(remoteBit(p));
    case EV_LOUD: return s.soundLevel >= (p ? p : LOUD_DEFAULT);
    case EV_MAGNET: return s.magOk && s.magStrength >= (p ? p : MAGNET_NEAR_LEVEL);
    case EV_OBJECT_NEAR: return sensors.objectNear(p <= 4 ? p : 0);
    case EV_PICKED_UP: return pickedUp_;
    case EV_PUT_DOWN: return !pickedUp_;
    case EV_LONG_PRESS: return s.touchHeldMs[0] >= TOUCH_EVENT_LONG_MS;
    case EV_DOUBLE_TAP: return (int32_t)(doubleTapUntil_ - millis()) > 0;
    case EV_BATTERY_LOW: return s.batteryPct > 0 && s.batteryPct <= (p ? p : BATTERY_LOW_DEFAULT);
    default: return false;
  }
}

void RobotHost::onProgramStart() {
  timerStart_ = millis();
  mood_ = 0;
  action_ = A_NONE;
  touchCount_ = 0;
  angleZero_ = sensors.state().yaw;
  motion.setBrakeOnStop(true);
  face.show(en::face::NEUTRAL);
}

void RobotHost::onProgramStop() {
  if (action_ == A_CALIBRATE) sensors.endIrCalibration();  // keep what was learned, don't save
  action_ = A_NONE;
  motion.stop();
  sound.stop();
  lights.stopEffect();
}

void RobotHost::onError(VmError e, uint8_t script, uint16_t pc) {
  ESP_LOGW("vm", "runtime error %u in script %u at pc %u", (unsigned)e, script, pc);
  face.show(en::face::DIZZY);
  sound.playEffect(sfx::ERROR);
  if (errorReporter) errorReporter(e, script, pc);
}
