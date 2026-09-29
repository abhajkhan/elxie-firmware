#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include <stdint.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "../config/BoardConfig.h"

class DisplayController
{
public:
    void begin(bool oledAvailable);
    void drawHappyFace(uint8_t frame, bool batteryLow);
    void drawBatteryOverlay(bool batteryLow);
    void updateIdleDisplay(bool batteryLow);

    Adafruit_SSD1306 &getDisplay() { return display; }
    bool isPresent() const { return oledPresent; }

private:
    Adafruit_SSD1306 display{OLED_WIDTH, OLED_HEIGHT, &Wire, -1};
    bool oledPresent = false;
    unsigned long lastIdleFaceFrame = 0;
    uint8_t idleFaceFrame = 0;
};

#endif // DISPLAY_CONTROLLER_H
