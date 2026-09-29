#include "BleProtocol.h"
#include <string.h>

GameId BleProtocol::parseGameId(const char *idStr)
{
    if (!idStr)
        return GameId::NONE;
    if (strcmp(idStr, "color_quest") == 0)
        return GameId::COLOR_QUEST;
    if (strcmp(idStr, "echo_memory") == 0)
        return GameId::ECHO_MEMORY;
    if (strcmp(idStr, "reflex_arc") == 0)
        return GameId::REFLEX_ARC;
    if (strcmp(idStr, "driving_pro") == 0)
        return GameId::DRIVING_PRO;
    if (strcmp(idStr, "inverted_drive") == 0)
        return GameId::INVERTED_DRIVE;
    return GameId::NONE;
}

bool BleProtocol::parseIncomingLine(const String &line, RobotCommand &cmdOut, InputEvent &eventOut, JsonDocument &rawDocOut)
{
    cmdOut = RobotCommand();
    eventOut = InputEvent();

    DeserializationError error = deserializeJson(rawDocOut, line);
    if (error)
        return false;

    cmdOut.source = InputSource::BLE;
    eventOut.source = InputSource::BLE;

    const char *type = rawDocOut["type"];
    const char *command = rawDocOut["command"];

    // 1. Direct motion/hardware commands from legacy file 1 contract
    if (type && strcmp(type, "move") == 0)
    {
        cmdOut.type = CommandType::DRIVE_DIRECT;
        const char *dir = rawDocOut["direction"] | "stop";
        int speed = rawDocOut["speed"] | 180;
        if (strcmp(dir, "forward") == 0 || strcmp(dir, "front") == 0)
        {
            cmdOut.drive.leftSpeed = speed;
            cmdOut.drive.rightSpeed = speed;
        }
        else if (strcmp(dir, "backward") == 0 || strcmp(dir, "back") == 0)
        {
            cmdOut.drive.leftSpeed = -speed;
            cmdOut.drive.rightSpeed = -speed;
        }
        else if (strcmp(dir, "left") == 0)
        {
            cmdOut.drive.leftSpeed = 0;
            cmdOut.drive.rightSpeed = speed;
        }
        else if (strcmp(dir, "right") == 0)
        {
            cmdOut.drive.leftSpeed = speed;
            cmdOut.drive.rightSpeed = 0;
        }
        else
        {
            cmdOut.type = CommandType::STOP_MOTORS;
        }
        return true;
    }

    if (type && strcmp(type, "stop") == 0)
    {
        cmdOut.type = CommandType::STOP_MOTORS;
        return true;
    }

    if (type && strcmp(type, "led_set") == 0)
    {
        cmdOut.type = CommandType::SET_LED_COLOR;
        cmdOut.led.r = rawDocOut["r"] | 0;
        cmdOut.led.g = rawDocOut["g"] | 0;
        cmdOut.led.b = rawDocOut["b"] | 0;
        return true;
    }

    if (type && strcmp(type, "buzzer") == 0)
    {
        cmdOut.type = CommandType::PLAY_BUZZER_TONE;
        cmdOut.buzzer.frequency = rawDocOut["frequency"] | 2000;
        cmdOut.buzzer.durationMs = rawDocOut["duration"] | 200;
        return true;
    }

    // 2. Structured JSON SSoT Command Messages
    if (type && strcmp(type, "command") == 0 && command)
    {
        if (strcmp(command, "rc_start") == 0)
        {
            cmdOut.type = CommandType::NONE;
            return true;
        }
        if (strcmp(command, "rc_stop") == 0)
        {
            cmdOut.type = CommandType::STOP_MOTORS;
            return true;
        }
        if (strcmp(command, "rc_input") == 0 || strcmp(command, "game_input") == 0)
        {
            eventOut.type = InputEventType::JOYSTICK;
            JsonObject payload = rawDocOut["payload"];
            if (!payload.isNull())
            {
                eventOut.data.joystick.x = payload["x"] | 0;
                eventOut.data.joystick.y = payload["y"] | 0;
            }
            return true;
        }
        if (strcmp(command, "game_start") == 0)
        {
            cmdOut.type = CommandType::START_GAME;
            JsonObject payload = rawDocOut["payload"];
            if (!payload.isNull())
            {
                const char *gIdStr = payload["gameId"] | "";
                cmdOut.game.gameId = parseGameId(gIdStr);
                cmdOut.game.level = payload["level"] | 1;
            }
            return true;
        }
        if (strcmp(command, "game_abort") == 0)
        {
            cmdOut.type = CommandType::ABORT_GAME;
            return true;
        }
    }

    return true; // Valid JSON parsed even if command is unmapped/informational
}

String BleProtocol::serializeDeviceInfo(const char *deviceName, const char *firmwareVersion, bool bleConnected, bool remoteConnected)
{
    JsonDocument doc;
    doc["type"] = "device_info";
    doc["device"] = deviceName;
    doc["version"] = firmwareVersion;
    doc["ble_connected"] = bleConnected;
    doc["esp_now_connected"] = remoteConnected;
    String out;
    serializeJson(doc, out);
    return out;
}

String BleProtocol::serializeState(const char *stateStr, const char *ownerStr, bool bleConnected, bool remoteConnected)
{
    JsonDocument doc;
    doc["type"] = "state";
    doc["state"] = stateStr;
    doc["controlOwner"] = ownerStr;
    doc["bleConnected"] = bleConnected;
    doc["remoteConnected"] = remoteConnected;
    String out;
    serializeJson(doc, out);
    return out;
}

String BleProtocol::serializeHelloAck(const char *msgId, bool accepted, uint8_t protocolVersion)
{
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "hello_ack";
    if (msgId && strlen(msgId) > 0)
        doc["id"] = msgId;
    else
        doc["id"] = "hello-ack";

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["device"] = "elxie-robot";
    payload["protocolVersion"] = protocolVersion;
    payload["accepted"] = accepted;

    String out;
    serializeJson(doc, out);
    return out;
}

String BleProtocol::serializeResponse(const char *command, bool success, const char *message)
{
    JsonDocument doc;
    doc["type"] = "response";
    doc["command"] = command;
    doc["success"] = success;
    if (message)
        doc["message"] = message;
    String out;
    serializeJson(doc, out);
    return out;
}

String BleProtocol::serializeError(const char *message)
{
    JsonDocument doc;
    doc["type"] = "error";
    doc["message"] = message;
    String out;
    serializeJson(doc, out);
    return out;
}
