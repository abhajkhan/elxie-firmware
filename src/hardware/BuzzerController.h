#ifndef BUZZER_CONTROLLER_H
#define BUZZER_CONTROLLER_H

#include <stdint.h>
#include <Arduino.h>
#include "../config/BoardConfig.h"

class BuzzerController
{
public:
    void begin();
    void playTone(int freq, int durationMs);
    bool isBusy() const;
    void stop();

private:
    unsigned long buzzerFreeAt = 0;
};

#endif // BUZZER_CONTROLLER_H
