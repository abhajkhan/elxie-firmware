#include "BleManager.h"

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHAR_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHAR_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

void BleManager::begin(const char *deviceName)
{
    BLEDevice::init(deviceName);
    BLEDevice::setMTU(247);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(this);

    BLEService *pService = pServer->createService(NUS_SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        NUS_CHAR_TX_UUID,
        BLECharacteristic::PROPERTY_NOTIFY);
    pTxCharacteristic->addDescriptor(new BLE2902());

    pRxCharacteristic = pService->createCharacteristic(
        NUS_CHAR_RX_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    pRxCharacteristic->setCallbacks(this);

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(NUS_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Nordic UART Service started & advertising (MTU 247 requested)");
}

void BleManager::onConnect(BLEServer *server)
{
    deviceConnected = true;
    clientReady = false;
    rxLineBuffer = "";
    havePendingLine = false;
    Serial.println("BLE Client connected.");
}

void BleManager::onDisconnect(BLEServer *server)
{
    deviceConnected = false;
    clientReady = false;
    rxLineBuffer = "";
    havePendingLine = false;
    Serial.println("BLE Client disconnected.");
    BLEDevice::startAdvertising();
}

void BleManager::onWrite(BLECharacteristic *characteristic)
{
    String value = String(characteristic->getValue().c_str());
    if (value.length() == 0)
        return;

    for (size_t i = 0; i < value.length(); i++)
    {
        char ch = value[i];
        if (ch == '\n' || ch == '\r')
        {
            if (rxLineBuffer.length() == 0)
                continue;

            pendingLine = rxLineBuffer;
            havePendingLine = true;
            Serial.print("[BLE RX Line]: ");
            Serial.println(pendingLine);
            rxLineBuffer = "";
        }
        else
        {
            if (rxLineBuffer.length() < 512)
                rxLineBuffer += ch;
        }
    }
}

void BleManager::update()
{
    // Periodic tasks / health checks can be added here if needed
}

bool BleManager::getPendingCommand(RobotCommand &cmdOut, InputEvent &eventOut, JsonDocument &rawDocOut)
{
    if (!havePendingLine)
        return false;

    String lineToProcess = pendingLine;
    havePendingLine = false;

    return BleProtocol::parseIncomingLine(lineToProcess, cmdOut, eventOut, rawDocOut);
}

void BleManager::sendTx(const String &data)
{
    if (deviceConnected && pTxCharacteristic)
    {
        String msg = data + "\n";
        pTxCharacteristic->setValue((uint8_t *)msg.c_str(), msg.length());
        pTxCharacteristic->notify();
        Serial.print("[BLE TX Sent]: ");
        Serial.print(msg);
    }
}
