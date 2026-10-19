#include "platform.h"
#include "motion.h"

#include <math.h>

#include "motors.h"
#include "remote_link.h"
#include "sensor_hub.h"
#include "blocks_config.h"

#ifndef TURN_FALLBACK_DEG_PER_S
#define TURN_FALLBACK_DEG_PER_S 180.0f  // used only if the IMU is missing
#endif

Motion motion;

static float clampSpeed(float s) { return constrain(s, -(float)MOTOR_MAX_SPEED, (float)MOTOR_MAX_SPEED); }

void Motion::begin() { motors::begin(); }

uint32_t Motion::newJob(Mode m) {
  mode_ = m;
  return ++token_;
}

uint32_t Motion::driveTimed(bool forward, float speed, float secs) {
  speed_ = clampSpeed(fabsf(speed)) * (forward ? 1 : -1);
  startYaw_ = sensors.state().yaw;
  endMs_ = millis() + (uint32_t)(constrain(secs, 0.0f, 60.0f) * 1000.0f);
  return newJob(secs > 0 ? TIMED : IDLE);
}

uint32_t Motion::turn(bool left, float degrees) {
  degrees = constrain(degrees, -1080.0f, 1080.0f);
  if (degrees < 0) { left = !left; degrees = -degrees; }
  turnSign_ = left ? 1 : -1;  // positive yaw = counter-clockwise (left)
  startYaw_ = sensors.state().yaw;
  targetYaw_ = startYaw_ + turnSign_ * degrees;
  uint32_t est = sensors.state().imuOk ? (uint32_t)(degrees * 25.0f)
                                       : (uint32_t)(degrees / TURN_FALLBACK_DEG_PER_S * 1000.0f);
  endMs_ = millis() + max<uint32_t>(sensors.state().imuOk ? TURN_TIMEOUT_MIN_MS : 0, est);
  return newJob(degrees > 0.5f ? TURN : IDLE);
}

void Motion::drive(bool forward, float speed) {
  float s = clampSpeed(fabsf(speed)) * (forward ? 1 : -1);
  setMotors(s, s);
}

void Motion::setMotors(float left, float right) {
  cmdL_ = clampSpeed(left);
  cmdR_ = clampSpeed(right);
  newJob(CONTINUOUS);
}

uint32_t Motion::spin(bool left, float secs) {
  turnSign_ = left ? 1 : -1;
  endMs_ = millis() + (uint32_t)(constrain(secs, 0.0f, 60.0f) * 1000.0f);
  return newJob(secs > 0 ? SPIN : IDLE);
}

uint32_t Motion::arc(bool left, float speed, float secs) {
  speed_ = clampSpeed(speed);
  arcLeft_ = left;
  endMs_ = millis() + (uint32_t)(constrain(secs, 0.0f, 60.0f) * 1000.0f);
  return newJob(secs > 0 ? ARC : IDLE);
}

uint32_t Motion::moveUntil(bool forward, float speed, uint8_t until) {
  speed_ = clampSpeed(fabsf(speed)) * (forward ? 1 : -1);
  until_ = until;
  untilMet_ = false;
  startYaw_ = sensors.state().yaw;
  endMs_ = millis() + UNTIL_MAX_MS;
  return newJob(UNTIL);
}

uint32_t Motion::followLine(float speed, float secs) {
  speed_ = clampSpeed(fabsf(speed));
  endMs_ = millis() + (secs > 0 ? (uint32_t)(constrain(secs, 0.0f, 600.0f) * 1000.0f) : FOLLOW_MAX_MS);
  timed_ = secs > 0;
  return newJob(FOLLOW);
}

uint32_t Motion::joystickDrive(float secs) {
  timed_ = secs > 0;
  endMs_ = millis() + (uint32_t)(constrain(secs, 0.0f, 3600.0f) * 1000.0f);
  return newJob(JOYSTICK);
}

void Motion::stop() {
  cmdL_ = cmdR_ = 0;
  newJob(IDLE);
  if (brakeOnStop_) brakeUntil_ = millis() + 80;
}

void Motion::emergencyStop() {
  stop();
  outL_ = outR_ = 0;
  motors::brake();
}

