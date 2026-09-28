#ifndef ROBOT_STATE_H
#define ROBOT_STATE_H

#include "RobotTypes.h"
#include <stdint.h>

struct RobotState
{
    TopLevelState mode = TopLevelState::IDLE;
    InputSource activeOwner = InputSource::NONE;
    GameId currentGame = GameId::NONE;

    bool bleConnected = false;
    bool espNowConnected = false;

    uint8_t batteryPercent = 100;
    bool batteryLow = false;

    int16_t leftMotorSpeed = 0;
    int16_t rightMotorSpeed = 0;
};

#endif // ROBOT_STATE_H
