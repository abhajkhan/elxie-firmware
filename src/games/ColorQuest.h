#ifndef COLOR_QUEST_H
#define COLOR_QUEST_H

#include "GameEngine.h"
#include "../hardware/DisplayController.h"
#include "../hardware/LedController.h"
#include "../hardware/BuzzerController.h"
#include "../communication/BleManager.h"
#include <ArduinoJson.h>

struct GameColor
{
    const char *name;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class ColorQuest : public GameEngine
{
public:
    ColorQuest() = default;
    void begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble = nullptr);

    GameId getGameId() const override { return GameId::COLOR_QUEST; }
    bool start(uint8_t level, const String &sessionId = "") override;
    void update(const SensorSnapshot &sensors) override;
    void handleInput(const InputEvent &input) override;
    void abort() override;
    bool isFinished() const override { return finished; }
    GameResult getResult() const override { return result; }

private:
    void buildTask();
    void illuminateTask();
    void sendTaskBle(const char *phase);
    void beginAnswerPhase();
    void answerDirection(int direction);
    void finishLevel();

    void showTaskPromptOled();
    void showAnswerOled();
    void showFeedbackOled(bool correct);
    void showResultOled();

    void stopOutputs();
    unsigned long getDisplayDuration(uint8_t level) const;
    unsigned long getAnswerTimeout(uint8_t level) const;
    int inputToDirection(const InputEvent &input);

    DisplayController *pDisplay = nullptr;
    LedController *pLeds = nullptr;
    BuzzerController *pBuzzer = nullptr;
    BleManager *pBle = nullptr;

    enum State
    {
        STATE_IDLE,
        STATE_SHOWING,
        STATE_WAIT_INPUT,
        STATE_FEEDBACK,
        STATE_FINISHED
    };

    State state = STATE_IDLE;
    uint8_t level = 1;
    String sessionId;

    int taskIndex = 0;
    int correctCount = 0;
    int colorOption[4] = {0, 1, 2, 3};
    int colorTargetDirection = 0;

    unsigned long showUntil = 0;
    unsigned long answerUntil = 0;
    unsigned long feedbackUntil = 0;
    bool lastAnswerCorrect = false;

    bool finished = false;
    GameResult result;
};

#endif // COLOR_QUEST_H
