#include "GameManager.h"
#include <Arduino.h>

void GameManager::begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble)
{
    pDisplay = display;
    pLeds = leds;
    pBuzzer = buzzer;
    pBle = ble;

    colorQuest.begin(display, leds, buzzer, ble);
    pActiveEngine = nullptr;
}

bool GameManager::startGame(GameId gameId, uint8_t level, const String &sessionId)
{
    abortGame();

    if (gameId == GameId::COLOR_QUEST)
    {
        pActiveEngine = &colorQuest;
        return pActiveEngine->start(level, sessionId);
    }

    Serial.printf("[GameManager] Unsupported game ID: %d\n", (int)gameId);
    return false;
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
