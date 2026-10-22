#include "face.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "esp_log.h"
#include "esp_random.h"
#include <Adafruit_SSD1306.h>
#include <U8g2lib.h>  // only its C core (clib/u8g2.h) is used: draws into RAM
#undef BLACK  // Adafruit colour macros clash with the block enums
#undef WHITE
#undef INVERSE
#include "blocks_config.h"
#include "vm/opcodes_gen.h"

Face face;

static u8g2_t oled;
static Adafruit_SSD1306* s_display = nullptr;  // owned by DisplayController
static volatile bool s_active = false;         // false: the game code owns the screen
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;  // guards g_state

enum Mode : uint8_t { M_EYES, M_TEXT, M_NUMBER, M_BLANK, M_MENU, M_MESSAGE, M_ICON, M_ANIM, M_CANVAS, M_CAROUSEL };

struct FaceState {
  uint8_t mode = M_EYES;
  uint8_t expr = 0;
  int8_t lookX = 0, lookY = 0;
  uint32_t blinkReq = 0;
  float number = 0;
  char text[40] = {0};
  char small[24] = {0};
  char items[6][18] = {{0}};
  uint8_t count = 0, selected = 0;
  uint32_t textSince = 0;
  uint8_t icon = 0, anim = 0;
  uint32_t animStart = 0, winkUntil = 0;
  bool glance = false;  // idle eyes look around by themselves (home screen only)
};
static FaceState g_state;

#define LOCKED(code)          \
  do {                        \
    portENTER_CRITICAL(&g_mux); \
    code;                     \
    portEXIT_CRITICAL(&g_mux);  \
  } while (0)

void Face::show(uint8_t e) { LOCKED(g_state.mode = M_EYES; g_state.expr = e <= 7 ? e : 0; g_state.glance = false); }

void Face::idleEyes(uint8_t e) {
  LOCKED(g_state.mode = M_EYES; g_state.expr = e <= FACE_BLUSH ? e : 0; g_state.lookX = 0; g_state.lookY = 0;
         g_state.glance = true);
}

void Face::carousel(const char* title, uint8_t icon, const char* label, uint8_t index, uint8_t count) {
  portENTER_CRITICAL(&g_mux);
  g_state.mode = M_CAROUSEL;
  g_state.glance = false;
  strncpy(g_state.small, title ? title : "", sizeof g_state.small - 1);
  g_state.small[sizeof g_state.small - 1] = 0;
  strncpy(g_state.text, label ? label : "", sizeof g_state.text - 1);
  g_state.text[sizeof g_state.text - 1] = 0;
  g_state.icon = icon;
  g_state.selected = index;
  g_state.count = count;
  portEXIT_CRITICAL(&g_mux);
}

void Face::look(uint8_t dir) {
  int8_t x = 0, y = 0;
  switch (dir) {
    case elx::enums::look::LEFT: x = -1; break;
    case elx::enums::look::RIGHT: x = 1; break;
    case elx::enums::look::UP: y = -1; break;
    case elx::enums::look::DOWN: y = 1; break;
    default: break;
  }
  LOCKED(g_state.mode = M_EYES; g_state.lookX = x; g_state.lookY = y);
}

void Face::blink() { LOCKED(g_state.mode = M_EYES; g_state.blinkReq = millis()); }

void Face::text(const char* s) {
  LOCKED(g_state.mode = M_TEXT; strlcpy(g_state.text, s ? s : "", sizeof g_state.text);
         g_state.textSince = millis());
}

void Face::number(float n) { LOCKED(g_state.mode = M_NUMBER; g_state.number = n); }

// 128x64 canvas in the SSD1306 page layout u8g2 uses (byte = 8 vertical pixels).
static uint8_t g_canvas[OLED_WIDTH * OLED_HEIGHT / 8];

void Face::clear() {
  LOCKED(g_state.mode = M_BLANK; memset(g_canvas, 0, sizeof g_canvas));
}

void Face::icon(uint8_t id) { LOCKED(g_state.mode = M_ICON; g_state.icon = id); }

uint32_t Face::wink() {
  const uint32_t ms = 450;
  LOCKED(g_state.mode = M_EYES; g_state.winkUntil = millis() + ms);
  return ms;
}

