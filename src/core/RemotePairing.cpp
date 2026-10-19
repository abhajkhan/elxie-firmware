#include "RemotePairing.h"

#include "../blocks/blocks.h"
#include "../communication/BleManager.h"
#include "../communication/EspNowManager.h"
#include "../config/BoardConfig.h"
#include "../hardware/BuzzerController.h"
#include "../hardware/DisplayController.h"
#include "../hardware/LedController.h"
#include "RobotController.h"

namespace pairing
{
    static DisplayController *pDisplay = nullptr;
    static LedController *pLeds = nullptr;
    static BuzzerController *pBuzzer = nullptr;
    static RobotController *pRobot = nullptr;
    static BleManager *pBle = nullptr;
    static uint32_t resultUntil = 0; // showing the result screen until then
    static uint32_t lastDraw = 0;
    static bool blink = false;
    static uint32_t bootMs = 0;
    static bool touchAtBoot = false;
    static char serialLine[32];
    static uint8_t serialLen = 0;

    static EspNowManager &radio() { return EspNowManager::getInstance(); }

    void begin(DisplayController &display, LedController &leds, BuzzerController &buzzer, RobotController &robot,
               BleManager &ble)
    {
        pDisplay = &display;
        pLeds = &leds;
        pBuzzer = &buzzer;
        pRobot = &robot;
        pBle = &ble;
    }

    String remoteMacString()
    {
        char s[18];
        EspNowManager::macToString(radio().getPeerMac(), s);
        return String(s);
    }

    static void screen(const char *big, const char *line1, const char *line2)
    {
        if (!pDisplay || !pDisplay->isPresent())
            return;
        Adafruit_SSD1306 &d = pDisplay->getDisplay();
        d.clearDisplay();
        d.setTextColor(SSD1306_WHITE);
        d.setTextSize(2);
        d.setCursor(0, 0);
        d.println(big);
        d.setTextSize(1);
        d.setCursor(0, 36);
        d.println(line1);
        d.setCursor(0, 50);
        d.println(line2);
        d.display();
    }

    static void sendJson(const String &json)
    {
        if (pBle && pBle->isConnected())
            pBle->sendTx(json);
    }

    void start()
    {
        blocks::deactivate();    // Blocks mode hands the robot back
        if (pRobot)
            pRobot->goIdle();    // ends RC / games, stops the motors
        radio().startPairing(30000);
        resultUntil = 0;
        lastDraw = 0;
        if (pBuzzer)
            pBuzzer->playTone(1500, 120);
    }

    void cancel() { radio().stopPairing(); }

    void forget()
    {
        radio().stopPairing();
        radio().forgetRemote();
        sendJson("{\"v\":1,\"type\":\"remote_paired\",\"payload\":{\"result\":\"forgotten\",\"remoteMac\":\"" +
                 remoteMacString() + "\"}}");
    }

    bool active() { return radio().isPairing() || resultUntil != 0; }

    void update()
    {
        const uint32_t now = millis();
        EspNowManager::PairResult res;
        if (radio().takePairResult(res))
        {
            const String mac = remoteMacString();
            const char *word = res == EspNowManager::PairResult::PAIRED    ? "paired"
                               : res == EspNowManager::PairResult::TIMEOUT ? "timeout"
                                                                           : "cancelled";
            sendJson("{\"v\":1,\"type\":\"remote_paired\",\"payload\":{\"result\":\"" + String(word) +
                     "\",\"remoteMac\":\"" + mac + "\"}}");
            if (res == EspNowManager::PairResult::PAIRED)
            {
                screen("Paired!", "Remote", mac.c_str());
                if (pLeds)
                    pLeds->setStripColor(0, 60, 0);
                if (pBuzzer)
                    pBuzzer->playTone(2000, 300);
            }
            else
            {
                screen(res == EspNowManager::PairResult::TIMEOUT ? "No remote" : "Cancelled", "Still paired with:", mac.c_str());
                if (pLeds)
                    pLeds->setStripColor(40, 0, 0);
                if (pBuzzer)
                    pBuzzer->playTone(400, 400);
            }
            resultUntil = now + 2500;
            return;
        }
        if (resultUntil)
        {
            if ((int32_t)(now - resultUntil) >= 0)
            {
                resultUntil = 0;
                if (pLeds)
                    pLeds->setStripColor(0, 0, 0);
            }
            return;
        }
        if (!radio().isPairing() || now - lastDraw < 400)
            return;
        lastDraw = now;
        blink = !blink;
        char secs[24];
        snprintf(secs, sizeof secs, "%lu s left", (unsigned long)((radio().pairingMsLeft() + 999) / 1000));
        screen("Pairing", "Remote: hold A + B and", blink ? "switch it on" : secs);
        if (pLeds)
            pLeds->setStripColor(0, 0, blink ? 80 : 0);
    }

    void pollSerial()
    {
        while (Serial.available())
        {
            char c = (char)Serial.read();
            if (c != '\n' && c != '\r')
            {
                if (serialLen + 1u < sizeof serialLine)
                    serialLine[serialLen++] = c;
                continue;
            }
            if (!serialLen)
                continue;
            serialLine[serialLen] = 0;
            serialLen = 0;
            if (!strcmp(serialLine, "pair"))
                start();
            else if (!strcmp(serialLine, "unpair"))
                forget();
            else if (!strcmp(serialLine, "cancel"))
                cancel();
            else if (!strcmp(serialLine, "remote?"))
            {
                uint8_t me[6];
                WiFi.macAddress(me);
                char meStr[18];
                EspNowManager::macToString(me, meStr);
                Serial.printf("REMOTE %s %s robot %s\n", remoteMacString().c_str(),
                              radio().hasStoredRemote() ? "paired" : "default", meStr);
            }
        }
    }

    void markBootStart()
    {
        bootMs = millis();
        pinMode(PIN_TOUCH, INPUT);
        touchAtBoot = digitalRead(PIN_TOUCH) == HIGH;
    }

    bool bootTouchHeld()
    {
        if (!touchAtBoot)
            return false;
        // Keep holding until 3 s after power-on (the rest of setup already took some of it).
        bool shown = false;
        while (millis() - bootMs < 3000)
        {
            if (digitalRead(PIN_TOUCH) != HIGH)
                return false;
            if (!shown)
            {
                screen("Pair?", "Keep holding the touch", "pad to pair a remote");
                shown = true;
            }
            delay(20);
        }
        return digitalRead(PIN_TOUCH) == HIGH;
    }
} // namespace pairing
