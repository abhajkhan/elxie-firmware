#include "InvertedDrive.h"
#include "../communication/BleProtocol.h"
#include <Arduino.h>

void InvertedDrive::begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble, MotorController *motors)
{
    pDisplay = display;
    pLeds = leds;
    pBuzzer = buzzer;
    pBle = ble;
    pMotors = motors;
}

void InvertedDrive::initLevelParams()
{
    switch (level)
    {
    case 1:
        baseConcept = CONCEPT_FWD_BWD;
        levelTimerMs = 30000;
        requiredSafeDriveMs = 15000;
        minTotalTurns = 5;
        minLeftTurns = 2;
        minRightTurns = 3;
        break;
    case 2:
        baseConcept = CONCEPT_FWD_BWD;
        levelTimerMs = 30000;
        requiredSafeDriveMs = 15000;
        minTotalTurns = 5;
        minLeftTurns = 2;
        minRightTurns = 2;
        break;
    case 3:
        baseConcept = CONCEPT_STEERING;
        levelTimerMs = 30000;
        requiredSafeDriveMs = 15000;
        minTotalTurns = 3;
        minLeftTurns = 1;
        minRightTurns = 1;
        break;
    case 4:
        baseConcept = CONCEPT_STEERING;
        levelTimerMs = 30000;
        requiredSafeDriveMs = 15000;
        minTotalTurns = 5;
        minLeftTurns = 2;
        minRightTurns = 2;
        break;
    case 5:
        baseConcept = CONCEPT_FULL;
        levelTimerMs = 30000;
        requiredSafeDriveMs = 15000;
        minTotalTurns = 4;
        minLeftTurns = 2;
        minRightTurns = 2;
        break;
    case 6:
        baseConcept = CONCEPT_DYNAMIC;
        levelTimerMs = 30000;
        requiredSafeDriveMs = 15000;
        minTotalTurns = 4;
        minLeftTurns = 2;
        minRightTurns = 2;
        break;
    default:
        baseConcept = CONCEPT_FWD_BWD;
        levelTimerMs = 30000;
        requiredSafeDriveMs = 15000;
        minTotalTurns = 5;
        minLeftTurns = 2;
        minRightTurns = 3;
        break;
    }
}

bool InvertedDrive::start(uint8_t lvl, const String &sessId)
{
    return startWithId(lvl, sessId.length() > 0 ? sessId.c_str() : "game-001");
}

bool InvertedDrive::startWithId(uint8_t lvl, const char *msgId)
{
    if (lvl < 1 || lvl > 6)
    {
        lvl = 1;
    }

    stopOutputs();

    level = lvl;
    requestMsgId = (msgId && strlen(msgId) > 0) ? msgId : "game-001";
    finished = false;

    result = GameResult{};
    result.gameId = GameId::INVERTED_DRIVE;
    result.level = level;

    initLevelParams();

    totalTurns = 0;
    leftTurns = 0;
    rightTurns = 0;
    lastMappedSteerDir = 0;

    task1Passed = false;
    task1Failed = false;
    task2Passed = false;
    task3Passed = false;
    passedCount = 0;
    collisionOccurred = false;

    activeDriveMs = 0;

    currentLogicalDir = DIR_NONE;
    currentMappedDir = DIR_NONE;
    currentSpeed = 0;

    levelStartMs = millis();
    lastWarningMs = 0;
    lastUpdateMs = millis();
    lastProgressMs = 0;
    lastReportedConcept = CONCEPT_NORMAL;
    state = IDS_RUNNING;

    Serial.printf("[InvertedDrive] Starting L%d (timer=%dms, turns=%d, L=%d, R=%d)\n",
                  level, levelTimerMs, minTotalTurns, minLeftTurns, minRightTurns);

    if (pBuzzer)
    {
        pBuzzer->playTone(1800, 80);
    }

    sendGameStartedBle();
    showOledRunning();

    return true;
}

void InvertedDrive::stopOutputs()
{
    if (pLeds)
    {
        pLeds->clearStripBuffer();
        pLeds->show();
    }
    if (pMotors)
    {
        pMotors->stop();
    }
}

