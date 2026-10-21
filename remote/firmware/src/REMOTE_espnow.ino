/* ============================================================
 *  SINGLE-HAND REMOTE — ESP-NOW CONTROLLER  (ESP8266 / D1 Mini)
 * ============================================================
 *  Reads joystick (via 4051 mux) + 3 buttons, streams them to the
 *  MAINBOT over ESP-NOW ~50x/sec. Listens for an alert byte back
 *  from the robot and fires the vibration motor:
 *     DROP     -> heartbeat buzz
 *     OBSTACLE -> short buzz
 *  Local button presses still buzz too (kept from original).
 *
 *  Sends to the PAIRED robot (saved in EEPROM). Never paired: sends nothing, and any
 *  button press gives two long buzzes ("not paired"). Set REQUIRE_PAIRING 0 to fall
 *  back to MAINBOT_MAC for development.
 *
 *  PAIRING (Stage 5): hold the FRONT + BOTTOM buttons while switching the remote on.
 *  It buzzes twice, then looks for a robot in pairing mode for 30 s (robot: hold its
 *  touch pad while switching on, or "Pair remote" in the app). Success = heartbeat buzz,
 *  nothing found = one long buzz (the old pairing is kept).
 *
 *  NOTE: ESP8266 and ESP32 use DIFFERENT esp-now APIs. This is the
 *  ESP8266 side (espnow.h). The struct layout matches the robot's.
 * ============================================================ */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <math.h>

#include <EEPROM.h>        // pairing memory (listed here so PlatformIO finds the library)
#include "ElxiePairing.h"  // robot <-> remote pairing (Stage 5)

// ---- MAINBOT (peer) MAC ----
// Replaced at boot by the robot saved in EEPROM by pairing.
// REQUIRE_PAIRING 1: a never-paired remote controls no robot (no stray remote can drive
// someone else's robot). 0: a never-paired remote drives this default robot (development).
#define REQUIRE_PAIRING 1
uint8_t MAINBOT_MAC[6] = {0x90, 0x70, 0x69, 0x14, 0xD4, 0x38};
bool linked = false; // true when this remote may send to MAINBOT_MAC

// ---- Mux control pins ----
const uint8_t MUX_S0 = D5;
const uint8_t MUX_S1 = D6;
const uint8_t MUX_S2 = D7;
const uint8_t MUX_COM = A0;

// ---- Mux channel assignments ----
const uint8_t CH_X = 7;
const uint8_t CH_Y = 5;
const uint8_t CH_BATT = 4; // battery divider (100k / 100k) on mux channel 4

// ---- Battery (one 18650 cell) ----
// cell+ --100k-- mux ch4 --100k-- GND, mux COM -> D1 Mini A0 (A0 pin 3.2 V = raw 1023).
// Measured on the Elxie remote (Oct 2026): no loading of the divider by A0 is visible
// (cell 3.92 V on the meter, firmware 4.03 V with plain x2), so A0_LOAD = 0, and
// BATT_CAL = 3.92 / 4.03 corrects the ESP8266 ADC's gain.
const float BATT_R_TOP = 100000.0f;    // battery side
const float BATT_R_BOTTOM = 100000.0f; // to GND
const float A0_LOAD = 0.0f;            // 0 = no loading (measured); 320000 if A0 loads the divider
const float A0_FULL_SCALE_V = 3.2f;    // A0 pin volts at raw 1023
const float BATT_CAL = 0.972f;         // multimeter / reading (3.92 V meter / 4.03 V reading, Oct 2026)
const unsigned long BATT_REPORT_MS = 2000;
// Percent: straight line on the CELL voltage, set from measurements (Oct 2026):
//   cell 4.10 V (2.05 V at mux ch4, unloaded) = 100 %,  cell 3.65 V (1.825 V) = 0 %
const uint16_t BATT_FULL_MV = 4100;
const uint16_t BATT_EMPTY_MV = 3650;
uint8_t batteryPercent(uint16_t cellMv) { return elxLinearPercent(cellMv, BATT_FULL_MV, BATT_EMPTY_MV, 0); }

// ---- Direct digital inputs ----
const uint8_t PIN_SW = D3;   // Joystick button
const uint8_t PIN_BTN1 = D1; // Front button
const uint8_t PIN_BTN2 = D2; // Button under joystick

