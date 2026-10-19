#include "app.h"

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

elx::VM& vm() { return g_vm; }
bool running() { return g_vm.running(); }
uint8_t runningSlot() { return g_vm.running() ? g_runningSlot : 0xFF; }
uint8_t autorunSlot() { return g_settings.autorun; }

bool setAutorun(uint8_t slot) {
  g_settings.autorun = slot;  // stored for the app; the Home firmware never autoruns
  return storage::saveBlob("/settings.bin", &g_settings, sizeof g_settings);
}

void slotsChanged() { g_idleDirty = true; }

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
}

void showIdle() { g_idleDirty = true; }

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
      face.message("Cancelled", "");
      g_menuAt = now + 1000;
      return;
    }
    if ((int32_t)(now - g_magCalEnd) >= 0) {
      g_magCalEnd = 0;
      motion.stop();
      bool ok = sensors.endMagCalibration() &&
                storage::saveBlob("/magcal.bin", &sensors.magCalibration(), sizeof(MagCalibration));
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
  if (g_vm.running() && (g_abHeldMs >= REMOTE_STOP_HOLD_MS || held >= TOUCH_STOP_MS)) {
    stopProgram();
    face.message("Stopped", "");
    sound.playEffect(sfx::BEEP);
  }

  // ---- Program start/stop transitions -> status to app, return to launcher later.
  bool run = g_vm.running();
  if (run != g_wasRunning) {
    g_wasRunning = run;
    vmlink::notifyStatus();
    if (!run) g_menuAt = now + 1500;
  }
  if (run) return;  // while a program runs, the robot belongs to the program

  // ---- Idle screen (no program running)
  if (g_menuAt && (int32_t)(now - g_menuAt) >= 0) {
    g_menuAt = 0;
    g_idleDirty = true;
  }
  if (g_idleDirty && !g_menuAt && !sensors.calibrating()) {
    g_idleDirty = false;
    face.message("Blocks", vmlink::bleConnected() ? "Ready for a program" : "Connect the app");
  }
}

}  // namespace app
