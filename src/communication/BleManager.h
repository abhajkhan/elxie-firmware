#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WString.h>

#include "../config/BoardConfig.h"
#include "../core/RobotCommand.h"
#include "../core/InputEvent.h"
#include "BleProtocol.h"

class BleManager : public BLEServerCallbacks, public BLECharacteristicCallbacks
{
public:
    void begin(const char *deviceName = DEVICE_NAME);
    void update();

    bool isConnected() const { return deviceConnected; }
    bool isClientReady() const { return clientReady; }
    void setClientReady(bool ready) { clientReady = ready; }

    bool getPendingCommand(RobotCommand &cmdOut, InputEvent &eventOut, JsonDocument &rawDocOut);
    void sendTx(const String &data);

    // BLEServerCallbacks overrides
    void onConnect(BLEServer *server) override;
    void onDisconnect(BLEServer *server) override;
    void onMtuChanged(BLEServer *server, esp_ble_gatts_cb_param_t *param) override;

    // BLECharacteristicCallbacks overrides
    void onWrite(BLECharacteristic *characteristic) override;

private:
    BLEServer *pServer = nullptr;
    BLECharacteristic *pRxCharacteristic = nullptr;
    BLECharacteristic *pTxCharacteristic = nullptr;

    bool deviceConnected = false;
    bool clientReady = false;

    String rxLineBuffer;
    String pendingLine;
    volatile bool havePendingLine = false;
};

#endif // BLE_MANAGER_H
