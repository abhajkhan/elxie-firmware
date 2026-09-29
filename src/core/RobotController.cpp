#include "RobotController.h"
#include "LocalMenu.h"
#include <Arduino.h>

void RobotController::begin(MotorController *motors, LedController *leds, DisplayController *display, BuzzerController *buzzer, LocalMenu *menu)
{
    pMotors = motors;
    pLeds = leds;
    pDisplay = display;
    pBuzzer = buzzer;
    pLocalMenu = menu;

    currentState.mode = TopLevelState::IDLE;
    currentState.activeOwner = InputSource::NONE;
    currentState.currentGame = GameId::NONE;
}

void RobotController::update()
{
    // State machine tick
    switch (currentState.mode)
    {
    case TopLevelState::IDLE:
        // Idle ambient updates owned by display/hardware modules
        break;
    case TopLevelState::RC:
        // RC active
        break;
    case TopLevelState::GAME:
        // Game engine updates when integrated in Phase 7
        break;
    }
}

void RobotController::stopAllMotion()
{
    if (pMotors)
    {
        pMotors->stop();
    }
    currentState.leftMotorSpeed = 0;
    currentState.rightMotorSpeed = 0;
}

void RobotController::onOwnershipChanged(InputSource previousOwner, InputSource newOwner)
{
    // 1. Invalidate previous joystick state & stop motion immediately for safety
    stopAllMotion();

    if (pLeds)
        pLeds->clearStripBuffer();
    if (pBuzzer)
        pBuzzer->stop();

    currentState.activeOwner = newOwner;

    // If BLE connects at any point, reset/exit local OLED menu
    if (newOwner == InputSource::BLE && pLocalMenu)
    {
        pLocalMenu->reset();
    }

    // 2. Establish top-level mode based on new owner
    if (newOwner == InputSource::NONE)
    {
        transitionToState(TopLevelState::IDLE);
    }
    else if (newOwner == InputSource::BLE)
    {
        // BLE connection does NOT automatically start RC or GAME; stays ready until explicitly commanded
        transitionToState(TopLevelState::IDLE);
    }
    else if (newOwner == InputSource::ESP_NOW)
    {
        // ESP-NOW connection goes to IDLE so the menu takes over
        transitionToState(TopLevelState::IDLE);
    }
}

void RobotController::transitionToState(TopLevelState newState)
{
    if (currentState.mode == newState)
        return;

    Serial.printf("[RobotController] State transition: %d -> %d\n", (int)currentState.mode, (int)newState);
    currentState.mode = newState;

    if (newState == TopLevelState::IDLE)
    {
        currentState.currentGame = GameId::NONE;
        stopAllMotion();
    }
}

void RobotController::handleCommand(const RobotCommand &cmd)
{
    switch (cmd.type)
    {
    case CommandType::STOP_MOTORS:
        stopAllMotion();
        if (currentState.mode == TopLevelState::RC)
        {
            // Stay in RC, just zero speed
        }
        break;

    case CommandType::DRIVE_DIRECT:
        if (currentState.mode == TopLevelState::RC && pMotors)
        {
            pMotors->driveBSigned(cmd.drive.leftSpeed);
            pMotors->driveASigned(cmd.drive.rightSpeed);
            currentState.leftMotorSpeed = cmd.drive.leftSpeed;
            currentState.rightMotorSpeed = cmd.drive.rightSpeed;
        }
        break;

    case CommandType::SET_LED_COLOR:
        if (pLeds)
        {
            pLeds->setStripColor(cmd.led.r, cmd.led.g, cmd.led.b);
        }
        break;

    case CommandType::PLAY_BUZZER_TONE:
        if (pBuzzer)
        {
            pBuzzer->playTone(cmd.buzzer.frequency, cmd.buzzer.durationMs);
        }
        break;

    case CommandType::START_RC:
        if (currentState.activeOwner == cmd.source)
        {
            transitionToState(TopLevelState::RC);
        }
        break;

    case CommandType::EXIT_RC:
        if (currentState.mode == TopLevelState::RC)
        {
            stopAllMotion();
            transitionToState(TopLevelState::IDLE);
        }
        break;

    case CommandType::START_GAME:
        if (currentState.activeOwner == cmd.source)
        {
            transitionToState(TopLevelState::GAME);
            currentState.currentGame = cmd.game.gameId;
        }
        break;

    case CommandType::ABORT_GAME:
        if (currentState.mode == TopLevelState::GAME)
        {
            stopAllMotion();
            transitionToState(TopLevelState::IDLE);
        }
        break;

    default:
        break;
    }
}

void RobotController::handleInputEvent(const InputEvent &event)
{
    if (event.type == InputEventType::JOYSTICK)
    {
        if (currentState.mode == TopLevelState::RC && pMotors)
        {
            // Direct normalized joystick drive execution for RC mode
            int16_t left = event.data.joystick.y + event.data.joystick.x;
            int16_t right = event.data.joystick.y - event.data.joystick.x;

            pMotors->driveBSigned(constrain(left, -255, 255));
            pMotors->driveASigned(constrain(right, -255, 255));

            currentState.leftMotorSpeed = left;
            currentState.rightMotorSpeed = right;
        }
    }
}
