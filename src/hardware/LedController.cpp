#include "LedController.h"

void LedController::begin()
{
    strip.begin();
    strip.show();
}

void LedController::setStripColor(uint8_t r, uint8_t g, uint8_t b)
{
    lastR = r;
    lastG = g;
    lastB = b;
    for (int i = 0; i < NUM_STRIP_PIXELS; i++)
    {
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
}

void LedController::setRegionPixels(const uint8_t *pixels, size_t count, uint8_t r, uint8_t g, uint8_t b)
{
    for (size_t i = 0; i < count; i++)
    {
        strip.setPixelColor(pixels[i], strip.Color(r, g, b));
    }
}

void LedController::setRegionColor(StripRegion region, uint8_t r, uint8_t g, uint8_t b)
{
    switch (region)
    {
    case REGION_FRONT:
        setRegionPixels(FRONT_PIXELS, FRONT_PIXEL_COUNT, r, g, b);
        break;
    case REGION_BACK:
        setRegionPixels(BACK_PIXELS, BACK_PIXEL_COUNT, r, g, b);
        break;
    case REGION_LEFT:
        setRegionPixels(LEFT_PIXELS, LEFT_PIXEL_COUNT, r, g, b);
        break;
    case REGION_RIGHT:
        setRegionPixels(RIGHT_PIXELS, RIGHT_PIXEL_COUNT, r, g, b);
        break;
    }
}

void LedController::clearStripBuffer()
{
    for (int i = 0; i < NUM_STRIP_PIXELS; i++)
    {
        strip.setPixelColor(i, 0);
    }
}

void LedController::show()
{
    strip.show();
}
