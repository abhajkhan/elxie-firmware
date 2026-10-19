#include "EspNowManager.h"
#include <Preferences.h>

// NVS: namespace "elxie", key "remote_mac" = 6 bytes (Stage 5 pairing)
static const char *kPrefsNs = "elxie";
static const char *kPrefsKey = "remote_mac";
static const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

EspNowManager &EspNowManager::getInstance()
{
    static EspNowManager instance;
    return instance;
}

bool EspNowManager::begin(const uint8_t *remoteMac)
{
    if (remoteMac)
        memcpy(peerMac, remoteMac, 6);
    memcpy(defaultMac, peerMac, 6);

    // A remote paired earlier (stored in flash) wins over the BoardConfig default.
    Preferences prefs;
    if (prefs.begin(kPrefsNs, true))
    {
        uint8_t stored[6];
        if (prefs.getBytes(kPrefsKey, stored, 6) == 6 && elxMacValid(stored))
        {
            memcpy(peerMac, stored, 6);
            storedRemote = true;
        }
        prefs.end();
    }

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

    // Broadcast peer for pairing beacons
    esp_now_peer_info_t bc = {};
    memcpy(bc.peer_addr, kBroadcast, 6);
    bc.channel = 0;
    bc.encrypt = false;
    esp_now_add_peer(&bc);

    char macStr[18];
    macToString(peerMac, macStr);
    Serial.printf("ESP-NOW remote: %s (%s)\n", macStr, storedRemote ? "paired" : "BoardConfig default");

    initialized = true;
    return true;
}

void EspNowManager::onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    EspNowManager &mgr = getInstance();

    // Pairing request from any remote, only while pairing mode is on
    if (mgr.pairing && elxPairIs(data, len, "ELXR", sizeof(ElxPairMsg)))
    {
        if (elxMacValid(mac_addr) && !mgr.pairRequestSeen)
        {
            memcpy(mgr.pairCandidate, mac_addr, 6);
            mgr.pairRequestSeen = true;
        }
        return;
    }
    // The remote we just paired with did not hear our confirm and is still asking.
    if (!mgr.pairing && elxPairIs(data, len, "ELXR", sizeof(ElxPairMsg)) && memcmp(mac_addr, mgr.peerMac, 6) == 0)
    {
        mgr.resendConfirm = true;
        return;
    }

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
    if (resendConfirm)
    {
        resendConfirm = false;
        ElxPairMsg ok;
        memcpy(ok.magic, "ELXK", 4);
        ok.version = ELX_PAIR_VERSION;
        WiFi.macAddress(ok.mac);
        esp_now_send(peerMac, (const uint8_t *)&ok, sizeof(ok));
    }
    if (!pairing)
        return;

    const uint32_t now = millis();
    if (pairRequestSeen)
    {
        // Save the remote, switch to it, and confirm a few times (ESP-NOW has no delivery guarantee).
        uint8_t mac[6];
        memcpy(mac, pairCandidate, 6);
        Preferences prefs;
        if (prefs.begin(kPrefsNs, false))
        {
            prefs.putBytes(kPrefsKey, mac, 6);
            prefs.end();
        }
        storedRemote = true;
        setPeer(mac);

        ElxPairMsg ok;
        memcpy(ok.magic, "ELXK", 4);
        ok.version = ELX_PAIR_VERSION;
        WiFi.macAddress(ok.mac);
        for (int i = 0; i < 5; i++)
        {
            esp_now_send(peerMac, (const uint8_t *)&ok, sizeof(ok));
            delay(20);
        }
        pairing = false;
        pairRequestSeen = false;
        pairResult = PairResult::PAIRED;
        pairResultPending = true;
        char macStr[18];
        macToString(mac, macStr);
        Serial.printf("PAIR OK remote %s\n", macStr);
        return;
    }
    if ((int32_t)(now - pairingEndMs) >= 0)
    {
        pairing = false;
        pairResult = PairResult::TIMEOUT;
        pairResultPending = true;
        Serial.println("PAIR TIMEOUT no remote answered");
        return;
    }
    if (now - lastBeaconMs >= 200)
    {
        lastBeaconMs = now;
        sendBeacon();
    }
}

void EspNowManager::sendBeacon()
{
    ElxPairBeacon b;
    memcpy(b.magic, "ELXB", 4);
    b.version = ELX_PAIR_VERSION;
    WiFi.macAddress(b.mac);
    memset(b.name, 0, sizeof(b.name));
    strncpy(b.name, "ELXIE", sizeof(b.name));
    esp_now_send(kBroadcast, (const uint8_t *)&b, sizeof(b));
}

void EspNowManager::startPairing(uint32_t durationMs)
{
    if (!initialized)
        return;
    pairRequestSeen = false;
    pairResultPending = false;
    pairingEndMs = millis() + durationMs;
    lastBeaconMs = 0;
    pairing = true;
    char me[18];
    uint8_t mac[6];
    WiFi.macAddress(mac);
    macToString(mac, me);
    Serial.printf("PAIR START robot %s, waiting %lu s for a remote\n", me, (unsigned long)(durationMs / 1000));
}

void EspNowManager::stopPairing()
{
    if (!pairing)
        return;
    pairing = false;
    pairResult = PairResult::CANCELLED;
    pairResultPending = true;
    Serial.println("PAIR CANCELLED");
}

uint32_t EspNowManager::pairingMsLeft() const
{
    if (!pairing)
        return 0;
    int32_t left = (int32_t)(pairingEndMs - millis());
    return left > 0 ? (uint32_t)left : 0;
}

bool EspNowManager::takePairResult(PairResult &out)
{
    if (!pairResultPending)
        return false;
    pairResultPending = false;
    out = pairResult;
    return true;
}

void EspNowManager::forgetRemote()
{
    Preferences prefs;
    if (prefs.begin(kPrefsNs, false))
    {
        prefs.remove(kPrefsKey);
        prefs.end();
    }
    storedRemote = false;
    setPeer(defaultMac);
    Serial.println("PAIR FORGOTTEN (back to the BoardConfig remote)");
}

void EspNowManager::setPeer(const uint8_t *mac)
{
    if (initialized && memcmp(mac, peerMac, 6) != 0)
        esp_now_del_peer(peerMac);
    portENTER_CRITICAL(&dataMux);
    memcpy(peerMac, mac, 6);
    lastRxMs = 0;
    haveNewCmd = false;
    portEXIT_CRITICAL(&dataMux);
    if (initialized && !esp_now_is_peer_exist(peerMac))
    {
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, peerMac, 6);
        peer.channel = 0;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
    }
}

void EspNowManager::macToString(const uint8_t *mac, char *out)
{
    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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

bool EspNowManager::peekLegacyCommand(LegacyRemoteCommandPacket &packetOut)
{
    if (lastRxMs == 0)
        return false;

    portENTER_CRITICAL(&dataMux);
    packetOut = latestLegacyCmd;
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
