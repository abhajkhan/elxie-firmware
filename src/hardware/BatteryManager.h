#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <stdint.h>
#include <Arduino.h>
#include "../config/BoardConfig.h"
#include "BuzzerController.h"

class BatteryManager
{
public:
    void begin();
    int readPercentage();
    void service(BuzzerController &buzzer);

    int getPercent() const { return batteryPercent; }
    bool isLow() const { return batteryLow; }

private:
    int batteryPercent = 100;
    bool batteryLow = false;

    unsigned long lastBattSampleMs = 0;
    unsigned long lastBattAlertMs = 0;
    bool battAlertPlaying = false;
    uint8_t battAlertStep = 0;
    unsigned long battAlertStepMs = 0;
};

#endif // BATTERY_MANAGER_H
