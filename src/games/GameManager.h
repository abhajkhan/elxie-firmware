#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "GameEngine.h"
#include "ColorQuest.h"
#include "../hardware/DisplayController.h"
#include "../hardware/LedController.h"
#include "../hardware/BuzzerController.h"
#include "../communication/BleManager.h"

class GameManager
{
public:
    void begin(DisplayController *display, LedController *leds, BuzzerController *buzzer, BleManager *ble = nullptr);

    bool startGame(GameId gameId, uint8_t level, const String &sessionId = "");
    void update(const SensorSnapshot &sensors);
    void handleInput(const InputEvent &input);
    void abortGame();

    bool isGameActive() const { return pActiveEngine != nullptr && !pActiveEngine->isFinished(); }
    GameId getCurrentGameId() const { return pActiveEngine ? pActiveEngine->getGameId() : GameId::NONE; }
    bool isGameFinished() const { return pActiveEngine ? pActiveEngine->isFinished() : false; }
    GameResult getGameResult() const { return pActiveEngine ? pActiveEngine->getResult() : GameResult{}; }
    GameEngine *getActiveEngine() const { return pActiveEngine; }

private:
    DisplayController *pDisplay = nullptr;
    LedController *pLeds = nullptr;
    BuzzerController *pBuzzer = nullptr;
    BleManager *pBle = nullptr;

    GameEngine *pActiveEngine = nullptr;
    ColorQuest colorQuest;
};

#endif // GAME_MANAGER_H
