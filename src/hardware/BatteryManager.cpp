#include "BatteryManager.h"

#include "../communication/ElxiePairingProtocol.h"  // elxLiIonPercent()
#include "BatteryLevel.h"

static uint16_t g_packMv = 0;
static uint8_t g_percent = 0;

namespace battery {
uint16_t packMv() { return g_packMv; }
uint8_t percent() { return g_percent; }
}  // namespace battery

void BatteryManager::begin()
{
    pinMode(PIN_BATTERY, INPUT);
    analogSetPinAttenuation(PIN_BATTERY, ADC_11db); // up to ~3.1 V on the pin
    batteryPercent = readPercentage();
    if (batteryPercent <= BATT_WARN_PCT)
        batteryLow = true;
    Serial.printf("[Battery] %.2f V (%d cells), %d%%\n", packMv / 1000.0f, BATT_CELLS, batteryPercent);
}

// Pack voltage from the 2k/1k divider, with the ESP32's factory-calibrated ADC
// (analogReadMilliVolts). Smoothed, so motor current dips don't make it jump.
int BatteryManager::readPercentage()
{
    uint32_t sum = 0;
    for (int i = 0; i < 16; ++i)
        sum += analogReadMilliVolts(PIN_BATTERY);
    const float pinMv = sum / 16.0f;
    const float mv = pinMv * (BATT_R_TOP_OHM + BATT_R_BOTTOM_OHM) / BATT_R_BOTTOM_OHM * BATT_CAL;
    packMvSmooth = packMvSmooth <= 0 ? mv : packMvSmooth * 0.8f + mv * 0.2f;
    packMv = (uint16_t)(packMvSmooth + 0.5f);
    const uint8_t pct = elxLiIonPercent((uint16_t)(packMv / BATT_CELLS));
    g_packMv = packMv;
    g_percent = pct;
    return pct;
}

void BatteryManager::service(BuzzerController &buzzer)
{
    unsigned long now = millis();

    if (now - lastBattSampleMs >= BATT_SAMPLE_MS)
    {
        lastBattSampleMs = now;
        batteryPercent = readPercentage();

        if (!batteryLow && batteryPercent <= BATT_WARN_PCT)
        {
            batteryLow = true;
            lastBattAlertMs = 0; // Trigger alert immediately
        }
        else if (batteryLow && batteryPercent >= BATT_CLEAR_PCT)
        {
            batteryLow = false;
            battAlertPlaying = false;
        }
    }

    if (batteryLow && !battAlertPlaying &&
        (lastBattAlertMs == 0 || now - lastBattAlertMs >= BATT_ALERT_PERIOD_MS))
    {
        if (!buzzer.isBusy())
        {
            battAlertPlaying = true;
            battAlertStep = 0;
            battAlertStepMs = now;
            lastBattAlertMs = now;
        }
    }

    if (battAlertPlaying)
    {
        if (battAlertStep == 0)
        {
            buzzer.playTone(800, 150);
            battAlertStep = 1;
            battAlertStepMs = now;
        }
        else if (battAlertStep == 1 && (now - battAlertStepMs >= 200))
        {
            buzzer.playTone(600, 250);
            battAlertStep = 2;
            battAlertStepMs = now;
        }
        else if (battAlertStep == 2 && (now - battAlertStepMs >= 300))
        {
            battAlertPlaying = false;
        }
    }
}
