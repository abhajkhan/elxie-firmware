#include "EchoMemory.h"
#include "../communication/BleProtocol.h"
#include <Arduino.h>

struct EchoColorRGB
{
    const char *name;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

static const EchoColorRGB ECHO_COLORS[6] = {
    {"red", 255, 0, 0},
    {"yellow", 255, 180, 0},
    {"green", 0, 255, 0},
    {"blue", 0, 0, 255},
    {"purple", 150, 0, 255},
    {"white", 255, 255, 255}};

static const char *ACTION_NAMES[6] = {"up", "right", "down", "left", "pet", "honk"};
static const unsigned long GAP_MS = 400;

void EchoMemory::begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble)
{
    pDisplay = display;
    pLeds = leds;
    pBuzzer = buzzer;
    pBle = ble;
}

bool EchoMemory::start(uint8_t lvl, const String &sessId)
{
    if (lvl < 1 || lvl > 6)
    {
        lvl = 1;
    }

    stopOutputs();

    level = lvl;
    sessionId = sessId;
    finished = false;
    lastTouchActive = false;

    result = GameResult{};
    result.gameId = GameId::ECHO_MEMORY;
    result.level = level;

    // Table parameters based on level
    if (level == 1)
    {
        sequenceLength = 4;
        flashInterval = 3000;
        actionCount = 4;
        fullStrip = false;
    }
    else if (level == 2)
    {
        sequenceLength = 5;
        flashInterval = 1500;
        actionCount = 4;
        fullStrip = false;
    }
    else if (level == 3)
    {
        sequenceLength = 6;
        flashInterval = 1500;
        actionCount = 4;
        fullStrip = false;
    }
    else if (level == 4)
    {
        sequenceLength = 5;
        flashInterval = 3000;
        actionCount = 6;
        fullStrip = true;
    }
    else if (level == 5)
    {
        sequenceLength = 6;
        flashInterval = 1500;
        actionCount = 6;
        fullStrip = true;
    }
    else if (level == 6)
    {
        sequenceLength = 5;
        flashInterval = 1500;
        actionCount = 6;
        fullStrip = true;
    }

    // Identity mapping for L1-L5
    for (uint8_t i = 0; i < 6; i++)
    {
        actionColor[i] = i;
    }

    // Dynamic 1-to-1 bijection for Level 6 (Fisher-Yates shuffle)
    if (level == 6)
    {
        for (uint8_t i = 5; i > 0; i--)
        {
            uint8_t j = random(0, i + 1);
            uint8_t tmp = actionColor[i];
            actionColor[i] = actionColor[j];
            actionColor[j] = tmp;
        }
    }

    flashIndex = 0;
    flashInGap = false;
    inputIndex = 0;
    wrongCount = 0;
    feedbackUntil = 0;

    // Generate random sequence
    for (uint8_t i = 0; i < sequenceLength; i++)
    {
        sequence[i] = random(0, actionCount);
    }

    Serial.printf("[EchoMemory] Starting L%d (len=%d, flash=%dms, actions=%d, full=%s)\n",
                  level, sequenceLength, flashInterval, actionCount, fullStrip ? "yes" : "no");

    if (pBuzzer)
    {
        pBuzzer->playTone(1800, 80);
    }

    sendStartedBleResponse();

    state = EchoMemoryState::MAPPING;
    showMappingOled();

    return true;
}

void EchoMemory::stopOutputs()
{
    if (pLeds)
    {
        pLeds->clearStripBuffer();
        pLeds->show();
    }
}

void EchoMemory::sendStartedBleResponse()
{
    if (!pBle || !pBle->isConnected())
        return;

    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "response";
    doc["response"] = "game_started";
    if (sessionId.length() > 0)
    {
        doc["id"] = sessionId;
    }
    else
    {
        doc["id"] = "game-001";
    }

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["gameId"] = "echo_memory";
    payload["level"] = level;
    payload["status"] = "started";

    JsonObject mapping = payload["mapping"].to<JsonObject>();
    for (uint8_t a = 0; a < actionCount; a++)
    {
        mapping[ACTION_NAMES[a]] = ECHO_COLORS[actionColor[a]].name;
    }

    String out;
    serializeJson(doc, out);
    pBle->sendTx(out);
}

void EchoMemory::startFlashingSequence()
{
    state = EchoMemoryState::FLASHING;
    flashIndex = 0;
    flashInGap = false;

    showAction(sequence[0]);
    phaseUntil = millis() + (flashInterval - GAP_MS);
    showFlashingOled();
}