void InvertedDrive::sendGameStartedBle()
{
    if (!pBle || !pBle->isConnected())
        return;

    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "response";
    doc["response"] = "game_started";
    doc["id"] = requestMsgId;

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["gameId"] = "inverted_drive";
    payload["level"] = level;
    payload["status"] = "running";

    JsonObject targets = payload["targets"].to<JsonObject>();
    targets["safeDriveTargetSec"] = requiredSafeDriveMs / 1000;
    targets["minTotalTurns"] = minTotalTurns;
    targets["minLeftTurns"] = minLeftTurns;
    targets["minRightTurns"] = minRightTurns;

    String out;
    serializeJson(doc, out);
    pBle->sendTx(out);
}

void InvertedDrive::sendGameStateTask(uint8_t taskNum, const char *statusStr)
{
    if (!pBle || !pBle->isConnected())
        return;

    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "game_state";
    doc["ts"] = millis();

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["game"] = "inverted_drive";
    payload["level"] = level;
    payload["task"] = taskNum;
    payload["status"] = statusStr;

    String out;
    serializeJson(doc, out);
    pBle->sendTx(out);
}

void InvertedDrive::sendGameStateLevel(const char *statusStr)
{
    if (!pBle || !pBle->isConnected())
        return;

    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "game_state";
    doc["ts"] = millis();

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["game"] = "inverted_drive";
    payload["level"] = level;
    payload["status"] = statusStr;

    String out;
    serializeJson(doc, out);
    pBle->sendTx(out);
}

void InvertedDrive::sendGameProgressBle()
{
    if (!pBle || !pBle->isConnected())
        return;

    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "response";
    doc["response"] = "game_feedback";
    doc["gameId"] = "inverted_drive";
    doc["level"] = level;

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["safeDriveElapsedSec"] = activeDriveMs / 1000;
    payload["safeDriveTargetSec"] = requiredSafeDriveMs / 1000;
    payload["totalTurns"] = totalTurns;
    payload["minTotalTurns"] = minTotalTurns;
    payload["leftTurns"] = leftTurns;
    payload["minLeftTurns"] = minLeftTurns;
    payload["rightTurns"] = rightTurns;
    payload["minRightTurns"] = minRightTurns;

    if (level == 6)
    {
        MappingConcept activeConcept = getActiveConcept();
        payload["controlMode"] = (activeConcept == CONCEPT_FULL) ? "full_invert" : "normal";
    }

    String out;
    serializeJson(doc, out);
    pBle->sendTx(out);
}

void InvertedDrive::sendGameResultBle(const char *statusStr)
{
    if (!pBle || !pBle->isConnected())
        return;

    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "game_result";
    doc["id"] = requestMsgId;

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["game"] = "inverted_drive";
    payload["level"] = level;
    payload["score"] = passedCount;
    payload["stars"] = passedCount;
    payload["status"] = statusStr;

    JsonObject tasks = payload["tasks"].to<JsonObject>();
    tasks["completed"] = 3;
    tasks["passed"] = passedCount;
    tasks["failed"] = 3 - passedCount;

    String out;
    serializeJson(doc, out);
    pBle->sendTx(out);
}

void InvertedDrive::sendHapticWarningBle()
{
    if (!pBle || !pBle->isConnected())
        return;

    JsonDocument doc;
    doc["type"] = "haptic";
    doc["pattern"] = "warning";

    String out;
    serializeJson(doc, out);
    pBle->sendTx(out);
}

InvertedDrive::MappingConcept InvertedDrive::getActiveConcept() const
{
    if (baseConcept != CONCEPT_DYNAMIC)
    {
        return baseConcept;
    }

    unsigned long elapsed = millis() - levelStartMs;
    if (elapsed < 10000)
    {
        return CONCEPT_NORMAL;
    }
    else if (elapsed < 20000)
    {
        return CONCEPT_FULL;
    }
    else
    {
        return CONCEPT_NORMAL;
    }
}