uint32_t Face::animate(uint8_t id) {
  const uint32_t ms = id == 3 ? 1800 : 2400;
  LOCKED(g_state.mode = M_ANIM; g_state.anim = id; g_state.animStart = millis());
  return ms;
}

static inline void canvasSet(int x, int y) {
  if (x < 0 || y < 0 || x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
  g_canvas[(y >> 3) * OLED_WIDTH + x] |= (uint8_t)(1u << (y & 7));
}

static void canvasLine(int x1, int y1, int x2, int y2) {
  int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1, dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1, err = dx + dy;
  for (int guard = 0; guard < 512; ++guard) {
    canvasSet(x1, y1);
    if (x1 == x2 && y1 == y2) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x1 += sx; }
    if (e2 <= dx) { err += dx; y1 += sy; }
  }
}

void Face::drawPixel(int x, int y) { LOCKED(g_state.mode = M_CANVAS; canvasSet(x, y)); }

void Face::drawLine(int x1, int y1, int x2, int y2) { LOCKED(g_state.mode = M_CANVAS; canvasLine(x1, y1, x2, y2)); }

void Face::drawRect(int x, int y, int w, int h, bool fill) {
  w = constrain(w, 0, OLED_WIDTH);
  h = constrain(h, 0, OLED_HEIGHT);
  LOCKED({
    g_state.mode = M_CANVAS;
    for (int yy = y; yy < y + h; ++yy)
      for (int xx = x; xx < x + w; ++xx)
        if (fill || yy == y || yy == y + h - 1 || xx == x || xx == x + w - 1) canvasSet(xx, yy);
  });
}

void Face::drawCircle(int cx, int cy, int r, bool fill) {
  r = constrain(r, 0, 64);
  LOCKED({
    g_state.mode = M_CANVAS;
    for (int yy = -r; yy <= r; ++yy)
      for (int xx = -r; xx <= r; ++xx) {
        const int d = xx * xx + yy * yy;
        if (d <= r * r && (fill || d >= (r - 1) * (r - 1))) canvasSet(cx + xx, cy + yy);
      }
  });
}

void Face::menu(const char* title, const char* const* items, uint8_t count, uint8_t selected, const char* footer) {
  LOCKED({
    g_state.mode = M_MENU;
    strlcpy(g_state.text, title, sizeof g_state.text);
    strlcpy(g_state.small, footer ? footer : "", sizeof g_state.small);
    g_state.count = count > 6 ? 6 : count;
    g_state.selected = selected;
    for (uint8_t i = 0; i < g_state.count; ++i) strlcpy(g_state.items[i], items[i], sizeof g_state.items[i]);
  });
}

void Face::message(const char* big, const char* small) {
  LOCKED(g_state.mode = M_MESSAGE; strlcpy(g_state.text, big, sizeof g_state.text);
         strlcpy(g_state.small, small ? small : "", sizeof g_state.small));
}

uint8_t Face::expression() const { return g_state.expr; }

// ------------------------------------------------------------------ drawing
static void eyeBox(int x, int y, int w, int h) {
  if (h < 1) h = 1;
  int r = min(8, min(w, h) / 2 - 1);
  if (r >= 1) u8g2_DrawRBox(&oled, x, y, w, h, r);
  else u8g2_DrawBox(&oled, x, y, w, h);
}

