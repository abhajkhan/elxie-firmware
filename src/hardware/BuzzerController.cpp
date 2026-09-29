#include "BuzzerController.h"

void BuzzerController::begin()
{
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
}

void BuzzerController::playTone(int freq, int durationMs)
{
    tone(PIN_BUZZER, freq, durationMs);
    buzzerFreeAt = millis() + durationMs;
}

bool BuzzerController::isBusy() const
{
    return (long)(millis() - buzzerFreeAt) < 0;
}

void BuzzerController::stop()
{
    noTone(PIN_BUZZER);
    buzzerFreeAt = 0;
}
