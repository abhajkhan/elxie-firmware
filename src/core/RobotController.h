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

    MotorController *pMotors = nullptr;
    LedController *pLeds = nullptr;
    DisplayController *pDisplay = nullptr;
    BuzzerController *pBuzzer = nullptr;
    LocalMenu *pLocalMenu = nullptr;
    GameManager *pGameManager = nullptr;

    RobotState currentState;
};

#endif // ROBOT_CONTROLLER_H
