// Petting at home (robot idle): touch-and-hold the touch pad for more than 0.5 s.
// While petting: blushing face (LocalMenu shows it), a gentle pink/purple breathing wave
// on the LEDs and cute sounds. Quick taps (the double tap for My Programs) are not petting.
// The app gets telemetry "pet": {"active", "seconds"} to raise the trust level.
#pragma once
#include <stdint.h>

class LedController;
class BuzzerController;

class PetMode
{
public:
    void begin(LedController *leds, BuzzerController *buzzer);
    // Every loop. atHome: robot idle, not Blocks / pairing / a game / Free Ride.
    void update(bool atHome, bool touch);
    bool petting() const { return active; }
    bool blushing() const; // petting, or just after (the face keeps blushing ~2 s)

private:
    void start(unsigned long now);
    void finish(unsigned long now, bool happyTune);
    void playNext(unsigned long now);
    void drawWave(unsigned long now, float level);
    void restoreLights();

    LedController *pLeds = nullptr;
    BuzzerController *pBuzzer = nullptr;
    bool active = false;
    bool touchWas = false;
    unsigned long touchSince = 0, petStart = 0, nextCoo = 0, blushUntil = 0;
    unsigned long fadeStart = 0, lastFrame = 0;
    bool fading = false, lightsTaken = false;
    uint8_t savedR = 0, savedG = 0, savedB = 0;
    // tiny note queue for the chirps
    const uint16_t *notes = nullptr;
    uint8_t noteCount = 0, noteIdx = 0;
    unsigned long noteAt = 0;
};

// For telemetry (BleProtocol): current petting state.
namespace pet
{
bool active();
uint16_t seconds(); // how long the current petting has lasted (0 when not petting)
}
