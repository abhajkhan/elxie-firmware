// BLE GATT server (ESP-IDF NimBLE host): one write characteristic (RX) and one
// notify characteristic (TX), both carrying the byte stream from spec/03-protocol.md.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace ble {
void begin(const char* deviceName);
bool connected();                             // connected AND subscribed to TX notifications
size_t read(uint8_t* buf, size_t max);        // non-blocking, bytes written by the app
bool send(const uint8_t* data, size_t len);   // split into MTU-sized notifications
bool takeDisconnectEvent();                   // true once after each disconnect
}  // namespace ble
