#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <stdint.h>
#include <stddef.h>
#include <Adafruit_NeoPixel.h>
#include "../config/BoardConfig.h"
#include "../core/RobotTypes.h"

class LedController
{
public:
    void begin();
    void setStripColor(uint8_t r, uint8_t g, uint8_t b);
    void setRegionPixels(const uint8_t *pixels, size_t count, uint8_t r, uint8_t g, uint8_t b);
    void setRegionColor(StripRegion region, uint8_t r, uint8_t g, uint8_t b);
    void clearStripBuffer();
    void show();

    Adafruit_NeoPixel &getStrip() { return strip; }

private:
    Adafruit_NeoPixel strip{NUM_STRIP_PIXELS, PIN_STRIP, NEO_GRB + NEO_KHZ800};
};

#endif // LED_CONTROLLER_H
