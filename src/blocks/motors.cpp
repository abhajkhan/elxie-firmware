#include "motors.h"

#include <math.h>

#include "../config/BoardConfig.h"
#include "blocks_config.h"
#include "platform.h"

// The game code's TB6612FNG wiring (src/config/BoardConfig.h, RobotController):
//   motor B = LEFT wheel, motor A = RIGHT wheel, xIN1 high = forward, unless
//   MOTOR_A_INVERT / MOTOR_B_INVERT in BoardConfig.h flips that wheel.
// PWM goes through analogWrite(), exactly like MotorController, so blocks and
// games use the same LEDC channels.
//   IN1=1 IN2=0 forward · IN1=0 IN2=1 reverse · IN1=IN2=1 short brake · PWM=0 coast
namespace motors {

struct Motor { int pwm, in1, in2; bool invert; };
static const Motor kLeft = {PIN_MOTOR_L_PWM, PIN_MOTOR_L_IN1, PIN_MOTOR_L_IN2, MOTOR_B_INVERT != 0};
static const Motor kRight = {PIN_MOTOR_R_PWM, PIN_MOTOR_R_IN1, PIN_MOTOR_R_IN2, MOTOR_A_INVERT != 0};
static const int kStby = PIN_MOTOR_STBY;

static void drive(const Motor& m, float pct) {
  float mag = fabsf(pct);
  if (mag < 1.0f) {  // coast
    analogWrite(m.pwm, 0);
    return;
  }
  if (mag > 100.0f) mag = 100.0f;
  // Map 1..100 % onto MIN_DUTY..1.0 so low speeds still move the wheels.
  const float frac = MOTOR_MIN_DUTY + (1.0f - MOTOR_MIN_DUTY) * (mag / 100.0f);
  const bool fwd = (pct > 0) != m.invert;
  digitalWrite(m.in1, fwd ? HIGH : LOW);
  digitalWrite(m.in2, fwd ? LOW : HIGH);
  analogWrite(m.pwm, (int)(frac * 255.0f + 0.5f));
}

static void brakeOne(const Motor& m) {
  digitalWrite(m.in1, HIGH);
  digitalWrite(m.in2, HIGH);
  analogWrite(m.pwm, 255);
}

void begin() {
  // MotorController::begin() already set the pins up; just make sure we're stopped.
  analogWrite(kLeft.pwm, 0);
  analogWrite(kRight.pwm, 0);
}

void write(float leftPct, float rightPct) {
  digitalWrite(kStby, HIGH);
  drive(kLeft, leftPct);
  drive(kRight, rightPct);
}

void release() {
  analogWrite(kLeft.pwm, 0);
  analogWrite(kRight.pwm, 0);
  digitalWrite(kStby, LOW);  // same as MotorController::stop()
}

void brake() {
  digitalWrite(kStby, HIGH);
  brakeOne(kLeft);
  brakeOne(kRight);
}

}  // namespace motors
