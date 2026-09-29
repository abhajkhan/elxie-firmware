#include "DisplayController.h"

void DisplayController::begin(bool oledAvailable)
{
    oledPresent = oledAvailable;
    if (!oledPresent)
        return;

    display.begin(SSD1306_SWITCHCAPVCC, ADDR_OLED);
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(8, 14);
    display.println("Welcome");
    display.setCursor(22, 38);
    display.println("Elxie!");
    display.display();

    Serial.println("OLED initialized.");
}

void DisplayController::drawBatteryOverlay(bool batteryLow)
{
    if (!batteryLow || !oledPresent)
        return;

    display.drawRect(114, 2, 12, 7, SSD1306_WHITE);
    display.fillRect(126, 4, 2, 3, SSD1306_WHITE);
    display.fillRect(116, 4, 3, 3, SSD1306_WHITE);
}

void DisplayController::drawHappyFace(uint8_t frame, bool batteryLow)
{
    if (!oledPresent)
        return;

    display.clearDisplay();
    display.drawCircle(64, 32, 30, SSD1306_WHITE);

    if (frame == 2)
    {
        display.drawLine(46, 24, 54, 24, SSD1306_WHITE);
        display.drawLine(74, 24, 82, 24, SSD1306_WHITE);
    }
    else
    {
        display.fillCircle(50, 24, 4, SSD1306_WHITE);
        display.fillCircle(78, 24, 4, SSD1306_WHITE);
    }

    if (frame == 1)
    {
        display.drawLine(46, 40, 50, 44, SSD1306_WHITE);
        display.drawLine(50, 44, 56, 47, SSD1306_WHITE);
        display.drawLine(56, 47, 64, 48, SSD1306_WHITE);
        display.drawLine(64, 48, 72, 47, SSD1306_WHITE);
        display.drawLine(72, 47, 78, 44, SSD1306_WHITE);
        display.drawLine(78, 44, 82, 40, SSD1306_WHITE);
    }
    else
    {
        display.drawLine(44, 40, 49, 45, SSD1306_WHITE);
        display.drawLine(49, 45, 56, 49, SSD1306_WHITE);
        display.drawLine(56, 49, 64, 51, SSD1306_WHITE);
        display.drawLine(64, 51, 72, 49, SSD1306_WHITE);
        display.drawLine(72, 49, 79, 45, SSD1306_WHITE);
        display.drawLine(79, 45, 84, 40, SSD1306_WHITE);
    }

    drawBatteryOverlay(batteryLow);
    display.display();
}

void DisplayController::updateIdleDisplay(bool batteryLow)
{
    if (!oledPresent)
        return;

    unsigned long now = millis();
    if (now - lastIdleFaceFrame < 500)
        return;

    lastIdleFaceFrame = now;
    idleFaceFrame = (idleFaceFrame + 1) % 3;
    drawHappyFace(idleFaceFrame, batteryLow);
}