void Motion::update(uint32_t dtMs) {
  const SensorState& s = sensors.state();
  const uint32_t now = millis();
  float tl = 0, tr = 0;

  switch (mode_) {
    case TIMED: {
      if ((int32_t)(now - endMs_) >= 0) {
        stop();
        break;
      }
      float c = s.imuOk ? HEADING_HOLD_KP * (s.yaw - startYaw_) : 0;  // drifted left -> steer right
      c = constrain(c, -25.0f, 25.0f);
      tl = speed_ + c;
      tr = speed_ - c;
      break;
    }
    case TURN: {
      bool timeUp = (int32_t)(now - endMs_) >= 0;
      if (s.imuOk) {
        float remaining = (targetYaw_ - s.yaw) * turnSign_;
        if (remaining <= TURN_EARLY_STOP_DEG || timeUp) {
          stop();
          break;
        }
        float sp = constrain(TURN_KP * remaining, TURN_MIN_SPEED, TURN_MAX_SPEED);
        tl = -sp * turnSign_;
        tr = sp * turnSign_;
      } else {
        if (timeUp) {
          stop();
          break;
        }
        tl = -50.0f * turnSign_;
        tr = 50.0f * turnSign_;
      }
      break;
    }
    case CONTINUOUS:
      tl = cmdL_;
      tr = cmdR_;
      break;
    case SPIN:
      if ((int32_t)(now - endMs_) >= 0) { stop(); break; }
      tl = -SPIN_SPEED * turnSign_;
      tr = SPIN_SPEED * turnSign_;
      break;
    case ARC: {
      if ((int32_t)(now - endMs_) >= 0) { stop(); break; }
      const float inner = speed_ * ARC_INNER_RATIO;
      tl = arcLeft_ ? inner : speed_;
      tr = arcLeft_ ? speed_ : inner;
      break;
    }
    case UNTIL: {
      bool met = false;
      switch (until_) {
        case 1: met = s.distanceCm < UNTIL_OBSTACLE_CM || (speed_ > 0 ? (s.cornerObject[0] || s.cornerObject[1]) : (s.cornerObject[2] || s.cornerObject[3])); break;
        case 2: met = sensors.lineInZone(0); break;
        case 3: met = speed_ > 0 ? sensors.frontEdge() : sensors.backEdge(); break;
      }
      if (met || (int32_t)(now - endMs_) >= 0) { untilMet_ = met; stop(); break; }
      float c = s.imuOk ? HEADING_HOLD_KP * (s.yaw - startYaw_) : 0;
      c = constrain(c, -25.0f, 25.0f);
      tl = speed_ + c;
      tr = speed_ - c;
      break;
    }
    case FOLLOW: {
      if ((int32_t)(now - endMs_) >= 0) { stop(); break; }
      if (!timed_) {  // until a junction: most of the array on the line
        int on = 0;
        for (int i = 0; i < 8; ++i) on += sensors.lineAt(i) ? 1 : 0;
        if (on >= FOLLOW_JUNCTION_N) { stop(); break; }
      }
      // line position -100 (left) .. +100 (right): steer toward it
      const float corr = FOLLOW_KP * s.linePosition / 100.0f * speed_;
      tl = constrain(speed_ + corr, -speed_, (float)MOTOR_MAX_SPEED);
      tr = constrain(speed_ - corr, -speed_, (float)MOTOR_MAX_SPEED);
      break;
    }
    case JOYSTICK: {
      if (timed_ && (int32_t)(now - endMs_) >= 0) { stop(); break; }
      const float y = remote.joyY(), x = remote.joyX();
      tl = clampSpeed(y + x * 0.6f);
      tr = clampSpeed(y - x * 0.6f);
      break;
    }
    case IDLE:
      break;
  }

  // Cliff guard: never drive a wheel toward a detected drop.
  guardActive_ = false;
#if CLIFF_GUARD
  if (sensors.frontEdge() && (tl > 0 || tr > 0)) {
    guardActive_ = true;
    tl = min(tl, 0.0f);
    tr = min(tr, 0.0f);
  }
  if (sensors.backEdge() && (tl < 0 || tr < 0)) {
    guardActive_ = true;
    tl = max(tl, 0.0f);
    tr = max(tr, 0.0f);
  }
  if (guardActive_ && (mode_ == TIMED || mode_ == ARC)) stop();  // finish the block so the program continues
#endif

  // Ramp toward target (stopping is immediate).
  const float step = MOTOR_RAMP_PER_S * dtMs / 1000.0f;
  auto ramp = [&](float out, float target) {
    if (target == 0) return 0.0f;
    if ((out > 0 && target < 0) || (out < 0 && target > 0)) out = 0;
    if (target > out) return min(target, out + step);
    return max(target, out - step);
  };
  outL_ = ramp(outL_, tl);
  outR_ = ramp(outR_, tr);

  if (outL_ == 0 && outR_ == 0 && (int32_t)(brakeUntil_ - now) > 0) motors::brake();
  else motors::write(outL_, outR_);
}