void EchoMemory::showAction(uint8_t action)
{
    if (action >= 6)
        return;

    if (fullStrip)
    {
        uint8_t colorIdx = actionColor[action];
        if (pLeds)
        {
            pLeds->setStripColor(ECHO_COLORS[colorIdx].r, ECHO_COLORS[colorIdx].g, ECHO_COLORS[colorIdx].b);
        }
    }
    else
    {
        if (pLeds)
        {
            pLeds->clearStripBuffer();
            switch (action)
            {
            case EMA_UP:
                pLeds->setRegionColor(REGION_FRONT, 255, 0, 0); // Red
                break;
            case EMA_RIGHT:
                pLeds->setRegionColor(REGION_RIGHT, 255, 180, 0); // Yellow
                break;
            case EMA_DOWN:
                pLeds->setRegionColor(REGION_BACK, 0, 255, 0); // Green
                break;
            case EMA_LEFT:
                pLeds->setRegionColor(REGION_LEFT, 0, 0, 255); // Blue
                break;
            default:
                break;
            }
            pLeds->show();
        }
    }
}

void EchoMemory::playHonkSound()
{
    if (!pBuzzer)
        return;

    pBuzzer->playTone(400, 100);
    delay(110);
    pBuzzer->playTone(550, 100);
    delay(110);
    pBuzzer->playTone(750, 180);
}

void EchoMemory::update(const SensorSnapshot &sensors)
{
    if (finished || state == EchoMemoryState::IDLE)
        return;

    unsigned long now = millis();

    // Check Pet Touch Input during USER_INPUT phase
    if (state == EchoMemoryState::USER_INPUT)
    {
        if (sensors.touchActive && !lastTouchActive)
        {
            if (actionCount >= 6)
            {
                answerAction(EMA_PET);
            }
        }
        lastTouchActive = sensors.touchActive;
    }

    switch (state)
    {
    case EchoMemoryState::MAPPING:
        // Wait for run_seq command from BLE or select button from ESP-NOW
        break;

    case EchoMemoryState::FLASHING:
        if ((long)(now - phaseUntil) >= 0)
        {
            if (!flashInGap)
            {
                stopOutputs();
                flashInGap = true;
                phaseUntil = now + GAP_MS;
            }
            else
            {
                flashInGap = false;
                flashIndex++;

                if (flashIndex >= sequenceLength)
                {
                    stopOutputs();
                    state = EchoMemoryState::WAIT;
                    phaseUntil = now + 3000;
                    showWaitOled();
                }
                else
                {
                    showAction(sequence[flashIndex]);
                    phaseUntil = now + (flashInterval - GAP_MS);
                    showFlashingOled();
                }
            }
        }
        break;

    case EchoMemoryState::WAIT:
        if ((long)(now - phaseUntil) >= 0)
        {
            stopOutputs();
            inputIndex = 0;
            wrongCount = 0;
            state = EchoMemoryState::USER_INPUT;
            showInputOled();
        }
        break;

    case EchoMemoryState::USER_INPUT:
        if (feedbackUntil != 0 && (long)(now - feedbackUntil) >= 0)
        {
            stopOutputs();
            feedbackUntil = 0;
        }
        break;

    default:
        break;
    }
}

int EchoMemory::parseInputToAction(const InputEvent &input)
{
    if (input.type == InputEventType::PET)
    {
        return EMA_PET;
    }

    if (input.type == InputEventType::HONK)
    {
        return EMA_HONK;
    }

    if (input.type == InputEventType::BUTTON && input.data.button.buttonId == 1 && input.data.button.pressed)
    {
        // Remote btn1 -> HONK action in L4-L6
        return EMA_HONK;
    }

    if (input.type == InputEventType::JOYSTICK)
    {
        int16_t x = input.data.joystick.x;
        int16_t y = input.data.joystick.y;

        if (abs(x) < 50 && abs(y) < 50)
            return -1;

        if (abs(x) > abs(y))
        {
            return (x > 0) ? EMA_RIGHT : EMA_LEFT;
        }
        else
        {
            return (y > 0) ? EMA_UP : EMA_DOWN;
        }
    }

    if (input.type == InputEventType::DIRECTIONAL)
    {
        if (input.data.directional.dirY > 0)
            return EMA_UP;
        if (input.data.directional.dirX > 0)
            return EMA_RIGHT;
        if (input.data.directional.dirY < 0)
            return EMA_DOWN;
        if (input.data.directional.dirX < 0)
            return EMA_LEFT;
    }

    if (input.type == InputEventType::MENU_NAVIGATE)
    {
        switch (input.data.menuAction)
        {
        case MenuAction::NAV_UP:
            return EMA_UP;
        case MenuAction::NAV_RIGHT:
            return EMA_RIGHT;
        case MenuAction::NAV_DOWN:
            return EMA_DOWN;
        case MenuAction::NAV_LEFT:
            return EMA_LEFT;
        default:
            break;
        }
    }

    return -1;
}

void EchoMemory::handleInput(const InputEvent &input)
{
    if (state == EchoMemoryState::MAPPING)
    {
        if (input.type == InputEventType::RUN_SEQ || input.type == InputEventType::ACTION_SELECT)
        {
            startFlashingSequence();
        }
        return;
    }

    if (state != EchoMemoryState::USER_INPUT)
        return;

    int act = parseInputToAction(input);
    if (act >= 0 && act < actionCount)
    {
        answerAction(act);
    }
}

