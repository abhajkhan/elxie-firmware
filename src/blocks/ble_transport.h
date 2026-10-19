// Elxie Blocks VM service on the game firmware's Bluetooth (Bluedroid) server.
// One write characteristic (RX) and one notify characteristic (TX), both carrying
// the binary frame stream from spec/03-protocol.md. It sits next to the game's
// Nordic UART service, so the web app reaches games and Blocks over one connection.
#pragma once
#include <stddef.h>
#include <stdint.h>

class BLEServer;
class BLEAdvertising;

namespace ble {
void attach(BLEServer* server);                     // adds + starts the VM service
void configureScanResponse(BLEAdvertising* adv, const char* name);  // name + VM service UUID
bool connected();                                   // connected AND subscribed to TX notifications
size_t read(uint8_t* buf, size_t max);              // non-blocking, bytes written by the app
bool send(const uint8_t* data, size_t len);         // split into MTU-sized notifications
void onMtu(uint16_t mtu);                           // called by BleManager
void onDisconnect();                                // called by BleManager
bool takeDisconnectEvent();                         // true once after each disconnect
}  // namespace ble
