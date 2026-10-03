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
    pActiveEngine = nullptr;
}

bool GameManager::startGame(GameId gameId, uint8_t level, const String &sessionId)
{
    return startGameWithId(gameId, level, "game-001");
}

bool GameManager::startGameWithId(GameId gameId, uint8_t level, const char *msgId)
{
    abortGame();

    if (gameId == GameId::COLOR_QUEST)
    {
        pActiveEngine = &colorQuest;
        return pActiveEngine->start(level, msgId);
    }
    else if (gameId == GameId::REFLEX_DASH)
    {
        pActiveEngine = &reflexDash;
        return reflexDash.startWithId(level, msgId);
    }

    Serial.printf("[GameManager] Unsupported game ID: %d\n", (int)gameId);
    return false;
}

void GameManager::runSequence()
{
    if (pActiveEngine && pActiveEngine->getGameId() == GameId::REFLEX_DASH)
    {
        reflexDash.runSequence();
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
