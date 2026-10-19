// Elxie remote side of robot <-> remote pairing (ESP8266, Stage 5).
// Used by REMOTE_espnow.ino and by the remote testbench (same file, via build_flags -I).
//
// The paired robot's MAC lives in EEPROM (flash emulation):
//   byte 0..1 = 'E','P'   byte 2..7 = robot MAC
// With nothing stored the sketch keeps using its fixed MAINBOT_MAC.
//
// Pairing: the robot (in pairing mode) broadcasts ELXB beacons. We answer the first
// one with ELXR (our MAC) until the robot replies ELXK, then save the robot's MAC.
#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

#include "ElxiePairingProtocol.h"

namespace elxpair {

static const int kEepromSize = 64;
static const int kAddr = 0;

inline void eepromBegin() {
  static bool started = false;
  if (!started) {
    EEPROM.begin(kEepromSize);
    started = true;
  }
}

// Robot MAC saved by an earlier pairing. Returns false (and leaves `mac` alone) if none.
inline bool loadRobotMac(uint8_t mac[6]) {
  eepromBegin();
  if (EEPROM.read(kAddr) != 'E' || EEPROM.read(kAddr + 1) != 'P') return false;
  uint8_t m[6];
  for (int i = 0; i < 6; ++i) m[i] = EEPROM.read(kAddr + 2 + i);
  if (!elxMacValid(m)) return false;
  memcpy(mac, m, 6);
  return true;
}

inline void saveRobotMac(const uint8_t mac[6]) {
  eepromBegin();
  EEPROM.write(kAddr, 'E');
  EEPROM.write(kAddr + 1, 'P');
  for (int i = 0; i < 6; ++i) EEPROM.write(kAddr + 2 + i, mac[i]);
  EEPROM.commit();
}

inline void forgetRobotMac() {
  eepromBegin();
  EEPROM.write(kAddr, 0xFF);
  EEPROM.write(kAddr + 1, 0xFF);
  EEPROM.commit();
}

inline void macToString(const uint8_t* m, char* out) {
  sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

// ---------------------------------------------------------------- pairing session
// The sketch's ESP-NOW receive callback must call onRecv() first and ignore the
// packet when it returns true.
struct Session {
  volatile bool active = false;
  volatile bool haveBeacon = false;
  volatile bool confirmed = false;
  uint8_t robot[6] = {0};
};
inline Session& session() {
  static Session s;
  return s;
}

inline bool onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  Session& s = session();
  if (elxPairIs(data, len, "ELXB", sizeof(ElxPairBeacon))) {
    if (s.active && !s.haveBeacon && elxMacValid(mac)) {
      memcpy(s.robot, mac, 6);  // the sender's address is the one we must send to
      s.haveBeacon = true;
    }
    return true;
  }
  if (elxPairIs(data, len, "ELXK", sizeof(ElxPairMsg))) {
    if (s.active && s.haveBeacon && memcmp(mac, s.robot, 6) == 0) s.confirmed = true;
    return true;
  }
  return false;
}

// Runs a pairing session (blocking). `tick` is called about every 10 ms (haptics,
// serial...). On success the robot's MAC is saved and copied to `outMac`.
inline bool run(uint32_t timeoutMs, uint8_t outMac[6], void (*tick)()) {
  Session& s = session();
  s.haveBeacon = false;
  s.confirmed = false;
  s.active = true;
  uint8_t me[6];
  WiFi.macAddress(me);
  ElxPairMsg req;
  memcpy(req.magic, "ELXR", 4);
  req.version = ELX_PAIR_VERSION;
  memcpy(req.mac, me, 6);

  const uint32_t t0 = millis();
  uint32_t lastReq = 0;
  bool peerAdded = false;
  while (millis() - t0 < timeoutMs) {
    if (s.haveBeacon && !peerAdded) {
      esp_now_add_peer(s.robot, ESP_NOW_ROLE_COMBO, 1, nullptr, 0);
      peerAdded = true;
    }
    if (peerAdded && millis() - lastReq >= 150) {
      lastReq = millis();
      esp_now_send(s.robot, (uint8_t*)&req, sizeof req);
    }
    if (s.confirmed) break;
    if (tick) tick();
    delay(10);
  }
  s.active = false;
  if (!s.confirmed) return false;
  saveRobotMac(s.robot);
  memcpy(outMac, s.robot, 6);
  return true;
}

}  // namespace elxpair
