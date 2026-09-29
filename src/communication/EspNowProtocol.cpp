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
    InputEvent event;
    event.source = InputSource::ESP_NOW;
    event.type = InputEventType::JOYSTICK;

    // Map 0..1024 ADC values around 512 center to normalized -255..255
    event.data.joystick.x = (packet.x - 512) / 2;
    event.data.joystick.y = (packet.y - 512) / 2;

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
