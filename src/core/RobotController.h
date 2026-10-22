#ifndef ROBOT_CONTROLLER_H
#define ROBOT_CONTROLLER_H

#include "RobotTypes.h"
#include "RobotState.h"
#include "RobotCommand.h"
#include "InputEvent.h"
#include "../hardware/MotorController.h"
#include "../hardware/LedController.h"
#include "../hardware/DisplayController.h"
#include "../hardware/BuzzerController.h"
#include "../hardware/SensorManager.h"
#include "../games/GameManager.h"

class LocalMenu; // Forward declaration

class RobotController
{
public:
    void begin(MotorController *motors, LedController *leds, DisplayController *display, BuzzerController *buzzer, LocalMenu *menu = nullptr, GameManager *gameManager = nullptr);
    void update(const SensorSnapshot &sensors = SensorSnapshot{}, bool batteryLow = false);

    void onOwnershipChanged(InputSource previousOwner, InputSource newOwner);
    void handleCommand(const RobotCommand &cmd);
    void handleInputEvent(const InputEvent &event);

    const RobotState &getState() const { return currentState; }

    // Elxie Blocks (src/blocks/blocks.cpp)
    void enterBlocks();
    void exitBlocks();
    void goIdle(); // remote pairing: end RC / games and stop

private:
    void transitionToState(TopLevelState newState);
    void stopAllMotion();
    void rcDrive(int16_t left, int16_t right); // Free Ride: drive with the collision stop applied
    void rcObstacleUpdate(const SensorSnapshot &sensors);

    MotorController *pMotors = nullptr;
    LedController *pLeds = nullptr;
    DisplayController *pDisplay = nullptr;
    BuzzerController *pBuzzer = nullptr;
    LocalMenu *pLocalMenu = nullptr;
    GameManager *pGameManager = nullptr;

    RobotState currentState;

    // Free Ride collision stop (front ultrasonic sensor)
    int16_t rcReqLeft = 0, rcReqRight = 0; // what the joystick asks for
    uint8_t rcCloseCount = 0;              // consecutive readings at or below RC_OBSTACLE_STOP_CM
    uint8_t rcClearCount = 0;              // consecutive clear readings (needed to lift the stop)
    bool rcBlocked = false;                // forward driving is blocked
    unsigned long rcBackoffUntil = 0;      // automatic back-off running until this time (0 = none)
    bool rcNeedRelease = false;            // after a stop: forward must be let go of before it is allowed again
};

#endif // ROBOT_CONTROLLER_H
