// Elxie robot <-> remote pairing over ESP-NOW (Stage 5).
// SAME FILE in: remote/firmware/src/ElxiePairingProtocol.h (remote, ESP8266; the remote testbench uses it too)
//               src/communication/ElxiePairingProtocol.h (robot, ESP32-S3)
//
//   robot  --BEACON (broadcast, every 200 ms while pairing)-->  remote
//   remote --REQUEST (to the beacon's sender)----------------->  robot   (remote's own MAC)
//   robot  --CONFIRM (to the remote)-------------------------->  remote  (robot saved the remote)
//   remote saves the robot's MAC when CONFIRM arrives.
//
// Lengths differ from the normal packets (remote->robot 7 bytes, robot->remote 1 byte),
// so old firmware ignores them.
#pragma once
#include <stdint.h>
#include <string.h>

#define ELX_PAIR_VERSION 1

typedef struct __attribute__((packed)) {
  char magic[4];      // "ELXB"
  uint8_t version;    // ELX_PAIR_VERSION
  uint8_t mac[6];     // robot's ESP-NOW (Wi-Fi STA) MAC
  char name[8];       // robot name, not 0-terminated if 8 chars
} ElxPairBeacon;      // 19 bytes

typedef struct __attribute__((packed)) {
  char magic[4];      // "ELXR" request (remote -> robot) or "ELXK" confirm (robot -> remote)
  uint8_t version;
  uint8_t mac[6];     // sender's own MAC
} ElxPairMsg;         // 11 bytes

static inline bool elxPairIs(const uint8_t* data, int len, const char* magic, int wantLen) {
  return len == wantLen && memcmp(data, magic, 4) == 0 && data[4] == ELX_PAIR_VERSION;
}
static inline bool elxMacValid(const uint8_t* m) {
  bool allZero = true, allFF = true;
  for (int i = 0; i < 6; ++i) {
    if (m[i] != 0x00) allZero = false;
    if (m[i] != 0xFF) allFF = false;
  }
  return !allZero && !allFF && !(m[0] & 0x01);  // unicast address
}
