#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

#include <ArduinoJson.h>
#include <WString.h>
#include "../core/RobotCommand.h"
#include "../core/InputEvent.h"
#include "../core/RobotTypes.h"

class BleProtocol
{
public:
    static bool parseIncomingLine(const String &line, RobotCommand &cmdOut, InputEvent &eventOut, JsonDocument &rawDocOut);
    
    static String serializeDeviceInfo(const char *deviceName, const char *firmwareVersion, bool bleConnected, bool remoteConnected);
    static String serializeState(const char *stateStr, const char *ownerStr, bool bleConnected, bool remoteConnected);
    static String serializeHelloAck(const char *msgId, bool accepted, uint8_t protocolVersion = 1);
    static String serializeResponse(const char *command, bool success, const char *message = nullptr);
    static String serializeError(const char *message);

private:
    static GameId parseGameId(const char *idStr);
};

#endif // BLE_PROTOCOL_H