static void drawEye(int cx, int cy, bool leftEye, uint8_t expr, float open) {
  const int W = 30, H = 36;
  int h = (int)(H * open);
  int x0 = cx - W / 2, y0 = cy - h / 2;
  namespace face = elx::enums::face;
  switch (expr) {
    case face::SURPRISED:
      u8g2_DrawDisc(&oled, cx, cy, open < 0.5f ? 3 : 17, U8G2_DRAW_ALL);
      if (open >= 0.5f) {
        u8g2_SetDrawColor(&oled, 0);
        u8g2_DrawDisc(&oled, cx, cy, 5, U8G2_DRAW_ALL);
        u8g2_SetDrawColor(&oled, 1);
      }
      return;
    case face::SLEEPY:
      eyeBox(x0, cy + 6, W, 6);
      return;
    case face::LOVE:
      u8g2_DrawDisc(&oled, cx - 7, cy - 5, 8, U8G2_DRAW_ALL);
      u8g2_DrawDisc(&oled, cx + 7, cy - 5, 8, U8G2_DRAW_ALL);
      u8g2_DrawTriangle(&oled, cx - 15, cy - 2, cx + 15, cy - 2, cx, cy + 15);
      return;
    case face::DIZZY:
      for (int d = -1; d <= 1; ++d) {
        u8g2_DrawLine(&oled, cx - 12 + d, cy - 12, cx + 12 + d, cy + 12);
        u8g2_DrawLine(&oled, cx - 12 + d, cy + 12, cx + 12 + d, cy - 12);
      }
      return;
    default:
      break;
  }
  eyeBox(x0, y0, W, h);
  if (open < 0.5f) return;
  u8g2_SetDrawColor(&oled, 0);
  switch (expr) {
    case face::HAPPY:  // cheeks push the eye up into an arc
      u8g2_DrawFilledEllipse(&oled, cx, y0 + h + 6, W / 2 + 6, h / 2 + 2, U8G2_DRAW_ALL);
      break;
    case face::SAD:  // outer-top corner cut
      if (leftEye) u8g2_DrawTriangle(&oled, x0 - 1, y0 - 1, x0 + W, y0 - 1, x0 - 1, y0 + h / 2);
      else u8g2_DrawTriangle(&oled, x0 + W, y0 - 1, x0 - 1, y0 - 1, x0 + W, y0 + h / 2);
      break;
    case face::ANGRY:  // inner-top corner cut
      if (leftEye) u8g2_DrawTriangle(&oled, x0 + W, y0 - 1, x0 - 1, y0 - 1, x0 + W, y0 + h / 2);
      else u8g2_DrawTriangle(&oled, x0 - 1, y0 - 1, x0 + W, y0 - 1, x0 - 1, y0 + h / 2);
      break;
    default:
      break;
  }
  u8g2_SetDrawColor(&oled, 1);
}

// Blushing cheek under an eye centred at (cx, cy): a row of short diagonal strokes.
static void drawBlush(int cx, int cy) {
  for (int i = 0; i < 5; ++i) {
    const int x = cx - 15 + i * 6;
    u8g2_DrawLine(&oled, x, cy + 18, x + 5, cy + 11);
    u8g2_DrawLine(&oled, x + 1, cy + 18, x + 6, cy + 11);
  }
}

static void drawCentered(const char* s, int y) {
  int w = u8g2_GetUTF8Width(&oled, s);
  u8g2_DrawUTF8(&oled, (OLED_WIDTH - w) / 2, y, s);
}

