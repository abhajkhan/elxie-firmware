#include "blocks.h"
#include <math.h>
#include <string.h>

#include "../communication/EspNowManager.h"
#include "../core/RobotController.h"
#include "../hardware/DisplayController.h"
#include "../hardware/LedController.h"
#undef BLACK  // Adafruit colour macros clash with the block enums
#undef WHITE
#undef INVERSE
#include "app.h"
#include "ble_transport.h"
#include "blocks_config.h"
#include "face.h"
#include "lights.h"
#include "motion.h"
#include "motors.h"
#include "platform.h"
#include "protocol.h"
#include "remote_link.h"
#include "robot_host.h"
#include "sensor_hub.h"
#include "sound.h"
#include "storage.h"

namespace blocks {

static RobotController* g_robot = nullptr;
static bool g_active = false;
static uint32_t g_lastTick = 0;
static uint32_t g_idleSince = 0;  // app gone and no program running since...
static bool g_local = false;      // opened on the robot (double tap), not by the app

// Double tap on the touch pad while the robot is idle in the games = open "My Programs".
static bool g_tapDown = false;
static uint32_t g_tapDownAt = 0, g_lastTapAt = 0, g_tapChangeAt = 0;
static bool g_tapRaw = false;

// ---- Factory checks over USB serial (tilt test, live sensor print)
enum TiltStep : uint8_t { TILT_OFF, TILT_LEVEL, TILT_NOSE, TILT_RIGHT };
static TiltStep g_tiltStep = TILT_OFF;
static uint32_t g_tiltStepAt = 0;
static float g_tiltSum[2];  // sums of raw accel X, Y over the end of the current step
static uint16_t g_tiltN = 0;
static float g_tiltAvg[3][2];  // [level, nose, right][x, y]
static bool g_live = false;
static uint32_t g_livePrintAt = 0;
static const uint32_t kTiltStepMs = 6000;    // time per pose
static const uint32_t kTiltSampleMs = 2000;  // averaged over the last 2 s of each pose

static void enter() {
  if (g_active) return;
  Serial.println("[Blocks] Blocks mode on");
  g_robot->enterBlocks();  // ends a running game / RC drive, stops the motors
  g_active = true;
  g_lastTick = millis();
  g_idleSince = 0;
  sensors.update(0);  // fresh readings before the first program tick
  face.setActive(true);
  lights.off();
  app::showIdle();
}

static void leave() {
  if (!g_active) return;
  Serial.println("[Blocks] Blocks mode off");
  g_active = false;
  g_local = false;
  g_tiltStep = TILT_OFF;
  g_live = false;
  app::stopProgram();  // also stops motion, sound and light effects
  motors::release();
  sound.stop();
  lights.off();
  lights.update();  // push the dark frame before the games take the LEDs back
  face.setActive(false);
  g_robot->exitBlocks();  // back to IDLE unless the games already switched mode
}

// Any frame on the Blocks service means the app's Blocks page is open.
static void onCommand(uint8_t) {
  if (EspNowManager::getInstance().isPairing()) return;  // the pairing screen keeps the robot
  enter();
}

// ------------------------------------------------------------------ factory checks
static const char* const kTiltPrompt[] = {"", "keep it LEVEL", "tip NOSE DOWN", "tip RIGHT side down"};

static void tiltStart(TiltStep step) {
  g_tiltStep = step;
  g_tiltStepAt = millis();
  g_tiltSum[0] = g_tiltSum[1] = 0;
  g_tiltN = 0;
  face.message("Tilt test", kTiltPrompt[step]);
  Serial.printf("TILT STEP %d: %s (about 45 degrees) and hold for %lu s\n", (int)step, kTiltPrompt[step],
                (unsigned long)(kTiltStepMs / 1000));
}

// Raw axis that moved most between two poses: 0 = X, 1 = Y. *sign = direction, *g = size.
static int tiltAxis(const float from[2], const float to[2], int* sign, float* g) {
  float dx = to[0] - from[0], dy = to[1] - from[1];
  int axis = fabsf(dx) >= fabsf(dy) ? 0 : 1;
  float d = axis ? dy : dx;
  *sign = d >= 0 ? 1 : -1;
  *g = fabsf(d);
  return axis;
}

static void tiltFinish() {
  g_tiltStep = TILT_OFF;
  int sN, sR;
  float gN, gR;
  int axN = tiltAxis(g_tiltAvg[0], g_tiltAvg[1], &sN, &gN);
  int axR = tiltAxis(g_tiltAvg[0], g_tiltAvg[2], &sR, &gR);
  Serial.printf("TILT RAW level x=%.2f y=%.2f  nose x=%.2f y=%.2f  right x=%.2f y=%.2f (g)\n", g_tiltAvg[0][0],
                g_tiltAvg[0][1], g_tiltAvg[1][0], g_tiltAvg[1][1], g_tiltAvg[2][0], g_tiltAvg[2][1]);
  if (gN < 0.25f || gR < 0.25f || axN == axR) {
    Serial.println("TILT RESULT UNCLEAR: tip further (about 45 degrees) and hold still, then run tilttest again");
    face.message("Tilt test", "unclear, try again");
    return;
  }
  // pitch = atan2(-a, ..) on the nose axis -> positive (nose down) when that axis goes negative;
  // roll  = atan2(a, z)  on the side axis -> positive (right down) when that axis goes positive.
  const int swap = axN == 1 ? 1 : 0;
  const int pitchSign = sN < 0 ? 1 : -1;
  const int rollSign = sR > 0 ? 1 : -1;
  const bool same = swap == IMU_SWAP_PITCH_ROLL && pitchSign == IMU_PITCH_SIGN && rollSign == IMU_ROLL_SIGN;
  Serial.printf("TILT RESULT IMU_SWAP_PITCH_ROLL %d  IMU_PITCH_SIGN (%+d)  IMU_ROLL_SIGN (%+d)  -> %s\n", swap,
                pitchSign, rollSign, same ? "MATCHES blocks_config.h" : "CHANGE blocks_config.h");
  face.message("Tilt test", same ? "settings OK" : "change settings");
}

static void serialChecksTick(uint32_t now) {
  const SensorState& s = sensors.state();
  if (g_tiltStep != TILT_OFF) {
    uint32_t t = now - g_tiltStepAt;
    if (t >= kTiltStepMs - kTiltSampleMs) {
      g_tiltSum[0] += s.accelX;
      g_tiltSum[1] += s.accelY;
      g_tiltN++;
    }
    if (t >= kTiltStepMs) {
      const int i = (int)g_tiltStep - 1;
      g_tiltAvg[i][0] = g_tiltN ? g_tiltSum[0] / g_tiltN : s.accelX;
      g_tiltAvg[i][1] = g_tiltN ? g_tiltSum[1] / g_tiltN : s.accelY;
      if (g_tiltStep == TILT_RIGHT) tiltFinish();
      else tiltStart((TiltStep)(g_tiltStep + 1));
    }
  }
  if (g_live && (int32_t)(now - g_livePrintAt) >= 0) {
    g_livePrintAt = now + 250;
    Serial.printf("TILT pitch %.0f roll %.0f | accel x %.2f y %.2f z %.2f | heading %.0f mag %d %d %d%s\n", s.pitch,
                  s.roll, s.accelX, s.accelY, s.accelZ, s.heading, s.magRaw[0], s.magRaw[1], s.magRaw[2],
                  sensors.magCalibration().valid ? "" : " (compass not calibrated)");
  }
}

bool serialCommand(const char* line) {
  const bool known = !strcmp(line, "calmag") || !strcmp(line, "compass?") || !strcmp(line, "tilttest") ||
                     !strcmp(line, "tilt");
  if (!known) return false;
  if (EspNowManager::getInstance().isPairing()) {
    Serial.println("BUSY pairing");
    return true;
  }
  if (!strcmp(line, "compass?")) {
    const MagCalibration& mc = sensors.magCalibration();
    Serial.printf("COMPASS %s %s x %d..%d y %d..%d z %d..%d\n", sensors.state().magOk ? "found" : "missing",
                  mc.valid ? "calibrated" : "not-calibrated", mc.min[0], mc.max[0], mc.min[1], mc.max[1], mc.min[2],
                  mc.max[2]);
    return true;
  }
  enter();  // the checks run in Blocks mode (block engine owns sensors and motors)
  if (!strcmp(line, "calmag")) {
    if (app::startMagCalibration()) Serial.printf("MAGCAL START spinning for %d s\n", MAG_CAL_MS / 1000);
    else Serial.println("MAGCAL FAILED (no magnetometer found)");
  } else if (!strcmp(line, "tilttest")) {
    if (!sensors.state().imuOk) Serial.println("TILT RESULT FAILED (no MPU6050 found)");
    else {
      app::stopProgram();
      g_live = false;
      tiltStart(TILT_LEVEL);
    }
  } else {  // "tilt": live print on/off
    g_live = !g_live;
    g_livePrintAt = 0;
    Serial.println(g_live ? "TILT LIVE ON (send tilt again to stop)" : "TILT LIVE OFF");
  }
  return true;
}

static void pollDoubleTap() {
  const uint32_t now = millis();
  const bool raw = hal::pinRead(PIN_TOUCH) == (TOUCH_ACTIVE_HIGH ? 1 : 0);
  if (raw != g_tapRaw) {  // debounce
    g_tapRaw = raw;
    g_tapChangeAt = now;
    return;
  }
  if (now - g_tapChangeAt < TOUCH_DEBOUNCE_MS || raw == g_tapDown) return;
  g_tapDown = raw;
  if (raw) {
    g_tapDownAt = now;
    return;
  }
  // released
  const bool isTap = now - g_tapDownAt <= TOUCH_TAP_MAX_MS;
  if (!isTap) {
    g_lastTapAt = 0;
    return;
  }
  if (g_lastTapAt && g_tapDownAt - g_lastTapAt <= TOUCH_OPEN_GAP_MS) {
    g_lastTapAt = 0;
    const bool idle = g_robot->getState().mode == TopLevelState::IDLE;
    const EspNowManager& radio = EspNowManager::getInstance();
    if (idle && !radio.isPairing()) {
      Serial.println("[Blocks] double tap: My Programs");
      openPrograms();
      sound.playEffect(sfx::BEEP);
    }
    return;
  }
  g_lastTapAt = now;
}

void begin(LedController& leds, DisplayController& display, RobotController& robot) {
  g_robot = &robot;
  hal::initI2c();
  storage::begin();
  motors::begin();
  sound.begin();
  lights.begin(&leds.getStrip());
  face.begin(display.isPresent() ? &display.getDisplay() : nullptr);
  sensors.begin();
  sensors.calibrateGyro(1000);  // robot must be still for ~1 s at boot
  remote.begin();
  vmlink::begin(onCommand);
  app::begin();
  Serial.printf("[Blocks] ready: bytecode v%d, %d slots\n", elx::BYTECODE_VERSION, elx::LIMIT_NUM_SLOTS);
}

void attachBle(BLEServer* server) { ble::attach(server); }
void configureAdvertising(BLEAdvertising* adv, const char* name) { ble::configureScanResponse(adv, name); }
void onBleMtu(uint16_t mtu) { ble::onMtu(mtu); }
void onBleDisconnect() { ble::onDisconnect(); }
bool active() { return g_active; }
void openPrograms() {
  if (EspNowManager::getInstance().isPairing()) return;
  enter();
  g_local = true;  // back to the games after LAUNCHER_TIMEOUT_MS without input, or remote B
}
void deactivate() { leave(); }

void loop() {
  vmlink::poll();  // may call enter()
  if (!g_active) {
    pollDoubleTap();
    return;
  }

  // The game code switched to RC or a game (app command): hand the robot back.
  if (g_robot->getState().mode != TopLevelState::BLOCKS) {
    leave();
    return;
  }

  const uint32_t now = millis();
  uint32_t dt = now - g_lastTick;
  if (dt < elx::LIMIT_TICK_MS) return;  // 100 Hz control tick
  g_lastTick = now;
  if (dt > 50) dt = 50;  // after a long BLE send, don't jump the motion ramps

  sensors.update(dt);
  remote.update();
  app::tick(dt);       // e-stop, calibration, idle screen
  app::vm().tick();    // run block programs
  robotHost.update();  // finish pet reactions
  motion.update(dt);   // after the VM so new commands apply this tick
  sound.update();
  lights.update();
  vmlink::periodic();    // telemetry / trace

  serialChecksTick(now);

  // Remote B in "My Programs" (no app): back to the games.
  if (app::takeLeaveRequest() && !vmlink::bleConnected() && !app::running()) {
    leave();
    return;
  }

  // Nothing running and no app: give the robot back to the games, after 3 s, or after
  // LAUNCHER_TIMEOUT_MS without input when "My Programs" was opened on the robot.
  const bool busy = app::running() || app::magCalibrating() || sensors.calibrating() || g_tiltStep != TILT_OFF || g_live;
  if (vmlink::bleConnected() || busy) {
    g_idleSince = 0;
  } else if (!g_idleSince) {
    g_idleSince = now;
  } else {
    const uint32_t limit = g_local ? LAUNCHER_TIMEOUT_MS : 3000;
    if (now - g_idleSince >= limit && now - app::lastInputMs() >= limit) leave();
  }
}

}  // namespace blocks
