#include "ReflexDash.h"
#include "../communication/BleProtocol.h"
#include <Arduino.h>

void ReflexDash::begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble, MotorController *motors)
{
    pDisplay = display;
    pLeds = leds;
    pBuzzer = buzzer;
    pBle = ble;
    pMotors = motors;
}

const char *ReflexDash::colorName(uint8_t idx) const
{
    switch (idx)
    {
    case 0:
        return "red";
    case 1:
        return "green";
    case 2:
        return "blue";
    case 3:
        return "yellow";
    case 4:
        return "purple";
    case 5:
        return "cyan";
    default:
        return "green";
    }
}

void ReflexDash::setColorRGB(uint8_t idx)
{
    if (!pLeds)
        return;
    switch (idx)
    {
    case 0:
        pLeds->setStripColor(255, 0, 0);
        break; // red
    case 1:
        pLeds->setStripColor(0, 255, 0);
        break; // green
    case 2:
        pLeds->setStripColor(0, 0, 255);
        break; // blue
    case 3:
        pLeds->setStripColor(255, 180, 0);
        break; // yellow
    case 4:
        pLeds->setStripColor(150, 0, 255);
        break; // purple
    case 5:
        pLeds->setStripColor(0, 255, 255);
        break; // cyan
    default:
        pLeds->setStripColor(0, 255, 0);
        break;
    }
    pLeds->show();
}

bool ReflexDash::isGoColor(uint8_t colorIdx) const
{
    for (uint8_t i = 0; i < numGoColors; i++)
    {
        if (goColors[i] == colorIdx)
            return true;
    }
    return false;
}

void ReflexDash::initLevelParams()
{
    switch (level)
    {
    case 1:
        totalPhases = 6;
        reactionWindowMs = 1000;
        break;
    case 2:
        totalPhases = 8;
        reactionWindowMs = 900;
        break;
    case 3:
        totalPhases = 8;
        reactionWindowMs = 800;
        break;
    case 4:
        totalPhases = 10;
        reactionWindowMs = 700;
        break;
    case 5:
        totalPhases = 10;
        reactionWindowMs = 500;
        break;
    case 6:
        totalPhases = 10;
        reactionWindowMs = 450;
        break;
    case 7:
        totalPhases = 10;
        reactionWindowMs = 500;
        break;
    case 8:
        totalPhases = 12;
        reactionWindowMs = 400;
        break;
    case 9:
        totalPhases = 12;
        reactionWindowMs = 350;
        break;
    case 10:
        totalPhases = 14;
        reactionWindowMs = 300;
        break;
    default:
        totalPhases = 6;
        reactionWindowMs = 1000;
        break;
    }
}

void ReflexDash::setupColorMapping()
{
    if (level <= 6)
    {
        // Fixed 2-color mapping: Green = GO, Red = STOP
        numGoColors = 1;
        goColors[0] = 1; // green
        numStopColors = 1;
        stopColors[0] = 0; // red
    }
    else if (level <= 8)
    {
        // Fixed 4-color mapping: GO = Green, Blue; STOP = Red, Yellow
        numGoColors = 2;
        goColors[0] = 1; // green
        goColors[1] = 2; // blue
        numStopColors = 2;
        stopColors[0] = 0; // red
        stopColors[1] = 3; // yellow
    }
    else
    {
        // Dynamic 6-color mapping (Levels 9-10)
        numGoColors = 3;
        numStopColors = 3;
        uint8_t pool[6] = {0, 1, 2, 3, 4, 5};
        for (uint8_t i = 5; i > 0; i--)
        {
            uint8_t j = random(i + 1);
            uint8_t tmp = pool[i];
            pool[i] = pool[j];
            pool[j] = tmp;
        }
        goColors[0] = pool[0];
        goColors[1] = pool[1];
        goColors[2] = pool[2];

        stopColors[0] = pool[3];
        stopColors[1] = pool[4];
        stopColors[2] = pool[5];
    }
}

bool ReflexDash::start(uint8_t lvl, const String &sessId)
{
    return startWithId(lvl, sessId.c_str());
}