static void drawIcon(uint8_t id) {
  const int cx = OLED_WIDTH / 2, cy = OLED_HEIGHT / 2;
  switch (id) {
    case 1:  // heart
      u8g2_DrawDisc(&oled, cx - 11, cy - 7, 12, U8G2_DRAW_ALL);
      u8g2_DrawDisc(&oled, cx + 11, cy - 7, 12, U8G2_DRAW_ALL);
      u8g2_DrawTriangle(&oled, cx - 23, cy - 3, cx + 23, cy - 3, cx, cy + 24);
      break;
    case 2: {  // star
      float px[10], py[10];
      for (int i = 0; i < 10; ++i) {
        const float a = -1.5708f + i * 0.6283f, r = (i % 2) ? 11.0f : 28.0f;
        px[i] = cx + r * cosf(a); py[i] = cy + 2 + r * sinf(a);
      }
      for (int i = 0; i < 10; ++i) {
        const int j = (i + 1) % 10;
        u8g2_DrawTriangle(&oled, cx, cy + 2, (int)px[i], (int)py[i], (int)px[j], (int)py[j]);
      }
      break;
    }
    case 3: u8g2_DrawTriangle(&oled, cx, cy - 26, cx - 22, cy, cx + 22, cy); u8g2_DrawBox(&oled, cx - 8, cy, 16, 24); break;  // up
    case 4: u8g2_DrawTriangle(&oled, cx, cy + 26, cx - 22, cy, cx + 22, cy); u8g2_DrawBox(&oled, cx - 8, cy - 24, 16, 24); break;  // down
    case 5: u8g2_DrawTriangle(&oled, cx - 30, cy, cx - 6, cy - 22, cx - 6, cy + 22); u8g2_DrawBox(&oled, cx - 6, cy - 8, 32, 16); break;  // left
    case 6: u8g2_DrawTriangle(&oled, cx + 30, cy, cx + 6, cy - 22, cx + 6, cy + 22); u8g2_DrawBox(&oled, cx - 26, cy - 8, 32, 16); break;  // right
    case 7:  // check
      for (int d = -3; d <= 3; ++d) { u8g2_DrawLine(&oled, cx - 24, cy + d, cx - 8, cy + 16 + d); u8g2_DrawLine(&oled, cx - 8, cy + 16 + d, cx + 24, cy - 18 + d); }
      break;
    case 8:  // cross
      for (int d = -3; d <= 3; ++d) { u8g2_DrawLine(&oled, cx - 20 + d, cy - 20, cx + 20 + d, cy + 20); u8g2_DrawLine(&oled, cx - 20 + d, cy + 20, cx + 20 + d, cy - 20); }
      break;
    case 9:  // music notes
      u8g2_DrawFilledEllipse(&oled, cx - 14, cy + 16, 8, 6, U8G2_DRAW_ALL);
      u8g2_DrawFilledEllipse(&oled, cx + 16, cy + 10, 8, 6, U8G2_DRAW_ALL);
      u8g2_DrawBox(&oled, cx - 8, cy - 22, 3, 38);
      u8g2_DrawBox(&oled, cx + 22, cy - 28, 3, 38);
      for (int t = 0; t < 6; ++t) u8g2_DrawLine(&oled, cx - 8, cy - 22 + t, cx + 24, cy - 28 + t);
      break;
    case 10:  // question mark
      u8g2_SetFont(&oled, u8g2_font_helvB18_tr);
      drawCentered("?", 44);
      break;
    case 11:  // smiley
      u8g2_DrawCircle(&oled, cx, cy, 28, U8G2_DRAW_ALL);
      u8g2_DrawDisc(&oled, cx - 10, cy - 8, 4, U8G2_DRAW_ALL);
      u8g2_DrawDisc(&oled, cx + 10, cy - 8, 4, U8G2_DRAW_ALL);
      u8g2_DrawDisc(&oled, cx, cy + 6, 14, U8G2_DRAW_LOWER_RIGHT | U8G2_DRAW_LOWER_LEFT);
      break;
    default:
      break;
  }
}

static void drawAnim(uint8_t id, uint32_t t) {
  namespace face = elx::enums::face;
  switch (id) {
    case 1: {  // sleeping: closed eyes + floating z's
      drawEye(40, 34, true, face::SLEEPY, 1);
      drawEye(88, 34, false, face::SLEEPY, 1);
      u8g2_SetFont(&oled, u8g2_font_6x10_tr);
      const int k = (t / 300) % 4;
      for (int i = 0; i < k; ++i) u8g2_DrawStr(&oled, 100 + i * 7, 20 - i * 6, "z");
      break;
    }
    case 2: {  // thinking: eyes up-right + dots
      drawEye(40 + 12, 32 - 8, true, face::NEUTRAL, 0.8f);
      drawEye(88 + 12, 32 - 8, false, face::NEUTRAL, 0.8f);
      const int k = 1 + (t / 350) % 3;
      for (int i = 0; i < k; ++i) u8g2_DrawDisc(&oled, 50 + i * 12, 58, 3, U8G2_DRAW_ALL);
      break;
    }
    default: {  // laughing: happy eyes bouncing
      const int dy = ((t / 150) % 2) ? -4 : 2;
      drawEye(40, 30 + dy, true, face::HAPPY, 1);
      drawEye(88, 30 + dy, false, face::HAPPY, 1);
      break;
    }
  }
}

