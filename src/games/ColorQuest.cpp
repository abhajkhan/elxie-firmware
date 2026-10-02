#include "ColorQuest.h"
#include <Arduino.h>

static const GameColor COLOR_PALETTE[] = {
    {"red", 255, 0, 0},
    {"green", 0, 255, 0},
    {"blue", 0, 0, 255},
    {"yellow", 255, 220, 0},
    {"cyan", 0, 255, 255},
    {"magenta", 255, 0, 255},
    {"orange", 255, 120, 0},
    {"purple", 150, 0, 255}};
static const size_t COLOR_PALETTE_N = sizeof(COLOR_PALETTE) / sizeof(COLOR_PALETTE[0]);

void ColorQuest::begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble)
{
    pDisplay = display;
    pLeds = leds;
    pBuzzer = buzzer;
    pBle = ble;
}

bool ColorQuest::start(uint8_t lvl, const String &sessId)
{
    if (lvl < 1 || lvl > 6)
    {
        lvl = 1;
    }

    stopOutputs();

    level = lvl;
    sessionId = sessId;
    taskIndex = 0;
    correctCount = 0;
    lastAnswerCorrect = false;
    finished = false;

    result = GameResult{};
    result.gameId = GameId::COLOR_QUEST;
    result.level = level;

    Serial.printf("[ColorQuest] Starting level %d, 10 tasks\n", level);
    if (pBuzzer)
    {
        pBuzzer->playTone(1800, 80);
    }

    buildTask();
    illuminateTask();
    return true;
}

void ColorQuest::stopOutputs()
{
    if (pLeds)
    {
        pLeds->clearStripBuffer();
        pLeds->show();
    }
}

unsigned long ColorQuest::getDisplayDuration(uint8_t lvl) const
{
    if (lvl % 2 != 0)
        return 5000;
    return 2500;
}

unsigned long ColorQuest::getAnswerTimeout(uint8_t lvl) const
{
    if (lvl % 2 != 0)
        return 5000;
    return 2500;
}

void ColorQuest::buildTask()
{
    static const int PRIMARY[] = {0, 1, 2};   // red, green, blue
    static const int SECONDARY[] = {3, 4, 5}; // yellow, cyan, magenta

    if (level <= 4)
    {
        int targetColor;
        int distractorColors[3];

        if (level <= 2)
        {
            targetColor = PRIMARY[random(3)];
            distractorColors[0] = SECONDARY[0];
            distractorColors[1] = SECONDARY[1];
            distractorColors[2] = SECONDARY[2];
        }
        else
        {
            targetColor = SECONDARY[random(3)];
            distractorColors[0] = PRIMARY[0];
            distractorColors[1] = PRIMARY[1];
            distractorColors[2] = PRIMARY[2];
        }

        for (int i = 2; i > 0; i--)
        {
            int j = random(i + 1);
            int t = distractorColors[i];
            distractorColors[i] = distractorColors[j];
            distractorColors[j] = t;
        }

        int targetSlot = random(4);
        int distractorCursor = 0;

        for (int slot = 0; slot < 4; slot++)
        {
            if (slot == targetSlot)
            {
                colorOption[slot] = targetColor;
            }
            else
            {
                colorOption[slot] = distractorColors[distractorCursor++];
            }
        }

        colorTargetDirection = targetSlot;
    }
    else
    {
        static const int TERTIARY[] = {6, 7}; // orange, purple
        int targetColor = TERTIARY[random(2)];

        int distractorColors[3];
        bool used[6] = {false};
        for (int i = 0; i < 3; i++)
        {
            int selected;
            do
            {
                selected = random(6);
            } while (used[selected]);
            used[selected] = true;
            distractorColors[i] = selected;
        }

        int targetSlot = random(4);
        int distractorCursor = 0;

        for (int slot = 0; slot < 4; slot++)
        {
            if (slot == targetSlot)
            {
                colorOption[slot] = targetColor;
            }
            else
            {
                colorOption[slot] = distractorColors[distractorCursor++];
            }
        }

        colorTargetDirection = targetSlot;
    }
}

