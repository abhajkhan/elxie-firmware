#ifndef ECHO_MEMORY_H
#define ECHO_MEMORY_H

#include "GameEngine.h"
#include "../hardware/DisplayController.h"
#include "../hardware/LedController.h"
#include "../hardware/BuzzerController.h"
#include "../communication/BleManager.h"
#include <ArduinoJson.h>

enum EchoMemoryAction
{
    EMA_UP = 0,    // Red in fixed mapping
    EMA_RIGHT = 1, // Yellow in fixed mapping
    EMA_DOWN = 2,  // Green in fixed mapping
    EMA_LEFT = 3,  // Blue in fixed mapping
    EMA_PET = 4,   // Purple in fixed mapping
    EMA_HONK = 5   // White in fixed mapping
};

enum class EchoMemoryState
{
    IDLE,
    MAPPING,
    FLASHING,
    WAIT,
    USER_INPUT,
    RESULT
};

class EchoMemory : public GameEngine
{
public:
    EchoMemory() = default;
    void begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble = nullptr);

    GameId getGameId() const override { return GameId::ECHO_MEMORY; }
    bool start(uint8_t level, const String &sessionId = "") override;
    void update(const SensorSnapshot &sensors) override;
    void handleInput(const InputEvent &input) override;
    void abort() override;
    bool isFinished() const override { return finished; }
    GameResult getResult() const override { return result; }

private:
    void startFlashingSequence();
    void showAction(uint8_t action);
    void playHonkSound();
    void stopOutputs();

    void answerAction(uint8_t action);
    void finishLevel();

    void showMappingOled();
    void showFlashingOled();
    void showWaitOled();
    void showInputOled();
    void showFeedbackOled(bool correct);
    void showResultOled();

    int parseInputToAction(const InputEvent &input);
    void sendStartedBleResponse();

    DisplayController *pDisplay = nullptr;
    LedController *pLeds = nullptr;
    BuzzerController *pBuzzer = nullptr;
    BleManager *pBle = nullptr;

    EchoMemoryState state = EchoMemoryState::IDLE;
    uint8_t level = 1;
    String sessionId;

    static const uint8_t MAX_SEQUENCE_LENGTH = 8;
    uint8_t sequence[MAX_SEQUENCE_LENGTH];
    uint8_t sequenceLength = 4;
    unsigned int flashInterval = 3000;
    uint8_t actionCount = 4;
    bool fullStrip = false;

    // 1-to-1 mapping from action ID (0..5) to color ID (0..5)
    uint8_t actionColor[6] = {0, 1, 2, 3, 4, 5};

    uint8_t flashIndex = 0;
    bool flashInGap = false;
    unsigned long phaseUntil = 0;

    uint8_t inputIndex = 0;
    uint8_t wrongCount = 0;
    unsigned long feedbackUntil = 0;

    bool lastTouchActive = false;

    bool finished = false;
    GameResult result;
};

#endif // ECHO_MEMORY_H
