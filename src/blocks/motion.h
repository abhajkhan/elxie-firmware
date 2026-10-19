// Motion service: timed drives with gyro heading-hold, gyro turns, continuous
// drive, tank drive. Owns the cliff guard, speed cap and acceleration ramp.
// Every new command supersedes the previous one (new job token), which also
// releases any script that was waiting on the old job.
#pragma once
#include <stdint.h>

class Motion {
 public:
  void begin();
  uint32_t driveTimed(bool forward, float speed, float secs);
  uint32_t turn(bool left, float degrees);
  void drive(bool forward, float speed);
  void setMotors(float left, float right);
  uint32_t spin(bool left, float secs);
  uint32_t arc(bool left, float speed, float secs);
  uint32_t moveUntil(bool forward, float speed, uint8_t until);  // 1 obstacle, 2 line, 3 edge
  uint32_t followLine(float speed, float secs);                    // secs 0 = until a junction
  uint32_t joystickDrive(float secs);                              // secs 0 = until stopped
  void setBrakeOnStop(bool brake) { brakeOnStop_ = brake; }
  void stop();
  void emergencyStop();
  bool done(uint32_t token) const { return token != token_ || mode_ == IDLE || mode_ == CONTINUOUS; }
  bool lastStopWasCondition() const { return untilMet_; }
  void update(uint32_t dtMs);
  bool guardActive() const { return guardActive_; }
  float outLeft() const { return outL_; }
  float outRight() const { return outR_; }

 private:
  enum Mode : uint8_t { IDLE, TIMED, TURN, CONTINUOUS, SPIN, ARC, UNTIL, FOLLOW, JOYSTICK };
  uint32_t newJob(Mode m);
  Mode mode_ = IDLE;
  uint32_t token_ = 0;
  uint32_t endMs_ = 0;
  float speed_ = 0, cmdL_ = 0, cmdR_ = 0, outL_ = 0, outR_ = 0;
  float startYaw_ = 0, targetYaw_ = 0;
  int8_t turnSign_ = 1;
  bool guardActive_ = false;
  uint32_t brakeUntil_ = 0;
  bool brakeOnStop_ = true;
  bool timed_ = false;     // endMs_ applies (SPIN/ARC/FOLLOW/JOYSTICK)
  uint8_t until_ = 0;
  bool untilMet_ = false;
  bool arcLeft_ = false;
};

extern Motion motion;