void ColorQuest::illuminateTask()
{
    GameColor taskColors[4];
    for (int slot = 0; slot < 4; slot++)
    {
        taskColors[slot] = COLOR_PALETTE[colorOption[slot] % COLOR_PALETTE_N];
    }

    if (pLeds)
    {
        pLeds->clearStripBuffer();
        pLeds->setRegionColor(REGION_FRONT, taskColors[0].r, taskColors[0].g, taskColors[0].b);
        pLeds->setRegionColor(REGION_RIGHT, taskColors[1].r, taskColors[1].g, taskColors[1].b);
        pLeds->setRegionColor(REGION_BACK, taskColors[2].r, taskColors[2].g, taskColors[2].b);
        pLeds->setRegionColor(REGION_LEFT, taskColors[3].r, taskColors[3].g, taskColors[3].b);
        pLeds->show();
    }

    unsigned long now = millis();
    showUntil = now + getDisplayDuration(level);
    showTaskPromptOled();
    // sendTaskBle("memorize");
    state = STATE_SHOWING;
}

// void ColorQuest::sendTaskBle(const char *phaseStr)
// {
//     if (!pBle || !pBle->isConnected())
//         return;

//     JsonDocument doc;
//     doc["type"] = "task";
//     doc["game"] = "color-quest";
//     doc["level"] = level;
//     doc["index"] = taskIndex;
//     doc["phase"] = phaseStr;
//     doc["input"] = "region";
//     doc["regions"][0] = "front";
//     doc["regions"][1] = "right";
//     doc["regions"][2] = "back";
//     doc["regions"][3] = "left";

//     String payload;
//     serializeJson(doc, payload);
//     pBle->sendTx(payload);
// }

void ColorQuest::beginAnswerPhase()
{
    stopOutputs();
    showAnswerOled();
    // sendTaskBle("answer");

    state = STATE_WAIT_INPUT;
    answerUntil = millis() + getAnswerTimeout(level);
}

void ColorQuest::update(const SensorSnapshot &sensors)
{
    if (finished || state == STATE_IDLE)
        return;

    unsigned long now = millis();

    switch (state)
    {
    case STATE_SHOWING:
        if (now >= showUntil)
        {
            beginAnswerPhase();
        }
        break;

    case STATE_WAIT_INPUT:
        if (now >= answerUntil)
        {
            // Timeout - no input received
            answerDirection(-1);
        }
        break;

    case STATE_FEEDBACK:
        if (now >= feedbackUntil)
        {
            taskIndex++;
            if (taskIndex >= 10)
            {
                finishLevel();
            }
            else
            {
                buildTask();
                illuminateTask();
            }
        }
        break;

    default:
        break;
    }
}

int ColorQuest::inputToDirection(const InputEvent &input)
{
    if (input.type == InputEventType::DIRECTIONAL)
    {
        if (input.data.directional.dirY > 0)
            return 0; // FRONT
        if (input.data.directional.dirX > 0)
            return 1; // RIGHT
        if (input.data.directional.dirY < 0)
            return 2; // BACK
        if (input.data.directional.dirX < 0)
            return 3; // LEFT
    }
    else if (input.type == InputEventType::JOYSTICK)
    {
        int16_t x = input.data.joystick.x;
        int16_t y = input.data.joystick.y;
        if (abs(x) < 50 && abs(y) < 50)
            return -1;
        if (abs(x) > abs(y))
        {
            return (x > 0) ? 1 : 3;
        }
        else
        {
            return (y > 0) ? 0 : 2;
        }
    }
    else if (input.type == InputEventType::MENU_NAVIGATE)
    {
        switch (input.data.menuAction)
        {
        case MenuAction::NAV_UP:
            return 0;
        case MenuAction::NAV_RIGHT:
            return 1;
        case MenuAction::NAV_DOWN:
            return 2;
        case MenuAction::NAV_LEFT:
            return 3;
        default:
            break;
        }
    }
    return -1;
}

void ColorQuest::handleInput(const InputEvent &input)
{
    if (state != STATE_WAIT_INPUT && state != STATE_SHOWING)
        return;

    int dir = inputToDirection(input);
    if (dir >= 0 && dir <= 3)
    {
        answerDirection(dir);
    }
}

