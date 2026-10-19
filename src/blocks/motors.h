// Low-level dual H-bridge driver for the block engine (TB6612FNG).
#pragma once

namespace motors {
void begin();
// -100..100 %. Applies inversion and the dead-zone lift (MOTOR_MIN_DUTY).
void write(float leftPct, float rightPct);
void brake();
void release();  // coast + driver off (hand the motors back to the games)
}  // namespace motors
