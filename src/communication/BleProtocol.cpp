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
    if (strcmp(idStr, "reflex_dash") == 0)
        return GameId::REFLEX_DASH;
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

    // 2. Structured JSON SSoT Command Messages
    if (type && strcmp(type, "command") == 0 && command)
    {

        if (strcmp(command, "game_start") == 0)
        {
            cmdOut.type = CommandType::START_GAME;
            const char *idStr = rawDocOut["id"] | "";
            snprintf(cmdOut.game.reqId, sizeof(cmdOut.game.reqId), "%s", idStr);
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
        if (strcmp(command, "led_set") == 0)
        {
            cmdOut.type = CommandType::SET_LED_COLOR;
            cmdOut.led.r = rawDocOut["r"] | 0;
            cmdOut.led.g = rawDocOut["g"] | 0;
            cmdOut.led.b = rawDocOut["b"] | 0;
            return true;
        }

        if (strcmp(command, "honk") == 0)
        {
            cmdOut.type = CommandType::PLAY_BUZZER_TONE;
            cmdOut.buzzer.frequency = rawDocOut["frequency"] | 2000;
            cmdOut.buzzer.durationMs = rawDocOut["duration"] | 200;
            return true;
        }
    }

    // 3. Mobile input format:
    // {"v":1,"type":"input","ts":1790797156355,"payload":{"inputType":"joystick","dir":"up","magnitude":0.96}}
    if (type && strcmp(type, "input") == 0)
    {
        JsonObject payload = rawDocOut["payload"];
        if (!payload.isNull())
        {
            const char *inputType = payload["inputType"] | "";
            if (strcmp(inputType, "joystick") == 0)
            {
                eventOut.type = InputEventType::JOYSTICK;
                const char *dir = payload["dir"] | "";
                float mag = payload["magnitude"] | 1.0f;
                mag = constrain(mag, 0.0f, 1.0f);
                int16_t speed = (int16_t)(mag * 255.0f);

                if (strcmp(dir, "up") == 0 || strcmp(dir, "forward") == 0)
                {
                    eventOut.data.joystick.x = 0;
                    eventOut.data.joystick.y = speed;
                }
                else if (strcmp(dir, "down") == 0 || strcmp(dir, "backward") == 0)
                {
                    eventOut.data.joystick.x = 0;
                    eventOut.data.joystick.y = -speed;
                }
                else if (strcmp(dir, "left") == 0)
                {
                    eventOut.data.joystick.x = -speed;
                    eventOut.data.joystick.y = 0;
                }
                else if (strcmp(dir, "right") == 0)
                {
                    eventOut.data.joystick.x = speed;
                    eventOut.data.joystick.y = 0;
                }
                else
                {
                    eventOut.data.joystick.x = 0;
                    eventOut.data.joystick.y = 0;
                    cmdOut.type = CommandType::STOP_MOTORS;
                }

                if (!payload["x"].isNull() && !payload["y"].isNull())
                {
                    eventOut.data.joystick.x = payload["x"] | 0;
                    eventOut.data.joystick.y = payload["y"] | 0;
                }

                return true;
            }
        }
    }

    return true; // Valid JSON parsed even if command is unmapped/informational
}

