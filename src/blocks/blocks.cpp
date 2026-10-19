#include "blocks.h"

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
void deactivate() { leave(); }

void loop() {
  vmlink::poll();  // may call enter()
  if (!g_active) return;

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

  // App closed and nothing running: after 3 s give the robot back to the games.
  const bool busy = app::running() || app::magCalibrating() || sensors.calibrating();
  if (vmlink::bleConnected() || busy) {
    g_idleSince = 0;
  } else if (!g_idleSince) {
    g_idleSince = now;
  } else if (now - g_idleSince >= 3000) {
    leave();
  }
}

}  // namespace blocks
