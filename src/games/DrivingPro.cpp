#include "DrivingPro.h"
#include "../communication/BleProtocol.h"
#include <Arduino.h>

void DrivingPro::begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble, MotorController *motors)
{
    pDisplay = display;
    pLeds = leds;
    pBuzzer = buzzer;
    pBle = ble;
    pMotors = motors;
}

bool DrivingPro::start(uint8_t lvl, const String &sessId)
{
    if (lvl != 1)
    {
        lvl = 1;
    }

    stopOutputs();

    level = lvl;
    requestMsgId = (sessId.length() > 0) ? sessId : "game-001";
    finished = false;
    currentTaskId = 1;
    tasksCompleted = 0;
    currentTaskPassed = false;

    result = GameResult{};
    result.gameId = GameId::DRIVING_PRO;
    result.level = level;
    result.totalTasks = 3;

    Serial.printf("[DrivingPro] Starting Level 1 (id: %s)\n", requestMsgId.c_str());
    if (pBuzzer)
    {
        pBuzzer->playTone(1800, 80);
    }

    sendGameStartedBle();

    state = DrivingProState::WAIT_TASK_START;
    showOledStatus("DRIVING PRO", "Ready - Task 1");

    return true;
}

void DrivingPro::startTask(uint8_t taskId, const char *msgId)
{
    if (taskId < 1 || taskId > 3)
    {
        taskId = 1;
    }

    stopOutputs();

    currentTaskId = taskId;
    if (currentTaskId == 1)
    {
        taskDurationMs = 7000;
    }
    else if (currentTaskId == 2)
    {
        taskDurationMs = 7000;
    }
    else
    {
        taskDurationMs = 9000;
    }

    totalTurns = 0;
    leftTurns = 0;
    rightTurns = 0;
    currentSteering = STEER_STRAIGHT;
    previousSteering = STEER_STRAIGHT;

    taskStartMs = millis();
    taskUntilMs = taskStartMs + taskDurationMs;
    state = DrivingProState::TASK_RUNNING;

    Serial.printf("[DrivingPro] Starting Task %d (%dms duration)\n", currentTaskId, taskDurationMs);

    sendTaskStartedBle(msgId);
    showOledStatus("TASK RUNNING", "Drive Safe!");
    updateMotors();
}

void DrivingPro::update(const SensorSnapshot &sensors)
{
    if (finished || state == DrivingProState::IDLE)
        return;

    unsigned long now = millis();

    switch (state)
    {
    case DrivingProState::TASK_RUNNING:
    {
        // 1. Collision detection check
        bool collision = (sensors.frontDistanceCm > 0 && sensors.frontDistanceCm <= 15) || sensors.obstacleFrontLeft || sensors.obstacleFrontRight;
        if (collision)
        {
            Serial.println("[DrivingPro] Collision detected! Game Over.");
            stopOutputs();

            if (pBuzzer)
            {
                pBuzzer->playTone(500, 300);
            }

            result.completed = false;
            result.aborted = false;
            finished = true;
            state = DrivingProState::FINISHED;

            sendGameOverBle("collision");
            showOledStatus("GAME OVER!", "Collision!");
            return;
        }

        // 2. Drive motors
        updateMotors();

        // 3. Check Task Timeout
        if (now >= taskUntilMs)
        {
            stopOutputs();

            if (currentTaskId == 1)
            {
                currentTaskPassed = true;
            }
            else if (currentTaskId == 2)
            {
                currentTaskPassed = (totalTurns >= 4);
            }
            else if (currentTaskId == 3)
            {
                currentTaskPassed = (leftTurns >= 2 && rightTurns >= 3);
            }

            if (currentTaskPassed)
            {
                tasksCompleted++;
                result.score = (float)tasksCompleted;
                result.correctCount = tasksCompleted;

                if (pBuzzer)
                {
                    pBuzzer->playTone(2000, 150);
                }

                sendTaskResultBle(true);

                if (currentTaskId < 3)
                {
                    currentTaskId++;
                    state = DrivingProState::WAIT_TASK_START;
                    showOledStatus("TASK PASSED!", "Ready Next Task");
                }
                else
                {
                    result.completed = true;
                    finished = true;
                    state = DrivingProState::FINISHED;

                    sendGameResultBle();
                    showOledStatus("LEVEL COMPLETE!", "3 Stars!");
                }
            }
            else
            {
                result.completed = false;
                finished = true;
                state = DrivingProState::FINISHED;

                if (pBuzzer)
                {
                    pBuzzer->playTone(400, 300);
                }

                sendTaskResultBle(false);
                sendGameOverBle("task_failed");
                showOledStatus("GAME OVER!", "Task Failed!");
            }
        }
        break;
    }

    default:
        break;
    }
}

