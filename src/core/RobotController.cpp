#include "RobotController.h"
#include "LocalMenu.h"
#include <Arduino.h>

void RobotController::begin(MotorController *motors, LedController *leds, DisplayController *display, BuzzerController *buzzer, LocalMenu *menu, GameManager *gameManager)
{
    pMotors = motors;
    pLeds = leds;
    pDisplay = display;
    pBuzzer = buzzer;
    pLocalMenu = menu;
    pGameManager = gameManager;

    currentState.mode = TopLevelState::IDLE;
    currentState.activeOwner = InputSource::NONE;
    currentState.currentGame = GameId::NONE;
}

void RobotController::update(const SensorSnapshot &sensors, bool batteryLow)
{
    // State machine tick
    switch (currentState.mode)
    {
    case TopLevelState::IDLE:
        // Home screen (eyes / icon menu) is drawn by LocalMenu through the face engine.
        (void)batteryLow;
        break;
    case TopLevelState::RC:
        // RC active
        break;
    case TopLevelState::GAME:
        if (pGameManager)
        {
            pGameManager->update(sensors);
        }
        break;
    case TopLevelState::BLOCKS:
        // The block engine (src/blocks) drives motors, OLED, LEDs and buzzer.
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
    // A block program keeps running when the app or the remote comes or goes;
    // Blocks mode ends itself (src/blocks/blocks.cpp).
    if (currentState.mode == TopLevelState::BLOCKS)
    {
        currentState.activeOwner = newOwner;
        if (newOwner == InputSource::BLE && pLocalMenu)
            pLocalMenu->reset();
        return;
    }

    // 1. Invalidate previous joystick state & stop motion immediately for safety
    stopAllMotion();

    if (pLeds)
        pLeds->clearStripBuffer();
    if (pBuzzer)
        pBuzzer->stop();
    if (pGameManager && currentState.mode == TopLevelState::GAME)
        pGameManager->abortGame();

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

void RobotController::enterBlocks()
{
    if (pBuzzer)
        pBuzzer->stop();
    transitionToState(TopLevelState::BLOCKS); // aborts a running game
    stopAllMotion();
}

void RobotController::exitBlocks()
{
    if (currentState.mode != TopLevelState::BLOCKS)
        return;
    stopAllMotion();
    transitionToState(TopLevelState::IDLE);
}

void RobotController::goIdle()
{
    stopAllMotion();
    if (pBuzzer)
        pBuzzer->stop();
    transitionToState(TopLevelState::IDLE);
}

void RobotController::transitionToState(TopLevelState newState)
{
    if (currentState.mode == newState)
        return;

    Serial.printf("[RobotController] State transition: %d -> %d\n", (int)currentState.mode, (int)newState);
    if (currentState.mode == TopLevelState::GAME && pGameManager)
    {
        pGameManager->abortGame();
    }

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
            if (pGameManager)
            {
                pGameManager->startGame(cmd.game.gameId, cmd.game.level, cmd.game.reqId);
            }
        }
        break;

    case CommandType::ABORT_GAME:
        if (currentState.mode == TopLevelState::GAME)
        {
            if (pGameManager)
            {
                pGameManager->abortGame();
            }
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
    // In Blocks mode the joystick and buttons belong to the block program.
    if (currentState.mode == TopLevelState::BLOCKS)
        return;

    if (currentState.mode == TopLevelState::GAME && pGameManager)
    {
        pGameManager->handleInput(event);
        return;
    }

    if (event.source == InputSource::BLE && currentState.mode == TopLevelState::IDLE)
    {
        transitionToState(TopLevelState::RC);
    }

    if (currentState.mode == TopLevelState::RC && pMotors)
    {
        if (event.type == InputEventType::JOYSTICK)
        {
            int16_t left = event.data.joystick.y + event.data.joystick.x;
            int16_t right = event.data.joystick.y - event.data.joystick.x;

            pMotors->driveBSigned(constrain(left, -255, 255));
            pMotors->driveASigned(constrain(right, -255, 255));

            currentState.leftMotorSpeed = left;
            currentState.rightMotorSpeed = right;
        }
    }
    else if (event.type == InputEventType::ACTION_SELECT)
    {
        if (currentState.mode == TopLevelState::RC && pBuzzer)
        {
            // Horn: Play 1000Hz tone for 1000ms
            pBuzzer->playTone(1000, 1000);
        }
    }
    else if (event.type == InputEventType::BUTTON)
    {
        if (currentState.mode == TopLevelState::RC && pLeds)
        {
            if (event.data.button.buttonId == 1 && event.data.button.pressed)
            {
                static uint8_t colorState = 0;
                if (colorState == 0)
                    pLeds->setStripColor(255, 0, 0); // Red
                else if (colorState == 1)
                    pLeds->setStripColor(0, 255, 0); // Green
                else if (colorState == 2)
                    pLeds->setStripColor(0, 0, 255); // Blue
                else
                    pLeds->setStripColor(0, 0, 0); // Off

                colorState = (colorState + 1) % 4;
            }
        }
    }
}