InvertedDrive::Direction InvertedDrive::parseInputDirection(const InputEvent &input, int16_t &outSpeed)
{
    outSpeed = 0;
    if (input.type == InputEventType::JOYSTICK)
    {
        int16_t x = input.data.joystick.x;
        int16_t y = input.data.joystick.y;

        if (abs(x) < 40 && abs(y) < 40)
        {
            return DIR_NONE;
        }

        if (abs(y) >= abs(x))
        {
            outSpeed = constrain(abs(y), 120, 255);
            return (y > 0) ? DIR_FORWARD : DIR_BACKWARD;
        }
        else
        {
            outSpeed = constrain(abs(x), 120, 255);
            return (x > 0) ? DIR_RIGHT : DIR_LEFT;
        }
    }
    else if (input.type == InputEventType::DIRECTIONAL)
    {
        outSpeed = 180;
        if (input.data.directional.dirY > 0)
            return DIR_FORWARD;
        if (input.data.directional.dirY < 0)
            return DIR_BACKWARD;
        if (input.data.directional.dirX > 0)
            return DIR_RIGHT;
        if (input.data.directional.dirX < 0)
            return DIR_LEFT;
    }
    else if (input.type == InputEventType::MENU_NAVIGATE)
    {
        outSpeed = 180;
        switch (input.data.menuAction)
        {
        case MenuAction::NAV_UP:
            return DIR_FORWARD;
        case MenuAction::NAV_DOWN:
            return DIR_BACKWARD;
        case MenuAction::NAV_RIGHT:
            return DIR_RIGHT;
        case MenuAction::NAV_LEFT:
            return DIR_LEFT;
        default:
            break;
        }
    }

    return DIR_NONE;
}

InvertedDrive::Direction InvertedDrive::mapDirection(Direction logicalDir)
{
    if (logicalDir == DIR_NONE)
        return DIR_NONE;

    MappingConcept activeConcept = getActiveConcept();

    switch (activeConcept)
    {
    case CONCEPT_NORMAL:
        return logicalDir;

    case CONCEPT_FWD_BWD:
        if (logicalDir == DIR_FORWARD)
            return DIR_BACKWARD;
        if (logicalDir == DIR_BACKWARD)
            return DIR_FORWARD;
        return logicalDir; // Left/Right remain normal

    case CONCEPT_STEERING:
        if (logicalDir == DIR_LEFT)
            return DIR_RIGHT;
        if (logicalDir == DIR_RIGHT)
            return DIR_LEFT;
        return logicalDir; // Forward/Backward remain normal

    case CONCEPT_FULL:
        if (logicalDir == DIR_FORWARD)
            return DIR_BACKWARD;
        if (logicalDir == DIR_BACKWARD)
            return DIR_FORWARD;
        if (logicalDir == DIR_LEFT)
            return DIR_RIGHT;
        if (logicalDir == DIR_RIGHT)
            return DIR_LEFT;
        return DIR_NONE;

    default:
        return logicalDir;
    }
}

void InvertedDrive::updateSteeringTurnCount(Direction mappedDir)
{
    uint8_t currentSteer = 0;
    if (mappedDir == DIR_LEFT)
        currentSteer = 1;
    else if (mappedDir == DIR_RIGHT)
        currentSteer = 2;

    if (currentSteer != lastMappedSteerDir)
    {
        if (currentSteer == 1)
        {
            totalTurns++;
            leftTurns++;
            Serial.printf("[InvertedDrive] Mapped Left Turn! (total=%d, L=%d, R=%d)\n", totalTurns, leftTurns, rightTurns);
        }
        else if (currentSteer == 2)
        {
            totalTurns++;
            rightTurns++;
            Serial.printf("[InvertedDrive] Mapped Right Turn! (total=%d, L=%d, R=%d)\n", totalTurns, leftTurns, rightTurns);
        }
    }

    lastMappedSteerDir = currentSteer;
}

