#ifndef ESP_NOW_MANAGER_H
#define ESP_NOW_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "EspNowProtocol.h"
#include "../core/InputEvent.h"

class EspNowManager
{
public:
    static EspNowManager &getInstance();

    bool begin(const uint8_t *remoteMac);
    void update();

    bool sendFeedback(uint8_t alert);
    bool sendFeedbackPacket(const RemoteFeedbackPacket &fb);

    bool isConnected() const;
    uint32_t getLastRxMs() const { return lastRxMs; }

    bool getLatestLegacyCommand(LegacyRemoteCommandPacket &packetOut);
    bool getLatestInputEvent(InputEvent &eventOut);

    // Static callback wrapper for ESP-IDF/Arduino C callback signature
    static void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len);

private:
    EspNowManager() = default;

    uint8_t peerMac[6] = {0x18, 0xfe, 0x34, 0xf5, 0xe8, 0x66};
    bool initialized = false;

    volatile uint32_t lastRxMs = 0;
    volatile bool haveNewCmd = false;

    LegacyRemoteCommandPacket latestLegacyCmd = {0, 0, 0, 0, 0};
    portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;
};

#endif // ESP_NOW_MANAGER_H