// ---- Vibration motor ----
const uint8_t PIN_MOTOR = D8;
const int PWM_MAX = 1023;

// ---- Joystick center calibration ----
const int X_CENTER = 512;
const int Y_CENTER = 512;
const int DEAD = 150;

// ---- Alert codes (match robot) ----
#define ALERT_OK 0
#define ALERT_OBSTACLE 1
#define ALERT_DROP 2

// ── Packed structs — MUST match the robot byte-for-byte ─────
typedef struct __attribute__((packed))
{
  int16_t x;
  int16_t y;
  uint8_t btn1; // Button On Front
  uint8_t btn2; // Button On Bottom
  uint8_t sw;   // Joystick Centre button
} CommandPacket;

typedef struct __attribute__((packed))
{
  uint8_t alert;
} FeedbackPacket;

// =====================================================================
//  Non-blocking haptic engine (from original controller firmware)
// =====================================================================
const unsigned long PATTERN_SHORT[] = {100};
const unsigned long PATTERN_HEARTBEAT[] = {80, 120, 80};
const unsigned long PATTERN_NOT_PAIRED[] = {400, 150, 400}; // two long buzzes
const unsigned long SINE_DURATION_MS = 800;

enum HapticType
{
  HAPTIC_TYPE_NONE,
  HAPTIC_TYPE_SEQUENCE,
  HAPTIC_TYPE_SINE
};
HapticType hapticType = HAPTIC_TYPE_NONE;
bool hapticActive = false;

const unsigned long *hapticSeq = nullptr;
uint8_t hapticSeqLen = 0, hapticStep = 0;
bool hapticOn = false;
unsigned long hapticStepStart = 0, sineStartTime = 0;

void startHapticSequence(const unsigned long *seq, uint8_t len)
{
  if (hapticActive)
    return;
  hapticType = HAPTIC_TYPE_SEQUENCE;
  hapticSeq = seq;
  hapticSeqLen = len;
  hapticStep = 0;
  hapticOn = true;
  hapticActive = true;
  hapticStepStart = millis();
  digitalWrite(PIN_MOTOR, HIGH);
}
void startHapticShort()
{
  startHapticSequence(PATTERN_SHORT,
                      sizeof(PATTERN_SHORT) / sizeof(unsigned long));
}
void startHapticHeartbeat()
{
  startHapticSequence(PATTERN_HEARTBEAT,
                      sizeof(PATTERN_HEARTBEAT) / sizeof(unsigned long));
}
void startHapticNotPaired()
{
  startHapticSequence(PATTERN_NOT_PAIRED,
                      sizeof(PATTERN_NOT_PAIRED) / sizeof(unsigned long));
}
void startHapticSine()
{
  if (hapticActive)
    return;
  hapticType = HAPTIC_TYPE_SINE;
  hapticActive = true;
  sineStartTime = millis();
}
void updateHaptic()
{
  if (!hapticActive)
    return;
  if (hapticType == HAPTIC_TYPE_SEQUENCE)
  {
    if (millis() - hapticStepStart < hapticSeq[hapticStep])
      return;
    hapticStep++;
    if (hapticStep >= hapticSeqLen)
    {
      digitalWrite(PIN_MOTOR, LOW);
      hapticActive = false;
      hapticType = HAPTIC_TYPE_NONE;
      return;
    }
    hapticOn = !hapticOn;
    digitalWrite(PIN_MOTOR, hapticOn ? HIGH : LOW);
    hapticStepStart = millis();
  }
  else if (hapticType == HAPTIC_TYPE_SINE)
  {
    unsigned long elapsed = millis() - sineStartTime;
    if (elapsed >= SINE_DURATION_MS)
    {
      analogWrite(PIN_MOTOR, 0);
      hapticActive = false;
      hapticType = HAPTIC_TYPE_NONE;
      return;
    }
    float shaped = sin(PI * (float)elapsed / (float)SINE_DURATION_MS);
    analogWrite(PIN_MOTOR, (int)(shaped * PWM_MAX));
  }
}
// =====================================================================

int readMux(uint8_t channel)
{
  digitalWrite(MUX_S0, channel & 0x01);
  digitalWrite(MUX_S1, (channel >> 1) & 0x01);
  digitalWrite(MUX_S2, (channel >> 2) & 0x01);
  delayMicroseconds(50);
  return analogRead(MUX_COM);
}

