#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <stdint.h>
#include <Arduino.h>
#include "../config/BoardConfig.h"

class MotorController
{
public:
    void begin();
    void driveMotorA(int speed, bool forward);
    void driveMotorB(int speed, bool forward);
    void driveASigned(int spd);
    void driveBSigned(int spd);
    void handleMove(const char *direction, int speed);
    void stop();
    void enable();

private:
    bool enabled = false;
};

#endif // MOTOR_CONTROLLER_H