bool ReflexDash::startWithId(uint8_t lvl, const char *msgId)
{
    if (lvl < 1 || lvl > 10)
        lvl = 1;

    stopOutputs();

    level = lvl;
    requestMsgId = (msgId && strlen(msgId) > 0) ? msgId : "game-001";
    phaseIndex = 0;
    correctCount = 0;
    finished = false;

    result = GameResult{};
    result.gameId = GameId::REFLEX_DASH;
    result.level = level;

    initLevelParams();
    setupColorMapping();

    Serial.printf("[ReflexDash] Starting Level %d (%d phases, %dms window)\n", level, totalPhases, reactionWindowMs);
    if (pBuzzer)
        pBuzzer->playTone(1800, 80);

    sendGameStartedBle();

    if (level >= 9)
    {
        state = RDS_MAPPING;
        showOledStatus("REFLEX DASH", "Study mapping");
    }
    else
    {
        state = RDS_WAIT_SEQ;
        showOledStatus("REFLEX DASH", "Ready - Press GO");
    }

    return true;
}

void ReflexDash::sendGameStartedBle()
{
    if (!pBle || !pBle->isConnected())
        return;

    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "response";
    doc["response"] = "game_started";
    doc["id"] = requestMsgId;

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["gameId"] = "reflex_dash";
    payload["level"] = level;
    payload["status"] = "started";

    JsonObject mapping = payload["mapping"].to<JsonObject>();
    JsonArray goArr = mapping["go"].to<JsonArray>();
    for (uint8_t i = 0; i < numGoColors; i++)
        goArr.add(colorName(goColors[i]));

    JsonArray stopArr = mapping["stop"].to<JsonArray>();
    for (uint8_t i = 0; i < numStopColors; i++)
        stopArr.add(colorName(stopColors[i]));

    String out;
    serializeJson(doc, out);
    pBle->sendTx(out);
}

void ReflexDash::runSequence()
{
    if (state == RDS_WAIT_SEQ || state == RDS_MAPPING)
    {
        state = RDS_SIGNAL_CUE;
        startPhase();
    }
}

void ReflexDash::stopOutputs()
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

void ReflexDash::startPhase()
{
    currentSignalIsGo = (random(2) == 1);

    if (currentSignalIsGo)
    {
        currentSignalColor = goColors[random(numGoColors)];
        currentPhaseDurationMs = 3000 + random(1500);
    }
    else
    {
        currentSignalColor = stopColors[random(numStopColors)];
        currentPhaseDurationMs = 2000 + random(1500);
    }

    setColorRGB(currentSignalColor);

    showOledStatus(currentSignalIsGo ? "GO!" : "STOP!", currentSignalIsGo ? "Keep Driving" : "Halt Movement");

    sendSignalChangeEvent(currentSignalIsGo ? "GO" : "STOP", colorName(currentSignalColor), currentPhaseDurationMs);

    phaseStartMs = millis();
    phaseUntilMs = phaseStartMs + currentPhaseDurationMs;
    playerIsMoving = false;
    violationTriggered = false;
    phaseAnswered = false;
    state = RDS_SIGNAL_CUE;
}

void ReflexDash::sendSignalChangeEvent(const char *signalStr, const char *colorStr, uint32_t durationMs)
{
    if (!pBle || !pBle->isConnected())
        return;

    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "game_event";
    doc["event"] = "signal_change";
    doc["gameId"] = "reflex_dash";
    doc["level"] = level;
    doc["phaseIndex"] = phaseIndex + 1;
    doc["signal"] = signalStr;
    doc["color"] = colorStr;
    doc["durationMs"] = durationMs;

    String out;
    serializeJson(doc, out);
    pBle->sendTx(out);
}

void ReflexDash::update(const SensorSnapshot &sensors)
{
    if (finished || state == RDS_IDLE)
        return;

    unsigned long now = millis();

    switch (state)
    {
    case RDS_WAIT_SEQ:
    case RDS_MAPPING:
        // Waiting for run_seq command from mobile or user action
        break;

    case RDS_SIGNAL_CUE:
        // Check violation during STOP signal phase
        if (!currentSignalIsGo && playerIsMoving)
        {
            if ((now - phaseStartMs) > reactionWindowMs && !violationTriggered)
            {
                violationTriggered = true;
                if (pBuzzer)
                    pBuzzer->playTone(500, 200);
                if (pLeds)
                {
                    pLeds->setStripColor(255, 0, 0); // Flash RED warning
                    pLeds->show();
                }
            }
        }

        if (now >= phaseUntilMs)
        {
            evaluatePhase();
        }
        break;

    case RDS_FEEDBACK:
        if (now >= feedbackUntilMs)
        {
            phaseIndex++;
            if (phaseIndex >= totalPhases)
            {
                finishLevel();
            }
            else
            {
                startPhase();
            }
        }
        break;

    default:
        break;
    }
}

