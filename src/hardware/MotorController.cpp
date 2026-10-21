#include "MotorController.h"
#include <string.h>

void MotorController::begin()
{
    pinMode(PIN_STBY, OUTPUT);
    pinMode(PIN_AIN1, OUTPUT);
    pinMode(PIN_AIN2, OUTPUT);
    pinMode(PIN_BIN1, OUTPUT);
    pinMode(PIN_BIN2, OUTPUT);
    pinMode(PIN_PWMA, OUTPUT);
    pinMode(PIN_PWMB, OUTPUT);

    digitalWrite(PIN_STBY, LOW);
    enabled = false;
}

void MotorController::driveMotorA(int speed, bool forward)
{
    if (MOTOR_A_INVERT)
        forward = !forward;
    digitalWrite(PIN_AIN1, forward ? HIGH : LOW);
    digitalWrite(PIN_AIN2, forward ? LOW : HIGH);
    analogWrite(PIN_PWMA, constrain(speed, 0, 255));
}

void MotorController::driveMotorB(int speed, bool forward)
{
    if (MOTOR_B_INVERT)
        forward = !forward;
    digitalWrite(PIN_BIN1, forward ? HIGH : LOW);
    digitalWrite(PIN_BIN2, forward ? LOW : HIGH);
    analogWrite(PIN_PWMB, constrain(speed, 0, 255));
}

void MotorController::driveASigned(int spd)
{
    enable();
    if (spd >= 0)
        driveMotorA(spd, true);
    else
        driveMotorA(-spd, false);
}

void MotorController::driveBSigned(int spd)
{
    enable();
    if (spd >= 0)
        driveMotorB(spd, true);
    else
        driveMotorB(-spd, false);
}

void MotorController::stop()
{
    analogWrite(PIN_PWMA, 0);
    analogWrite(PIN_PWMB, 0);
    digitalWrite(PIN_STBY, LOW);
    enabled = false;
}

void MotorController::enable()
{
    digitalWrite(PIN_STBY, HIGH);
    enabled = true;
}

void MotorController::handleMove(const char *direction, int speed)
{
    speed = constrain(speed, 0, 255);

    if (strcmp(direction, "forward") == 0 || strcmp(direction, "front") == 0)
    {
        enable();
        driveMotorA(speed, true);
        driveMotorB(speed, true);
    }
    else if (strcmp(direction, "backward") == 0 || strcmp(direction, "back") == 0)
    {
        enable();
        driveMotorA(speed, false);
        driveMotorB(speed, false);
    }
    else if (strcmp(direction, "right") == 0)
    {
        enable();
        driveMotorA(0, true);
        driveMotorB(speed, true);
    }
    else if (strcmp(direction, "left") == 0)
    {
        enable();
        driveMotorA(speed, true);
        driveMotorB(0, true);
    }
    else
    {
        Serial.print("Unknown movement direction: ");
        Serial.println(direction);
    }
}