void EchoMemory::answerAction(uint8_t action)
{
    if (state != EchoMemoryState::USER_INPUT)
        return;

    if (feedbackUntil != 0 && (long)(millis() - feedbackUntil) < 0)
        return;

    if (action == EMA_HONK)
    {
        playHonkSound();
    }

    bool correct = (action == sequence[inputIndex]);

    if (correct)
    {
        if (pBuzzer)
            pBuzzer->playTone(2400, 100);
        if (pLeds)
            pLeds->setStripColor(0, 80, 0);
    }
    else
    {
        wrongCount++;
        if (pBuzzer)
            pBuzzer->playTone(500, 250);
        if (pLeds)
            pLeds->setStripColor(80, 0, 0);
    }

    inputIndex++;
    uint8_t correctCountSoFar = inputIndex - wrongCount;

    if (pBle && pBle->isConnected())
    {
        String fbJson = BleProtocol::serializeGameFeedback("echo_memory", level, inputIndex, correct, correctCountSoFar);
        pBle->sendTx(fbJson);
    }

    showFeedbackOled(correct);
    feedbackUntil = millis() + 300;

    if (inputIndex >= sequenceLength)
    {
        delay(300);
        finishLevel();
    }
}

void EchoMemory::finishLevel()
{
    stopOutputs();

    uint8_t correctCount = sequenceLength - wrongCount;
    uint8_t scorePercent = (correctCount * 100) / sequenceLength;

    uint8_t stars = 0;
    if (scorePercent >= 90)
        stars = 3;
    else if (scorePercent >= 70)
        stars = 2;
    else if (scorePercent >= 50)
        stars = 1;

    state = EchoMemoryState::RESULT;
    finished = true;

    result.gameId = GameId::ECHO_MEMORY;
    result.level = level;
    result.score = (float)scorePercent / 100.0f;
    result.correctCount = correctCount;
    result.totalTasks = sequenceLength;
    result.completed = true;
    result.aborted = false;

    if (pBle && pBle->isConnected())
    {
        String resJson = BleProtocol::serializeGameResult("echo_memory", level, correctCount, stars, correctCount, sequenceLength);
        pBle->sendTx(resJson);
    }

    Serial.printf("[EchoMemory] L%d Complete: %d/%d, score=%d%%, stars=%d\n",
                  level, correctCount, sequenceLength, scorePercent, stars);

    if (pBuzzer)
    {
        pBuzzer->playTone(scorePercent >= 50 ? 2400 : 900, 180);
    }

    showResultOled();
}

void EchoMemory::abort()
{
    stopOutputs();
    if (pBuzzer)
        pBuzzer->stop();

    state = EchoMemoryState::IDLE;
    finished = true;
    result.aborted = true;

    if (pBle && pBle->isConnected())
    {
        String resp = BleProtocol::serializeGameAborted(sessionId.c_str());
        pBle->sendTx(resp);
    }
}

void EchoMemory::showMappingOled()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("ECHO L%d - MAP", level);

    if (level == 6)
    {
        display.setCursor(0, 16);
        display.println("Study dynamic map!");
        display.setCursor(0, 32);
        display.println("Sent to App...");
    }
    else
    {
        display.setCursor(0, 16);
        display.println("Fixed Color Map");
        display.setCursor(0, 32);
        display.println(level <= 3 ? "4 Actions (D-Pad)" : "6 Actions (+Honk,Pet)");
    }

    display.setCursor(0, 52);
    display.println("Press GO or SW");

    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void EchoMemory::showFlashingOled()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("ECHO L%d", level);
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.println("WATCH!");
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.printf("Step %d/%d", flashIndex + 1, sequenceLength);

    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void EchoMemory::showWaitOled()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(20, 16);
    display.println("WAIT...");
    display.setTextSize(1);
    display.setCursor(16, 44);
    display.println("Get ready!");

    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void EchoMemory::showInputOled()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("ECHO L%d", level);
    display.setTextSize(2);
    display.setCursor(10, 18);
    display.println("REPEAT!");
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.println(level >= 4 ? "D-pad, Honk, Pet" : "Use D-pad");

    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void EchoMemory::showFeedbackOled(bool correct)
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(correct ? 14 : 20, 20);
    display.println(correct ? "CORRECT" : "WRONG");
    display.setTextSize(1);
    display.setCursor(20, 48);
    display.printf("Step %d/%d", inputIndex, sequenceLength);

    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void EchoMemory::showResultOled()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("ECHO L%d COMPLETE", level);
    display.setTextSize(2);
    display.setCursor(24, 18);
    display.printf("%d/%d", sequenceLength - wrongCount, sequenceLength);
    display.setTextSize(1);
    display.setCursor(0, 46);
    display.printf("Stars: %d/3", (result.score >= 0.9f) ? 3 : ((result.score >= 0.7f) ? 2 : ((result.score >= 0.5f) ? 1 : 0)));
    display.setCursor(0, 57);
    display.println("Double BTN2 to Exit");

    pDisplay->drawBatteryOverlay(false);
    display.display();
}
