#ifndef CONTROLLER_MANAGER_H
#define CONTROLLER_MANAGER_H

#include "../core/RobotTypes.h"
#include "../core/RobotState.h"
#include "../core/RobotCommand.h"
#include "../core/InputEvent.h"

class RobotController; // Forward declaration

class ControllerManager
{
public:
    void begin(RobotController *controller);
    void update(bool bleConnected, bool espNowConnected);

    InputSource getActiveOwner() const { return activeOwner; }
    bool isInputAllowed(InputSource source) const;

    void processCommand(const RobotCommand &cmd);
    void processInputEvent(const InputEvent &event);

private:
    void transitionOwner(InputSource newOwner);

    RobotController *robotController = nullptr;
    InputSource activeOwner = InputSource::NONE;
    bool prevBleConnected = false;
    bool prevEspNowConnected = false;
};

#endif // CONTROLLER_MANAGER_H
