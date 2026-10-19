// elx_sim: runs an .elxb image on the real VM core with a simulated robot and
// prints a deterministic trace. Used for golden tests (tests/run_tests.sh) and
// as the reference for the app's WASM simulator host.
//
//   elx_sim prog.elxb [--ms 20000] [--stim touched.1@500:700] [--sensor distance=25]
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "vm.h"

using namespace elx;

static const char* sysName(uint8_t id) { return SYSCALL_NAMES[id] ? SYSCALL_NAMES[id] : "?"; }
static const char* evName(uint8_t ev) { return ev < 32 && EVENT_NAMES[ev] ? EVENT_NAMES[ev] : "?"; }

struct Stim { std::string key; uint32_t from, to; };

class SimHost : public Host {
 public:
  uint32_t now = 0;
  uint32_t rng = 12345;
  std::vector<Stim> stims;
  std::map<std::string, float> sensors;
  uint32_t motionTok = 0, motionEnd = 0, soundTok = 0, soundEnd = 0, actTok = 0, actEnd = 0;
  float tempo = 120, mood = 0;
  uint32_t timerStart = 0;

  uint32_t nowMs() override { return now; }
  uint32_t random32() override { rng = rng * 1103515245u + 12345u; return rng >> 1; }

  void log(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    printf("[%6u] ", now);
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    printf("\n");
  }

  bool syscall(uint8_t id, const float* a, uint8_t argc, float* r, Wait* w, const Program& p) override {
    char args[128] = {0};
    for (uint8_t k = 0; k < argc; ++k) {
      char b[24];
      snprintf(b, sizeof b, "%s%.9g", k ? ", " : "", (double)a[k]);
      strcat(args, b);
    }
    if (id == SYS_SHOW_TEXT) {
      const char* s = programString(p, (int32_t)a[0]);
      if (!s) return false;
      log("show_text(\"%s\")", s);
      return true;
    }
    auto motion = [&](uint32_t ms) { motionTok++; motionEnd = now + ms; *w = {WaitKind::MOTION, motionTok}; };
    auto sound = [&](uint32_t ms) { soundTok++; soundEnd = now + ms; *w = {WaitKind::SOUND, soundTok}; };
    switch (id) {
      case SYS_DRIVE_TIMED: motion((uint32_t)(a[2] * 1000)); break;
      case SYS_TURN_DEGREES: motion((uint32_t)(fabsf(a[1]) / 180.0f * 1000)); break;  // 180 deg/s
      case SYS_TURN_TO_HEADING: {
        float h = sensors.count("heading") ? sensors["heading"] : 0.0f;
        float d = fmodf(a[0] - h + 540.0f, 360.0f) - 180.0f;  // shortest way
        motion((uint32_t)(fabsf(d) / 180.0f * 1000));
        sensors["heading"] = fmodf(a[0] + 360.0f, 360.0f);
        break;
      }
      case SYS_DRIVE: case SYS_SET_MOTORS: case SYS_STOP_MOTORS: motionTok++; motionEnd = now; break;
      case SYS_PLAY_NOTE: sound((uint32_t)(a[1] * 60000.0f / tempo)); break;
      case SYS_PLAY_SOUND: sound(600); break;
      case SYS_REST: *w = {WaitKind::TIME, now + (uint32_t)(a[0] * 60000.0f / tempo)}; break;
      case SYS_SET_TEMPO: tempo = a[0] < 20 ? 20 : a[0]; break;
      case SYS_REACT: actTok++; actEnd = now + 1200; *w = {WaitKind::ACTION, actTok}; break;
      case SYS_SET_MOOD: mood = a[0]; break;
      case SYS_MOOD: *r = mood; break;
      case SYS_TIMER: *r = (now - timerStart) / 1000.0f; break;
      case SYS_RESET_TIMER: timerStart = now; break;
      default:
        if (SYSCALL_INFO[id].flags & SYSF_RETURNS) {
          auto it = sensors.find(sysName(id));
          *r = it == sensors.end() ? 0.0f : it->second;
        }
    }
    if (SYSCALL_INFO[id].flags & SYSF_RETURNS) log("%s(%s) -> %.9g", sysName(id), args, (double)*r);
    else log("%s(%s)", sysName(id), args);
    return true;
  }

  bool waitDone(const Wait& w) override {
    switch (w.kind) {
      case WaitKind::MOTION: return w.value != motionTok || (int32_t)(now - motionEnd) >= 0;
      case WaitKind::SOUND: return w.value != soundTok || (int32_t)(now - soundEnd) >= 0;
      case WaitKind::ACTION: return w.value != actTok || (int32_t)(now - actEnd) >= 0;
      default: return true;
    }
  }

  bool eventCondition(uint8_t ev, uint8_t param) override {
    char key[40];
    snprintf(key, sizeof key, "%s.%u", evName(ev), param);
    for (auto& s : stims) {
      if (now < s.from || now >= s.to) continue;
      if (s.key == key || s.key == evName(ev)) return true;
    }
    return false;
  }

  void onProgramStart() override { timerStart = now; log("program start"); }
  void onProgramStop() override { log("program stop"); motionTok++; soundTok++; actTok++; }
  void onError(VmError e, uint8_t script, uint16_t pc) override {
    log("RUNTIME ERROR %u in script %u at pc %u", (unsigned)e, script, pc);
  }
};

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s prog.elxb [--ms N] [--stim event.param@from:to] [--sensor name=v]\n", argv[0]);
    return 2;
  }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { perror(argv[1]); return 2; }
  static uint8_t buf[8192];
  size_t n = fread(buf, 1, sizeof buf, f);
  fclose(f);

  static SimHost host;
  uint32_t maxMs = 20000;
  for (int i = 2; i < argc; ++i) {
    if (!strcmp(argv[i], "--ms") && i + 1 < argc) maxMs = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--stim") && i + 1 < argc) {
      std::string s = argv[++i];
      size_t at = s.find('@'), colon = s.find(':');
      host.stims.push_back({s.substr(0, at), (uint32_t)atoi(s.c_str() + at + 1),
                            (uint32_t)atoi(s.c_str() + colon + 1)});
    } else if (!strcmp(argv[i], "--sensor") && i + 1 < argc) {
      std::string s = argv[++i];
      size_t eq = s.find('=');
      host.sensors[s.substr(0, eq)] = (float)atof(s.c_str() + eq + 1);
    }
  }

  static VM vm(host);
  LoadError le = vm.load(buf, n, [](uint8_t id) { return (SYSCALL_INFO[id].flags & SYSF_V1) != 0; },
                         [](uint8_t ev) { return ev <= EV_BATTERY_LOW; });
  if (le != LoadError::OK) {
    printf("LOAD ERROR %u\n", (unsigned)le);
    return 1;
  }
  printf("loaded '%s': %u scripts, %u vars, %u code bytes\n", vm.program().name, vm.program().numScripts,
         vm.program().numVars, vm.program().codeLen);
  if (vm.program().numLists) printf("lists: %u\n", vm.program().numLists);
  vm.start();
  while (vm.running() && host.now < maxMs) {
    vm.tick();
    host.now += LIMIT_TICK_MS;
  }
  if (vm.running()) printf("[%6u] (still running at time limit)\n", host.now);
  return 0;
}
