#ifndef INVERTED_DRIVE_H
#define INVERTED_DRIVE_H

#include "GameEngine.h"
#include "../hardware/DisplayController.h"
#include "../hardware/LedController.h"
#include "../hardware/BuzzerController.h"
#include "../hardware/MotorController.h"
#include "../communication/BleManager.h"
#include <ArduinoJson.h>

class InvertedDrive : public GameEngine
{
public:
    InvertedDrive() = default;
    void begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble = nullptr, MotorController *motors = nullptr);

    GameId getGameId() const override { return GameId::INVERTED_DRIVE; }
    bool start(uint8_t level, const String &sessionId = "") override;
    bool startWithId(uint8_t level, const char *msgId);
    void update(const SensorSnapshot &sensors) override;
    void handleInput(const InputEvent &input) override;
    void abort() override;
    bool isFinished() const override { return finished; }
    GameResult getResult() const override { return result; }

private:
    enum MappingConcept
    {
        CONCEPT_NORMAL = 0,
        CONCEPT_FWD_BWD,
        CONCEPT_STEERING,
        CONCEPT_FULL,
        CONCEPT_DYNAMIC
    };

    enum Direction
    {
        DIR_NONE = 0,
        DIR_FORWARD,
        DIR_BACKWARD,
        DIR_LEFT,
        DIR_RIGHT
    };

    void initLevelParams();
    void sendGameStartedBle();
    void sendGameStateTask(uint8_t taskNum, const char *statusStr);
    void sendGameStateLevel(const char *statusStr);
    void sendGameResultBle(const char *statusStr);
    void sendHapticWarningBle();

    Direction parseInputDirection(const InputEvent &input, int16_t &outSpeed);
    Direction mapDirection(Direction logicalDir);
    MappingConcept getActiveConcept() const;

    void updateSteeringTurnCount(Direction mappedDir);
    void evaluateObjectives();
    void handleCollision();
    void finishLevel(bool success);
    void stopOutputs();

    void showOledRunning();
    void showFeedbackOled(const char *msg);
    void showResultOled();

    DisplayController *pDisplay = nullptr;
    LedController *pLeds = nullptr;
    BuzzerController *pBuzzer = nullptr;
    BleManager *pBle = nullptr;
    MotorController *pMotors = nullptr;

    enum State
    {
        IDS_IDLE,
        IDS_RUNNING,
        IDS_FINISHED
    };

    State state = IDS_IDLE;
    uint8_t level = 1;
    String requestMsgId;

    MappingConcept baseConcept = CONCEPT_FWD_BWD;
    uint32_t levelTimerMs = 30000;
    uint8_t minTotalTurns = 3;
    uint8_t minLeftTurns = 1;
    uint8_t minRightTurns = 1;

    unsigned long levelStartMs = 0;
    unsigned long lastWarningMs = 0;

    Direction currentLogicalDir = DIR_NONE;
    Direction currentMappedDir = DIR_NONE;
    int16_t currentSpeed = 0;

    uint8_t lastMappedSteerDir = 0; // 0 = straight/stop, 1 = left, 2 = right
    uint8_t totalTurns = 0;
    uint8_t leftTurns = 0;
    uint8_t rightTurns = 0;

    uint32_t requiredSafeDriveMs = 15000; // 15s active safe driving required for Task 1
    uint32_t activeDriveMs = 0;          // Cumulative active driving duration (ms)
    unsigned long lastUpdateMs = 0;      // Timestamp of last update tick

    bool task1Passed = false; // Safe driving without collision for required duration
    bool task1Failed = false; // Set true when a collision occurs during Task 1
    bool task2Passed = false; // Required total turns
    bool task3Passed = false; // Required left/right turns
    uint8_t passedCount = 0;

    bool collisionOccurred = false;
    bool finished = false;
    GameResult result;
};

#endif // INVERTED_DRIVE_H
