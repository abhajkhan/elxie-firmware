#include <Arduino.h>
#include "config/BoardConfig.h"
#include "core/RobotTypes.h"
#include "core/RobotState.h"
#include "core/RobotCommand.h"
#include "core/InputEvent.h"
#include "core/RobotController.h"
#include "core/ControllerManager.h"

#include "hardware/MotorController.h"
#include "hardware/SensorManager.h"
#include "hardware/LedController.h"
#include "hardware/DisplayController.h"
#include "hardware/BuzzerController.h"
#include "hardware/BatteryManager.h"

#include "communication/BleProtocol.h"
#include "communication/BleManager.h"
#include "communication/EspNowProtocol.h"
#include "communication/EspNowManager.h"

// Hardware Abstraction Instances
static MotorController motorController;
static SensorManager sensorManager;
static LedController ledController;
static DisplayController displayController;
static BuzzerController buzzerController;
static BatteryManager batteryManager;

// Core Logic Instances
static RobotController robotController;
static ControllerManager controllerManager;

// Communication Instances
static BleManager bleManager;
static EspNowManager &espNowManager = EspNowManager::getInstance();

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("Starting ELXIE Firmware (Centralized Controller Ownership)...");

    // Initialize HAL Modules
    motorController.begin();
    sensorManager.begin();
    ledController.begin();
    displayController.begin(sensorManager.isOledPresent());
    buzzerController.begin();
    batteryManager.begin();

    // Initialize Core Logic Modules
    robotController.begin(&motorController, &ledController, &displayController, &buzzerController);
    controllerManager.begin(&robotController);

    // Initialize Communication Modules
    bleManager.begin(DEVICE_NAME);
    espNowManager.begin(REMOTE_MAC);
}

void loop()
{
    // 1. Update HAL services
    sensorManager.update();
    batteryManager.service(buzzerController);
    displayController.updateIdleDisplay(batteryManager.isLow());

    // 2. Update Communication services
    bleManager.update();
    espNowManager.update();

    // 3. Centralized Controller Ownership arbitration (SSoT Section 2.1 Rules)
    bool bleConnected = bleManager.isConnected();
    bool espNowConnected = espNowManager.isConnected();
    controllerManager.update(bleConnected, espNowConnected);

    // 4. Process incoming BLE messages through ControllerManager
    RobotCommand bleCmd;
    InputEvent bleEvent;
    JsonDocument rawDoc;
    if (bleManager.getPendingCommand(bleCmd, bleEvent, rawDoc))
    {
        if (bleCmd.type != CommandType::NONE)
        {
            controllerManager.processCommand(bleCmd);
        }
        if (bleEvent.type != InputEventType::NONE)
        {
            controllerManager.processInputEvent(bleEvent);
        }
    }

    // 5. Process incoming ESP-NOW messages through ControllerManager
    InputEvent espNowEvent;
    if (espNowManager.getLatestInputEvent(espNowEvent))
    {
        controllerManager.processInputEvent(espNowEvent);
    }

    // 6. Update Robot Controller state machine
    robotController.update();
}