void ReflexDash::handleInput(const InputEvent &input)
{
    int16_t x = 0;
    int16_t y = 0;

    if (input.type == InputEventType::JOYSTICK)
    {
        x = input.data.joystick.x;
        y = input.data.joystick.y;
        playerIsMoving = (abs(x) > 50 || abs(y) > 50);
    }
    else if (input.type == InputEventType::DIRECTIONAL)
    {
        x = input.data.directional.dirX * 200;
        y = input.data.directional.dirY * 200;
        playerIsMoving = (x != 0 || y != 0);
    }

    if (pMotors)
    {
        if (playerIsMoving)
        {
            int16_t left = constrain(y + x, -255, 255);
            int16_t right = constrain(y - x, -255, 255);
            pMotors->driveBSigned(left);
            pMotors->driveASigned(right);
        }
        else
        {
            pMotors->stop();
        }
    }

    if (state == RDS_WAIT_SEQ || state == RDS_MAPPING)
    {
        if (input.type == InputEventType::ACTION_SELECT || (input.type == InputEventType::BUTTON && input.data.button.pressed))
        {
            runSequence();
        }
    }
}

void ReflexDash::evaluatePhase()
{
    bool correct = false;

    if (currentSignalIsGo)
    {
        correct = playerIsMoving;
        if (correct)
        {
            correctCount++;
            if (pBuzzer)
                pBuzzer->playTone(2000, 100);
        }
    }
    else
    {
        correct = !violationTriggered;
        if (correct)
        {
            correctCount++;
            if (pBuzzer)
                pBuzzer->playTone(2400, 100);
        }
    }

    if (pBle && pBle->isConnected())
    {
        String payload = BleProtocol::serializeGameFeedback("reflex_dash", level, phaseIndex + 1, correct, correctCount);
        pBle->sendTx(payload);
    }

    feedbackUntilMs = millis() + 300;
    state = RDS_FEEDBACK;
}

void ReflexDash::finishLevel()
{
    stopOutputs();

    float scoreFloat = (float)correctCount / (float)totalPhases;
    uint8_t stars = 0;
    if (scoreFloat >= 0.90f)
        stars = 3;
    else if (scoreFloat >= 0.70f)
        stars = 2;
    else if (scoreFloat >= 0.50f)
        stars = 1;

    result.gameId = GameId::REFLEX_DASH;
    result.level = level;
    result.score = scoreFloat;
    result.correctCount = correctCount;
    result.totalTasks = totalPhases;
    result.completed = true;
    result.aborted = false;

    if (pBle && pBle->isConnected())
    {
        String payload = BleProtocol::serializeGameResult("reflex_dash", level, correctCount, stars, correctCount, totalPhases);
        pBle->sendTx(payload);
    }

    Serial.printf("[ReflexDash] Level %d Complete: %d/%d, stars=%d, score=%.2f\n", level, correctCount, totalPhases, stars, scoreFloat);
    if (pBuzzer)
    {
        pBuzzer->playTone(stars >= 2 ? 2400 : 900, 150);
    }

    showResultOled();
    state = RDS_FINISHED;
    finished = true;
}

void ReflexDash::abort()
{
    stopOutputs();
    if (pBuzzer)
        pBuzzer->stop();

    state = RDS_IDLE;
    finished = true;
    result.aborted = true;

    if (pBle && pBle->isConnected())
    {
        String payload = BleProtocol::serializeGameAborted(requestMsgId.c_str());
        pBle->sendTx(payload);
    }
}

void ReflexDash::showOledStatus(const char *title, const char *subtitle)
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("REFLEX L%d  P%d/%d", level, phaseIndex + 1, totalPhases);

    display.setTextSize(2);
    display.setCursor(10, 20);
    display.println(title);

    if (subtitle)
    {
        display.setTextSize(1);
        display.setCursor(0, 48);
        display.println(subtitle);
    }

    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void ReflexDash::showResultOled()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    uint8_t stars = 0;
    float scoreFloat = (float)correctCount / (float)totalPhases;
    if (scoreFloat >= 0.90f)
        stars = 3;
    else if (scoreFloat >= 0.70f)
        stars = 2;
    else if (scoreFloat >= 0.50f)
        stars = 1;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("REFLEX L%d COMPLETE", level);

    display.setTextSize(2);
    display.setCursor(24, 18);
    display.printf("%d/%d", correctCount, totalPhases);

    display.setTextSize(1);
    display.setCursor(0, 46);
    display.printf("Stars: %d/3", stars);
    display.setCursor(0, 57);
    display.println("Double BTN2 to Exit");

    pDisplay->drawBatteryOverlay(false);
    display.display();
}
