#include <Arduino.h>
#include "config/BoardConfig.h"
#include "core/RobotTypes.h"
#include "core/RobotState.h"
#include "core/RobotCommand.h"
#include "core/InputEvent.h"
#include "core/RobotController.h"
#include "core/ControllerManager.h"
#include "core/LocalMenu.h"
#include "games/GameManager.h"

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
static LocalMenu localMenu;
static GameManager gameManager;

// Communication Instances
static BleManager bleManager;
static EspNowManager &espNowManager = EspNowManager::getInstance();

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("Starting ELXIE Firmware (Local OLED Menu & Control Arbitration)...");

    // Initialize HAL Modules
    motorController.begin();
    sensorManager.begin();
    ledController.begin();
    displayController.begin(sensorManager.isOledPresent());
    buzzerController.begin();
    batteryManager.begin();

    // Initialize Core Logic Modules
    gameManager.begin(&displayController, &ledController, &buzzerController, &bleManager, &motorController);
    localMenu.begin(&displayController, &robotController);
    robotController.begin(&motorController, &ledController, &displayController, &buzzerController, &localMenu, &gameManager);
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

    // 2. Update Communication services
    bleManager.update();
    espNowManager.update();

    // 3. Centralized Controller Ownership arbitration
    bool bleConnected = bleManager.isConnected();
    bool espNowConnected = espNowManager.isConnected();
    controllerManager.update(bleConnected, espNowConnected);

    // 4. Update Local OLED Menu if Remote owns control
    if (!bleConnected && espNowConnected)
    {
        localMenu.update(batteryManager.isLow());
    }

    // 5. Process incoming BLE messages
    RobotCommand bleCmd;
    InputEvent bleEvent;
    JsonDocument rawDoc;
    if (bleManager.getPendingCommand(bleCmd, bleEvent, rawDoc))
    {
        const char *type = rawDoc["type"];
        if (type && strcmp(type, "hello") == 0)
        {
            const char *msgId = rawDoc["id"] | "hello-001";
            uint8_t protoVer = rawDoc["payload"]["protocolVersion"] | rawDoc["v"] | 1;
            bool accepted = (protoVer == 1);
            String ackMsg = BleProtocol::serializeHelloAck(msgId, accepted, 1);
            Serial.printf("[Event] Handshake 'hello' received (id: %s). Sending ack (accepted=%s)\n", msgId, accepted ? "true" : "false");
            bleManager.sendTx(ackMsg);
            if (accepted)
            {
                bleManager.setClientReady(true);
            }
        }
        else if (type && strcmp(type, "device_info_request") == 0)
        {
            const char *msgId = rawDoc["id"] | "";
            String macAddress = BLEDevice::getAddress().toString().c_str();
            macAddress.toUpperCase();
            macAddress.replace(":", "");
            String devInfoMsg = BleProtocol::serializeDeviceInfo(
                msgId,
                macAddress.c_str(),
                DEVICE_NAME,
                FIRMWARE_VERSION,
                bleConnected,
                espNowConnected,
                sensorManager.isOledPresent(),
                sensorManager.isMpu6050Present());
            Serial.printf("[Event] 'device_info_request' received (id: %s). Sending device_info response.\n", msgId);
            bleManager.sendTx(devInfoMsg);
        }
        else if (type && strcmp(type, "ping") == 0)
        {
            const char *msgId = rawDoc["id"] | "";
            String pongMsg = BleProtocol::serializePong(msgId);
            bleManager.sendTx(pongMsg);
        }
        else
        {
            const char *command = rawDoc["command"];
            if (command && strcmp(command, "run_seq") == 0)
            {
                Serial.println("[Event] Process BLE run_seq command");
                gameManager.runSequence();
            }
            else if (bleCmd.type == CommandType::START_GAME)
            {
                const char *msgId = bleCmd.game.reqId[0] != '\0' ? bleCmd.game.reqId : (rawDoc["id"] | "game-001");
                snprintf(bleCmd.game.reqId, sizeof(bleCmd.game.reqId), "%s", msgId);
                const char *gIdStr = rawDoc["payload"]["gameId"] | "color_quest";
                uint8_t lvl = rawDoc["payload"]["level"] | 1;

                if (strcmp(gIdStr, "reflex_dash") != 0)
                {
                    String resp = BleProtocol::serializeGameStarted(msgId, gIdStr, lvl);
                    bleManager.sendTx(resp);
                }

                Serial.printf("[Event] Process BLE START_GAME: %s level %d (id: %s)\n", gIdStr, lvl, msgId);
                controllerManager.processCommand(bleCmd);
            }
            else if (bleCmd.type == CommandType::ABORT_GAME)
            {
                const char *msgId = rawDoc["id"] | "abort-001";
                String resp = BleProtocol::serializeGameAborted(msgId);
                bleManager.sendTx(resp);

                Serial.println("[Event] Process BLE ABORT_GAME");
                controllerManager.processCommand(bleCmd);
            }
            else
            {
                if (bleCmd.type != CommandType::NONE)
                {
                    Serial.printf("[Event] Process BLE Command Type: %d\n", (int)bleCmd.type);
                    controllerManager.processCommand(bleCmd);
                }
                if (bleEvent.type != InputEventType::NONE)
                {
                    Serial.printf("[Event] Process BLE Input Event Type: %d\n", (int)bleEvent.type);
                    controllerManager.processInputEvent(bleEvent);
                }
            }
        }
    }

    // 6. Process incoming ESP-NOW messages
    InputEvent espNowEvent;
    if (espNowManager.getLatestInputEvent(espNowEvent))
    {
        if (controllerManager.getActiveOwner() == InputSource::ESP_NOW)
        {
            localMenu.handleInput(espNowEvent);
        }
        controllerManager.processInputEvent(espNowEvent);
    }

    // 7. Update Robot Controller state machine
    robotController.update(sensorManager.getSnapshot(), batteryManager.isLow());

    // 8. Send live telemetry to mobile if BLE is connected and robot is in RC or GAME mode
    static unsigned long lastTelemetryMs = 0;
    unsigned long now = millis();
    if (now - lastTelemetryMs >= 200)
    {
        lastTelemetryMs = now;
        TopLevelState currentMode = robotController.getState().mode;
        if (bleConnected)
        {
            String telemetryLine = BleProtocol::serializeTelemetry(
                sensorManager.getSnapshot(),
                controllerManager.getActiveOwner(),
                bleConnected,
                espNowConnected);
            bleManager.sendTx(telemetryLine);
        }
    }
}