void ColorQuest::answerDirection(int direction)
{
    if (state != STATE_WAIT_INPUT && state != STATE_SHOWING)
        return;

    lastAnswerCorrect = (direction == colorTargetDirection);

    if (lastAnswerCorrect)
    {
        correctCount++;
        if (pBuzzer)
            pBuzzer->playTone(2400, 100);
        if (pLeds)
        {
            pLeds->setStripColor(0, 80, 0);
        }
    }
    else
    {
        if (pBuzzer)
            pBuzzer->playTone(500, 250);
        if (pLeds)
        {
            pLeds->setStripColor(80, 0, 0);
        }
    }

    showFeedbackOled(lastAnswerCorrect);

    if (pBle && pBle->isConnected())
    {
        String payload = BleProtocol::serializeGameFeedback("color_quest", level, taskIndex + 1, lastAnswerCorrect, correctCount);
        pBle->sendTx(payload);
    }

    feedbackUntil = millis() + 700;
    state = STATE_FEEDBACK;
}

void ColorQuest::finishLevel()
{
    stopOutputs();

    float scoreFloat = (float)correctCount / 10.0f;
    uint8_t stars = 0;
    if (correctCount >= 8)
        stars = 3;
    else if (correctCount >= 5)
        stars = 2;
    else if (correctCount >= 1)
        stars = 1;

    result.gameId = GameId::COLOR_QUEST;
    result.level = level;
    result.score = scoreFloat;
    result.correctCount = correctCount;
    result.totalTasks = 10;
    result.completed = true;
    result.aborted = false;

    if (pBle && pBle->isConnected())
    {
        String payload = BleProtocol::serializeGameResult("color_quest", level, correctCount, stars, correctCount, 10);
        pBle->sendTx(payload);
    }

    Serial.printf("[ColorQuest] L%d Complete: %d/10, stars=%d, score=%.2f\n", level, correctCount, stars, scoreFloat);
    if (pBuzzer)
    {
        pBuzzer->playTone(scoreFloat >= 0.8f ? 2400 : 900, 150);
    }

    showResultOled();
    state = STATE_FINISHED;
    finished = true;
}

void ColorQuest::abort()
{
    stopOutputs();
    if (pBuzzer)
        pBuzzer->stop();

    state = STATE_IDLE;
    finished = true;
    result.aborted = true;

    if (pBle && pBle->isConnected())
    {
        JsonDocument response;
        response["type"] = "aborted";
        response["game"] = "color-quest";
        String payload;
        serializeJson(response, payload);
        pBle->sendTx(payload);
    }
}

void ColorQuest::showTaskPromptOled()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("CQ L%d  T%d/10", level, taskIndex + 1);
    display.setCursor(0, 16);
    display.println("Find the TARGET");
    display.println("colour region.");
    display.setCursor(0, 48);
    display.println("SHOW + ANSWER");
    display.setCursor(0, 58);
    display.println("F  R  B  L");
    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void ColorQuest::showAnswerOled()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("CQ L%d  T%d/10", level, taskIndex + 1);
    display.setTextSize(2);
    display.setCursor(22, 18);
    display.println("ANSWER");
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.println("Choose: F R B L");
    display.setCursor(0, 58);
    display.println("Time running!");
    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void ColorQuest::showFeedbackOled(bool correct)
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
    display.setCursor(24, 44);
    display.printf("Score: %d/10", correctCount);
    display.setCursor(8, 56);
    display.println("Next task...");
    pDisplay->drawBatteryOverlay(false);
    display.display();
}

void ColorQuest::showResultOled()
{
    if (!pDisplay || !pDisplay->isPresent())
        return;

    auto &display = pDisplay->getDisplay();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("LEVEL %d COMPLETE", level);
    display.setTextSize(2);
    display.setCursor(24, 18);
    display.printf("%d/10", correctCount);
    display.setTextSize(1);
    display.setCursor(0, 46);
    display.printf("Score: %d%%", (int)round(result.score * 100.0f));
    display.setCursor(0, 57);
    display.println("Double BTN2 to Exit");
    pDisplay->drawBatteryOverlay(false);
    display.display();
}
