#ifndef REFLEX_DASH_H
#define REFLEX_DASH_H

#include "GameEngine.h"
#include "../hardware/DisplayController.h"
#include "../hardware/LedController.h"
#include "../hardware/BuzzerController.h"
#include "../hardware/MotorController.h"
#include "../communication/BleManager.h"
#include <ArduinoJson.h>

class ReflexDash : public GameEngine
{
public:
    ReflexDash() = default;
    void begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble = nullptr, MotorController *motors = nullptr);

    GameId getGameId() const override { return GameId::REFLEX_DASH; }
    bool start(uint8_t level, const String &sessionId = "") override;
    bool startWithId(uint8_t level, const char *msgId);
    void update(const SensorSnapshot &sensors) override;
    void handleInput(const InputEvent &input) override;
    void runSequence();
    void abort() override;
    bool isFinished() const override { return finished; }
    GameResult getResult() const override { return result; }

private:
    void initLevelParams();
    void setupColorMapping();
    void sendGameStartedBle();
    void startPhase();
    void evaluatePhase();
    void finishLevel();
    void stopOutputs();

    void showOledStatus(const char *title, const char *subtitle);
    void showResultOled();
    void sendSignalChangeEvent(const char *signalStr, const char *colorStr, uint32_t durationMs);

    bool isGoColor(uint8_t colorIdx) const;
    void setColorRGB(uint8_t colorIdx);
    const char *colorName(uint8_t colorIdx) const;

    DisplayController *pDisplay = nullptr;
    LedController *pLeds = nullptr;
    BuzzerController *pBuzzer = nullptr;
    BleManager *pBle = nullptr;
    MotorController *pMotors = nullptr;

    enum State
    {
        RDS_IDLE,
        RDS_WAIT_SEQ,
        RDS_MAPPING,
        RDS_SIGNAL_CUE,
        RDS_FEEDBACK,
        RDS_FINISHED
    };

    State state = RDS_IDLE;
    uint8_t level = 1;
    String requestMsgId;

    uint8_t totalPhases = 6;
    uint16_t reactionWindowMs = 1000;
    uint8_t phaseIndex = 0;
    uint8_t correctCount = 0;

    uint8_t goColors[3] = {1, 0, 0};   // 1 = green
    uint8_t stopColors[3] = {0, 0, 0}; // 0 = red
    uint8_t numGoColors = 1;
    uint8_t numStopColors = 1;

    uint8_t currentSignalColor = 0;
    bool currentSignalIsGo = false;
    uint32_t currentPhaseDurationMs = 3000;
    unsigned long phaseStartMs = 0;
    unsigned long phaseUntilMs = 0;
    unsigned long feedbackUntilMs = 0;

    bool playerIsMoving = false;
    bool violationTriggered = false;
    bool phaseAnswered = false;

    bool finished = false;
    GameResult result;
};

#endif // REFLEX_DASH_H
