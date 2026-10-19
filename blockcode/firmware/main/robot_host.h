// VM host for the real robot: maps every syscall and event onto the services.
#pragma once
#include "vm.h"

class RobotHost : public elx::Host {
 public:
  struct Step {  // one step of a pet routine (dance, hello, sleepy)
    uint16_t ms;
    int8_t left, right;   // motor %, 0/0 = stop
    uint8_t face;         // 0xFF = keep
    uint8_t look;         // 0xFF = keep
    uint8_t sfx;          // 0 = none
    uint8_t effect;       // 0xFF = keep, 0 = solid
    uint32_t color;
  };
  uint32_t nowMs() override;
  uint32_t random32() override;
  bool syscall(uint8_t id, const float* args, uint8_t argc, float* result, elx::Wait* wait,
               const elx::Program& prog) override;
  bool waitDone(const elx::Wait& w) override;
  bool eventCondition(uint8_t event, uint8_t param) override;
  void onProgramStart() override;
  void onProgramStop() override;
  void onError(elx::VmError e, uint8_t script, uint16_t pc) override;

  void update();  // per tick: pet reactions/routines, touch gestures, pick-up detection
  uint8_t mood() const { return mood_; }

  // Capability checks used by the validator (what THIS firmware implements).
  static bool syscallSupported(uint8_t id);
  static bool eventSupported(uint8_t ev);

  // Set by the link layer so runtime errors reach the app.
  void (*errorReporter)(elx::VmError e, uint8_t script, uint16_t pc) = nullptr;

 private:
  uint32_t react(uint8_t emotion);
  uint32_t startRoutine(const Step* steps, uint8_t count);
  uint32_t startAction(uint32_t ms);
  void endAction();
  void applyStep(const Step& st);

  uint32_t timerStart_ = 0;
  uint8_t mood_ = 0;
  // one "action" at a time (react / routine / calibration); a newer one replaces it
  enum ActionKind : uint8_t { A_NONE, A_REACT, A_ROUTINE, A_CALIBRATE };
  ActionKind action_ = A_NONE;
  uint32_t actionTok_ = 0;
  uint32_t actionEnd_ = 0;
  const Step* routine_ = nullptr;
  uint8_t routineLen_ = 0, routineIdx_ = 0;
  // gestures and state for the new sensor blocks
  bool lastTouch_ = false;
  uint32_t touchDownAt_ = 0, lastTapAt_ = 0, doubleTapUntil_ = 0;
  uint16_t touchCount_ = 0;
  bool pickedUp_ = false;
  uint32_t pickChangeSince_ = 0;
  float angleZero_ = 0;
};

extern RobotHost robotHost;
