#include "app.h"

#include <math.h>
#include <stdio.h>

#include "platform.h"

#include "blocks_config.h"
#include "sensor_hub.h"
#include "protocol.h"
#include "robot_host.h"
#include "face.h"
#include "lights.h"
#include "motion.h"
#include "remote_link.h"
#include "sound.h"
#include "storage.h"

namespace app {

static elx::VM g_vm(robotHost);
static uint8_t g_runningSlot = 0xFF;
static bool g_wasRunning = false;

struct Settings {
  uint8_t magic = 0xE5;
  uint8_t autorun = 0xFF;
  uint8_t reserved[6] = {0};
};
static Settings g_settings;

static bool g_idleDirty = true;
static uint32_t g_menuAt = 0;  // when to show the idle screen again after a program
static uint32_t g_abHeldMs = 0;
static uint32_t g_magCalEnd = 0;

// ---- "My Programs" list on the robot (touch: tap = next, hold = run; remote: joystick + A, B = leave)
static uint8_t g_slots[elx::LIMIT_NUM_SLOTS];
static char g_labels[elx::LIMIT_NUM_SLOTS][20];
static const char* g_labelPtrs[elx::LIMIT_NUM_SLOTS];
static uint8_t g_count = 0, g_sel = 0;
static bool g_prevTouch = false;
static bool g_ignorePress = true;  // a press that began outside the list (opening it, e-stop, a program) is ignored
static bool g_longFired = false;
static bool g_launchHold = false;  // the press that started a program: not an e-stop until released
static int8_t g_lastJoy = 0;
static bool g_lastA = true, g_lastB = true;
static uint32_t g_lastInputMs = 0;
static bool g_leaveRequest = false;
static uint32_t g_liftMs = 0;    // lift / tilt while driving, for the safety stop

elx::VM& vm() { return g_vm; }
bool running() { return g_vm.running(); }
uint8_t runningSlot() { return g_vm.running() ? g_runningSlot : 0xFF; }
uint8_t autorunSlot() { return g_settings.autorun; }

bool setAutorun(uint8_t slot) {
  g_settings.autorun = slot;  // stored for the app; the Home firmware never autoruns
  return storage::saveBlob("/settings.bin", &g_settings, sizeof g_settings);
}

void slotsChanged() {
  g_count = 0;
  for (uint8_t slot = 0; slot < elx::LIMIT_NUM_SLOTS; ++slot) {
    SlotInfo in = storage::slotInfo(slot);
    if (!in.used) continue;
    g_slots[g_count] = slot;
    snprintf(g_labels[g_count], sizeof g_labels[0], "%u %.16s", (unsigned)(slot + 1), in.name[0] ? in.name : "(no name)");
    g_labelPtrs[g_count] = g_labels[g_count];
    g_count++;
  }
  if (g_sel >= g_count) g_sel = g_count ? g_count - 1 : 0;
  g_idleDirty = true;
}

uint32_t lastInputMs() { return g_lastInputMs; }
bool takeLeaveRequest() {
  bool r = g_leaveRequest;
  g_leaveRequest = false;
  return r;
}

bool runSlot(uint8_t slot, elx::LoadError* err) {
  static uint8_t buf[elx::LIMIT_MAX_PROGRAM_BYTES];
  if (g_magCalEnd) return false;  // busy calibrating
  if (err) *err = elx::LoadError::OK;
  size_t n = storage::loadSlot(slot, buf, sizeof buf);
  if (n == 0) return false;
  elx::LoadError le = g_vm.load(buf, n, RobotHost::syscallSupported, RobotHost::eventSupported);
  if (err) *err = le;
  if (le != elx::LoadError::OK) return false;
  g_runningSlot = slot;
  lights.off();
  g_vm.start();
  vmlink::notifyStatus();
  return true;
}

void stopProgram() {
  g_vm.stop();
  motion.stop();
}

void startIrCalibration() {
  if (g_vm.running()) stopProgram();
  sensors.beginIrCalibration();
  face.message("Calibrating", "slide over line + floor");
}

bool startMagCalibration() {
  if (!sensors.state().magOk) return false;
  if (g_vm.running()) stopProgram();
  sensors.beginMagCalibration();
  motion.setMotors(-MAG_CAL_SPIN_SPEED, MAG_CAL_SPIN_SPEED);  // spin in place
  face.message("Compass", "spinning...");
  g_magCalEnd = millis() + MAG_CAL_MS;
  return true;
}

bool magCalibrating() { return g_magCalEnd != 0; }

bool finishIrCalibration() {
  sensors.endIrCalibration();
  g_idleDirty = true;
  return storage::saveBlob("/ircal.bin", &sensors.calibration(), sizeof(IrCalibration));
}

void begin() {
  Settings s;
  if (storage::loadBlob("/settings.bin", &s, sizeof s) && s.magic == 0xE5) g_settings = s;
  IrCalibration cal;
  if (storage::loadBlob("/ircal.bin", &cal, sizeof cal)) sensors.calibration() = cal;
  MagCalibration mc;
  if (storage::loadBlob("/magcal.bin", &mc, sizeof mc) && mc.valid) sensors.magCalibration() = mc;
  robotHost.errorReporter = vmlink::reportError;
  slotsChanged();
}

void showIdle() {
  g_idleDirty = true;
  g_ignorePress = true;  // a press still down from opening the list is not a list action
  g_prevTouch = true;
  g_lastA = g_lastB = true;
  g_lastInputMs = millis();
}

static void drawList() {
  if (g_count == 0) {
    face.message("My Programs", vmlink::bleConnected() ? "Ready for a program" : "none yet: make one in the app");
    return;
  }
  face.menu("My Programs", g_labelPtrs, g_count, g_sel, "tap: next  hold: run");
}

void tick(uint32_t dtMs) {
  const uint32_t now = millis();
  const SensorState& s = sensors.state();

  // ---- Compass self-calibration: the robot spins in place, then saves.
  if (g_magCalEnd) {
    bool abort = (remote.button(REMOTE_BTN_A) && remote.button(REMOTE_BTN_B)) || s.touch[0];
    if (abort) {
      g_magCalEnd = 0;
      motion.stop();
      sensors.cancelMagCalibration();
      Serial.println("MAGCAL CANCELLED");
      face.message("Cancelled", "");
      g_menuAt = now + 1000;
      return;
    }
    if ((int32_t)(now - g_magCalEnd) >= 0) {
      g_magCalEnd = 0;
      motion.stop();
      bool ok = sensors.endMagCalibration() &&
                storage::saveBlob("/magcal.bin", &sensors.magCalibration(), sizeof(MagCalibration));
      const MagCalibration& mc = sensors.magCalibration();
      if (ok)
        Serial.printf("MAGCAL OK x %d..%d y %d..%d z %d..%d field %.0f\n", mc.min[0], mc.max[0], mc.min[1],
                      mc.max[1], mc.min[2], mc.max[2], mc.baseline);
      else
        Serial.println("MAGCAL FAILED (spin on a flat floor, away from metal and magnets)");
      face.message(ok ? "Compass ready" : "Try again", ok ? "" : "spin on a flat floor");
      sound.playEffect(ok ? sfx::LEVELUP : sfx::ERROR);
      g_menuAt = now + 1500;
    }
    return;
  }

  // ---- Emergency stop: hold remote A + B together, or hold the touch sensor (always active).
  bool ab = remote.button(REMOTE_BTN_A) && remote.button(REMOTE_BTN_B);
  g_abHeldMs = ab ? g_abHeldMs + dtMs : 0;
  uint32_t held = s.touchHeldMs[0];
  if (!s.touch[0]) g_launchHold = false;
  if (g_vm.running() && (g_abHeldMs >= REMOTE_STOP_HOLD_MS || (held >= TOUCH_STOP_MS && !g_launchHold))) {
    stopProgram();
    face.message("Stopped", "");
    sound.playEffect(sfx::BEEP);
  }

  // ---- Lift / tilt safety stop: tilted or upside down WHILE the wheels drive.
  const bool driving = fabsf(motion.outLeft()) > 5.0f || fabsf(motion.outRight()) > 5.0f;
  const bool tilted = s.imuOk && (fabsf(s.pitch) > LIFT_STOP_DEG || fabsf(s.roll) > LIFT_STOP_DEG || s.accelZ < -0.3f);
  g_liftMs = (g_vm.running() && driving && tilted) ? g_liftMs + dtMs : 0;
  if (g_liftMs >= LIFT_STOP_MS) {
    g_liftMs = 0;
    stopProgram();
    face.message("Put me down", "program stopped");
    sound.playEffect(sfx::ERROR);
    Serial.println("[Blocks] lift/tilt stop: program stopped");
    g_menuAt = now + 2500;
  }

  // ---- Program start/stop transitions -> status to app, return to launcher later.
  bool run = g_vm.running();
  if (run != g_wasRunning) {
    g_wasRunning = run;
    vmlink::notifyStatus();
    if (!run) g_menuAt = now + 1500;
  }
  if (run) {  // while a program runs, the robot belongs to the program
    g_prevTouch = s.touch[0];
    if (g_prevTouch) g_ignorePress = true;  // touches used by the program don't reach the list
    return;
  }

  // ---- "My Programs" list (no program running)
  if (g_menuAt && (int32_t)(now - g_menuAt) >= 0) {
    g_menuAt = 0;
    g_idleDirty = true;
  }
  bool launch = false;
  const bool touch = s.touch[0];
  const bool listLive = !g_menuAt && !sensors.calibrating();
  if (!listLive && touch) g_ignorePress = true;
  if (listLive) {
    // touch: tap = next program, hold TOUCH_LONGPRESS_MS = run it
    if (touch && !g_ignorePress && !g_longFired && held >= TOUCH_LONGPRESS_MS) {
      g_longFired = true;
      launch = true;
    }
    if (!touch && g_prevTouch && !g_ignorePress && !g_longFired && g_count) {
      g_sel = (uint8_t)((g_sel + 1) % g_count);
      g_idleDirty = true;
      sound.playEffect(sfx::BEEP);
    }
    if (touch || g_prevTouch) g_lastInputMs = now;

    // remote: joystick up/down = choose, A or stick = run, B = leave
    const int8_t jy = remote.joyY();
    const int8_t dir = jy > 60 ? 1 : (jy < -60 ? -1 : 0);
    if (dir != g_lastJoy && dir != 0 && g_count) {
      g_sel = (uint8_t)((g_sel + g_count + (dir > 0 ? -1 : 1)) % g_count);
      g_idleDirty = true;
      g_lastInputMs = now;
    }
    g_lastJoy = dir;
    const bool a = remote.button(REMOTE_BTN_A) || remote.button(REMOTE_BTN_STICK);
    const bool b = remote.button(REMOTE_BTN_B);
    if (a && !g_lastA && !b) launch = true;
    if (b && !g_lastB && !a) {
      g_leaveRequest = true;
      g_lastInputMs = now;
    }
    g_lastA = a;
    g_lastB = b;
  }
  if (!touch) {  // released: the next press is a fresh one
    g_ignorePress = false;
    g_longFired = false;
  }
  g_prevTouch = touch;
  if (launch && g_count) {
    g_lastInputMs = now;
    g_launchHold = touch;
    sound.playEffect(sfx::BEEP);
    if (!runSlot(g_slots[g_sel])) {
      face.message("Can't run", "update it in the app");
      sound.playEffect(sfx::ERROR);
      g_menuAt = now + 2000;
    }
    return;
  }
  if (g_idleDirty && !g_menuAt && !sensors.calibrating()) {
    g_idleDirty = false;
    drawList();
  }
}

}  // namespace app
