#include "vm.h"

#include <math.h>
#include <string.h>

namespace elx {

static inline float rdf32(const uint8_t* p) {
  float f;
  memcpy(&f, p, 4);  // image is little-endian; so are ESP32, x86 and WASM
  return f;
}
static inline uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline bool truthy(float v) { return v != 0.0f; }
static inline bool isInt(float v) { return v == floorf(v); }

LoadError VM::load(const uint8_t* image, size_t len, SupportFn sysOk, SupportFn evOk) {
  stop();
  loaded_ = false;
  if (len > sizeof(image_)) return LoadError::BAD_LENGTH;
  memcpy(image_, image, len);
  LoadError e = parseProgram(image_, len, prog_, sysOk, evOk);
  loaded_ = (e == LoadError::OK);
  return e;
}

void VM::start() {
  if (!loaded_) return;
  stop();
  memset(vars_, 0, sizeof(vars_));
  memset(threads_, 0, sizeof(threads_));
  memset(prevCond_, 0, sizeof(prevCond_));
  memset(listLen_, 0, sizeof(listLen_));
  lastError_ = VmError::NONE;
  running_ = true;
  rr_ = 0;
  host_.onProgramStart();
  for (uint8_t i = 0; i < prog_.numScripts; ++i)
    if (prog_.scripts[i].event == EV_START) startThread(i);
}

void VM::stop() {
  if (!running_) return;
  running_ = false;
  for (auto& t : threads_) t.state = ThreadState::IDLE;
  host_.onProgramStop();
}

void VM::startThread(uint8_t i) {
  Thread& t = threads_[i];
  t.state = ThreadState::RUNNING;
  t.pc = prog_.scripts[i].offset;
  t.sp = 0;
  t.wait = {WaitKind::NONE, 0};
}

void VM::fail(uint8_t i, VmError e) {
  lastError_ = e;
  const uint16_t pc = threads_[i].pc;
  stop();  // stop first so the host's error feedback (face/sound) isn't cancelled
  host_.onError(e, i, pc);
}

bool VM::broadcast(uint8_t msg, uint8_t self) {
  bool restartedSelf = false;
  for (uint8_t i = 0; i < prog_.numScripts; ++i) {
    if (prog_.scripts[i].event == EV_MESSAGE && prog_.scripts[i].param == msg) {
      startThread(i);  // Scratch semantics: a running receiver restarts
      if (i == self) restartedSelf = true;
    }
  }
  return restartedSelf;
}

float VM::randomBetween(float a, float b) {
  float lo = a < b ? a : b, hi = a < b ? b : a;
  uint32_t r = host_.random32();
  if (isInt(lo) && isInt(hi)) {
    uint32_t span = (uint32_t)(int32_t)(hi - lo) + 1u;
    return lo + (float)(r % span);
  }
  return lo + (hi - lo) * ((float)(r >> 8) / 16777216.0f);
}

void VM::tick() {
  if (!running_) return;

  // 1. Event hat blocks fire on a false -> true edge; ignored if that script is still running.
  bool hasEventScripts = false;
  for (uint8_t i = 0; i < prog_.numScripts; ++i) {
    const ScriptEntry& s = prog_.scripts[i];
    if (s.event == EV_START) continue;
    hasEventScripts = true;
    bool c = host_.eventCondition(s.event, s.param);
    if (c && !prevCond_[i] && threads_[i].state == ThreadState::IDLE) startThread(i);
    prevCond_[i] = c;
  }

  // 2. Run threads round-robin (rotating start for fairness on shared outputs).
  const uint8_t n = prog_.numScripts;
  const uint32_t now = host_.nowMs();
  for (uint8_t k = 0; k < n && running_; ++k) {
    const uint8_t i = (uint8_t)((rr_ + k) % n);
    Thread& t = threads_[i];
    if (t.state == ThreadState::WAITING) {
      bool done = (t.wait.kind == WaitKind::TIME) ? (int32_t)(now - t.wait.value) >= 0
                                                   : host_.waitDone(t.wait);
      if (!done) continue;
      t.state = ThreadState::RUNNING;
      t.wait = {WaitKind::NONE, 0};
    }
    if (t.state == ThreadState::RUNNING) runThread(i);
  }
  rr_ = (uint8_t)((rr_ + 1) % (n ? n : 1));
  if (!running_) return;

  // 3. A program with no event scripts ends when every script has finished.
  if (!hasEventScripts) {
    for (uint8_t i = 0; i < n; ++i)
      if (threads_[i].state != ThreadState::IDLE) return;
    stop();
  }
}

#define NEED(k)                                   \
  do {                                            \
    if (t.sp < (k)) { t.pc = pc; return fail(i, VmError::STACK_UNDERFLOW); } \
  } while (0)
#define PUSH(v)                                   \
  do {                                            \
    if (t.sp >= LIMIT_STACK_DEPTH) { t.pc = pc; return fail(i, VmError::STACK_OVERFLOW); } \
    const float pushv_ = (v);                     \
    t.stack[t.sp++] = pushv_;                     \
  } while (0)
#define BINOP(expr)                               \
  do {                                            \
    NEED(2);                                      \
    float b = t.stack[--t.sp];                    \
    float a = t.stack[t.sp - 1];                  \
    t.stack[t.sp - 1] = (expr);                   \
  } while (0)
#define UNOP(expr)                                \
  do {                                            \
    NEED(1);                                      \
    float a = t.stack[t.sp - 1];                  \
    t.stack[t.sp - 1] = (expr);                   \
  } while (0)

void VM::runThread(uint8_t i) {
  Thread& t = threads_[i];
  const uint8_t* code = prog_.code;
  for (uint32_t ops = 0; ops < LIMIT_OP_BUDGET_PER_TICK; ++ops) {
    if (t.pc >= prog_.codeLen) {  // fell off the end = END
      t.state = ThreadState::IDLE;
      return;
    }
    const uint16_t pc = t.pc;
    const uint8_t op = code[pc];
    const uint8_t* arg = code + pc + 1;
    t.pc = (uint16_t)(pc + OP_LENGTH[op]);

    switch (op) {
      case OP_NOP: break;
      case OP_PUSH_I8: PUSH((float)(int8_t)arg[0]); break;
      case OP_PUSH_I16: PUSH((float)(int16_t)rd16(arg)); break;
      case OP_PUSH_F32: PUSH(rdf32(arg)); break;
      case OP_POP: NEED(1); --t.sp; break;
      case OP_DUP: NEED(1); PUSH(t.stack[t.sp - 1]); break;

      case OP_LOAD_VAR: PUSH(vars_[arg[0]]); break;
      case OP_STORE_VAR: NEED(1); vars_[arg[0]] = t.stack[--t.sp]; break;
      case OP_CHANGE_VAR: NEED(1); vars_[arg[0]] += t.stack[--t.sp]; break;

      case OP_ADD: BINOP(a + b); break;
      case OP_SUB: BINOP(a - b); break;
      case OP_MUL: BINOP(a * b); break;
      case OP_DIV: BINOP(b == 0.0f ? 0.0f : a / b); break;
      case OP_MOD: {
        NEED(2);
        float b = t.stack[--t.sp];
        float a = t.stack[t.sp - 1];
        float r = 0.0f;
        if (b != 0.0f) {
          r = fmodf(a, b);
          if (r != 0.0f && ((r < 0.0f) != (b < 0.0f))) r += b;
        }
        t.stack[t.sp - 1] = r;
        break;
      }
      case OP_NEG: UNOP(-a); break;
      case OP_ABS: UNOP(fabsf(a)); break;
      case OP_ROUND: UNOP(roundf(a)); break;
      case OP_RANDOM: {
        NEED(2);
        float b = t.stack[--t.sp];
        float a = t.stack[t.sp - 1];
        t.stack[t.sp - 1] = randomBetween(a, b);
        break;
      }

      case OP_LT: BINOP(a < b ? 1.0f : 0.0f); break;
      case OP_GT: BINOP(a > b ? 1.0f : 0.0f); break;
      case OP_EQ: {
        NEED(2);
        float b = t.stack[--t.sp];
        float a = t.stack[t.sp - 1];
        float m = fmaxf(1.0f, fmaxf(fabsf(a), fabsf(b)));
        t.stack[t.sp - 1] = fabsf(a - b) <= 1e-6f * m ? 1.0f : 0.0f;
        break;
      }
      case OP_LE: BINOP(a <= b ? 1.0f : 0.0f); break;
      case OP_GE: BINOP(a >= b ? 1.0f : 0.0f); break;
      case OP_NE: BINOP(a != b ? 1.0f : 0.0f); break;
      case OP_AND: BINOP(truthy(a) && truthy(b) ? 1.0f : 0.0f); break;
      case OP_OR: BINOP(truthy(a) || truthy(b) ? 1.0f : 0.0f); break;
      case OP_NOT: UNOP(truthy(a) ? 0.0f : 1.0f); break;

      case OP_JMP: {
        uint16_t target = rd16(arg);
        t.pc = target;
        if (target <= pc) return;  // loop iteration: yield until next tick
        break;
      }
      case OP_JZ: {
        NEED(1);
        if (!truthy(t.stack[--t.sp])) {
          uint16_t target = rd16(arg);
          t.pc = target;
          if (target <= pc) return;
        }
        break;
      }
      case OP_JNZ: {
        NEED(1);
        if (truthy(t.stack[--t.sp])) {
          uint16_t target = rd16(arg);
          t.pc = target;
          if (target <= pc) return;
        }
        break;
      }
      case OP_YIELD: return;
      case OP_END: t.state = ThreadState::IDLE; return;
      case OP_STOP_ALL: stop(); return;
      case OP_WAIT: {
        NEED(1);
        float secs = t.stack[--t.sp];
        if (secs > 0.0f) {
          t.state = ThreadState::WAITING;
          t.wait = {WaitKind::TIME, host_.nowMs() + (uint32_t)(secs * 1000.0f + 0.5f)};
        }
        return;  // a wait always yields, even wait(0)
      }

      case OP_LIST_ADD: {
        NEED(1);
        const uint8_t l = arg[0];
        const float v = t.stack[--t.sp];
        if (listLen_[l] < LIMIT_LIST_CAPACITY) lists_[l][listLen_[l]++] = v;
        break;
      }
      case OP_LIST_GET: {
        NEED(1);
        const int idx = (int)lroundf(t.stack[t.sp - 1]) - 1;
        t.stack[t.sp - 1] = (idx >= 0 && idx < listLen_[arg[0]]) ? lists_[arg[0]][idx] : 0.0f;
        break;
      }
      case OP_LIST_SET: {
        NEED(2);
        const float v = t.stack[--t.sp];
        const int idx = (int)lroundf(t.stack[--t.sp]) - 1;
        if (idx >= 0 && idx < listLen_[arg[0]]) lists_[arg[0]][idx] = v;
        break;
      }
      case OP_LIST_LEN: PUSH((float)listLen_[arg[0]]); break;
      case OP_LIST_CLEAR: listLen_[arg[0]] = 0; break;
      case OP_LIST_DELETE: {
        NEED(1);
        const uint8_t l = arg[0];
        const int idx = (int)lroundf(t.stack[--t.sp]) - 1;
        if (idx >= 0 && idx < listLen_[l]) {
          for (int k = idx; k + 1 < listLen_[l]; ++k) lists_[l][k] = lists_[l][k + 1];
          listLen_[l]--;
        }
        break;
      }
      case OP_LIST_INSERT: {
        NEED(2);
        const uint8_t l = arg[0];
        const float v = t.stack[--t.sp];
        const int idx = (int)lroundf(t.stack[--t.sp]) - 1;
        if (idx >= 0 && idx <= listLen_[l] && listLen_[l] < LIMIT_LIST_CAPACITY) {
          for (int k = listLen_[l]; k > idx; --k) lists_[l][k] = lists_[l][k - 1];
          lists_[l][idx] = v;
          listLen_[l]++;
        }
        break;
      }
      case OP_LIST_CONTAINS: {
        NEED(1);
        const float v = t.stack[t.sp - 1];
        float found = 0.0f;
        for (int k = 0; k < listLen_[arg[0]]; ++k)
          if (fabsf(lists_[arg[0]][k] - v) <= 1e-6f * fmaxf(1.0f, fabsf(v))) { found = 1.0f; break; }
        t.stack[t.sp - 1] = found;
        break;
      }

      case OP_SYS: {
        const uint8_t id = arg[0];
        if (id == SYS_BROADCAST) {  // handled by the VM: starts "when I receive" scripts
          NEED(1);
          const uint8_t msg = (uint8_t)lroundf(t.stack[--t.sp]);
          if (broadcast(msg, i)) return;  // this thread restarted itself
          break;
        }
        const SyscallInfo& info = SYSCALL_INFO[id];
        NEED(info.argc);
        t.sp = (uint8_t)(t.sp - info.argc);
        float result = 0.0f;
        Wait w{WaitKind::NONE, 0};
        if (!host_.syscall(id, &t.stack[t.sp], info.argc, &result, &w, prog_))
          { t.pc = pc; return fail(i, VmError::BAD_SYSCALL); }
        if (!running_) return;  // syscall may have stopped the program
        if (info.flags & SYSF_RETURNS) PUSH(result);
        if (w.kind != WaitKind::NONE) {
          t.state = ThreadState::WAITING;
          t.wait = w;
          return;
        }
        break;
      }

      default:
        return fail(i, VmError::BAD_OPCODE);  // unreachable after validation
    }
  }
  // Budget exhausted: stay RUNNING, continue next tick.
}

}  // namespace elx
