#include "BatteryManager.h"

void BatteryManager::begin()
{
    pinMode(PIN_BATTERY, INPUT);
    batteryPercent = readPercentage();
    if (batteryPercent <= BATT_WARN_PCT)
        batteryLow = true;
}

int BatteryManager::readPercentage()
{
    int raw = analogRead(PIN_BATTERY);
    return constrain((raw * 100L) / 4095L, 0, 100);
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
