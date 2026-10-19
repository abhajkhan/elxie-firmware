// OLED face: procedural eyes (expressions, look direction, blinking), text,
// numbers and simple system screens. Rendering runs in its own low-priority
// task so a 20+ ms I2C frame never delays the 100 Hz control loop.
#pragma once
#include <stdint.h>

class Adafruit_SSD1306;

class Face {
 public:
  void begin(Adafruit_SSD1306* display);  // display = DisplayController's (nullptr: no OLED)
  void setActive(bool on);                // true while the block engine owns the screen
  void show(uint8_t expression);
  void look(uint8_t dir);
  void blink();
  void text(const char* s);
  void number(float n);
  void clear();
  void icon(uint8_t id);              // 1 heart .. 11 smile (enum icon)
  uint32_t wink();                    // returns duration ms
  uint32_t animate(uint8_t id);       // 1 sleeping, 2 thinking, 3 laughing; returns duration ms
  // Drawing canvas (128 x 64, origin top-left). Any draw switches the face to the canvas.
  void drawPixel(int x, int y);
  void drawLine(int x1, int y1, int x2, int y2);
  void drawRect(int x, int y, int w, int h, bool fill);
  void drawCircle(int cx, int cy, int r, bool fill);
  // System screens (launcher / status). Program blocks override them.
  void menu(const char* title, const char* const* items, uint8_t count, uint8_t selected, const char* footer);
  void message(const char* big, const char* small);
  uint8_t expression() const;
};

extern Face face;
