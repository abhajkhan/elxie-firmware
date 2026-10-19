// Elxie Blocks inside the Home firmware: glue between the game firmware and the
// block engine (VM, sensors, motion, face, sound, lights, Blocks BLE service).
//
// Blocks mode starts when the app's Blocks page talks to the robot (any frame on
// the Blocks BLE service). RobotController then sits in TopLevelState::BLOCKS and
// the block engine owns motors, OLED, LEDs and buzzer. Starting RC or a game from
// the app ends Blocks mode, and so does the app leaving once no program runs.
#pragma once
#include <stdint.h>

class BLEServer;
class BLEAdvertising;
class LedController;
class DisplayController;
class RobotController;

namespace blocks {
// After the hardware controllers are begun and before BleManager::begin().
void begin(LedController& leds, DisplayController& display, RobotController& robot);
// Called by BleManager.
void attachBle(BLEServer* server);
void configureAdvertising(BLEAdvertising* adv, const char* name);
void onBleMtu(uint16_t mtu);
void onBleDisconnect();
// Every Arduino loop().
void loop();
bool active();      // true while the block engine owns the robot
void deactivate();  // leave Blocks mode now (e.g. remote pairing starts)
}  // namespace blocks
