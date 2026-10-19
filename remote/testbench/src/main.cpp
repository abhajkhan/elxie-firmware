// Elxie REMOTE testbench (ESP8266 / D1 Mini).
// Checks every part of the hand remote: joystick, 3 buttons, battery, vibration
// motor and the ESP-NOW radio. Pins are the same as REMOTE_espnow.ino.
//
// Output on USB serial (115200), two kinds of lines:
//  * readable text, e.g.  RESULT joyrange PASS X 18..1009, Y 25..1015
//                         TESTBENCH_DONE pass=N fail=N warn=N check=N
//  * '@' + one JSON object for the dashboard (tools/remote-testbench-ui/RemoteTestbench.html):
//      hello | start | test | ask | res | end | done | live | ack | err
// Commands (one per line): run | run <id> | live 0|1 | link 0|1 | buzz short|heart|sine|on|off |
//                          ping | mac | stop | pair | forget
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <math.h>
#include <stdarg.h>

#include <EEPROM.h>        // listed here so PlatformIO adds the EEPROM library (ElxiePairing.h needs it)
#include "ElxiePairing.h"  // from ../firmware/src (same pairing code as the real remote)

#define TB_VERSION "1.1"

// ---------------------------------------------------------------- pins (= REMOTE_espnow.ino)
static const uint8_t MUX_S0 = D5, MUX_S1 = D6, MUX_S2 = D7, MUX_COM = A0;
static const uint8_t CH_X = 7, CH_Y = 5, CH_BATT = 4;
static const uint8_t PIN_SW = D3;    // joystick press
static const uint8_t PIN_BTN1 = D1;  // front button  (= block "A")
static const uint8_t PIN_BTN2 = D2;  // bottom button (= block "B")
static const uint8_t PIN_MOTOR = D8; // vibration motor
static const int PWM_MAX = 1023;

// Robot the remote talks to: the paired one (EEPROM), else MAINBOT_MAC of REMOTE_espnow.ino
static const uint8_t DEFAULT_ROBOT_MAC[6] = {0x90, 0x70, 0x69, 0x14, 0xD4, 0x38};
static uint8_t ROBOT_MAC[6] = {0x90, 0x70, 0x69, 0x14, 0xD4, 0x38};
static bool g_paired = false;

// Battery: V_A0pin = raw / 1023 * 3.2 (D1 Mini's own A0 divider), then the remote's divider.
// Set BATT_R_TOP / BATT_R_BOTTOM to the resistors between battery+ and the mux input
// (top = battery side, bottom = to GND). 0 / 1 means "no extra divider".
#define BATT_R_TOP    100.0f
#define BATT_R_BOTTOM 100.0f
static const float kBattScale = 3.2f / 1023.0f * (BATT_R_TOP + BATT_R_BOTTOM) / BATT_R_BOTTOM;

// ---------------------------------------------------------------- packets (= robot's)
typedef struct __attribute__((packed)) {
  int16_t x, y;
  uint8_t btn1, btn2, sw;
} CommandPacket;
typedef struct __attribute__((packed)) {
  uint8_t alert;
} FeedbackPacket;

// ---------------------------------------------------------------- state
enum Status { ST_PASS, ST_FAIL, ST_WARN, ST_CHECK };
static const char* kSt[] = {"PASS", "FAIL", "WARN", "CHECK"};
static int g_count[4];
static bool g_live = true;      // stream @live lines
static bool g_link = false;     // keep sending real packets to the robot (like the remote)
static bool g_inTest = false;
static bool g_stopReq = false;
static bool g_radioOk = false;

static volatile uint32_t g_txDone = 0, g_txAck = 0, g_rxFb = 0;
static volatile uint8_t g_lastAlert = 0;
static uint32_t g_txSent = 0;

