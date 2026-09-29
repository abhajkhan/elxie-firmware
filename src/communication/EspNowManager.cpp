#include "EspNowManager.h"

EspNowManager &EspNowManager::getInstance()
{
    static EspNowManager instance;
    return instance;
}

bool EspNowManager::begin(const uint8_t *remoteMac)
{
    if (remoteMac)
        memcpy(peerMac, remoteMac, 6);

    WiFi.mode(WIFI_STA);
    delay(100);

    Serial.print("ESP-NOW MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP-NOW Init Failed!");
        return false;
    }

    esp_now_register_recv_cb(EspNowManager::onDataRecv);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, peerMac, 6);
    peer.channel = 0;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK)
    {
        Serial.println("ESP-NOW add_peer failed!");
    }

    initialized = true;
    return true;
}

void EspNowManager::onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    EspNowManager &mgr = getInstance();

    for (int i = 0; i < 6; i++)
    {
        if (mac_addr[i] != mgr.peerMac[i])
            return;
    }

    if (len == sizeof(LegacyRemoteCommandPacket))
    {
        portENTER_CRITICAL(&mgr.dataMux);
        memcpy(&mgr.latestLegacyCmd, data, sizeof(LegacyRemoteCommandPacket));
        mgr.lastRxMs = millis();
        mgr.haveNewCmd = true;
        portEXIT_CRITICAL(&mgr.dataMux);
    }
}

void EspNowManager::update()
{
    // Connection health & status updates can be polled here
}

bool EspNowManager::sendFeedback(uint8_t alert)
{
    if (!initialized)
        return false;

    uint8_t buf[sizeof(LegacyRemoteFeedbackPacket)];
    size_t len = EspNowProtocol::encodeLegacyFeedback(alert, buf, sizeof(buf));
    if (len == 0)
        return false;

    return esp_now_send(peerMac, buf, len) == ESP_OK;
}

bool EspNowManager::sendFeedbackPacket(const RemoteFeedbackPacket &fb)
{
    if (!initialized)
        return false;

    uint8_t buf[sizeof(RemoteFeedbackPacket)];
    size_t len = EspNowProtocol::encodeTargetFeedback(fb, buf, sizeof(buf));
    if (len == 0)
        return false;

    return esp_now_send(peerMac, buf, len) == ESP_OK;
}

bool EspNowManager::isConnected() const
{
    return (millis() - lastRxMs) < 500;
}

bool EspNowManager::getLatestLegacyCommand(LegacyRemoteCommandPacket &packetOut)
{
    if (!haveNewCmd)
        return false;

    portENTER_CRITICAL(&dataMux);
    packetOut = latestLegacyCmd;
    haveNewCmd = false;
    portEXIT_CRITICAL(&dataMux);

    return true;
}

bool EspNowManager::getLatestInputEvent(InputEvent &eventOut)
{
    LegacyRemoteCommandPacket packet;
    if (!getLatestLegacyCommand(packet))
        return false;

    eventOut = EspNowProtocol::normalizeLegacyInput(packet);
    return true;
}
