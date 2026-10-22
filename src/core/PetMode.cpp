#include "PetMode.h"

#include <Arduino.h>
#include <math.h>

#include "../config/BoardConfig.h"
#include "../hardware/BuzzerController.h"
#include "../hardware/LedController.h"

static const unsigned long PET_HOLD_MS = 500;     // hold longer than this = petting
static const unsigned long PET_COO_EVERY_MS = 2200;
static const unsigned long PET_BLUSH_AFTER_MS = 2000;
static const unsigned long PET_FADE_MS = 1000;
static const unsigned long PET_FRAME_MS = 30;
static const float PET_LED_MAX = 0.35f; // gentle: at most 35 % brightness

// note pairs: frequency (Hz), length (ms)
static const uint16_t CHIRP[] = {1400, 70, 2000, 90};             // petting starts
static const uint16_t COO[] = {1100, 120, 900, 160};              // while petting
static const uint16_t HAPPY[] = {1319, 90, 1568, 90, 2093, 160}; // let go after a good pet

static volatile bool g_active = false;
static volatile unsigned long g_petStart = 0;

namespace pet
{
bool active() { return g_active; }
uint16_t seconds() { return g_active ? (uint16_t)((millis() - g_petStart) / 1000) : 0; }
}

void PetMode::begin(LedController *leds, BuzzerController *buzzer)
{
    pLeds = leds;
    pBuzzer = buzzer;
}

bool PetMode::blushing() const
{
    return active || (blushUntil && (long)(millis() - blushUntil) < 0);
}

void PetMode::update(bool atHome, bool touch)
{
    const unsigned long now = millis();

    if (!atHome)
    { // a game, Free Ride, Blocks or pairing took over: stop at once
        if (active)
            finish(now, false);
        if (fading)
        {
            fading = false;
            restoreLights();
        }
        blushUntil = 0;
        touchWas = touch;
        touchSince = now;
        return;
    }

    if (touch && !touchWas)
        touchSince = now;
    if (!active && touch && now - touchSince >= PET_HOLD_MS)
        start(now);
    if (active && !touch)
        finish(now, now - petStart >= 1000);
    touchWas = touch;

    if (active && (long)(now - nextCoo) >= 0)
    {
        nextCoo = now + PET_COO_EVERY_MS;
        notes = COO;
        noteCount = sizeof(COO) / sizeof(COO[0]) / 2;
        noteIdx = 0;
        noteAt = now;
    }
    playNext(now);

    if (now - lastFrame >= PET_FRAME_MS)
    {
        lastFrame = now;
        if (active)
        {
            const float ramp = min(1.0f, (now - petStart) / 400.0f); // fade in
            drawWave(now, ramp);
        }
        else if (fading)
        {
            const float left = 1.0f - (float)(now - fadeStart) / PET_FADE_MS;
            if (left <= 0)
            {
                fading = false;
                restoreLights();
            }
            else
                drawWave(now, left);
        }
    }
}

void PetMode::start(unsigned long now)
{
    active = true;
    g_active = true;
    petStart = now;
    g_petStart = now;
    nextCoo = now + PET_COO_EVERY_MS;
    if (pLeds && !lightsTaken && !fading)
        pLeds->getStripColor(savedR, savedG, savedB); // put back afterwards
    lightsTaken = true;
    fading = false;
    notes = CHIRP;
    noteCount = sizeof(CHIRP) / sizeof(CHIRP[0]) / 2;
    noteIdx = 0;
    noteAt = now;
    Serial.println("[Pet] petting");
}

void PetMode::finish(unsigned long now, bool happyTune)
{
    Serial.printf("[Pet] petted %lu ms\n", now - petStart);
    active = false;
    g_active = false;
    blushUntil = now + PET_BLUSH_AFTER_MS;
    if (blushUntil == 0)
        blushUntil = 1;
    fading = lightsTaken;
    fadeStart = now;
    if (happyTune)
    {
        notes = HAPPY;
        noteCount = sizeof(HAPPY) / sizeof(HAPPY[0]) / 2;
        noteIdx = 0;
        noteAt = now;
    }
    else
    {
        notes = nullptr;
    }
}

void PetMode::playNext(unsigned long now)
{
    if (!notes || !pBuzzer || (long)(now - noteAt) < 0)
        return;
    if (noteIdx >= noteCount)
    {
        notes = nullptr;
        return;
    }
    const uint16_t f = notes[noteIdx * 2], ms = notes[noteIdx * 2 + 1];
    pBuzzer->playTone(f, ms);
    noteAt = now + ms + 25;
    noteIdx++;
}

// Pink <-> purple wave running along the strip, breathing slowly.
void PetMode::drawWave(unsigned long now, float level)
{
    if (!pLeds)
        return;
    Adafruit_NeoPixel &strip = pLeds->getStrip();
    const float t = now / 1000.0f;
    const float breathe = 0.55f + 0.45f * sinf(t * 2.0f * PI / 2.6f); // 2.6 s per breath
    const float k = PET_LED_MAX * breathe * level;
    for (int i = 0; i < NUM_STRIP_PIXELS; i++)
    {
        const float mix = 0.5f + 0.5f * sinf(i * 0.42f - t * 2.2f); // 0 pink .. 1 purple
        const uint8_t r = (uint8_t)((255 - 95 * mix) * k);
        const uint8_t g = (uint8_t)(50 * k);
        const uint8_t b = (uint8_t)((130 + 125 * mix) * k);
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
}

void PetMode::restoreLights()
{
    if (pLeds && lightsTaken)
        pLeds->setStripColor(savedR, savedG, savedB);
    lightsTaken = false;
}
