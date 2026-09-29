#ifndef ESP_NOW_PROTOCOL_H
#define ESP_NOW_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "../core/InputEvent.h"
#include "../core/RobotTypes.h"

// Legacy ESP-NOW Remote Command Packet (from existing working remote)
typedef struct __attribute__((packed))
{
    int16_t x;
    int16_t y;
    uint8_t btn1;
    uint8_t btn2;
    uint8_t sw;
} LegacyRemoteCommandPacket;

// Legacy ESP-NOW Remote Feedback Packet
typedef struct __attribute__((packed))
{
    uint8_t alert;
} LegacyRemoteFeedbackPacket;

// Target SSoT Binary Input Packet
typedef struct __attribute__((packed))
{
    uint8_t version;
    uint8_t type;
    uint16_t sequence;
    int8_t joystickX;
    int8_t joystickY;
    uint16_t buttons;
    uint8_t battery;
    uint8_t flags;
} RemoteInputPacket;

// Target SSoT Binary Feedback Packet
typedef struct __attribute__((packed))
{
    uint8_t version;
    uint8_t type;
    uint16_t sequence;
    uint8_t pattern;
    uint8_t intensity;
    uint16_t durationMs;
    uint8_t flags;
} RemoteFeedbackPacket;

class EspNowProtocol
{
public:
    static bool parseLegacyPacket(const uint8_t *data, size_t len, LegacyRemoteCommandPacket &packetOut);
    static bool parseTargetPacket(const uint8_t *data, size_t len, RemoteInputPacket &packetOut);

    static InputEvent normalizeLegacyInput(const LegacyRemoteCommandPacket &packet);
    static InputEvent normalizeTargetInput(const RemoteInputPacket &packet);

    static size_t encodeLegacyFeedback(uint8_t alert, uint8_t *bufferOut, size_t maxLen);
    static size_t encodeTargetFeedback(const RemoteFeedbackPacket &feedback, uint8_t *bufferOut, size_t maxLen);
};

#endif // ESP_NOW_PROTOCOL_H