void InvertedDrive::handleInput(const InputEvent &input)
{
    if (finished || state != IDS_RUNNING)
        return;

    int16_t speed = 0;
    Direction logicalDir = parseInputDirection(input, speed);
    Direction mappedDir = mapDirection(logicalDir);

    currentLogicalDir = logicalDir;
    currentMappedDir = mappedDir;
    currentSpeed = speed;

    updateSteeringTurnCount(mappedDir);

    if (pMotors)
    {
        if (mappedDir == DIR_NONE || speed == 0)
        {
            pMotors->stop();
        }
        else
        {
            int16_t left = 0;
            int16_t right = 0;

            switch (mappedDir)
            {
            case DIR_FORWARD:
                left = speed;
                right = speed;
                break;
            case DIR_BACKWARD:
                left = -speed;
                right = -speed;
                break;
            case DIR_LEFT:
                left = -speed;
                right = speed;
                break;
            case DIR_RIGHT:
                left = speed;
                right = -speed;
                break;
            default:
                break;
            }

            pMotors->driveBSigned(left);
            pMotors->driveASigned(right);
        }
    }
}

void InvertedDrive::handleCollision()
{
    if (finished || state != IDS_RUNNING)
        return;

    stopOutputs();
    currentMappedDir = DIR_NONE;
    currentLogicalDir = DIR_NONE;
    currentSpeed = 0;

    if (pBuzzer)
    {
        pBuzzer->playTone(500, 300);
    }

    // Fail ONLY Task 1 (Safe driving without collision) when a collision occurs
    if (!task1Passed && !task1Failed)
    {
        task1Failed = true;
        sendGameStateTask(1, "failed");
        showFeedbackOled("OBJ 1 FAILED!");
    }
}

void InvertedDrive::update(const SensorSnapshot &sensors)
{
    if (finished || state == IDS_IDLE)
        return;

    unsigned long now = millis();
    unsigned long elapsed = now - levelStartMs;
    unsigned long dt = (lastUpdateMs > 0 && now > lastUpdateMs) ? (now - lastUpdateMs) : 0;
    lastUpdateMs = now;

    // Warning check for Level 6 Dynamic Invert mapping changes
    if (baseConcept == CONCEPT_DYNAMIC)
    {
        if ((elapsed >= 9000 && elapsed < 10000) || (elapsed >= 19000 && elapsed < 20000))
        {
            if (now - lastWarningMs > 3000)
            {
                lastWarningMs = now;
                if (pBuzzer)
                    pBuzzer->playTone(1500, 100);
                sendHapticWarningBle();
            }
        }
    }

    // Active driving contributes to Task 1 safe-driving duration ONLY when moving without collision
    if (currentMappedDir != DIR_NONE && currentSpeed > 0 && !task1Failed && !task1Passed)
    {
        activeDriveMs += dt;
        if (activeDriveMs >= requiredSafeDriveMs)
        {
            task1Passed = true;
            passedCount++;
            sendGameStateTask(1, "passed");
            if (pBuzzer)
                pBuzzer->playTone(2000, 100);
            showFeedbackOled("OBJ 1: SAFE DRIVE!");
        }
    }

    // Safety & Collision Check BEFORE driving into obstacles (evaluated only when actively moving)
    if (currentMappedDir == DIR_FORWARD && (sensors.obstacleFrontLeft || sensors.obstacleFrontRight))
    {
        handleCollision();
    }
    else if (currentMappedDir == DIR_BACKWARD && (sensors.obstacleRearLeft || sensors.obstacleRearRight))
    {
        handleCollision();
    }

    // Evaluate Task 2 (Total turns)
    if (!task2Passed && totalTurns >= minTotalTurns)
    {
        task2Passed = true;
        passedCount++;
        sendGameStateTask(2, "passed");
        if (pBuzzer)
            pBuzzer->playTone(2000, 100);
        showFeedbackOled("OBJ 2: TURNS OK!");
    }

    // Evaluate Task 3 (Left and Right turns)
    if (!task3Passed && leftTurns >= minLeftTurns && rightTurns >= minRightTurns)
    {
        task3Passed = true;
        passedCount++;
        sendGameStateTask(3, "passed");
        if (pBuzzer)
            pBuzzer->playTone(2000, 100);
        showFeedbackOled("OBJ 3: L/R OK!");
    }

    // Check Level Timer
    if (elapsed >= levelTimerMs)
    {
        stopOutputs();

        // Level timer expired -> if Task 1 safe-driving duration was not reached, Task 1 fails!
        if (!task1Passed && !task1Failed)
        {
            task1Failed = true;
            sendGameStateTask(1, "failed");
        }

        // If Task 2 or Task 3 were not completed by timeout, report them as failed
        if (!task2Passed)
        {
            sendGameStateTask(2, "failed");
        }
        if (!task3Passed)
        {
            sendGameStateTask(3, "failed");
        }

        finishLevel(passedCount == 3);
        return;
    }

    // If all 3 tasks are already passed before timer expiry:
    if (task1Passed && task2Passed && task3Passed)
    {
        finishLevel(true);
        return;
    }

    // Level 6 Dynamic Invert: Check if active control mode has transitioned to notify immediately
    bool conceptChanged = false;
    if (baseConcept == CONCEPT_DYNAMIC)
    {
        MappingConcept activeConcept = getActiveConcept();
        if (activeConcept != lastReportedConcept)
        {
            lastReportedConcept = activeConcept;
            conceptChanged = true;
            Serial.printf("[InvertedDrive] Dynamic Mode switched to: %s\n", (activeConcept == CONCEPT_FULL) ? "full_invert" : "normal");
        }
    }

    // Periodically send live objective progress telemetry to frontend (every 250ms or immediately on mode change)
    if (conceptChanged || (now - lastProgressMs >= 250))
    {
        lastProgressMs = now;
        sendGameProgressBle();
    }

    showOledRunning();
}