// Home menu icons, about 34 x 28 px around (cx, cy). Simple original drawings.
static void drawMenuIcon(uint8_t id, int cx, int cy, const char* label) {
  switch (id) {
    case homeicon::GAMES:  // game pad
      u8g2_DrawRFrame(&oled, cx - 17, cy - 10, 34, 20, 7);
      u8g2_DrawBox(&oled, cx - 12, cy - 1, 9, 3);  // d-pad
      u8g2_DrawBox(&oled, cx - 9, cy - 4, 3, 9);
      u8g2_DrawDisc(&oled, cx + 7, cy - 2, 2, U8G2_DRAW_ALL);
      u8g2_DrawDisc(&oled, cx + 11, cy + 2, 2, U8G2_DRAW_ALL);
      break;
    case homeicon::FREE_RIDE:  // car, side view
      u8g2_DrawRBox(&oled, cx - 17, cy - 1, 34, 9, 3);
      u8g2_DrawLine(&oled, cx - 9, cy - 1, cx - 5, cy - 9);
      u8g2_DrawLine(&oled, cx - 5, cy - 9, cx + 6, cy - 9);
      u8g2_DrawLine(&oled, cx + 6, cy - 9, cx + 11, cy - 1);
      u8g2_SetDrawColor(&oled, 0);
      u8g2_DrawDisc(&oled, cx - 9, cy + 9, 5, U8G2_DRAW_ALL);
      u8g2_DrawDisc(&oled, cx + 9, cy + 9, 5, U8G2_DRAW_ALL);
      u8g2_SetDrawColor(&oled, 1);
      u8g2_DrawDisc(&oled, cx - 9, cy + 9, 3, U8G2_DRAW_ALL);
      u8g2_DrawDisc(&oled, cx + 9, cy + 9, 3, U8G2_DRAW_ALL);
      break;
    case homeicon::MY_PROGRAMS:  // stacked code blocks
      u8g2_DrawRBox(&oled, cx - 16, cy - 12, 22, 7, 2);
      u8g2_DrawRFrame(&oled, cx - 10, cy - 3, 26, 7, 2);
      u8g2_DrawRBox(&oled, cx - 16, cy + 6, 18, 7, 2);
      u8g2_DrawBox(&oled, cx - 13, cy - 5, 4, 2);  // connector notches
      u8g2_DrawBox(&oled, cx - 7, cy + 4, 4, 2);
      break;
    case homeicon::COLOUR_QUEST:  // three colour dots
      u8g2_DrawDisc(&oled, cx, cy - 6, 6, U8G2_DRAW_ALL);
      u8g2_DrawCircle(&oled, cx - 10, cy + 6, 6, U8G2_DRAW_ALL);
      u8g2_DrawCircle(&oled, cx + 10, cy + 6, 6, U8G2_DRAW_ALL);
      u8g2_DrawDisc(&oled, cx + 10, cy + 6, 3, U8G2_DRAW_ALL);
      break;
    case homeicon::ECHO_MEMORY:  // two music notes
      u8g2_DrawDisc(&oled, cx - 9, cy + 9, 4, U8G2_DRAW_ALL);
      u8g2_DrawDisc(&oled, cx + 8, cy + 6, 4, U8G2_DRAW_ALL);
      u8g2_DrawBox(&oled, cx - 6, cy - 10, 2, 19);
      u8g2_DrawBox(&oled, cx + 11, cy - 13, 2, 19);
      for (int t = 0; t < 4; ++t) u8g2_DrawLine(&oled, cx - 6, cy - 10 + t, cx + 12, cy - 13 + t);
      break;
    case homeicon::REFLEX_DASH:  // lightning bolt
      u8g2_DrawTriangle(&oled, cx + 5, cy - 14, cx - 9, cy + 2, cx + 1, cy + 2);
      u8g2_DrawTriangle(&oled, cx - 1, cy - 2, cx + 9, cy - 2, cx - 5, cy + 14);
      break;
    case homeicon::DRIVING_PRO:  // road in perspective
      u8g2_DrawLine(&oled, cx - 4, cy - 13, cx - 16, cy + 13);
      u8g2_DrawLine(&oled, cx + 4, cy - 13, cx + 16, cy + 13);
      u8g2_DrawBox(&oled, cx - 1, cy - 10, 2, 4);
      u8g2_DrawBox(&oled, cx - 1, cy - 2, 2, 5);
      u8g2_DrawBox(&oled, cx - 2, cy + 7, 3, 6);
      break;
    case homeicon::INVERTED_DRIVE:  // up and down arrows swapped
      u8g2_DrawTriangle(&oled, cx - 9, cy - 14, cx - 16, cy - 5, cx - 2, cy - 5);
      u8g2_DrawBox(&oled, cx - 11, cy - 5, 4, 17);
      u8g2_DrawTriangle(&oled, cx + 9, cy + 14, cx + 2, cy + 5, cx + 16, cy + 5);
      u8g2_DrawBox(&oled, cx + 7, cy - 12, 4, 17);
      break;
    case homeicon::LEVEL:  // the label, big (level number)
      u8g2_SetFont(&oled, u8g2_font_logisoso32_tn);
      drawCentered(label, cy + 14);
      u8g2_SetFont(&oled, u8g2_font_6x10_tr);
      drawCentered("Level", 56);
      break;
    default:
      break;
  }
}

