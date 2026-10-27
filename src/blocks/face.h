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
  bool isActive() const;                  // the face engine is drawing on the OLED
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
  // Home screen (game firmware, robot idle): eyes that blink and glance around by themselves,
  // and a one-item-at-a-time icon menu (big icon, label, < > arrows, position dots).
  void idleEyes(uint8_t expression);  // 0..7 as show(), or FACE_BLUSH (home screen petting)
  void carousel(const char* title, uint8_t icon, const char* label, uint8_t index, uint8_t count);
  uint8_t expression() const;
};

extern Face face;

// Home-screen only expression (not a Blocks dropdown value): happy eyes + blushing cheeks
#define FACE_BLUSH 8

// Icons for Face::carousel()
namespace homeicon {
enum : uint8_t { GAMES = 1, FREE_RIDE, MY_PROGRAMS, COLOUR_QUEST, ECHO_MEMORY, REFLEX_DASH, DRIVING_PRO, INVERTED_DRIVE,
                 LEVEL = 20 };  // LEVEL: the label's text is drawn big (the level number)
}