void InvertedDrive::finishLevel(bool success)
{
    stopOutputs();

    const char *statusStr = success ? "completed" : "failed";
    sendGameStateLevel(statusStr);
    sendGameResultBle(statusStr);

    result.gameId = GameId::INVERTED_DRIVE;
    result.level = level;
    result.score = (float)passedCount / 3.0f;
    result.correctCount = passedCount;
    result.totalTasks = 3;
    result.completed = success;
    result.aborted = false;

    Serial.printf("[InvertedDrive] L%d Finished: %s (score=%d/3, stars=%d)\n", level, statusStr, passedCount, passedCount);

    if (pBuzzer)
    {
        pBuzzer->playTone(success ? 2400 : 900, 150);
    }

    showResultOled();
    state = IDS_FINISHED;
    finished = true;
}

void InvertedDrive::abort()
{
    stopOutputs();
    if (pBuzzer)
        pBuzzer->stop();

    state = IDS_IDLE;
    finished = true;
    result.aborted = true;

    sendGameStateLevel("aborted");
}

void InvertedDrive::showOledRunning()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    unsigned long elapsed = millis() - levelStartMs;
    uint32_t remainSec = (levelTimerMs > elapsed) ? (levelTimerMs - elapsed) / 1000 : 0;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("INV L%d   %ds", level, remainSec);

    display.setCursor(0, 16);
    MappingConcept c = getActiveConcept();
    if (c == CONCEPT_FWD_BWD)
        display.println("FWD/BWD INVERT");
    else if (c == CONCEPT_STEERING)
        display.println("STEERING INVERT");
    else if (c == CONCEPT_FULL)
        display.println("FULL INVERT");
    else if (c == CONCEPT_DYNAMIC)
        display.println("DYNAMIC INVERT");
    else
        display.println("NORMAL CONTROL");

    display.setCursor(0, 32);
    display.printf("Score: %d/3 Stars", passedCount);

    display.setCursor(0, 48);
    display.printf("Turns: %d (L:%d R:%d)", totalTurns, leftTurns, rightTurns);

    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void InvertedDrive::showFeedbackOled(const char *msg)
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("INV L%d  PASSED", level);

    display.setTextSize(2);
    display.setCursor(0, 22);
    display.println(msg);

    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void InvertedDrive::showResultOled()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    bool success = (passedCount == 3);

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("INV L%d %s", level, success ? "PASSED" : "FAILED");

    display.setTextSize(2);
    display.setCursor(24, 18);
    display.printf("%d/3 Stars", passedCount);

    display.setTextSize(1);
    display.setCursor(0, 46);
    display.printf("Score: %d%%", (passedCount * 100) / 3);
    display.setCursor(0, 57);
    display.println("Double BTN2 to Exit");

    pDisplay->drawBatteryOverlay(false);
    display.display();
}
