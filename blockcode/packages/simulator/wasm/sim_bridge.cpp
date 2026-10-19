// WebAssembly bridge: exposes the robot's VM (firmware/src/vm) to JavaScript.
// The virtual robot (packages/simulator/src/SimRobot.ts) implements the host
// functions declared below and is linked in with --js-library host_imports.js.
#include <stdint.h>

#include "vm.h"

using namespace elx;

extern "C" {
// ---- implemented in JavaScript (host_imports.js) ----
uint32_t js_now_ms(void);
uint32_t js_random32(void);
// Returns 0 on error. Writes *result for reporters; sets *wait_kind / *wait_value for blocking calls.
int js_syscall(uint8_t id, const float* args, uint8_t argc, float* result, uint8_t* wait_kind,
               uint32_t* wait_value, const char* strings, uint16_t strings_len);
int js_wait_done(uint8_t kind, uint32_t value);
int js_event(uint8_t event, uint8_t param);
void js_program_start(void);
void js_program_stop(void);
void js_error(uint8_t code, uint8_t script, uint16_t pc);
}

class WasmHost : public Host {
 public:
  uint32_t nowMs() override { return js_now_ms(); }
  uint32_t random32() override { return js_random32(); }
  bool syscall(uint8_t id, const float* a, uint8_t argc, float* r, Wait* w, const Program& p) override {
    uint8_t kind = 0;
    uint32_t value = 0;
    int ok = js_syscall(id, a, argc, r, &kind, &value, p.strings, p.stringsLen);
    w->kind = (WaitKind)kind;
    w->value = value;
    return ok != 0;
  }
  bool waitDone(const Wait& w) override { return js_wait_done((uint8_t)w.kind, w.value) != 0; }
  bool eventCondition(uint8_t e, uint8_t p) override { return js_event(e, p) != 0; }
  void onProgramStart() override { js_program_start(); }
  void onProgramStop() override { js_program_stop(); }
  void onError(VmError e, uint8_t s, uint16_t pc) override { js_error((uint8_t)e, s, pc); }
};

static WasmHost g_host;
static VM g_vm(g_host);
static uint8_t g_upload[LIMIT_MAX_PROGRAM_BYTES];

static bool v1Syscall(uint8_t id) { return (SYSCALL_INFO[id].flags & SYSF_V1) != 0; }
static bool v1Event(uint8_t ev) { return ev <= EV_BATTERY_LOW; }

extern "C" {
// JS writes the image into this buffer, then calls vm_load(len).
uint8_t* vm_upload_buffer(void) { return g_upload; }
int vm_load(uint32_t len) { return (int)g_vm.load(g_upload, len, v1Syscall, v1Event); }  // 0 = OK, else LoadError
void vm_start(void) { g_vm.start(); }
void vm_stop(void) { g_vm.stop(); }
void vm_tick(void) { g_vm.tick(); }
int vm_running(void) { return g_vm.running(); }
int vm_num_threads(void) { return g_vm.numThreads(); }
int vm_thread_state(int i) { return (int)g_vm.thread((uint8_t)i).state; }
int vm_thread_pc(int i) { return g_vm.thread((uint8_t)i).pc; }
int vm_num_vars(void) { return g_vm.loaded() ? g_vm.program().numVars : 0; }
float vm_var(int i) { return g_vm.var((uint8_t)i); }
int vm_tick_ms(void) { return (int)LIMIT_TICK_MS; }
}