// ---- ESP-NOW receive (robot -> remote alert) ----
volatile uint8_t pendingAlert = ALERT_OK;
volatile bool alertFlag = false;

void onRecv(uint8_t *mac, uint8_t *data, uint8_t len)
{
  if (elxpair::onRecv(mac, data, len))
    return; // pairing packet
  if (len != sizeof(FeedbackPacket))
    return;
  FeedbackPacket fb;
  memcpy(&fb, data, sizeof(fb));
  pendingAlert = fb.alert;
  alertFlag = true;
}

bool prevBtn1 = false, prevBtn2 = false, prevSw = false;

// ---- battery: millivolts of the cell (averaged, smoothed) ----
float battMvSmooth = 0;
uint16_t lastBattRaw = 0; // last averaged ADC reading of mux ch4 (for checking the divider maths)
uint16_t readBatteryMv()
{
  readMux(CH_BATT); // settle after switching the mux
  long sum = 0;
  for (int i = 0; i < 8; ++i)
    sum += readMux(CH_BATT);
  const float vPin = (sum / 8.0f) * A0_FULL_SCALE_V / 1023.0f;
  const float rLow = A0_LOAD > 0 ? BATT_R_BOTTOM * A0_LOAD / (BATT_R_BOTTOM + A0_LOAD) : BATT_R_BOTTOM;
  lastBattRaw = (uint16_t)(sum / 8);
  const float vBat = vPin * (BATT_R_TOP + rLow) / rLow * BATT_CAL;
  const float mv = vBat * 1000.0f;
  battMvSmooth = battMvSmooth <= 0 ? mv : battMvSmooth * 0.7f + mv * 0.3f;
  return (uint16_t)(battMvSmooth + 0.5f);
}

unsigned long lastBattReport = 0;
void reportBattery()
{
  ElxRemoteStatus st;
  memcpy(st.magic, "ELXS", 4);
  st.version = ELX_PAIR_VERSION;
  st.battMv = readBatteryMv();
  st.battPct = batteryPercent(st.battMv);
  esp_now_send(MAINBOT_MAC, (uint8_t *)&st, sizeof st);
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 30000)
  {
    lastLog = millis();
    Serial.printf("BATTERY %u mV %u%% (raw %u)\n", st.battMv, st.battPct, lastBattRaw);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n--- ELXIE ESP-NOW REMOTE STARTING ---");

  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_MOTOR, OUTPUT);
  digitalWrite(PIN_MOTOR, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0)
  {
    // fatal: buzz SOS-ish forever
    while (1)
    {
      digitalWrite(PIN_MOTOR, HIGH);
      delay(100);
      digitalWrite(PIN_MOTOR, LOW);
      delay(400);
    }
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO); // send + receive
  esp_now_register_recv_cb(onRecv);

  // ---- which robot? (saved by pairing, else the default MAINBOT_MAC) ----
  bool paired = elxpair::loadRobotMac(MAINBOT_MAC);
  char macStr[18];
  elxpair::macToString(MAINBOT_MAC, macStr);
  if (paired)
    Serial.printf("Robot: %s (paired)\n", macStr);
  else if (REQUIRE_PAIRING)
    Serial.println("Robot: none (not paired: hold FRONT + BOTTOM while switching on to pair)");
  else
    Serial.printf("Robot: %s (default)\n", macStr);

  // ---- pairing: FRONT + BOTTOM held at power-on ----
  if (digitalRead(PIN_BTN1) == LOW && digitalRead(PIN_BTN2) == LOW)
  {
    delay(800); // still held? (not a bump)
    if (digitalRead(PIN_BTN1) == LOW && digitalRead(PIN_BTN2) == LOW)
    {
      Serial.println("PAIRING: looking for a robot in pairing mode (30 s)");
      startHapticShort();
      while (hapticActive)
        updateHaptic();
      delay(150);
      startHapticShort();
      while (hapticActive)
        updateHaptic();
      uint8_t robot[6];
      bool ok = elxpair::run(30000, robot, []() {
        // a tiny tick every second while searching
        static unsigned long last = 0;
        if (millis() - last > 1000)
        {
          last = millis();
          startHapticShort();
        }
        updateHaptic();
      });
      digitalWrite(PIN_MOTOR, LOW);
      hapticActive = false;
      if (ok)
      {
        memcpy(MAINBOT_MAC, robot, 6);
        paired = true;
        elxpair::macToString(MAINBOT_MAC, macStr);
        Serial.printf("PAIRED with robot %s\n", macStr);
        delay(300);
        startHapticHeartbeat();
      }
      else
      {
        Serial.println("PAIRING: no robot found, keeping the old one");
        digitalWrite(PIN_MOTOR, HIGH);
        delay(1000);
        digitalWrite(PIN_MOTOR, LOW);
      }
      while (hapticActive)
        updateHaptic();
      // wait until both buttons are released, so they don't count as presses
      while (digitalRead(PIN_BTN1) == LOW || digitalRead(PIN_BTN2) == LOW)
        delay(10);
    }
  }
  linked = paired || !REQUIRE_PAIRING;
  if (linked)
    esp_now_add_peer(MAINBOT_MAC, ESP_NOW_ROLE_COMBO, 1, NULL, 0);

  {
    const uint16_t mv = readBatteryMv();
    Serial.printf("BATTERY %u mV %u%% (raw %u)\n", mv, batteryPercent(mv), lastBattRaw);
  }

  // boot buzz: one short = ready, two long = not paired
  if (linked)
    startHapticShort();
  else
    startHapticNotPaired();
  while (hapticActive)
    updateHaptic();
}

