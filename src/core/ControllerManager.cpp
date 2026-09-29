#include "ControllerManager.h"
#include "RobotController.h"
#include <Arduino.h>

void ControllerManager::begin(RobotController *controller)
{
    robotController = controller;
    activeOwner = InputSource::NONE;
    prevBleConnected = false;
    prevEspNowConnected = false;
}

void ControllerManager::update(bool bleConnected, bool espNowConnected)
{
    InputSource targetOwner = InputSource::NONE;

    // Strict priority resolution:
    // 1. BLE Mobile (if connected)
    // 2. ESP-NOW Remote (if connected and BLE is absent)
    // 3. None / IDLE (neither connected)
    if (bleConnected)
    {
        targetOwner = InputSource::BLE;
    }
    else if (espNowConnected)
    {
        targetOwner = InputSource::ESP_NOW;
    }
    else
    {
        targetOwner = InputSource::NONE;
    }

    if (targetOwner != activeOwner || bleConnected != prevBleConnected || espNowConnected != prevEspNowConnected)
    {
        transitionOwner(targetOwner);
        prevBleConnected = bleConnected;
        prevEspNowConnected = espNowConnected;
    }
}

void ControllerManager::transitionOwner(InputSource newOwner)
{
    InputSource oldOwner = activeOwner;
    activeOwner = newOwner;

    Serial.printf("[ControllerManager] Ownership transition: %d -> %d\n", (int)oldOwner, (int)newOwner);

    if (robotController)
    {
        robotController->onOwnershipChanged(oldOwner, newOwner);
    }
}

bool ControllerManager::isInputAllowed(InputSource source) const
{
    if (activeOwner == InputSource::NONE)
        return false;

    return source == activeOwner;
}

void ControllerManager::processCommand(const RobotCommand &cmd)
{
    if (!isInputAllowed(cmd.source))
    {
        Serial.printf("[ControllerManager] Command rejected from source %d (active owner: %d)\n", (int)cmd.source, (int)activeOwner);
        return;
    }

    if (robotController)
    {
        robotController->handleCommand(cmd);
    }
}

void ControllerManager::processInputEvent(const InputEvent &event)
{
    if (!isInputAllowed(event.source))
    {
        return;
    }

    if (robotController)
    {
        robotController->handleInputEvent(event);
    }
}
