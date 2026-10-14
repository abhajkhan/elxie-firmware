#ifndef DRIVING_PRO_H
#define DRIVING_PRO_H

#include "GameEngine.h"
#include "../hardware/DisplayController.h"
#include "../hardware/LedController.h"
#include "../hardware/BuzzerController.h"
#include "../hardware/MotorController.h"
#include "../communication/BleManager.h"
#include <ArduinoJson.h>

enum class DrivingProState
{
    IDLE,
    WAIT_TASK_START,
    TASK_RUNNING,
    TASK_FEEDBACK,
    FINISHED
};

class DrivingPro : public GameEngine
{
public:
    DrivingPro() = default;
    void begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble = nullptr, MotorController *motors = nullptr);

    GameId getGameId() const override { return GameId::DRIVING_PRO; }
    bool start(uint8_t level, const String &sessionId = "") override;
    void startTask(uint8_t taskId, const char *msgId = "");
    void update(const SensorSnapshot &sensors) override;
    void handleInput(const InputEvent &input) override;
    void abort() override;
    bool isFinished() const override { return finished; }
    GameResult getResult() const override { return result; }

private:
    void initTaskParams();
    void sendGameStartedBle();
    void sendTaskStartedBle(const char *msgId);
    void sendTaskResultBle(bool passed);
    void sendGameOverBle(const char *reason);
    void sendGameResultBle();

    void updateMotors();
    void stopOutputs();

    void showOledStatus(const char *title, const char *subtitle);

    DisplayController *pDisplay = nullptr;
    LedController *pLeds = nullptr;
    BuzzerController *pBuzzer = nullptr;
    BleManager *pBle = nullptr;
    MotorController *pMotors = nullptr;

    DrivingProState state = DrivingProState::IDLE;
    uint8_t level = 1;
    String requestMsgId;
    bool finished = false;
    GameResult result;

    uint8_t currentTaskId = 1;
    uint32_t taskDurationMs = 7000;
    unsigned long taskStartMs = 0;
    unsigned long taskUntilMs = 0;
    unsigned long feedbackUntilMs = 0;

    enum SteeringDir
    {
        STEER_STRAIGHT,
        STEER_LEFT,
        STEER_RIGHT
    };
    SteeringDir currentSteering = STEER_STRAIGHT;
    SteeringDir previousSteering = STEER_STRAIGHT;

    uint8_t totalTurns = 0;
    uint8_t leftTurns = 0;
    uint8_t rightTurns = 0;

    uint8_t tasksCompleted = 0;
    bool currentTaskPassed = false;
};

#endif // DRIVING_PRO_H