unsigned long lastSend = 0;

void loop()
{
  // ---- read inputs ----
  int xVal = readMux(CH_X);
  int yVal = readMux(CH_Y);

  bool sw = (digitalRead(PIN_SW) == LOW);
  bool btn1 = (digitalRead(PIN_BTN1) == LOW);
  bool btn2 = (digitalRead(PIN_BTN2) == LOW);

  // ---- not paired: nothing is sent; a press only reminds the user ----
  if (!linked)
  {
    if ((btn1 && !prevBtn1) || (btn2 && !prevBtn2) || (sw && !prevSw))
    {
      Serial.println("[LOG] not paired: hold FRONT + BOTTOM while switching on to pair");
      startHapticNotPaired();
    }
    prevBtn1 = btn1;
    prevBtn2 = btn2;
    prevSw = sw;
    updateHaptic();
    delay(2);
    return;
  }

  // ---- local haptics on press (kept from original) ----
  if (btn1 && !prevBtn1)
  {
    Serial.println("[LOG] BTN1 (Front Button) Clicked -> Sending BUTTON_1 (LED Toggle)");
    startHapticShort();
  }
  if (btn2 && !prevBtn2)
  {
    Serial.println("[LOG] BTN2 (Bottom Button) Clicked -> Sending BACK command");
    startHapticShort();
  }
  if (sw && !prevSw)
  {
    Serial.println("[LOG] SW (Joystick) Clicked -> Sending ACTION_SELECT (Horn)");
    startHapticSine();
  }
  prevBtn1 = btn1;
  prevBtn2 = btn2;
  prevSw = sw;

  // ---- send command ~50Hz ----
  if (millis() - lastSend >= 20)
  {
    CommandPacket pkt;
    pkt.x = (int16_t)xVal;
    pkt.y = (int16_t)yVal;
    pkt.btn1 = btn1 ? 1 : 0;
    pkt.btn2 = btn2 ? 1 : 0;
    pkt.sw = sw ? 1 : 0;
    esp_now_send(MAINBOT_MAC, (uint8_t *)&pkt, sizeof(pkt));
    lastSend = millis();
  }

  // ---- battery status to the robot (it shows in the app) ----
  if (millis() - lastBattReport >= BATT_REPORT_MS)
  {
    lastBattReport = millis();
    reportBattery();
  }

  // ---- react to robot alert (edge-triggered) ----
  static uint8_t prevAlert = ALERT_OK;
  if (alertFlag)
  {
    alertFlag = false;
    if (pendingAlert != prevAlert)
    { // only on change
      if (pendingAlert == ALERT_DROP)
        startHapticHeartbeat();
      else if (pendingAlert == ALERT_OBSTACLE)
        startHapticShort();
      prevAlert = pendingAlert;
    }
  }

  updateHaptic();
  delay(2);
}