// ---------------------------------------------------------------- output helpers
static void jsonEsc(char* out, size_t cap, const char* in) {
  size_t k = 0;
  for (; *in && k + 2 < cap; ++in) {
    char c = *in;
    if (c == '"' || c == '\\') { out[k++] = '\\'; out[k++] = c; }
    else if ((uint8_t)c >= 0x20) out[k++] = c;
  }
  out[k] = 0;
}
static void emitf(const char* fmt, ...) {
  char line[300];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  Serial.print('@');
  Serial.println(line);
}
static void say(const char* fmt, ...) {
  char line[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  Serial.println(line);
}
static void ask(const char* msg) {
  char esc[200];
  jsonEsc(esc, sizeof esc, msg);
  say(">>> %s", msg);
  emitf("{\"t\":\"ask\",\"m\":\"%s\"}", esc);
}
static void result(const char* id, Status st, const char* fmt, ...) {
  char d[160], esc[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(d, sizeof d, fmt, ap);
  va_end(ap);
  g_count[st]++;
  say("RESULT %-9s %-5s %s", id, kSt[st], d);
  jsonEsc(esc, sizeof esc, d);
  emitf("{\"t\":\"res\",\"id\":\"%s\",\"st\":\"%s\",\"d\":\"%s\"}", id, kSt[st], esc);
}

// ---------------------------------------------------------------- inputs
static int readMux(uint8_t ch) {
  digitalWrite(MUX_S0, ch & 1);
  digitalWrite(MUX_S1, (ch >> 1) & 1);
  digitalWrite(MUX_S2, (ch >> 2) & 1);
  delayMicroseconds(50);
  return analogRead(MUX_COM);
}
struct Inputs { int x, y, bat; bool b1, b2, sw; };
static Inputs readInputs() {
  Inputs in;
  in.x = readMux(CH_X);
  in.y = readMux(CH_Y);
  in.bat = readMux(CH_BATT);
  in.b1 = digitalRead(PIN_BTN1) == LOW;
  in.b2 = digitalRead(PIN_BTN2) == LOW;
  in.sw = digitalRead(PIN_SW) == LOW;
  return in;
}

// ---------------------------------------------------------------- radio
static void onSent(uint8_t*, uint8_t status) {
  g_txDone++;
  if (status == 0) g_txAck++;  // 0 = the robot's radio acknowledged the packet
}
static void onRecv(uint8_t* mac, uint8_t* data, uint8_t len) {
  if (elxpair::onRecv(mac, data, len)) return;  // pairing packet
  if (len == sizeof(FeedbackPacket)) {
    g_lastAlert = data[0];
    g_rxFb++;
  }
}
static void sendPacket(const Inputs* in) {
  if (!g_radioOk) return;
  CommandPacket p;
  if (in) {
    p.x = (int16_t)in->x; p.y = (int16_t)in->y;
    p.btn1 = in->b1; p.btn2 = in->b2; p.sw = in->sw;
  } else {  // neutral: centre stick, nothing pressed (safe for the robot)
    p.x = 512; p.y = 512; p.btn1 = p.btn2 = p.sw = 0;
  }
  esp_now_send(ROBOT_MAC, (uint8_t*)&p, sizeof p);
  g_txSent++;
}
static void macStr(const uint8_t* m, char* out) {
  sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

// ---------------------------------------------------------------- vibration motor
static void motorOff() { analogWrite(PIN_MOTOR, 0); digitalWrite(PIN_MOTOR, LOW); }
static void serviceFor(uint32_t ms);
static void buzzPattern(const char* which) {
  if (!strcmp(which, "short")) {
    digitalWrite(PIN_MOTOR, HIGH); serviceFor(100); motorOff();
  } else if (!strcmp(which, "heart")) {
    digitalWrite(PIN_MOTOR, HIGH); serviceFor(80); motorOff(); serviceFor(120);
    digitalWrite(PIN_MOTOR, HIGH); serviceFor(80); motorOff();
  } else if (!strcmp(which, "sine")) {
    uint32_t t0 = millis();
    while (millis() - t0 < 800) {
      float s = sinf(PI * (float)(millis() - t0) / 800.0f);
      analogWrite(PIN_MOTOR, (int)(s * PWM_MAX));
      serviceFor(10);
    }
    motorOff();
  } else if (!strcmp(which, "on")) {
    digitalWrite(PIN_MOTOR, HIGH);
  } else {
    motorOff();
  }
}

// ---------------------------------------------------------------- live stream + commands
static uint32_t g_lastLive = 0, g_lastLink = 0, g_rateT0 = 0;
static uint32_t g_rateSent0 = 0, g_rateAck0 = 0, g_rateDone0 = 0;
static float g_txRate = 0, g_ackPct = -1;
static char g_cmd[48];
static uint8_t g_cmdLen = 0;
static void handleCommand(char* c);

static void service() {
  const uint32_t now = millis();
  // commands
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (g_cmdLen) { g_cmd[g_cmdLen] = 0; g_cmdLen = 0; handleCommand(g_cmd); }
    } else if (g_cmdLen + 1u < sizeof g_cmd) {
      g_cmd[g_cmdLen++] = ch;
    }
  }
  // real remote packets at 50 Hz (only when the dashboard asks)
  if (g_link && now - g_lastLink >= 20) {
    g_lastLink = now;
    Inputs in = readInputs();
    sendPacket(&in);
  }
  // packet rate / ack rate once a second
  if (now - g_rateT0 >= 1000) {
    const uint32_t dt = now - g_rateT0, sent = g_txSent - g_rateSent0, done = g_txDone - g_rateDone0,
                   acked = g_txAck - g_rateAck0;
    g_txRate = sent * 1000.0f / dt;
    g_ackPct = done ? acked * 100.0f / done : -1;
    g_rateT0 = now; g_rateSent0 = g_txSent; g_rateDone0 = g_txDone; g_rateAck0 = g_txAck;
  }
  // live data at 20 Hz
  if (g_live && now - g_lastLive >= 50) {
    g_lastLive = now;
    Inputs in = readInputs();
    emitf("{\"t\":\"live\",\"ms\":%lu,\"x\":%d,\"y\":%d,\"b1\":%d,\"b2\":%d,\"sw\":%d,\"bat\":%d,\"bv\":%.2f,"
          "\"radio\":%d,\"link\":%d,\"tx\":%.0f,\"ack\":%.0f,\"sent\":%lu,\"acked\":%lu,\"fb\":%lu,\"alert\":%u,\"rssi\":0}",
          (unsigned long)now, in.x, in.y, in.b1, in.b2, in.sw, in.bat, in.bat * kBattScale, g_radioOk, g_link,
          g_txRate, g_ackPct, (unsigned long)g_txSent, (unsigned long)g_txAck, (unsigned long)g_rxFb, g_lastAlert);
  }
  yield();
}
static void serviceFor(uint32_t ms) {
  const uint32_t t0 = millis();
  while (millis() - t0 < ms) { service(); delay(2); }
}

// ---------------------------------------------------------------- tests
struct Test { const char* id; const char* title; void (*fn)(); };

static void testAdc() {
  int mn[8], mx[8];
  for (int c = 0; c < 8; ++c) { mn[c] = 1024; mx[c] = -1; }
  for (int i = 0; i < 20; ++i) {
    for (int c = 0; c < 8; ++c) { int v = readMux(c); mn[c] = min(mn[c], v); mx[c] = max(mx[c], v); }
    serviceFor(5);
  }
  say("  mux channel  0..7 min/max:");
  for (int c = 0; c < 8; ++c) say("    CH%d %4d..%4d%s", c, mn[c], mx[c], c == CH_X ? "  (joystick X)" : c == CH_Y ? "  (joystick Y)" : c == CH_BATT ? "  (battery)" : "");
  bool flat = true;
  for (int c = 1; c < 8; ++c) if (abs(mx[c] - mx[0]) > 20) flat = false;
  if (flat) result("adc", ST_FAIL, "all 8 mux channels read the same (%d): check S0-S2 wiring (D5/D6/D7) and the 4051", mx[0]);
  else if (mx[CH_X] >= 1020 || mn[CH_X] <= 3 || mx[CH_Y] >= 1020 || mn[CH_Y] <= 3)
    result("adc", ST_WARN, "X %d, Y %d at rest: a stick axis is at the end of its range", mn[CH_X], mn[CH_Y]);
  else result("adc", ST_PASS, "mux switches channels: X %d, Y %d, battery %d", mn[CH_X], mn[CH_Y], mn[CH_BATT]);
}

static int g_cx = 512, g_cy = 512;
static void testCentre() {
  ask("Don't touch the joystick (3 s)");
  serviceFor(800);
  long sx = 0, sy = 0; int n = 0, mnx = 1024, mxx = 0, mny = 1024, mxy = 0;
  const uint32_t t0 = millis();
  while (millis() - t0 < 2000) {
    Inputs in = readInputs();
    sx += in.x; sy += in.y; n++;
    mnx = min(mnx, in.x); mxx = max(mxx, in.x); mny = min(mny, in.y); mxy = max(mxy, in.y);
    serviceFor(10);
  }
  g_cx = sx / n; g_cy = sy / n;
  const int noise = max(mxx - mnx, mxy - mny);
  emitf("{\"t\":\"cal\",\"cx\":%d,\"cy\":%d}", g_cx, g_cy);
  if (abs(g_cx - 512) > 150 || abs(g_cy - 512) > 150)
    result("centre", ST_FAIL, "centre X %d, Y %d (expected ~512 +-150): stick off-centre or wiring", g_cx, g_cy);
  else if (abs(g_cx - 512) > 80 || abs(g_cy - 512) > 80 || noise > 40)
    result("centre", ST_WARN, "centre X %d, Y %d, noise %d (games use a 150 dead zone, still OK)", g_cx, g_cy, noise);
  else result("centre", ST_PASS, "centre X %d, Y %d, noise %d counts", g_cx, g_cy, noise);
}

static void testRange() {
  ask("Move the joystick in big circles, all the way to the edge (8 s)");
  int mnx = 1024, mxx = 0, mny = 1024, mxy = 0;
  const uint32_t t0 = millis();
  while (millis() - t0 < 8000 && !g_stopReq) {
    Inputs in = readInputs();
    mnx = min(mnx, in.x); mxx = max(mxx, in.x); mny = min(mny, in.y); mxy = max(mxy, in.y);
    serviceFor(10);
  }
  emitf("{\"t\":\"cal\",\"x0\":%d,\"x1\":%d,\"y0\":%d,\"y1\":%d}", mnx, mxx, mny, mxy);
  const int sx = mxx - mnx, sy = mxy - mny;
  if (sx < 300 || sy < 300)
    result("joyrange", ST_FAIL, "X %d..%d, Y %d..%d: %s axis hardly moves", mnx, mxx, mny, mxy, sx < sy ? "X" : "Y");
  else if (mnx > 200 || mxx < 820 || mny > 200 || mxy < 820)
    result("joyrange", ST_WARN, "X %d..%d, Y %d..%d: does not reach both ends (needs <200 and >820)", mnx, mxx, mny, mxy);
  else result("joyrange", ST_PASS, "X %d..%d, Y %d..%d", mnx, mxx, mny, mxy);
}

// Holds the stick one way and reports which axis moved and which way.
static bool holdDir(const char* name, int& dx, int& dy) {
  char m[80];
  snprintf(m, sizeof m, "Push the joystick %s and hold it (4 s)", name);
  ask(m);
  const uint32_t t0 = millis();
  while (millis() - t0 < 4000 && !g_stopReq) {
    Inputs in = readInputs();
    dx = in.x - g_cx; dy = in.y - g_cy;
    if (abs(dx) > 300 || abs(dy) > 300) { serviceFor(300); in = readInputs(); dx = in.x - g_cx; dy = in.y - g_cy; return true; }
    serviceFor(10);
  }
  return false;
}
static void testDirection() {
  int ux, uy, rx, ry;
  if (!holdDir("UP (away from you)", ux, uy)) { result("joydir", ST_FAIL, "no big movement seen for UP"); return; }
  ask("Let go of the joystick");
  serviceFor(1000);
  if (!holdDir("RIGHT", rx, ry)) { result("joydir", ST_FAIL, "no big movement seen for RIGHT"); return; }
  ask("Let go of the joystick");
  // The robot expects: UP -> Y goes up (dy > 0), RIGHT -> X goes up (dx > 0).
  const bool upOnY = abs(uy) > abs(ux), rightOnX = abs(rx) > abs(ry);
  char d[120];
  snprintf(d, sizeof d, "UP moved %c%s, RIGHT moved %c%s", upOnY ? 'Y' : 'X', (upOnY ? uy : ux) > 0 ? "+" : "-",
           rightOnX ? 'X' : 'Y', (rightOnX ? rx : ry) > 0 ? "+" : "-");
  if (!upOnY || !rightOnX) result("joydir", ST_FAIL, "%s: X and Y are swapped (robot expects UP=Y+, RIGHT=X+)", d);
  else if (uy < 0 || rx < 0) result("joydir", ST_WARN, "%s: an axis is reversed (robot expects UP=Y+, RIGHT=X+)", d);
  else result("joydir", ST_PASS, "%s, as the robot expects", d);
  serviceFor(800);
}

static void testButtons() {
  Inputs in = readInputs();
  if (in.b1 || in.b2 || in.sw) {
    serviceFor(500);
    in = readInputs();
    if (in.b1 || in.b2 || in.sw) {
      result("buttons", ST_FAIL, "stuck pressed at start:%s%s%s", in.b1 ? " front" : "", in.b2 ? " bottom" : "", in.sw ? " stick" : "");
      return;
    }
  }
  struct { const char* name; uint8_t pin; } btn[3] = {
      {"the FRONT button (A)", PIN_BTN1}, {"the BOTTOM button (B)", PIN_BTN2}, {"DOWN on the joystick (click)", PIN_SW}};
  char missed[60] = "";
  for (auto& b : btn) {
    char m[80];
    snprintf(m, sizeof m, "Press %s", b.name);
    ask(m);
    bool seen = false;
    const uint32_t t0 = millis();
    while (millis() - t0 < 8000 && !g_stopReq) {
      if (digitalRead(b.pin) == LOW) { seen = true; break; }
      serviceFor(5);
    }
    if (seen) {
      say("  PASS %s", b.name);
      const uint32_t t1 = millis();
      while (digitalRead(b.pin) == LOW && millis() - t1 < 3000) serviceFor(5);  // wait for release
    } else {
      say("  FAIL %s not seen", b.name);
      strncat(missed, b.pin == PIN_BTN1 ? " front" : b.pin == PIN_BTN2 ? " bottom" : " stick", sizeof missed - strlen(missed) - 1);
    }
    serviceFor(200);
  }
  if (missed[0]) result("buttons", ST_FAIL, "not seen:%s (check the switch and its pin)", missed);
  else result("buttons", ST_PASS, "front, bottom and stick press all seen");
}

static void testBattery() {
  long s = 0;
  for (int i = 0; i < 16; ++i) { s += readMux(CH_BATT); serviceFor(5); }
  const int raw = s / 16;
  const float v = raw * kBattScale;
  if (BATT_R_TOP == 0.0f) {
    result("battery", ST_WARN, "raw %d = %.2f V at A0 (divider not set in the testbench yet, so no battery voltage)", raw, v);
  } else if (raw < 20) {
    result("battery", ST_FAIL, "raw %d: no battery voltage on mux channel 4", raw);
  } else if (v < 3.3f || v > 4.35f) {
    result("battery", ST_WARN, "%.2f V (raw %d): outside 3.3-4.35 V, charge or check the divider", v, raw);
  } else {
    result("battery", ST_PASS, "%.2f V (raw %d)", v, raw);
  }
}

static void testMotor() {
  ask("Hold the remote: it will buzz 3 times (short, double, soft swell)");
  serviceFor(1200);
  buzzPattern("short");  serviceFor(700);
  buzzPattern("heart");  serviceFor(700);
  buzzPattern("sine");   serviceFor(300);
  result("motor", ST_CHECK, "confirm you felt a short buzz, a double buzz and a soft swell");
}

static void testRadio() {
  char mine[20], robot[20];
  uint8_t mac[6];
  WiFi.macAddress(mac);
  macStr(mac, mine);
  macStr(ROBOT_MAC, robot);
  say("  this remote's MAC: %s   (robot's REMOTE_MAC must be this)", mine);
  say("  sends to robot MAC: %s", robot);
  emitf("{\"t\":\"mac\",\"me\":\"%s\",\"robot\":\"%s\",\"paired\":%d}", mine, robot, g_paired);
  if (!g_radioOk) { result("radio", ST_FAIL, "ESP-NOW did not start"); return; }
  ask("Switch the robot ON and keep it near (3 s)");
  serviceFor(1500);
  const uint32_t done0 = g_txDone, ack0 = g_txAck;
  for (int i = 0; i < 40; ++i) { sendPacket(nullptr); serviceFor(25); }
  serviceFor(200);
  const uint32_t done = g_txDone - done0, acked = g_txAck - ack0;
  if (done == 0) result("radio", ST_FAIL, "no send reports: radio problem");
  else if (acked >= done * 3 / 4) result("radio", ST_PASS, "robot %s answered %lu of %lu packets. Me: %s", robot, (unsigned long)acked, (unsigned long)done, mine);
  else if (acked > 0) result("radio", ST_WARN, "robot answered only %lu of %lu packets: move closer / check battery", (unsigned long)acked, (unsigned long)done);
  else result("radio", ST_WARN, "radio works, but robot %s did not answer: is it on, and is that its MAC? Me: %s", robot, mine);
}

static const Test kTests[] = {
    {"adc", "Joystick mux and ADC", testAdc},
    {"centre", "Joystick centre", testCentre},
    {"joyrange", "Joystick range", testRange},
    {"joydir", "Joystick direction", testDirection},
    {"buttons", "Buttons", testButtons},
    {"battery", "Battery", testBattery},
    {"motor", "Vibration motor", testMotor},
    {"radio", "Radio to the robot", testRadio},
};
static const int kNumTests = sizeof kTests / sizeof kTests[0];

static void runOne(int i, int n) {
  say("[%02d] %s", i + 1, kTests[i].title);
  emitf("{\"t\":\"test\",\"n\":%d,\"id\":\"%s\",\"title\":\"%s\"}", n, kTests[i].id, kTests[i].title);
  g_inTest = true;
  kTests[i].fn();
  g_inTest = false;
}
static void runAll() {
  memset(g_count, 0, sizeof g_count);
  g_stopReq = false;
  const uint32_t t0 = millis();
  say("=== Elxie remote testbench " TB_VERSION ": %d tests ===", kNumTests);
  emitf("{\"t\":\"start\",\"n\":%d}", kNumTests);
  for (int i = 0; i < kNumTests && !g_stopReq; ++i) runOne(i, i + 1);
  const unsigned secs = (millis() - t0) / 1000;
  say("SUMMARY pass=%d fail=%d warn=%d check=%d (%u s)", g_count[ST_PASS], g_count[ST_FAIL], g_count[ST_WARN], g_count[ST_CHECK], secs);
  say("TESTBENCH_DONE pass=%d fail=%d warn=%d check=%d", g_count[ST_PASS], g_count[ST_FAIL], g_count[ST_WARN], g_count[ST_CHECK]);
  emitf("{\"t\":\"done\",\"pass\":%d,\"fail\":%d,\"warn\":%d,\"check\":%d,\"secs\":%u}", g_count[ST_PASS], g_count[ST_FAIL],
        g_count[ST_WARN], g_count[ST_CHECK], secs);
}

static int g_pendingRun = -2;  // -2 none, -1 all, >=0 one test (runs from loop(), not inside service())
static bool g_pendingPair = false;

static void usePeer(const uint8_t* mac) {
  esp_now_del_peer(ROBOT_MAC);
  memcpy(ROBOT_MAC, mac, 6);
  esp_now_add_peer(ROBOT_MAC, ESP_NOW_ROLE_COMBO, 1, nullptr, 0);
}
static void runPairing() {
  ask("Pairing: put the robot in pairing mode (hold its touch pad while switching on). Searching 30 s");
  emitf("{\"t\":\"pair\",\"st\":\"searching\"}");
  uint8_t robot[6];
  const bool ok = elxpair::run(30000, robot, [] { serviceFor(1); });
  char m[20];
  if (ok) {
    usePeer(robot);
    g_paired = true;
    elxpair::macToString(ROBOT_MAC, m);
    say("PAIRED with robot %s", m);
    emitf("{\"t\":\"pair\",\"st\":\"ok\",\"robot\":\"%s\"}", m);
    buzzPattern("heart");
  } else {
    elxpair::macToString(ROBOT_MAC, m);
    say("PAIRING: no robot found, still talking to %s", m);
    emitf("{\"t\":\"pair\",\"st\":\"fail\",\"robot\":\"%s\"}", m);
    buzzPattern("sine");
  }
}
static void handleCommand(char* line) {
  char c[48];  // own copy: buzz patterns call service(), which reuses the receive buffer
  strncpy(c, line, sizeof c - 1);
  c[sizeof c - 1] = 0;
  char* arg = strchr(c, ' ');
  if (arg) *arg++ = 0;
  if (!strcmp(c, "run")) {
    if (g_inTest) { emitf("{\"t\":\"err\",\"m\":\"busy: a test is running\"}"); return; }
    if (!arg || !*arg || !strcmp(arg, "all")) { g_pendingRun = -1; return; }
    for (int i = 0; i < kNumTests; ++i)
      if (!strcmp(arg, kTests[i].id)) { g_pendingRun = i; return; }
    emitf("{\"t\":\"err\",\"m\":\"no test called %s\"}", arg);
  } else if (!strcmp(c, "live")) {
    g_live = arg && *arg == '1';
    emitf("{\"t\":\"ack\",\"m\":\"live %d\"}", g_live);
  } else if (!strcmp(c, "link")) {
    g_link = arg && *arg == '1';
    emitf("{\"t\":\"ack\",\"m\":\"%s\"}", g_link ? "sending to the robot like the real remote" : "not sending");
  } else if (!strcmp(c, "buzz")) {
    buzzPattern(arg ? arg : "short");
    emitf("{\"t\":\"ack\",\"m\":\"buzz %s\"}", arg ? arg : "short");
  } else if (!strcmp(c, "ping")) {
    for (int i = 0; i < 10; ++i) sendPacket(nullptr);
    emitf("{\"t\":\"ack\",\"m\":\"sent 10 neutral packets\"}");
  } else if (!strcmp(c, "mac")) {
    char mine[20], robot[20];
    uint8_t mac[6];
    WiFi.macAddress(mac);
    macStr(mac, mine);
    macStr(ROBOT_MAC, robot);
    emitf("{\"t\":\"mac\",\"me\":\"%s\",\"robot\":\"%s\",\"paired\":%d}", mine, robot, g_paired);
  } else if (!strcmp(c, "pair")) {
    if (g_inTest || g_pendingPair) { emitf("{\"t\":\"err\",\"m\":\"busy\"}"); return; }
    g_pendingPair = true;
  } else if (!strcmp(c, "forget")) {
    elxpair::forgetRobotMac();
    usePeer(DEFAULT_ROBOT_MAC);
    g_paired = false;
    emitf("{\"t\":\"pair\",\"st\":\"forgotten\",\"robot\":\"90:70:69:14:D4:38\"}");
  } else if (!strcmp(c, "stop")) {
    g_stopReq = true;
    g_link = false;
    motorOff();
    emitf("{\"t\":\"ack\",\"m\":\"stopped\"}");
  } else {
    emitf("{\"t\":\"err\",\"m\":\"unknown command %s\"}", c);
  }
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_MOTOR, OUTPUT);
  motorOff();
  analogWriteRange(PWM_MAX);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  g_paired = elxpair::loadRobotMac(ROBOT_MAC);
  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_add_peer(ROBOT_MAC, ESP_NOW_ROLE_COMBO, 1, nullptr, 0);
    esp_now_register_send_cb(onSent);
    esp_now_register_recv_cb(onRecv);
    g_radioOk = true;
  }
  char mine[20];
  uint8_t mac[6];
  WiFi.macAddress(mac);
  macStr(mac, mine);
  Serial.println();
  char robot[20];
  macStr(ROBOT_MAC, robot);
  emitf("{\"t\":\"hello\",\"v\":\"" TB_VERSION "\",\"me\":\"%s\",\"radio\":%d,\"robot\":\"%s\",\"paired\":%d}", mine,
        g_radioOk, robot, g_paired);
  say("Elxie remote testbench " TB_VERSION "  (MAC %s)", mine);
  g_rateT0 = millis();
  g_pendingRun = -1;  // run everything once at power-on
}

void loop() {
  service();
  if (g_pendingPair) {
    g_pendingPair = false;
    g_inTest = true;
    runPairing();
    g_inTest = false;
  }
  if (g_pendingRun != -2) {
    const int r = g_pendingRun;
    g_pendingRun = -2;
    g_stopReq = false;
    if (r == -1) runAll();
    else {
      runOne(r, r + 1);
      emitf("{\"t\":\"end\",\"id\":\"%s\"}", kTests[r].id);
    }
  }
  delay(2);
}
