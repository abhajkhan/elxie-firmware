// Low-level dual H-bridge driver (two PWM inputs per motor).
#pragma once

namespace motors {
void begin();
// -100..100 %. Applies inversion and the dead-zone lift (MOTOR_MIN_DUTY).
void write(float leftPct, float rightPct);
void brake();
}  // namespace motors