String BleProtocol::serializeDeviceInfo(const char *msgId, const char *macAddress, const char *deviceName, const char *firmwareVersion, bool bleConnected, bool remoteConnected, bool oledPresent, bool mpuPresent)
{
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "device_info";
    if (msgId && strlen(msgId) > 0)
    {
        doc["id"] = msgId;
    }

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["deviceId"] = macAddress ? macAddress : "7C4FAD214341";
    payload["name"] = deviceName ? deviceName : "elxie-robot";
    payload["model"] = "ESP32-S3 N16R8";
    payload["firmware"] = firmwareVersion ? firmwareVersion : "1.0.0";
    payload["protocolVersion"] = 1;

    JsonObject caps = payload["capabilities"].to<JsonObject>();
    caps["ble"] = bleConnected;
    caps["espNow"] = remoteConnected;
    caps["oled"] = oledPresent;
    caps["led"] = true;
    caps["sensors"] = mpuPresent;

    JsonArray games = caps["games"].to<JsonArray>();
    games.add("color_quest");
    games.add("echo_memory");
    games.add("reflex_dash");
    games.add("driving_pro");
    games.add("inverted_drive");

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

String BleProtocol::serializePong(const char *msgId)
{
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "pong";
    if (msgId && strlen(msgId) > 0)
        doc["id"] = msgId;
    String out;
    serializeJson(doc, out);
    return out;
}

String BleProtocol::serializeTelemetry(const SensorSnapshot &snapshot, InputSource activeOwner, bool bleConnected, bool remoteConnected)
{
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "telemetry";
    doc["timestamp"] = snapshot.timestamp > 0 ? snapshot.timestamp : millis();
    doc["direction"] = snapshot.heading;

    JsonObject dist = doc["distance"].to<JsonObject>();
    dist["front"] = snapshot.frontDistanceCm;

    JsonObject obs = doc["obstacle"].to<JsonObject>();
    obs["frontLeft"] = snapshot.obstacleFrontLeft;
    obs["frontRight"] = snapshot.obstacleFrontRight;
    obs["rearLeft"] = snapshot.obstacleRearLeft;
    obs["rearRight"] = snapshot.obstacleRearRight;

    JsonObject motion = doc["motion"].to<JsonObject>();
    motion["sudden"] = snapshot.suddenMotion;

    JsonObject pit = doc["pit"].to<JsonObject>();
    pit["detected"] = snapshot.pitDetected;

    JsonObject batt = doc["battery"].to<JsonObject>();
    batt["robot"] = snapshot.batteryPercent;

    JsonObject touch = doc["touch"].to<JsonObject>();
    touch["event"] = snapshot.touchActive ? "touched" : "none";

    JsonObject ctrl = doc["controller"].to<JsonObject>();
    if (activeOwner == InputSource::BLE)
        ctrl["active"] = "mobile";
    else if (activeOwner == InputSource::ESP_NOW)
        ctrl["active"] = "remote";
    else
        ctrl["active"] = "none";

    ctrl["bleConnected"] = bleConnected;
    ctrl["remoteConnected"] = remoteConnected;

    String out;
    serializeJson(doc, out);
    return out;
}

String BleProtocol::serializeGameStarted(const char *msgId, const char *gameIdStr, uint8_t level)
{
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "response";
    doc["response"] = "game_started";
    if (msgId && strlen(msgId) > 0)
        doc["id"] = msgId;

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["gameId"] = gameIdStr ? gameIdStr : "color_quest";
    payload["level"] = level;
    payload["status"] = "running";

    String out;
    serializeJson(doc, out);
    return out;
}

String BleProtocol::serializeGameFeedback(const char *gameIdStr, uint8_t level, uint8_t taskId, bool correct, uint8_t correctCount)
{
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "response";
    doc["response"] = "game_feedback";
    doc["gameId"] = gameIdStr ? gameIdStr : "color_quest";
    doc["level"] = level;
    doc["taskId"] = taskId;
    doc["correct"] = correct;
    doc["correctCount"] = correctCount;

    String out;
    serializeJson(doc, out);
    return out;
}

String BleProtocol::serializeGameResult(const char *gameIdStr, uint8_t level, uint8_t score, uint8_t stars, uint8_t tasksCompleted, uint8_t tasksTotal)
{
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "response";
    doc["response"] = "game_result";
    doc["gameId"] = gameIdStr ? gameIdStr : "color_quest";
    doc["level"] = level;
    doc["score"] = score;
    doc["stars"] = stars;
    doc["tasksCompleted"] = tasksCompleted;
    doc["tasksTotal"] = tasksTotal;

    String out;
    serializeJson(doc, out);
    return out;
}

String BleProtocol::serializeGameAborted(const char *msgId)
{
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "response";
    doc["response"] = "game_aborted";
    if (msgId && strlen(msgId) > 0)
        doc["id"] = msgId;

    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["status"] = "aborted";

    String out;
    serializeJson(doc, out);
    return out;
}

const char *BleProtocol::gameIdToString(GameId id)
{
    switch (id)
    {
    case GameId::COLOR_QUEST:
        return "color_quest";
    case GameId::ECHO_MEMORY:
        return "echo_memory";
    case GameId::REFLEX_DASH:
        return "reflex_dash";
    case GameId::DRIVING_PRO:
        return "driving_pro";
    case GameId::INVERTED_DRIVE:
        return "inverted_drive";
    default:
        return "unknown";
    }
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
