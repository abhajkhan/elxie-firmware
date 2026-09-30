#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include <Arduino.h>
#include <WString.h>
#include "../core/RobotTypes.h"
#include "../core/InputEvent.h"
#include "../hardware/SensorManager.h"

struct GameResult
{
    GameId gameId = GameId::NONE;
    uint8_t level = 1;
    float score = 0.0f;
    uint8_t correctCount = 0;
    uint8_t totalTasks = 0;
    bool completed = false;
    bool aborted = false;
};

class GameEngine
{
public:
    virtual ~GameEngine() = default;

    virtual GameId getGameId() const = 0;
    virtual bool start(uint8_t level, const String &sessionId = "") = 0;
    virtual void update(const SensorSnapshot &sensors) = 0;
    virtual void handleInput(const InputEvent &input) = 0;
    virtual void abort() = 0;
    virtual bool isFinished() const = 0;
    virtual GameResult getResult() const = 0;
};

#endif // GAME_ENGINE_H
