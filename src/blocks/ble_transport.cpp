#include "ble_transport.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

namespace ble {

// UUIDs from spec/03-protocol.md (also in elxie-ui lib/ble/constants.ts)
static const char* kSvcUuid = "8d3a0001-5e1c-4a8e-9b2d-e1a1e0c0de01";
static const char* kRxUuid = "8d3a0002-5e1c-4a8e-9b2d-e1a1e0c0de01";
static const char* kTxUuid = "8d3a0003-5e1c-4a8e-9b2d-e1a1e0c0de01";

static BLEServer* s_server = nullptr;
static BLECharacteristic* s_tx = nullptr;
static BLE2902* s_cccd = nullptr;
static StreamBufferHandle_t s_rx = nullptr;
static volatile bool s_disconnected = false;
static volatile uint16_t s_mtu = 23;  // ATT MTU of the current connection

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    std::string v = c->getValue();  // Bluedroid task: just queue the bytes
    if (s_rx && !v.empty()) xStreamBufferSend(s_rx, v.data(), v.size(), 0);
  }
};
static RxCallbacks s_rxCallbacks;

void attach(BLEServer* server) {
  s_server = server;
  if (!s_rx) s_rx = xStreamBufferCreate(2048, 1);
  // 4 handles per characteristic (decl + value + CCCD) plus the service itself
  BLEService* svc = server->createService(BLEUUID(kSvcUuid), 12);
  BLECharacteristic* rx = svc->createCharacteristic(
      kRxUuid, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(&s_rxCallbacks);
  s_tx = svc->createCharacteristic(kTxUuid, BLECharacteristic::PROPERTY_NOTIFY);
  s_cccd = new BLE2902();
  s_tx->addDescriptor(s_cccd);
  svc->start();
}

void configureScanResponse(BLEAdvertising* adv, const char* name) {
  // The advertising packet keeps the game service; the scan response carries the
  // name and the Blocks service, so the stand-alone Blocks editor finds Elxie too.
  BLEAdvertisementData sr;
  sr.setName(name);
  sr.setCompleteServices(BLEUUID(kSvcUuid));
  adv->setScanResponseData(sr);
}

bool connected() {
  return s_server && s_server->getConnectedCount() > 0 && s_cccd && s_cccd->getNotifications();
}

size_t read(uint8_t* buf, size_t max) { return s_rx ? xStreamBufferReceive(s_rx, buf, max, 0) : 0; }

bool send(const uint8_t* data, size_t len) {
  if (!connected() || !s_tx) return false;
  const uint16_t mtu = s_mtu;
  const size_t chunk = (mtu > 23 && mtu <= 517) ? mtu - 3 : 20;
  for (size_t off = 0; off < len; off += chunk) {
    size_t n = len - off < chunk ? len - off : chunk;
    s_tx->setValue((uint8_t*)data + off, n);
    s_tx->notify();
  }
  return true;
}

void onMtu(uint16_t mtu) { s_mtu = mtu; }

void onDisconnect() {
  s_disconnected = true;
  s_mtu = 23;
  if (s_cccd) s_cccd->setNotifications(false);  // the next app must subscribe again
  if (s_rx) xStreamBufferReset(s_rx);
}

bool takeDisconnectEvent() {
  if (!s_disconnected) return false;
  s_disconnected = false;
  return true;
}

}  // namespace ble
