#include "app.h"

#include <stdio.h>

#include "hal/platform.h"

#include "board_config.h"
#include "hal/sensor_hub.h"
#include "link/protocol.h"
#include "robot_host.h"
#include "services/face.h"
#include "services/lights.h"
#include "services/motion.h"
#include "services/remote_link.h"
#include "services/sound.h"
#include "services/storage.h"

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

// Launcher state
static uint8_t g_slots[elx::LIMIT_NUM_SLOTS];
static char g_labels[elx::LIMIT_NUM_SLOTS][18];
static const char* g_labelPtrs[elx::LIMIT_NUM_SLOTS];
static uint8_t g_count = 0, g_sel = 0;
static bool g_menuDirty = true;
static uint32_t g_menuAt = 0;       // when to show the launcher again after a program
static int8_t g_lastJoy = 0;
static bool g_lastA = false;
static bool g_wasTouch = false;
static bool g_longFired = false;
static bool g_lastBle = false, g_lastRemote = false;
static uint32_t g_abHeldMs = 0;
static uint32_t g_magCalEnd = 0;

elx::VM& vm() { return g_vm; }
bool running() { return g_vm.running(); }
uint8_t runningSlot() { return g_vm.running() ? g_runningSlot : 0xFF; }
uint8_t autorunSlot() { return g_settings.autorun; }

bool setAutorun(uint8_t slot) {
  g_settings.autorun = slot;
  g_menuDirty = true;
  return storage::saveBlob("/settings.bin", &g_settings, sizeof g_settings);
}

void slotsChanged() {
  g_count = 0;
  for (uint8_t s = 0; s < elx::LIMIT_NUM_SLOTS; ++s) {
    SlotInfo in = storage::slotInfo(s);
    if (!in.used) continue;
    g_slots[g_count] = s;
    snprintf(g_labels[g_count], sizeof g_labels[0], "%c%u %.14s", g_settings.autorun == s ? '*' : ' ', (unsigned)(s % 10),
             in.name[0] ? in.name : "(untitled)");
    g_labelPtrs[g_count] = g_labels[g_count];
    g_count++;
  }
  if (g_sel >= g_count) g_sel = g_count ? g_count - 1 : 0;
  g_menuDirty = true;
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
  link::notifyStatus();
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
  g_menuDirty = true;
  return storage::saveBlob("/ircal.bin", &sensors.calibration(), sizeof(IrCalibration));
}

void begin() {
  Settings s;
  if (storage::loadBlob("/settings.bin", &s, sizeof s) && s.magic == 0xE5) g_settings = s;
  IrCalibration cal;
  if (storage::loadBlob("/ircal.bin", &cal, sizeof cal)) sensors.calibration() = cal;
  MagCalibration mc;
  if (storage::loadBlob("/magcal.bin", &mc, sizeof mc) && mc.valid) sensors.magCalibration() = mc;
  robotHost.errorReporter = link::reportError;
  slotsChanged();
  if (g_settings.autorun != 0xFF) runSlot(g_settings.autorun);
}

static void showLauncher() {
  char footer[24];
  snprintf(footer, sizeof footer, "%s  %s", link::bleConnected() ? "BLE:on" : "BLE:--",
           remote.connected() ? "RC:on" : "RC:--");
  if (sensors.calibrating()) return;
  if (g_count == 0) face.message("Elxie Code", link::bleConnected() ? "Ready for a program" : "Connect the app");
  else face.menu("Tricks", g_labelPtrs, g_count, g_sel, footer);
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
  bool touch = s.touch[0];
  uint32_t held = s.touchHeldMs[0];
  if (g_vm.running() && (g_abHeldMs >= REMOTE_STOP_HOLD_MS || held >= TOUCH_STOP_MS)) {
    stopProgram();
    face.message("Stopped", "");
    sound.playEffect(sfx::BEEP);
    g_longFired = true;  // don't also treat this press as a launcher action
  }

  // ---- Program start/stop transitions -> status to app, return to launcher later.
  bool run = g_vm.running();
  if (run != g_wasRunning) {
    g_wasRunning = run;
    link::notifyStatus();
    if (!run) g_menuAt = now + 1500;
  }
  if (run) {
    g_wasTouch = touch;
    return;  // while a program runs, the robot belongs to the program
  }

  // ---- Launcher (idle only)
  bool ble = link::bleConnected(), rc = remote.connected();
  if (ble != g_lastBle || rc != g_lastRemote) {
    if (ble && !g_lastBle) sound.playEffect(sfx::CONNECT);
    g_lastBle = ble;
    g_lastRemote = rc;
    g_menuDirty = true;
  }
  if (g_menuAt && (int32_t)(now - g_menuAt) >= 0) {
    g_menuAt = 0;
    g_menuDirty = true;
  }

  // joystick up/down with edge detection
  int8_t jy = remote.joyY();
  int8_t dir = jy > 60 ? 1 : (jy < -60 ? -1 : 0);
  if (dir != g_lastJoy && dir != 0 && g_count) {
    g_sel = (uint8_t)((g_sel + g_count + (dir > 0 ? -1 : 1)) % g_count);
    g_menuDirty = true;
  }
  g_lastJoy = dir;

  bool a = remote.button(REMOTE_BTN_A) || remote.button(REMOTE_BTN_STICK);
  bool launch = a && !g_lastA && !remote.button(REMOTE_BTN_B);
  g_lastA = a;

  // touch sensor: tap = next, long press = run
  if (touch && held >= TOUCH_LONGPRESS_MS && !g_longFired) {
    g_longFired = true;
    launch = true;
  }
  if (!touch && g_wasTouch) {
    if (!g_longFired && g_count) {
      g_sel = (uint8_t)((g_sel + 1) % g_count);
      g_menuDirty = true;
    }
    g_longFired = false;
  }
  g_wasTouch = touch;

  if (launch && g_count && !g_menuAt) {
    sound.playEffect(sfx::BEEP);
    runSlot(g_slots[g_sel]);
    return;
  }
  if (g_menuDirty && !g_menuAt) {
    g_menuDirty = false;
    showLauncher();
  }
}

}  // namespace app
