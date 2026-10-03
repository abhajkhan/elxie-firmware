#ifndef ROBOT_COMMAND_H
#define ROBOT_COMMAND_H

#include "RobotTypes.h"
#include <stdint.h>

enum class CommandType : uint8_t
{
    NONE = 0,
    DRIVE_DIRECT,
    STOP_MOTORS,
    SET_LED_COLOR,
    SET_LED_REGION,
    PLAY_BUZZER_TONE,
    START_GAME,
    ABORT_GAME,
    START_RC,
    EXIT_RC
};

struct RobotCommand
{
    CommandType type = CommandType::NONE;
    InputSource source = InputSource::NONE;

    struct DrivePayload
    {
        int16_t leftSpeed = 0;  // -255 to 255
        int16_t rightSpeed = 0; // -255 to 255
    } drive;

    struct LedPayload
    {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t region = 0;
    } led;

    struct BuzzerPayload
    {
        uint16_t frequency = 0;
        uint16_t durationMs = 0;
    } buzzer;

    struct GamePayload
    {
        GameId gameId = GameId::NONE;
        uint8_t level = 1;
        char reqId[64] = {0};
    } game;
};

#endif // ROBOT_COMMAND_H
