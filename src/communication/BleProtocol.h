#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

#include <ArduinoJson.h>
#include <WString.h>
#include "../core/RobotCommand.h"
#include "../core/InputEvent.h"
#include "../core/RobotTypes.h"
#include "../hardware/SensorManager.h"

class BleProtocol
{
public:
    static bool parseIncomingLine(const String &line, RobotCommand &cmdOut, InputEvent &eventOut, JsonDocument &rawDocOut);
    
    static String serializeDeviceInfo(const char *msgId, const char *macAddress, const char *deviceName, const char *firmwareVersion, bool bleConnected, bool remoteConnected, bool oledPresent = true, bool mpuPresent = true);
    static String serializeState(const char *stateStr, const char *ownerStr, bool bleConnected, bool remoteConnected);
    static String serializeHelloAck(const char *msgId, bool accepted, uint8_t protocolVersion = 1);
    static String serializePong(const char *msgId);
    static String serializeTelemetry(const SensorSnapshot &snapshot, InputSource activeOwner, bool bleConnected, bool remoteConnected);
    static String serializeGameStarted(const char *msgId, const char *gameIdStr, uint8_t level);
    static String serializeTaskStarted(const char *msgId, const char *gameIdStr, uint8_t level, uint8_t taskId, uint32_t durationMs);
    static String serializeGameFeedback(const char *gameIdStr, uint8_t level, uint8_t taskId, bool correct, uint8_t correctCount);
    static String serializeTaskFeedback(const char *gameIdStr, uint8_t level, uint8_t taskId, bool correct, uint8_t correctCount);
    static String serializeTaskResult(const char *gameIdStr, uint8_t level, uint8_t taskId, const char *statusStr, uint8_t score, uint8_t stars);
    static String serializeGameOver(const char *gameIdStr, uint8_t level, const char *reasonStr, uint8_t tasksCompleted, uint8_t tasksTotal, uint8_t score, uint8_t stars);
    static String serializeGameResult(const char *gameIdStr, uint8_t level, uint8_t score, uint8_t stars, uint8_t tasksCompleted, uint8_t tasksTotal);
    static String serializeGameAborted(const char *msgId);
    static String serializeResponse(const char *command, bool success, const char *message = nullptr);
    static String serializeError(const char *message);

    static GameId parseGameId(const char *idStr);
    static const char *gameIdToString(GameId id);
};

#endif // BLE_PROTOCOL_H
