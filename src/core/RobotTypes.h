#ifndef ROBOT_TYPES_H
#define ROBOT_TYPES_H

#include <stdint.h>

enum class TopLevelState : uint8_t
{
    IDLE = 0,
    RC,
    GAME,
    BLOCKS // Elxie Blocks owns the robot (src/blocks)
};

enum class GameId : uint8_t
{
    NONE = 0,
    COLOR_QUEST,
    ECHO_MEMORY,
    REFLEX_DASH,
    DRIVING_PRO,
    INVERTED_DRIVE
};

enum class InputSource : uint8_t
{
    NONE = 0,
    BLE,
    ESP_NOW
};

enum class InputEventType : uint8_t
{
    NONE = 0,
    JOYSTICK,
    DIRECTIONAL,
    BUTTON,
    ACTION_SELECT,
    BACK,
    MENU_NAVIGATE,
    RUN_SEQ,
    PET,
    HONK
};

enum class MenuAction : uint8_t
{
    NONE = 0,
    NAV_UP,
    NAV_DOWN,
    NAV_LEFT,
    NAV_RIGHT,
    SELECT,
    BACK
};

enum StripRegion
{
    REGION_FRONT,
    REGION_BACK,
    REGION_LEFT,
    REGION_RIGHT
};

#endif // ROBOT_TYPES_H