static void render(const FaceState& s, uint32_t now) {
  static uint32_t nextAutoBlink = 3000, blinkStart = 0, lastReq = 0;
  u8g2_ClearBuffer(&oled);
  switch (s.mode) {
    case M_EYES: {
      if (s.blinkReq != lastReq) {
        lastReq = s.blinkReq;
        blinkStart = now;
      }
      if ((int32_t)(now - nextAutoBlink) >= 0) {
        blinkStart = now;
        nextAutoBlink = now + 2500 + (esp_random() % 3000);
      }
      float open = (now - blinkStart < 130) ? 0.12f : 1.0f;
      const bool winking = (int32_t)(s.winkUntil - now) > 0;
      // home screen: glance left / right / up now and then, then back to the middle
      static uint32_t nextGlance = 0, glanceEnd = 0;
      static int8_t gx = 0, gy = 0;
      if (s.glance && s.lookX == 0 && s.lookY == 0) {
        if (nextGlance == 0) nextGlance = now + 3000;
        if (gx == 0 && gy == 0 && (int32_t)(now - nextGlance) >= 0) {
          static const int8_t kDir[6][2] = {{-1, 0}, {1, 0}, {-1, 0}, {1, 0}, {0, -1}, {1, -1}};
          const uint32_t r = esp_random();
          gx = kDir[r % 6][0];
          gy = kDir[r % 6][1];
          glanceEnd = now + 900 + (r >> 8) % 700;
        } else if ((gx || gy) && (int32_t)(now - glanceEnd) >= 0) {
          gx = gy = 0;
          nextGlance = now + 3000 + esp_random() % 4000;
        }
      } else {
        gx = gy = 0;
      }
      int dx = (s.lookX ? s.lookX : gx) * 14, dy = (s.lookY ? s.lookY : gy) * 9;
      if (s.expr == FACE_BLUSH) {  // petting: happy eyes, still, with blushing cheeks
        const uint8_t happy = elx::enums::face::HAPPY;
        drawEye(40, 30, true, happy, open);
        drawEye(88, 30, false, happy, open);
        drawBlush(40, 30);
        drawBlush(88, 30);
        break;
      }
      drawEye(40 + dx, 32 + dy, true, s.expr, open);
      drawEye(88 + dx, 32 + dy, false, s.expr, winking ? 0.12f : open);
      break;
    }
    case M_ICON:
      drawIcon(s.icon);
      break;
    case M_ANIM:
      drawAnim(s.anim, now - s.animStart);
      break;
    case M_CANVAS:
      break;  // copied in faceTask
    case M_TEXT: {
      u8g2_SetFont(&oled, u8g2_font_helvB18_tr);
      int w = u8g2_GetUTF8Width(&oled, s.text);
      if (w > OLED_WIDTH - 4) {
        u8g2_SetFont(&oled, u8g2_font_helvB12_tr);
        w = u8g2_GetUTF8Width(&oled, s.text);
      }
      if (w <= OLED_WIDTH - 4) {
        drawCentered(s.text, 40);
      } else {  // marquee
        int span = w + OLED_WIDTH;
        int off = (int)(((now - s.textSince) / 25) % span);
        u8g2_DrawUTF8(&oled, OLED_WIDTH - off, 40, s.text);
      }
      break;
    }
    case M_NUMBER: {
      char buf[20];
      float n = s.number;
      if (n == (float)(int32_t)n) snprintf(buf, sizeof buf, "%ld", (long)(int32_t)n);
      else {
        snprintf(buf, sizeof buf, "%.2f", (double)n);
        char* e = buf + strlen(buf) - 1;
        while (*e == '0') *e-- = 0;
      }
      u8g2_SetFont(&oled, u8g2_font_logisoso32_tn);
      if (u8g2_GetStrWidth(&oled, buf) > OLED_WIDTH - 4) u8g2_SetFont(&oled, u8g2_font_helvB18_tr);
      drawCentered(buf, 48);
      break;
    }
    case M_MENU: {
      u8g2_SetFont(&oled, u8g2_font_6x10_tr);
      u8g2_DrawStr(&oled, 2, 9, s.text);
      u8g2_DrawHLine(&oled, 0, 11, OLED_WIDTH);
      uint8_t first = s.selected > 3 ? s.selected - 3 : 0;
      for (uint8_t i = first; i < s.count && i < first + 4; ++i) {
        int y = 22 + (i - first) * 10;
        if (i == s.selected) {
          u8g2_DrawBox(&oled, 0, y - 8, OLED_WIDTH, 10);
          u8g2_SetDrawColor(&oled, 0);
        }
        u8g2_DrawStr(&oled, 4, y, s.items[i]);
        u8g2_SetDrawColor(&oled, 1);
      }
      u8g2_DrawStr(&oled, 2, 63, s.small);
      break;
    }
    case M_CAROUSEL: {
      u8g2_SetFont(&oled, u8g2_font_6x10_tr);
      drawCentered(s.small, 9);  // title
      const int cy = 28;
      drawMenuIcon(s.icon, 64, cy, s.text);
      if (s.count > 1) {  // < > arrows: the joystick moves sideways
        u8g2_DrawTriangle(&oled, 2, cy, 9, cy - 7, 9, cy + 7);
        u8g2_DrawTriangle(&oled, 125, cy, 118, cy - 7, 118, cy + 7);
      }
      if (s.icon != homeicon::LEVEL) {
        u8g2_SetFont(&oled, u8g2_font_helvB10_tr);
        drawCentered(s.text, 55);  // label
      }
      const int n = s.count > 12 ? 12 : s.count;  // position dots
      const int x0 = 64 - (n - 1) * 4;
      for (int i = 0; i < n; ++i) {
        if (i == s.selected) u8g2_DrawDisc(&oled, x0 + i * 8, 61, 2, U8G2_DRAW_ALL);
        else u8g2_DrawCircle(&oled, x0 + i * 8, 61, 1, U8G2_DRAW_ALL);
      }
      break;
    }
    case M_MESSAGE:
      u8g2_SetFont(&oled, u8g2_font_helvB12_tr);
      drawCentered(s.text, 30);
      u8g2_SetFont(&oled, u8g2_font_6x10_tr);
      drawCentered(s.small, 52);
      break;
    case M_BLANK:
    default:
      break;
  }
}

