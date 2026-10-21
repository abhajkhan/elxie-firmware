#ifndef ESP_NOW_MANAGER_H
#define ESP_NOW_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "EspNowProtocol.h"
#include "ElxiePairingProtocol.h"
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

    // ---- Remote pairing (Stage 5). The paired remote's MAC is kept in flash (NVS);
    // with nothing stored, the MAC given to begin() (BoardConfig REMOTE_MAC) is used.
    enum class PairResult : uint8_t { NONE, PAIRED, TIMEOUT, CANCELLED };
    void startPairing(uint32_t durationMs = 30000);
    void stopPairing();                       // cancel
    bool isPairing() const { return pairing; }
    uint32_t pairingMsLeft() const;
    bool takePairResult(PairResult &out);     // true once when pairing ends
    void forgetRemote();                      // back to the BoardConfig default
    const uint8_t *getPeerMac() const { return peerMac; }
    bool hasStoredRemote() const { return storedRemote; }
    bool remoteAllowed() const;  // false: never paired and REMOTE_REQUIRE_PAIRING
    // Paired remote's battery (its "ELXS" status packet, every 2 s). False if none in 10 s.
    bool remoteBattery(uint8_t &pct, uint16_t &mv) const;
    static void macToString(const uint8_t *mac, char *out);  // "AA:BB:CC:DD:EE:FF"
    // Latest packet without consuming it (Elxie Blocks reads the remote too).
    bool peekLegacyCommand(LegacyRemoteCommandPacket &packetOut);

    // Static callback wrapper for ESP-IDF/Arduino C callback signature
    static void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len);

private:
    EspNowManager() = default;
    void setPeer(const uint8_t *mac);
    void sendBeacon();

    uint8_t peerMac[6] = {0x18, 0xfe, 0x34, 0xf5, 0xe8, 0x66};
    bool initialized = false;

    volatile uint32_t lastRxMs = 0;
    volatile bool haveNewCmd = false;

    LegacyRemoteCommandPacket latestLegacyCmd = {0, 0, 0, 0, 0};
    portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;

    uint8_t defaultMac[6] = {0};
    bool storedRemote = false;
    volatile uint16_t remoteBattMv = 0;
    volatile uint8_t remoteBattPct = 0;
    volatile uint32_t remoteBattMs = 0;
    volatile bool pairing = false;
    uint32_t pairingEndMs = 0;
    uint32_t lastBeaconMs = 0;
    volatile bool pairRequestSeen = false;
    volatile bool resendConfirm = false; // paired remote missed our confirm: send it again
    uint8_t pairCandidate[6] = {0};
    PairResult pairResult = PairResult::NONE;
    bool pairResultPending = false;
};

#endif // ESP_NOW_MANAGER_H
