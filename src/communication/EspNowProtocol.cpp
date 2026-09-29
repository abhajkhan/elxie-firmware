#include "EspNowProtocol.h"
#include <string.h>

bool EspNowProtocol::parseLegacyPacket(const uint8_t *data, size_t len, LegacyRemoteCommandPacket &packetOut)
{
    if (!data || len != sizeof(LegacyRemoteCommandPacket))
        return false;

    memcpy(&packetOut, data, sizeof(LegacyRemoteCommandPacket));
    return true;
}

bool EspNowProtocol::parseTargetPacket(const uint8_t *data, size_t len, RemoteInputPacket &packetOut)
{
    if (!data || len != sizeof(RemoteInputPacket))
        return false;

    memcpy(&packetOut, data, sizeof(RemoteInputPacket));
    return true;
}

InputEvent EspNowProtocol::normalizeLegacyInput(const LegacyRemoteCommandPacket &packet)
{
    static uint8_t lastSw = 0;
    static uint8_t lastBtn1 = 0;
    static uint8_t lastBtn2 = 0;

    InputEvent event;
    event.source = InputSource::ESP_NOW;

    // Detect rising edge on switch (for SELECT / Horn)
    if (packet.sw && !lastSw)
    {
        event.type = InputEventType::ACTION_SELECT;
        lastSw = packet.sw;
        lastBtn1 = packet.btn1;
        lastBtn2 = packet.btn2;
        return event;
    }
    // Detect rising edge on btn1 (Front Button -> LED toggle)
    if (packet.btn1 && !lastBtn1)
    {
        event.type = InputEventType::BUTTON;
        event.data.button.buttonId = 1;
        event.data.button.pressed = true;
        lastSw = packet.sw;
        lastBtn1 = packet.btn1;
        lastBtn2 = packet.btn2;
        return event;
    }
    // Detect rising edge on btn2 (Bottom Button -> BACK)
    if (packet.btn2 && !lastBtn2)
    {
        event.type = InputEventType::BACK;
        lastSw = packet.sw;
        lastBtn1 = packet.btn1;
        lastBtn2 = packet.btn2;
        return event;
    }

    lastSw = packet.sw;
    lastBtn1 = packet.btn1;
    lastBtn2 = packet.btn2;

    event.type = InputEventType::JOYSTICK;

    // Apply exact legacy remote axis mapping & deadzone calibration:
    // Raw ADC ranges from 0 to 1024 (center ~512).
    int dx = packet.x - 512;
    int dy = packet.y - 512;

    if (abs(dx) < 150)
        dx = 0;
    if (abs(dy) < 150)
        dy = 0;

    // Correct axis orientation for navigation & drive:
    // Joystick UP (forward) => dy > 0
    // Joystick DOWN (backward) => dy < 0
    // Joystick LEFT => dx < 0
    // Joystick RIGHT => dx > 0
    int fwdAxis = dy; // invert Y so UP is positive
    int turnAxis = dx;

    // Map to normalized range [-255, 255]
    event.data.joystick.x = (int16_t)constrain(map(turnAxis, -512, 512, -200, 200), -255, 255);
    event.data.joystick.y = (int16_t)constrain(map(fwdAxis, -512, 512, -200, 200), -255, 255);

    return event;
}

InputEvent EspNowProtocol::normalizeTargetInput(const RemoteInputPacket &packet)
{
    InputEvent event;
    event.source = InputSource::ESP_NOW;
    event.type = InputEventType::JOYSTICK;

    // Map -100..100 joystick to normalized range
    event.data.joystick.x = ((int16_t)packet.joystickX * 255) / 100;
    event.data.joystick.y = ((int16_t)packet.joystickY * 255) / 100;

    return event;
}

size_t EspNowProtocol::encodeLegacyFeedback(uint8_t alert, uint8_t *bufferOut, size_t maxLen)
{
    if (!bufferOut || maxLen < sizeof(LegacyRemoteFeedbackPacket))
        return 0;

    LegacyRemoteFeedbackPacket fb = {alert};
    memcpy(bufferOut, &fb, sizeof(fb));
    return sizeof(fb);
}

size_t EspNowProtocol::encodeTargetFeedback(const RemoteFeedbackPacket &feedback, uint8_t *bufferOut, size_t maxLen)
{
    if (!bufferOut || maxLen < sizeof(RemoteFeedbackPacket))
        return 0;

    memcpy(bufferOut, &feedback, sizeof(feedback));
    return sizeof(feedback);
}
