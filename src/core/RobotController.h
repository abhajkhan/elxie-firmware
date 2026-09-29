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

class LocalMenu; // Forward declaration

class RobotController
{
public:
    void begin(MotorController *motors, LedController *leds, DisplayController *display, BuzzerController *buzzer, LocalMenu *menu = nullptr);
    void update();

    void onOwnershipChanged(InputSource previousOwner, InputSource newOwner);
    void handleCommand(const RobotCommand &cmd);
    void handleInputEvent(const InputEvent &event);

    const RobotState &getState() const { return currentState; }

private:
    void transitionToState(TopLevelState newState);
    void stopAllMotion();

    MotorController *pMotors = nullptr;
    LedController *pLeds = nullptr;
    DisplayController *pDisplay = nullptr;
    BuzzerController *pBuzzer = nullptr;
    LocalMenu *pLocalMenu = nullptr;

    RobotState currentState;
};

#endif // ROBOT_CONTROLLER_H
