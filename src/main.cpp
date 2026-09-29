#include <Arduino.h>
#include "config/BoardConfig.h"
#include "core/RobotTypes.h"
#include "core/RobotState.h"
#include "core/RobotCommand.h"
#include "core/InputEvent.h"

#include "hardware/MotorController.h"
#include "hardware/SensorManager.h"
#include "hardware/LedController.h"
#include "hardware/DisplayController.h"
#include "hardware/BuzzerController.h"
#include "hardware/BatteryManager.h"

#include "communication/BleProtocol.h"
#include "communication/BleManager.h"

// Hardware Abstraction Instances
static MotorController motorController;
static SensorManager sensorManager;
static LedController ledController;
static DisplayController displayController;
static BuzzerController buzzerController;
static BatteryManager batteryManager;

// Communication Instances
static BleManager bleManager;

// Declarations of legacy functions still active during Phase 5A.5 transition
// extern void legacy_setup();
// extern void legacy_loop();

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("Starting ELXIE Firmware (Modular Architecture Entry Point)...");

    // Initialize HAL Modules
    motorController.begin();
    sensorManager.begin();
    ledController.begin();
    displayController.begin(sensorManager.isOledPresent());
    buzzerController.begin();
    batteryManager.begin();

    // Initialize BLE Communication Module (Phase 5A)
    bleManager.begin(DEVICE_NAME);

    // Invoke legacy setup for remaining unmigrated services (ESP-NOW, games, etc.)
    // legacy_setup();
}

void loop()
{
    // Update HAL services
    sensorManager.update();
    batteryManager.service(buzzerController);
    displayController.updateIdleDisplay(batteryManager.isLow());

    // Update Communication service
    bleManager.update();

    // Service remaining legacy application loop
    // legacy_loop();
}