void faceTask(void*) {
  static uint32_t lastSum = 0xFFFFFFFF;
  for (;;) {
    FaceState copy;
    portENTER_CRITICAL(&g_mux);
    copy = g_state;
    portEXIT_CRITICAL(&g_mux);
    render(copy, millis());
    if (copy.mode == M_CANVAS) {
      portENTER_CRITICAL(&g_mux);
      memcpy(u8g2_GetBufferPtr(&oled), g_canvas, sizeof g_canvas);
      portEXIT_CRITICAL(&g_mux);
    }
    // Only push the frame over I2C if it changed: keeps the bus free for the IMU.
    const uint8_t* b = u8g2_GetBufferPtr(&oled);
    uint32_t sum = 2166136261u;
    for (int i = 0; i < OLED_WIDTH * OLED_HEIGHT / 8; ++i) sum = (sum ^ b[i]) * 16777619u;
    if (!s_active) {
      lastSum = 0xFFFFFFFF;  // repaint fully when blocks get the screen back
    } else if (sum != lastSum && s_display) {
      // Same page layout in both libraries: copy U8g2's frame into Adafruit's and send it.
      memcpy(s_display->getBuffer(), b, OLED_WIDTH * OLED_HEIGHT / 8);
      s_display->display();  // Wire locks the bus per chunk, so sensors keep running
      lastSum = sum;
    }
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

// ------------------------------------------------------------------ U8g2 <-> Adafruit glue
// U8g2 only renders into its RAM buffer here; frames go out via Adafruit_SSD1306,
// the driver the game code already uses, so the OLED is set up exactly once.
static uint8_t noByteCb(u8x8_t*, uint8_t, uint8_t, void*) { return 1; }
static uint8_t noGpioCb(u8x8_t*, uint8_t, uint8_t, void*) { return 1; }

void Face::begin(Adafruit_SSD1306* display) {
  s_display = display;
  u8g2_Setup_ssd1306_i2c_128x64_noname_f(&oled, U8G2_R0, noByteCb, noGpioCb);
  xTaskCreatePinnedToCore(faceTask, "face", 4096, nullptr, 1, nullptr, 0);
}

void Face::setActive(bool on) { s_active = on; }
