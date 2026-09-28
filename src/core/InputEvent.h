#ifndef INPUT_EVENT_H
#define INPUT_EVENT_H

#include "RobotTypes.h"
#include <stdint.h>

struct InputEvent
{
    InputEventType type = InputEventType::NONE;
    InputSource source = InputSource::NONE;

    union {
        struct
        {
            int16_t x; // -255 to 255 or normalized analog range
            int16_t y;
        } joystick;

        struct
        {
            int8_t dirX; // -1, 0, 1
            int8_t dirY; // -1, 0, 1
        } directional;

        struct
        {
            uint8_t buttonId;
            bool pressed;
        } button;

        MenuAction menuAction;
    } data;
};

#endif // INPUT_EVENT_H
