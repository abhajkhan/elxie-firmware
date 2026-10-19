// Elxie bytecode virtual machine: cooperative, multi-script, deterministic.
//
// - One Thread per script in the program (max LIMIT_MAX_SCRIPTS).
// - tick() is called every LIMIT_TICK_MS. Each running thread executes until it
//   blocks (wait / motion / sound), yields, takes a backward jump (loop
//   iteration) or uses LIMIT_OP_BUDGET_PER_TICK instructions.
// - Everything hardware-related goes through the Host interface, so the same
//   VM runs on the robot, in PC tests and in the browser simulator (WASM).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "opcodes_gen.h"
#include "program.h"

namespace elx {

enum class WaitKind : uint8_t { NONE = 0, TIME, MOTION, SOUND, ACTION };

struct Wait {
  WaitKind kind;
  uint32_t value;  // TIME: deadline (ms). Others: host-defined job token.
};

enum class ThreadState : uint8_t { IDLE = 0, RUNNING, WAITING };

struct Thread {
  ThreadState state;
  uint8_t sp;
  uint16_t pc;
  Wait wait;
  float stack[LIMIT_STACK_DEPTH];
};

class Host {
 public:
  virtual ~Host() {}
  virtual uint32_t nowMs() = 0;
  virtual uint32_t random32() = 0;
  // Perform syscall `id`. args[0..argc-1] are in block order.
  // If the syscall returns a value, write it to *result.
  // If it blocks, set *wait (kind + token). Return false on error.
  virtual bool syscall(uint8_t id, const float* args, uint8_t argc, float* result, Wait* wait,
                       const Program& prog) = 0;
  // Is a MOTION / SOUND / ACTION job finished (or superseded)?
  virtual bool waitDone(const Wait& w) = 0;
  // Is the condition for an event hat block true right now? (edge-detected by the VM)
  virtual bool eventCondition(uint8_t event, uint8_t param) = 0;
  virtual void onProgramStart() {}
  virtual void onProgramStop() {}
  virtual void onError(VmError e, uint8_t script, uint16_t pc) { (void)e; (void)script; (void)pc; }
};

class VM {
 public:
  explicit VM(Host& host) : host_(host) {}

  // Copies the image into internal storage and validates it. Stops any running program.
  LoadError load(const uint8_t* image, size_t len, SupportFn syscallSupported = nullptr,
                 SupportFn eventSupported = nullptr);
  bool loaded() const { return loaded_; }

  void start();
  void stop();
  bool running() const { return running_; }
  void tick();

  const Program& program() const { return prog_; }
  const Thread& thread(uint8_t i) const { return threads_[i]; }
  uint8_t numThreads() const { return loaded_ ? prog_.numScripts : 0; }
  float var(uint8_t i) const { return i < LIMIT_MAX_VARS ? vars_[i] : 0.0f; }
  uint8_t listLength(uint8_t l) const { return l < LIMIT_MAX_LISTS ? listLen_[l] : 0; }
  float listItem(uint8_t l, uint8_t i) const { return (l < LIMIT_MAX_LISTS && i < listLen_[l]) ? lists_[l][i] : 0.0f; }
  VmError lastError() const { return lastError_; }

 private:
  void startThread(uint8_t i);
  void runThread(uint8_t i);
  void fail(uint8_t i, VmError e);
  float randomBetween(float a, float b);
  bool broadcast(uint8_t msg, uint8_t self);  // true if the calling thread was restarted

  Host& host_;
  Program prog_{};
  uint8_t image_[LIMIT_MAX_PROGRAM_BYTES];
  Thread threads_[LIMIT_MAX_SCRIPTS]{};
  float vars_[LIMIT_MAX_VARS]{};
  float lists_[LIMIT_MAX_LISTS][LIMIT_LIST_CAPACITY]{};
  uint8_t listLen_[LIMIT_MAX_LISTS]{};
  bool prevCond_[LIMIT_MAX_SCRIPTS]{};
  bool loaded_ = false;
  bool running_ = false;
  uint8_t rr_ = 0;
  VmError lastError_ = VmError::NONE;
};

}  // namespace elx