void DrivingPro::handleInput(const InputEvent &input)
{
    if (state == DrivingProState::WAIT_TASK_START)
    {
        if (input.type == InputEventType::ACTION_SELECT || input.type == InputEventType::RUN_SEQ || (input.type == InputEventType::BUTTON && input.data.button.pressed))
        {
            startTask(currentTaskId, "local");
            return;
        }
    }

    if (state == DrivingProState::TASK_RUNNING)
    {
        if (input.type == InputEventType::JOYSTICK)
        {
            int16_t x = input.data.joystick.x;
            if (x < -50)
            {
                currentSteering = STEER_LEFT;
            }
            else if (x > 50)
            {
                currentSteering = STEER_RIGHT;
            }
            else
            {
                currentSteering = STEER_STRAIGHT;
            }
        }
        else if (input.type == InputEventType::DIRECTIONAL)
        {
            int8_t dirX = input.data.directional.dirX;
            if (dirX < 0)
            {
                currentSteering = STEER_LEFT;
            }
            else if (dirX > 0)
            {
                currentSteering = STEER_RIGHT;
            }
            else
            {
                currentSteering = STEER_STRAIGHT;
            }
        }

        // Track turn direction changes
        if (currentSteering != previousSteering)
        {
            if (currentSteering == STEER_LEFT && previousSteering != STEER_LEFT)
            {
                leftTurns++;
                totalTurns++;
                Serial.printf("[DrivingPro] Left turn #%d (total: %d)\n", leftTurns, totalTurns);
            }
            else if (currentSteering == STEER_RIGHT && previousSteering != STEER_RIGHT)
            {
                rightTurns++;
                totalTurns++;
                Serial.printf("[DrivingPro] Right turn #%d (total: %d)\n", rightTurns, totalTurns);
            }
            previousSteering = currentSteering;
        }

        updateMotors();
    }
}

void DrivingPro::updateMotors()
{
    if (!pMotors)
        return;

    int16_t leftSpeed = 180;
    int16_t rightSpeed = 180;

    if (currentSteering == STEER_LEFT)
    {
        leftSpeed = 60;
        rightSpeed = 200;
    }
    else if (currentSteering == STEER_RIGHT)
    {
        leftSpeed = 200;
        rightSpeed = 60;
    }

    pMotors->driveBSigned(leftSpeed);
    pMotors->driveASigned(rightSpeed);
}

void DrivingPro::stopOutputs()
{
    if (pMotors)
    {
        pMotors->stop();
    }
    if (pLeds)
    {
        pLeds->clearStripBuffer();
        pLeds->show();
    }
}

void DrivingPro::abort()
{
    stopOutputs();
    finished = true;
    result.aborted = true;
    state = DrivingProState::FINISHED;
}

void DrivingPro::sendGameStartedBle()
{
    if (!pBle || !pBle->isConnected())
        return;

    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "response";
    doc["response"] = "game_started";
    doc["id"] = requestMsgId;

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["gameId"] = "driving_pro";
    payload["level"] = level;
    payload["status"] = "started";

    String out;
    serializeJson(doc, out);
    pBle->sendTx(out);
}

void DrivingPro::sendTaskStartedBle(const char *msgId)
{
    if (!pBle || !pBle->isConnected())
        return;

    String resp = BleProtocol::serializeTaskStarted(msgId, "driving_pro", level, currentTaskId, taskDurationMs);
    pBle->sendTx(resp);
}

void DrivingPro::sendTaskResultBle(bool passed)
{
    if (!pBle || !pBle->isConnected())
        return;

    uint8_t currentScore = (uint8_t)result.score;
    String resp = BleProtocol::serializeTaskResult("driving_pro", level, currentTaskId, passed ? "passed" : "failed", currentScore, currentScore);
    pBle->sendTx(resp);
}

void DrivingPro::sendGameOverBle(const char *reason)
{
    if (!pBle || !pBle->isConnected())
        return;

    uint8_t currentScore = (uint8_t)result.score;
    String resp = BleProtocol::serializeGameOver("driving_pro", level, reason, tasksCompleted, 3, currentScore, currentScore);
    pBle->sendTx(resp);
}

void DrivingPro::sendGameResultBle()
{
    if (!pBle || !pBle->isConnected())
        return;

    uint8_t currentScore = (uint8_t)result.score;
    String resp = BleProtocol::serializeGameResult("driving_pro", level, currentScore, currentScore, tasksCompleted, 3);
    pBle->sendTx(resp);
}

void DrivingPro::showOledStatus(const char *title, const char *subtitle)
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    Adafruit_SSD1306 &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("--- DRIVING PRO ---");

    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println(title);

    display.setCursor(0, 40);
    display.println(subtitle);

    display.display();
}
