#include "GameManager.h"
#include <Arduino.h>

void GameManager::begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble, MotorController *motors)
{
    pDisplay = display;
    pLeds = leds;
    pBuzzer = buzzer;
    pBle = ble;

    colorQuest.begin(display, leds, buzzer, ble);
    reflexDash.begin(display, leds, buzzer, ble, motors);
    echoMemory.begin(display, leds, buzzer, ble);
    drivingPro.begin(display, leds, buzzer, ble, motors);
    pActiveEngine = nullptr;
}

bool GameManager::startGame(GameId gameId, uint8_t level, const String &msgId)
{
    abortGame();

    switch (gameId)
    {
    case GameId::COLOR_QUEST:
        pActiveEngine = &colorQuest;
        break;
    case GameId::REFLEX_DASH:
        pActiveEngine = &reflexDash;
        break;
    case GameId::ECHO_MEMORY:
        pActiveEngine = &echoMemory;
        break;
    case GameId::DRIVING_PRO:
        pActiveEngine = &drivingPro;
        break;
    default:
        Serial.printf("[GameManager] Unsupported game ID: %d\n", (int)gameId);
        return false;
    }

    return pActiveEngine->start(level, msgId);
}

void GameManager::startTask(uint8_t taskId, const char *msgId)
{
    if (pActiveEngine && pActiveEngine->getGameId() == GameId::DRIVING_PRO)
    {
        drivingPro.startTask(taskId, msgId);
    }
}

void GameManager::runSequence()
{
    if (pActiveEngine)
    {
        if (pActiveEngine->getGameId() == GameId::REFLEX_DASH)
        {
            reflexDash.runSequence();
        }
        else if (pActiveEngine->getGameId() == GameId::DRIVING_PRO)
        {
            drivingPro.startTask(1, "run_seq");
        }
        else
        {
            InputEvent seqEvent;
            seqEvent.source = InputSource::BLE;
            seqEvent.type = InputEventType::RUN_SEQ;
            pActiveEngine->handleInput(seqEvent);
        }
    }
}

void GameManager::update(const SensorSnapshot &sensors)
{
    if (pActiveEngine && !pActiveEngine->isFinished())
    {
        pActiveEngine->update(sensors);
    }
}

void GameManager::handleInput(const InputEvent &input)
{
    if (pActiveEngine && !pActiveEngine->isFinished())
    {
        pActiveEngine->handleInput(input);
    }
}

void GameManager::abortGame()
{
    if (pActiveEngine)
    {
        pActiveEngine->abort();
        pActiveEngine = nullptr;
    }
}
