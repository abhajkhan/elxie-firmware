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
    default:
        Serial.printf("[GameManager] Unsupported game ID: %d\n", (int)gameId);
        return false;
    }

    return pActiveEngine->start(level, msgId);
}

void GameManager::runSequence()
{
    if (pActiveEngine)
    {
        if (pActiveEngine->getGameId() == GameId::REFLEX_DASH)
        {
            reflexDash.runSequence();
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
