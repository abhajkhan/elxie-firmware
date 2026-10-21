// Elxie hardware testbench.
// Runs every sensor and actuator once, prints a readable log on UART0 (115200)
// and ends with a summary. TESTBENCH.bat shows and saves the log.
//
// Two kinds of output share the line stream:
//  * human text (shown/saved by TESTBENCH.bat), e.g.
//      RESULT <test> PASS|FAIL|WARN|CHECK <details>
//      TESTBENCH_DONE pass=N fail=N warn=N check=N
//  * machine lines starting with '@' + one JSON object, for the live dashboard
//    (tools/testbench-ui/ElxieTestbench.html):
//      @{"t":"hello"...} @{"t":"start"} @{"t":"test","id":..} @{"t":"ask","m":..}
//      @{"t":"res","id":..,"st":..,"d":..} @{"t":"done",..} @{"t":"live",..} @{"t":"ack"|"err",..}
//  Commands (one per line, 115200): run | run <id> | live 0|1 | motor <l%> <r%> <ms> |
//      led <r> <g> <b> | buzz <hz> <ms> | stop
//
// Old human-log format notes:
//   RESULT <test> PASS|FAIL|WARN|CHECK <details>
//   TESTBENCH_DONE pass=N fail=N warn=N check=N
//
// Pins and constants come from firmware/main/board_config.h, and the I2C, ADC,
// PWM and motor code from firmware/main/hal — the same code the main firmware runs.
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_config.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "hal/motors.h"
#include "hal/platform.h"
#include "led_strip.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "u8g2.h"

#define TB_VERSION "1.1"

// ============================================================== result bookkeeping
enum Status { PASS, FAIL, WARN, CHECK };
static const char* kStatusName[] = {"PASS", "FAIL", "WARN", "CHECK"};
struct Result {
  char name[16];
  Status st;
  char detail[96];
};
static Result g_results[24];
static int g_nResults = 0;

static volatile bool g_running = false;  // a test (or the full run) is in progress
static volatile bool g_live = true;      // stream @live lines
static SemaphoreHandle_t g_sense = nullptr;  // IR mux / ADC / ultrasonic: one user at a time
struct SenseLock {
  bool ok;
  explicit SenseLock(uint32_t ms = 200) { ok = xSemaphoreTake(g_sense, pdMS_TO_TICKS(ms)) == pdTRUE; }
  ~SenseLock() {
    if (ok) xSemaphoreGive(g_sense);
  }
};

// copy s into out as a JSON string body (quotes and backslashes escaped, control chars dropped)
static void jsonEsc(char* out, size_t cap, const char* in) {
  size_t o = 0;
  for (; *in && o + 2 < cap; ++in) {
    const char c = *in;
    if (c == '"' || c == '\\') {
      out[o++] = '\\';
      out[o++] = c;
    } else if ((unsigned char)c >= 0x20) {
      out[o++] = c;
    }
  }
  out[o] = 0;
}

static void emitf(const char* fmt, ...) {  // one machine line: '@' + JSON object
  char line[400];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  printf("@%s\n", line);
  fflush(stdout);
}

static void logf(const char* fmt, ...) {
  char line[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  printf("%s\n", line);
  fflush(stdout);
  if (strncmp(line, ">>> ", 4) == 0) {  // instructions also go to the dashboard banner
    char esc[220];
    jsonEsc(esc, sizeof esc, line + 4);
    emitf("{\"t\":\"ask\",\"m\":\"%s\"}", esc);
  }
}

static void result(const char* name, Status st, const char* fmt, ...) {
  char detail[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(detail, sizeof detail, fmt, ap);
  va_end(ap);
  if (g_nResults < (int)(sizeof g_results / sizeof g_results[0])) {
    Result& r = g_results[g_nResults++];
    snprintf(r.name, sizeof r.name, "%s", name);
    r.st = st;
    snprintf(r.detail, sizeof r.detail, "%s", detail);
  }
  logf("RESULT %-10s %-5s %s", name, kStatusName[st], detail);
  char esc[200];
  jsonEsc(esc, sizeof esc, detail);
  emitf("{\"t\":\"res\",\"id\":\"%s\",\"st\":\"%s\",\"d\":\"%s\"}", name, kStatusName[st], esc);
}

static void section(int n, const char* id, const char* title) {
  emitf("{\"t\":\"test\",\"n\":%d,\"id\":\"%s\",\"title\":\"%s\"}", n, id, title);
  logf("");
  logf("---------------------------------------------------------------");
  logf("[%02d] %s", n, title);
  logf("---------------------------------------------------------------");
}

// ============================================================== OLED (U8g2, same glue as the main firmware)
static u8g2_t g_oled;
static i2c_master_dev_handle_t g_oledDev = nullptr;
static bool g_oledOk = false;

static uint8_t oledByteCb(u8x8_t*, uint8_t msg, uint8_t argInt, void* argPtr) {
  static uint8_t buf[160];
  static size_t n = 0;
  switch (msg) {
    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_SET_DC:
      break;
    case U8X8_MSG_BYTE_START_TRANSFER:
      n = 0;
      break;
    case U8X8_MSG_BYTE_SEND:
      if (n + argInt <= sizeof buf) {
        memcpy(buf + n, argPtr, argInt);
        n += argInt;
      }
      break;
    case U8X8_MSG_BYTE_END_TRANSFER: {
      hal::I2cLock lock(50);
      if (lock.ok && g_oledDev) i2c_master_transmit(g_oledDev, buf, n, 50);
      break;
    }
    default:
      return 0;
  }
  return 1;
}

static uint8_t oledDelayCb(u8x8_t*, uint8_t msg, uint8_t argInt, void*) {
  switch (msg) {
    case U8X8_MSG_DELAY_MILLI: delayMs(argInt); break;
    case U8X8_MSG_DELAY_10MICRO: esp_rom_delay_us(10u * argInt); break;
    case U8X8_MSG_DELAY_100NANO: esp_rom_delay_us(1); break;
    default: break;
  }
  return 1;
}

static void screen(const char* line1, const char* line2 = "", const char* line3 = "") {
  if (!g_oledOk) return;
  u8g2_ClearBuffer(&g_oled);
  u8g2_SetFont(&g_oled, u8g2_font_6x10_tr);
  u8g2_DrawStr(&g_oled, 0, 10, "ELXIE TESTBENCH");
  u8g2_DrawHLine(&g_oled, 0, 13, 128);
  u8g2_DrawStr(&g_oled, 0, 30, line1);
  u8g2_DrawStr(&g_oled, 0, 44, line2);
  u8g2_DrawStr(&g_oled, 0, 58, line3);
  u8g2_SendBuffer(&g_oled);
}

// ============================================================== small helpers
static const int kLineCh[8] = IR_LINE_CHANNELS;
static const int kEdgeCh[4] = IR_EDGE_CHANNELS;
static const int kDepthCh[4] = IR_DEPTH_CHANNELS;

static int readMux(int ch) {
  hal::pinWrite(PIN_MUX_S0, ch & 1);
  hal::pinWrite(PIN_MUX_S1, (ch >> 1) & 1);
  hal::pinWrite(PIN_MUX_S2, (ch >> 2) & 1);
  hal::pinWrite(PIN_MUX_S3, (ch >> 3) & 1);
  esp_rom_delay_us(MUX_SETTLE_US + 20);
  hal::adcRead(PIN_MUX_SIG);  // first read after switching settles the sample cap
  return hal::adcRead(PIN_MUX_SIG);
}

static bool touchActive() { return hal::pinRead(PIN_TOUCH) == (TOUCH_ACTIVE_HIGH ? 1 : 0); }

static void readAllIr(int v[16]) {
  SenseLock lock;
  for (int c = 0; c < 16; ++c) v[c] = lock.ok ? readMux(c) : 0;
}

static float batteryVolts() {
  SenseLock lock;
  int sum = 0;
  for (int i = 0; i < 16; ++i) sum += hal::adcRead(PIN_BATTERY);
  return sum / 16.0f / 4095.0f * 3.1f * BATT_DIVIDER;
}

// Ultrasonic echo timing (ISR)
static volatile uint32_t g_echoRise = 0, g_echoWidth = 0;
static volatile bool g_echoNew = false;
static void IRAM_ATTR echoIsr(void*) {
  uint32_t now = (uint32_t)esp_timer_get_time();
  if (gpio_get_level((gpio_num_t)PIN_US_ECHO)) {
    g_echoRise = now;
  } else {
    g_echoWidth = now - g_echoRise;
    g_echoNew = true;
  }
}

// Buzzer
static void tone(uint32_t hz, uint32_t ms) {
  hal::pwmFreq(LEDC_TIMER_BUZZER, hz);
  hal::pwmWrite(LEDC_CH_BUZZER, 1u << (BUZZER_PWM_BITS - 1));
  delayMs(ms);
  hal::pwmWrite(LEDC_CH_BUZZER, 0);
}

// NeoPixels
static led_strip_handle_t g_strip = nullptr;
static uint8_t dim(uint8_t v) { return (uint8_t)(v * 40 / 255); }  // ~16 %: safe on USB power
static void stripAll(uint8_t r, uint8_t g, uint8_t b) {
  if (!g_strip) return;
  for (int i = 0; i < NEOPIXEL_COUNT; ++i) led_strip_set_pixel(g_strip, i, dim(r), dim(g), dim(b));
  led_strip_refresh(g_strip);
}

// Remote (ESP-NOW)
static portMUX_TYPE g_rmux = portMUX_INITIALIZER_UNLOCKED;
static volatile int g_rxCount = 0, g_rxOurs = 0;
static volatile uint8_t g_rxMac[6];
static volatile uint8_t g_rxData[8];
static volatile int g_rxLen = 0;
// The remote this robot is paired with: read from the Home firmware's NVS ("elxie" /
// "remote_mac", same flash layout), else board_config's REMOTE_MAC.
static uint8_t g_remoteMac[6] = REMOTE_MAC;
static bool g_remotePaired = false;
static bool g_radioOk = false;

static void loadPairedRemote() {
  nvs_handle_t h;
  if (nvs_open("elxie", NVS_READONLY, &h) != ESP_OK) return;
  uint8_t mac[6];
  size_t n = sizeof mac;
  if (nvs_get_blob(h, "remote_mac", mac, &n) == ESP_OK && n == 6 && (mac[0] & 1) == 0 &&
      (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) != 0) {
    memcpy(g_remoteMac, mac, 6);
    g_remotePaired = true;
  }
  nvs_close(h);
}

// Joystick -100..100 (right +, up +) and button bits from either remote packet:
//   hand remote (ESP8266): int16 x, int16 y (raw ADC 0..1023), btn1, btn2, stick = 7 bytes
//   Block Code remote:     0xE1, 1, int8 x, int8 y, uint16 buttons
static int remoteAxis(int raw) {
  int d = raw - 512;
  if (abs(d) < 60) d = 0;
  d = d * 100 / 450;
  return d > 100 ? 100 : (d < -100 ? -100 : d);
}
static bool decodeRemote(const uint8_t* d, int len, int* x, int* y, int* btn) {
  if (len == 7) {
    *x = remoteAxis((int16_t)(d[0] | (d[1] << 8)));
    *y = remoteAxis((int16_t)(d[2] | (d[3] << 8)));
    *btn = (d[4] ? 1 << REMOTE_BTN_A : 0) | (d[5] ? 1 << REMOTE_BTN_B : 0) | (d[6] ? 1 << REMOTE_BTN_STICK : 0);
    return true;
  }
  if (len >= 6 && d[0] == 0xE1 && d[1] == 1) {
    *x = (int8_t)d[2];
    *y = (int8_t)d[3];
    *btn = d[4] | (d[5] << 8);
    return true;
  }
  return false;
}

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  const bool control = len == 7 || (len >= 6 && data[0] == 0xE1);  // not pairing packets
  if (!control) return;
  portENTER_CRITICAL(&g_rmux);
  g_rxCount = g_rxCount + 1;
  if (memcmp(info->src_addr, g_remoteMac, 6) == 0) g_rxOurs = g_rxOurs + 1;
  for (int i = 0; i < 6; ++i) g_rxMac[i] = info->src_addr[i];
  g_rxLen = len < 8 ? len : 8;
  for (int i = 0; i < g_rxLen; ++i) g_rxData[i] = data[i];
  portEXIT_CRITICAL(&g_rmux);
}

// IMU + compass handles, opened once at start (tests and the live stream share them)
static i2c_master_dev_handle_t g_imuDev = nullptr, g_magDev = nullptr;
static uint8_t g_imuAddr = 0;
static void sensorsInit() {
  g_imuAddr = hal::i2cProbe(IMU_I2C_ADDR) ? IMU_I2C_ADDR : (hal::i2cProbe(IMU_I2C_ADDR_ALT) ? IMU_I2C_ADDR_ALT : 0);
  if (g_imuAddr) {
    g_imuDev = hal::addI2cDevice(g_imuAddr);
    hal::I2cLock lock(50);
    hal::i2cWriteReg(g_imuDev, 0x6B, 0x00);  // wake
    hal::i2cWriteReg(g_imuDev, 0x1A, 0x03);
    hal::i2cWriteReg(g_imuDev, 0x1B, 0x08);  // +-500 dps
    hal::i2cWriteReg(g_imuDev, 0x1C, 0x00);  // +-2 g
  }
  if (hal::i2cProbe(MAG_QMCP_ADDR)) {
    g_magDev = hal::addI2cDevice(MAG_QMCP_ADDR);
    {
      hal::I2cLock lock(50);
      hal::i2cWriteReg(g_magDev, 0x0B, 0x08);  // set/reset on, +-8 G
    }
    delayMs(5);
    hal::I2cLock lock(50);
    hal::i2cWriteReg(g_magDev, 0x0A, 0xCF);  // continuous, 200 Hz
  }
}

// ============================================================== tests
static void testI2c() {
  section(1, "i2c", "I2C bus scan");
  logf("SDA=GPIO%d  SCL=GPIO%d", PIN_I2C_SDA, PIN_I2C_SCL);
  char found[96] = "";
  int n = 0;
  for (int a = 0x08; a <= 0x77; ++a) {
    if (hal::i2cProbe((uint8_t)a)) {
      char s[8];
      snprintf(s, sizeof s, "0x%02X ", a);
      strncat(found, s, sizeof found - strlen(found) - 1);
      ++n;
    }
  }
  logf("devices found: %d  %s", n, n ? found : "(none)");
  const bool imu = hal::i2cProbe(IMU_I2C_ADDR) || hal::i2cProbe(IMU_I2C_ADDR_ALT);
  const bool mag = hal::i2cProbe(MAG_QMCP_ADDR) || hal::i2cProbe(MAG_QMC_ADDR) || hal::i2cProbe(MAG_HMC_ADDR);
  const bool oled = hal::i2cProbe(OLED_I2C_ADDR);
  logf("IMU (0x%02X/0x%02X): %s   compass (0x%02X): %s   OLED (0x%02X): %s", IMU_I2C_ADDR, IMU_I2C_ADDR_ALT,
       imu ? "yes" : "NO", MAG_QMCP_ADDR, mag ? "yes" : "NO", OLED_I2C_ADDR, oled ? "yes" : "NO");
  if (n == 0)
    result("i2c", FAIL, "nothing answers: check SDA/SCL wiring, 3.3 V and pull-ups");
  else if (imu && mag && oled)
    result("i2c", PASS, "%d device(s): %s", n, found);
  else
    result("i2c", FAIL, "missing:%s%s%s", imu ? "" : " IMU", mag ? "" : " compass", oled ? "" : " OLED");
}

static void testOledStart() {
  if (!hal::i2cProbe(OLED_I2C_ADDR)) return;
  g_oledDev = hal::addI2cDevice(OLED_I2C_ADDR);
  u8g2_Setup_ssd1306_i2c_128x64_noname_f(&g_oled, U8G2_R0, oledByteCb, oledDelayCb);
  u8g2_InitDisplay(&g_oled);
  u8g2_SetPowerSave(&g_oled, 0);
  g_oledOk = true;
}

static void testOled() {
  section(2, "oled", "OLED display");
  if (!g_oledOk) {
    result("oled", FAIL, "no SSD1306 at 0x%02X", OLED_I2C_ADDR);
    return;
  }
  logf("look at the screen: full white, border + text, then a checkerboard");
  u8g2_ClearBuffer(&g_oled);
  u8g2_DrawBox(&g_oled, 0, 0, 128, 64);
  u8g2_SendBuffer(&g_oled);
  delayMs(700);
  u8g2_ClearBuffer(&g_oled);
  u8g2_DrawFrame(&g_oled, 0, 0, 128, 64);
  u8g2_SetFont(&g_oled, u8g2_font_6x10_tr);
  u8g2_DrawStr(&g_oled, 18, 28, "ELXIE TESTBENCH");
  u8g2_DrawStr(&g_oled, 40, 44, "v" TB_VERSION);
  u8g2_SendBuffer(&g_oled);
  delayMs(900);
  u8g2_ClearBuffer(&g_oled);
  for (int y = 0; y < 64; y += 8)
    for (int x = 0; x < 128; x += 8)
      if (((x + y) / 8) % 2 == 0) u8g2_DrawBox(&g_oled, x, y, 8, 8);
  u8g2_SendBuffer(&g_oled);
  delayMs(700);
  result("oled", CHECK, "answers at 0x%02X - confirm white / text / checkerboard appeared", OLED_I2C_ADDR);
}

static void testImu() {
  section(3, "imu", "MPU6050 accelerometer + gyro");
  const uint8_t addr = g_imuAddr;
  if (!addr || !g_imuDev) {
    result("imu", FAIL, "no answer at 0x%02X or 0x%02X", IMU_I2C_ADDR, IMU_I2C_ADDR_ALT);
    return;
  }
  i2c_master_dev_handle_t dev = g_imuDev;
  uint8_t who = 0;
  {
    hal::I2cLock lock(50);
    hal::i2cWriteReg(dev, 0x6B, 0x00);  // wake
    hal::i2cWriteReg(dev, 0x1A, 0x03);
    hal::i2cWriteReg(dev, 0x1B, 0x08);  // +-500 dps
    hal::i2cWriteReg(dev, 0x1C, 0x00);  // +-2 g
    hal::i2cReadRegs(dev, 0x75, &who, 1, 20);
  }
  logf("address 0x%02X  WHO_AM_I=0x%02X (%s)", addr, who,
       who == 0x68 ? "MPU6050" : (who == 0x70 ? "MPU6500" : (who == 0x72 ? "MPU6050 clone" : "unexpected")));
  delayMs(100);
  logf("keep Elxie still for 1 s...");
  double a[3] = {0, 0, 0}, g[3] = {0, 0, 0}, t = 0;
  int n = 0;
  for (int i = 0; i < 50; ++i) {
    uint8_t b[14];
    bool ok;
    {
      hal::I2cLock lock(20);
      ok = lock.ok && hal::i2cReadRegs(dev, 0x3B, b, 14, 20);
    }
    if (ok) {
      int16_t r[7];
      for (int k = 0; k < 7; ++k) r[k] = (int16_t)((b[2 * k] << 8) | b[2 * k + 1]);
      for (int k = 0; k < 3; ++k) a[k] += r[k] / 16384.0;
      t += r[3] / 340.0 + 36.53;
      for (int k = 0; k < 3; ++k) g[k] += r[4 + k] / 65.5;
      ++n;
    }
    delayMs(20);
  }
  if (n < 40) {
    result("imu", FAIL, "only %d/50 reads succeeded", n);
    return;
  }
  for (int k = 0; k < 3; ++k) { a[k] /= n; g[k] /= n; }
  t /= n;
  const double mag = sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  const double gmax = fmax(fabs(g[0]), fmax(fabs(g[1]), fabs(g[2])));
  logf("accel x=%+.2f y=%+.2f z=%+.2f g  |a|=%.2f g", a[0], a[1], a[2], mag);
  logf("gyro  x=%+.1f y=%+.1f z=%+.1f deg/s   temp=%.1f C", g[0], g[1], g[2], t);
  if (mag < 0.8 || mag > 1.2)
    result("imu", FAIL, "|accel| = %.2f g (expected ~1.0 g at rest)", mag);
  else if (gmax > 15)
    result("imu", WARN, "gyro offset %.1f deg/s is large (was it moving?)", gmax);
  else
    result("imu", PASS, "|a|=%.2f g, gyro offset %.1f deg/s, %s", mag, gmax, a[2] > 0.5 ? "upright" : "NOT flat/upright");
}

static void testMag() {
  section(4, "compass", "QMC5883P compass");
  if (!g_magDev) {
    result("compass", FAIL, "no QMC5883P at 0x%02X%s", MAG_QMCP_ADDR,
           (hal::i2cProbe(MAG_QMC_ADDR) || hal::i2cProbe(MAG_HMC_ADDR)) ? " (a different compass chip answered)" : "");
    return;
  }
  i2c_master_dev_handle_t dev = g_magDev;
  uint8_t id = 0;
  {
    hal::I2cLock lock(50);
    hal::i2cReadRegs(dev, 0x00, &id, 1, 20);
    hal::i2cWriteReg(dev, 0x0B, 0x08);  // set/reset on, +-8 G
  }
  delayMs(5);
  {
    hal::I2cLock lock(50);
    hal::i2cWriteReg(dev, 0x0A, 0xCF);  // continuous, 200 Hz
  }
  logf("chip id=0x%02X (QMC5883P reads 0x80)", id);
  delayMs(50);
  int16_t first[3] = {0, 0, 0};
  bool varied = false;
  double sumMag = 0;
  int n = 0, sat = 0;
  for (int i = 0; i < 20; ++i) {
    uint8_t b[6];
    bool ok;
    {
      hal::I2cLock lock(20);
      ok = lock.ok && hal::i2cReadRegs(dev, 0x01, b, 6, 20);
    }
    if (ok) {
      int16_t v[3] = {(int16_t)(b[0] | (b[1] << 8)), (int16_t)(b[2] | (b[3] << 8)), (int16_t)(b[4] | (b[5] << 8))};
      if (n == 0) memcpy(first, v, sizeof first);
      else if (memcmp(first, v, sizeof first) != 0) varied = true;
      for (int k = 0; k < 3; ++k)
        if (v[k] == 32767 || v[k] == -32768) ++sat;
      sumMag += sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
      if (i % 5 == 0) logf("raw x=%6d y=%6d z=%6d", v[0], v[1], v[2]);
      ++n;
    }
    delayMs(25);
  }
  if (n < 15) {
    result("compass", FAIL, "only %d/20 reads succeeded", n);
    return;
  }
  const double avg = sumMag / n;
  logf("average field magnitude = %.0f counts (Earth ~ 700-2500 at +-8 G)", avg);
  if (avg < 50)
    result("compass", FAIL, "reads all zero: not measuring");
  else if (sat)
    result("compass", WARN, "saturated readings: magnet or motor too close?");
  else if (!varied)
    result("compass", WARN, "values never change (stuck?)");
  else if (avg < 200 || avg > 12000)
    result("compass", WARN, "field %.0f counts is unusual (magnet nearby?)", avg);
  else
    result("compass", PASS, "id 0x%02X, field %.0f counts", id, avg);
}

static void testIr() {
  section(5, "ir", "16 IR sensors through the CD74HC4067 mux");
  logf("mux S0..S3=GPIO%d,%d,%d,%d  SIG=GPIO%d", PIN_MUX_S0, PIN_MUX_S1, PIN_MUX_S2, PIN_MUX_S3, PIN_MUX_SIG);
  int lo[16], hi[16];
  long sum[16];
  for (int c = 0; c < 16; ++c) { lo[c] = 4095; hi[c] = 0; sum[c] = 0; }
  int samples = 0;
  screen("IR SENSORS", "Move your hand /", "paper under Elxie");
  logf(">>> For 6 s: move a hand or white paper under the line sensors, edges and corners");
  const uint32_t end = millis() + 6000;
  uint32_t nextPrint = 0;
  while ((int32_t)(millis() - end) < 0) {
    int v[16];
    readAllIr(v);
    for (int c = 0; c < 16; ++c) {
      lo[c] = min(lo[c], v[c]);
      hi[c] = max(hi[c], v[c]);
      sum[c] += v[c];
    }
    ++samples;
    if ((int32_t)(millis() - nextPrint) >= 0) {
      nextPrint = millis() + 1000;
      logf("line %4d %4d %4d %4d %4d %4d %4d %4d | edge FL%4d FR%4d BL%4d BR%4d | corner %4d %4d %4d %4d",
           v[kLineCh[0]], v[kLineCh[1]], v[kLineCh[2]], v[kLineCh[3]], v[kLineCh[4]], v[kLineCh[5]], v[kLineCh[6]],
           v[kLineCh[7]], v[kEdgeCh[0]], v[kEdgeCh[1]], v[kEdgeCh[2]], v[kEdgeCh[3]], v[kDepthCh[0]], v[kDepthCh[1]],
           v[kDepthCh[2]], v[kDepthCh[3]]);
    }
    delayMs(20);
  }
  logf("channel  role        min   avg   max  range");
  int stuck = 0, quiet = 0;
  char stuckList[64] = "", quietList[64] = "";
  for (int c = 0; c < 16; ++c) {
    char role[12] = "?";
    for (int i = 0; i < 8; ++i)
      if (kLineCh[i] == c) snprintf(role, sizeof role, "line %d", i + 1);
    static const char* kCorner[4] = {"FL", "FR", "BL", "BR"};
    for (int i = 0; i < 4; ++i) {
      if (kEdgeCh[i] == c) snprintf(role, sizeof role, "edge %s", kCorner[i]);
      if (kDepthCh[i] == c) snprintf(role, sizeof role, "corner %s", kCorner[i]);
    }
    const int range = hi[c] - lo[c];
    const char* flag = "";
    char tag[6];
    snprintf(tag, sizeof tag, "%d ", c);
    if (hi[c] < 15 || lo[c] > 4080) {
      flag = "  <- STUCK";
      ++stuck;
      strncat(stuckList, tag, sizeof stuckList - strlen(stuckList) - 1);
    } else if (range < 150) {
      flag = "  <- no change";
      ++quiet;
      strncat(quietList, tag, sizeof quietList - strlen(quietList) - 1);
    }
    logf("  CH%-2d   %-9s %5d %5ld %5d %5d%s", c, role, lo[c], sum[c] / max(samples, 1), hi[c], range, flag);
  }
  if (stuck >= 16)
    result("ir", FAIL, "all channels stuck: check mux power / SIG pin / select pins");
  else if (stuck)
    result("ir", FAIL, "%d channel(s) stuck at 0 or 4095: CH %s", stuck, stuckList);
  else if (quiet)
    result("ir", WARN, "%d channel(s) did not react: CH %s", quiet, quietList);
  else
    result("ir", PASS, "all 16 channels read and react");
}

static void testUltrasonic() {
  section(6, "ultrasonic", "HC-SR04 ultrasonic distance");
  logf("TRIG=GPIO%d  ECHO=GPIO%d", PIN_US_TRIG, PIN_US_ECHO);
  screen("ULTRASONIC", "Hold a hand", "20 cm in front");
  logf(">>> hold a hand or a book about 20 cm in front of Elxie");
  delayMs(1500);
  int echoes = 0, valid = 0;
  float dmin = 1e9f, dmax = 0;
  for (int i = 0; i < 10; ++i) {
    bool got;
    {
      SenseLock lock;
      g_echoNew = false;
      hal::pinWrite(PIN_US_TRIG, 1);
      esp_rom_delay_us(10);
      hal::pinWrite(PIN_US_TRIG, 0);
      delayMs(60);
      got = g_echoNew;
    }
    if (got) {
      ++echoes;
      const float cm = g_echoWidth / 58.0f;
      if (cm >= 2 && cm <= US_MAX_CM) {
        ++valid;
        dmin = fminf(dmin, cm);
        dmax = fmaxf(dmax, cm);
      }
      logf("  ping %2d: echo %5lu us = %6.1f cm", i + 1, (unsigned long)g_echoWidth, cm);
    } else {
      logf("  ping %2d: no echo pulse", i + 1);
    }
  }
  if (echoes == 0)
    result("ultrasonic", FAIL, "no echo pulses: check 5 V, TRIG/ECHO wiring and the ECHO divider");
  else if (valid < 7)
    result("ultrasonic", WARN, "%d/10 valid readings (%d echoes)", valid, echoes);
  else
    result("ultrasonic", PASS, "%d/10 valid, %.1f..%.1f cm", valid, dmin, dmax);
}

static void testTouch() {
  section(7, "touch", "TTP223 touch sensor");
  logf("TOUCH=GPIO%d  active %s", PIN_TOUCH, TOUCH_ACTIVE_HIGH ? "HIGH" : "LOW");
  if (touchActive()) {
    logf("touch reads ACTIVE before anything touched it, waiting 2 s to see if it releases");
    delayMs(2000);
    if (touchActive()) {
      result("touch", FAIL, "stuck active (wrong polarity in TOUCH_ACTIVE_HIGH, or shorted)");
      return;
    }
  }
  screen("TOUCH", "Touch the pad", "within 10 s");
  logf(">>> touch the touch pad within 10 s");
  const uint32_t end = millis() + 10000;
  while ((int32_t)(millis() - end) < 0) {
    if (touchActive()) {
      const uint32_t t0 = millis();
      while (touchActive() && millis() - t0 < 3000) delayMs(5);
      logf("touched, held %lu ms", (unsigned long)(millis() - t0));
      result("touch", PASS, "press detected");
      return;
    }
    delayMs(5);
  }
  result("touch", FAIL, "no touch seen in 10 s");
}

static void testBattery() {
  section(8, "battery", "Battery voltage");
  const int raw = hal::adcRead(PIN_BATTERY);
  const float v = batteryVolts();
  float pct = (v - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
  pct = constrain(pct, 0.0f, 100.0f);
  logf("BATTERY=GPIO%d raw=%d  V=%.2f (divider x%.1f)  ~%.0f %%", PIN_BATTERY, raw, v, BATT_DIVIDER, pct);
  if (v < 0.5f)
    result("battery", FAIL, "reads ~0 V: battery off/disconnected, or divider not on GPIO%d", PIN_BATTERY);
  else if (v < 3.0f)
    result("battery", WARN, "%.2f V is very low (or BATT_DIVIDER is wrong)", v);
  else if (v > 4.4f)
    result("battery", WARN, "%.2f V is above a full Li-ion cell: check BATT_DIVIDER", v);
  else
    result("battery", PASS, "%.2f V (~%.0f %%)", v, pct);
}

static void testBuzzer() {
  section(9, "buzzer", "Buzzer");
  logf("BUZZER=GPIO%d: playing C-D-E-F-G-A-B-C, then a 2 kHz beep", PIN_BUZZER);
  screen("BUZZER", "Listen for a", "rising scale");
  static const uint16_t kScale[] = {523, 587, 659, 698, 784, 880, 988, 1047};
  for (uint16_t hz : kScale) tone(hz, 160);
  delayMs(150);
  tone(2000, 300);
  result("buzzer", CHECK, "confirm you heard 8 rising notes and a beep");
}

static void testLights() {
  section(10, "lights", "NeoPixels");
  logf("NEOPIXEL=GPIO%d count=%d (dimmed for the test)", PIN_NEOPIXEL, NEOPIXEL_COUNT);
  if (!g_strip) {
    result("lights", FAIL, "LED strip driver failed to start");
    return;
  }
  screen("LIGHTS", "Red Green Blue", "White, then 1..30");
  static const struct { const char* name; uint8_t r, g, b; } kCols[] = {
      {"red", 255, 0, 0}, {"green", 0, 255, 0}, {"blue", 0, 0, 255}, {"white", 255, 255, 255}};
  for (auto& c : kCols) {
    logf("  all %s", c.name);
    stripAll(c.r, c.g, c.b);
    delayMs(700);
  }
  stripAll(0, 0, 0);
  logf("  one by one: LED 1 .. %d (count them)", NEOPIXEL_COUNT);
  for (int i = 0; i < NEOPIXEL_COUNT; ++i) {
    led_strip_clear(g_strip);
    led_strip_set_pixel(g_strip, i, dim(0), dim(180), dim(255));
    led_strip_refresh(g_strip);
    delayMs(90);
  }
  led_strip_clear(g_strip);
  result("lights", CHECK, "confirm red/green/blue/white on all %d and a dot walking 1..%d", NEOPIXEL_COUNT, NEOPIXEL_COUNT);
}

static void testMotors() {
  section(11, "motors", "Motors (TB6612FNG)");
  logf("STBY=GPIO%d  LEFT (motor B): PWM=GPIO%d IN1=GPIO%d IN2=GPIO%d  RIGHT (motor A): PWM=GPIO%d IN1=GPIO%d IN2=GPIO%d", PIN_MOTOR_STBY,
       PIN_MOTOR_L_PWM, PIN_MOTOR_L_IN1, PIN_MOTOR_L_IN2, PIN_MOTOR_R_PWM, PIN_MOTOR_R_IN1, PIN_MOTOR_R_IN2);
  logf("invert: left=%d right=%d", MOTOR_L_INVERT, MOTOR_R_INVERT);
  screen("MOTORS", "LIFT THE WHEELS", "starting in 3 s");
  logf(">>> LIFT THE WHEELS off the table. Starting in 3 s");
  delayMs(3000);
  struct Step { const char* what; float l, r; };
  static const Step kSteps[] = {{"LEFT  forward ", 50, 0}, {"LEFT  backward", -50, 0},
                                {"RIGHT forward ", 0, 50}, {"RIGHT backward", 0, -50}, {"BOTH  forward ", 50, 50}};
  float vIdle = batteryVolts();
  int sagging = 0;
  for (auto& s : kSteps) {
    screen("MOTORS", s.what, "");
    motors::write(s.l, s.r);
    delayMs(400);
    const float vRun = batteryVolts();
    const int l1 = hal::pinRead(PIN_MOTOR_L_IN1), l2 = hal::pinRead(PIN_MOTOR_L_IN2);
    const int r1 = hal::pinRead(PIN_MOTOR_R_IN1), r2 = hal::pinRead(PIN_MOTOR_R_IN2);
    delayMs(600);
    motors::write(0, 0);
    const float drop = vIdle - vRun;
    if (drop > 0.04f) ++sagging;
    logf("  %s  L.IN1=%d L.IN2=%d R.IN1=%d R.IN2=%d STBY=%d  battery %.2f -> %.2f V (%s)", s.what, l1, l2, r1, r2,
         hal::pinRead(PIN_MOTOR_STBY), vIdle, vRun, drop > 0.04f ? "load seen" : "no load seen");
    delayMs(500);
  }
  motors::brake();
  delayMs(100);
  motors::write(0, 0);
  result("motors", CHECK, "confirm each wheel spun fwd then back, then both (battery dip in %d/5 steps)", sagging);
}

static void testRemote() {
  section(12, "remote", "Remote (ESP-NOW)");
  char mac[20];
  snprintf(mac, sizeof mac, "%02X:%02X:%02X:%02X:%02X:%02X", g_remoteMac[0], g_remoteMac[1], g_remoteMac[2],
           g_remoteMac[3], g_remoteMac[4], g_remoteMac[5]);
  logf("%s remote MAC %s, channel %d", g_remotePaired ? "paired" : "NOT PAIRED, default", mac, ESPNOW_CHANNEL);
  if (!g_remotePaired) logf(">>> pair this robot first (PAIR_ROBOT_AND_REMOTE.bat): an unpaired remote sends nothing");
  if (!g_radioOk) {
    result("remote", FAIL, "Wi-Fi / ESP-NOW failed to start");
    return;
  }
  const int total0 = g_rxCount, ours0 = g_rxOurs;
  uint8_t own[6] = {0};
  esp_wifi_get_mac(WIFI_IF_STA, own);
  logf("this robot's MAC %02X:%02X:%02X:%02X:%02X:%02X", own[0], own[1], own[2], own[3], own[4], own[5]);
  screen("REMOTE", "Turn remote on,", "move stick, press");
  logf(">>> turn the remote on; move the joystick and press A, B and the stick (10 s)");
  const uint32_t end = millis() + 10000;
  int lastX = 999, lastY = 999, lastB = -1;
  while ((int32_t)(millis() - end) < 0) {
    uint8_t d[8];
    int len;
    portENTER_CRITICAL(&g_rmux);
    len = g_rxLen;
    for (int i = 0; i < len; ++i) d[i] = g_rxData[i];
    portEXIT_CRITICAL(&g_rmux);
    int x, y, btn;
    if (decodeRemote(d, len, &x, &y, &btn)) {
      if (abs(x - lastX) > 10 || abs(y - lastY) > 10 || btn != lastB) {
        logf("  joystick x=%+4d y=%+4d  A=%d B=%d stick=%d", x, y, (btn >> REMOTE_BTN_A) & 1, (btn >> REMOTE_BTN_B) & 1,
             (btn >> REMOTE_BTN_STICK) & 1);
        lastX = x; lastY = y; lastB = btn;
      }
    }
    delayMs(50);
  }
  const int total = g_rxCount - total0, ours = g_rxOurs - ours0;
  logf("packets received: %d (from the paired remote: %d)", total, ours);
  if (ours > 0)
    result("remote", PASS, "%d packets from the paired remote", ours);
  else if (total > 0)
    result("remote", WARN, "packets only from %02X:%02X:%02X:%02X:%02X:%02X, not the paired %s", g_rxMac[0], g_rxMac[1],
           g_rxMac[2], g_rxMac[3], g_rxMac[4], g_rxMac[5], mac);
  else
    result("remote", WARN, "nothing received (remote off, or on another channel)");
}

static void radioInit() {
  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&cfg) != ESP_OK) return;
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(onRecv);
  g_radioOk = true;
}

// ============================================================== main
static void setupHardware() {
  hal::initI2c();
  // IR mux
  hal::pinOutput(PIN_MUX_S0);
  hal::pinOutput(PIN_MUX_S1);
  hal::pinOutput(PIN_MUX_S2);
  hal::pinOutput(PIN_MUX_S3);
  hal::adcAttach(PIN_MUX_SIG);
  hal::adcAttach(PIN_BATTERY);
  // touch
  hal::pinInput(PIN_TOUCH, !TOUCH_ACTIVE_HIGH, TOUCH_ACTIVE_HIGH);
  // ultrasonic
  hal::pinOutput(PIN_US_TRIG, 0);
  gpio_config_t echo = {};
  echo.pin_bit_mask = 1ULL << PIN_US_ECHO;
  echo.mode = GPIO_MODE_INPUT;
  echo.intr_type = GPIO_INTR_ANYEDGE;
  gpio_config(&echo);
  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)PIN_US_ECHO, echoIsr, nullptr);
  // buzzer
  hal::pwmTimer(LEDC_TIMER_BUZZER, 1000, BUZZER_PWM_BITS);
  hal::pwmAttach(PIN_BUZZER, LEDC_CH_BUZZER, LEDC_TIMER_BUZZER);
  // motors (also releases the native-USB pads on GPIO19/20)
  motors::begin();
  // keep driving the motor pins but also let us read them back for the log
  for (int p : {PIN_MOTOR_STBY, PIN_MOTOR_L_IN1, PIN_MOTOR_L_IN2, PIN_MOTOR_R_IN1, PIN_MOTOR_R_IN2})
    gpio_set_direction((gpio_num_t)p, GPIO_MODE_INPUT_OUTPUT);
  // NeoPixels
  led_strip_config_t sc = {};
  sc.strip_gpio_num = PIN_NEOPIXEL;
  sc.max_leds = NEOPIXEL_COUNT;
  sc.led_model = LED_MODEL_WS2812;
  sc.color_component_format.format.r_pos = 1;  // WS2812 wire order is G, R, B (as in lights.cpp)
  sc.color_component_format.format.g_pos = 0;
  sc.color_component_format.format.b_pos = 2;
  sc.color_component_format.format.num_components = 3;
  sc.color_component_format.format.bytes_per_color = 1;
  led_strip_rmt_config_t rc = {};
  rc.clk_src = RMT_CLK_SRC_DEFAULT;
  rc.resolution_hz = 10 * 1000 * 1000;
  if (led_strip_new_rmt_device(&sc, &rc, &g_strip) != ESP_OK) g_strip = nullptr;
  if (g_strip) led_strip_clear(g_strip);
}

// ============================================================== test table / runs
struct TestDef {
  const char* id;
  void (*fn)();
};
static const TestDef kTests[] = {
    {"i2c", testI2c},        {"oled", testOled},         {"imu", testImu},     {"compass", testMag},
    {"ir", testIr},          {"ultrasonic", testUltrasonic}, {"touch", testTouch}, {"battery", testBattery},
    {"buzzer", testBuzzer},  {"lights", testLights},     {"motors", testMotors}, {"remote", testRemote},
};
static const int kNumTests = sizeof kTests / sizeof kTests[0];

static void runAll() {
  g_running = true;
  g_nResults = 0;
  const uint32_t t0 = millis();
  logf("");
  logf("===============================================================");
  logf("ELXIE TESTBENCH v%s   (pins from firmware/main/board_config.h)", TB_VERSION);
  logf("===============================================================");
  logf("TESTBENCH_START");
  emitf("{\"t\":\"start\",\"n\":%d}", kNumTests);
  screen("Starting...", "Watch the PC");

  for (const TestDef& t : kTests) t.fn();
  motors::write(0, 0);

  int cnt[4] = {0, 0, 0, 0};
  for (int i = 0; i < g_nResults; ++i) cnt[g_results[i].st]++;
  logf("");
  logf("===============================================================");
  logf("SUMMARY  (%lu s)", (unsigned long)((millis() - t0) / 1000));
  logf("===============================================================");
  for (int i = 0; i < g_nResults; ++i)
    logf("  %-5s %-10s %s", kStatusName[g_results[i].st], g_results[i].name, g_results[i].detail);
  logf("---------------------------------------------------------------");
  logf("PASS %d   FAIL %d   WARN %d   CHECK %d  (CHECK = confirm what you saw/heard)", cnt[PASS], cnt[FAIL], cnt[WARN],
       cnt[CHECK]);
  logf("TESTBENCH_DONE pass=%d fail=%d warn=%d check=%d", cnt[PASS], cnt[FAIL], cnt[WARN], cnt[CHECK]);
  emitf("{\"t\":\"done\",\"pass\":%d,\"fail\":%d,\"warn\":%d,\"check\":%d,\"secs\":%lu}", cnt[PASS], cnt[FAIL],
        cnt[WARN], cnt[CHECK], (unsigned long)((millis() - t0) / 1000));
  char l2[24], l3[24];
  snprintf(l2, sizeof l2, "PASS %d  FAIL %d", cnt[PASS], cnt[FAIL]);
  snprintf(l3, sizeof l3, "WARN %d  CHECK %d", cnt[WARN], cnt[CHECK]);
  screen(cnt[FAIL] ? "DONE - SEE FAILS" : "DONE", l2, l3);
  if (g_strip) {
    stripAll(cnt[FAIL] ? 255 : 0, cnt[FAIL] ? 0 : 255, 0);
    delayMs(1500);
    led_strip_clear(g_strip);
  }
  tone(cnt[FAIL] ? 300 : 1500, 250);
  g_running = false;
}

static bool runOne(const char* id) {
  for (const TestDef& t : kTests) {
    if (strcmp(t.id, id) == 0) {
      g_running = true;
      g_nResults = 0;
      t.fn();
      motors::write(0, 0);
      emitf("{\"t\":\"end\",\"id\":\"%s\"}", id);
      screen("Ready", "Run a test from", "the dashboard");
      g_running = false;
      return true;
    }
  }
  return false;
}

// ============================================================== live stream (10 Hz)
static volatile float g_manualL = 0, g_manualR = 0;
static volatile uint32_t g_manualMotorUntil = 0;  // manual motor command auto-stop time (0 = none)
static volatile float g_lastCm = -1;               // last ultrasonic distance seen by the live task

static void liveTask(void*) {
  uint32_t lastPing = 0;
  for (;;) {
    delayMs(100);
    // manual motor commands always stop on their own
    if (g_manualMotorUntil && (int32_t)(millis() - g_manualMotorUntil) >= 0) {
      g_manualMotorUntil = 0;
      motors::write(0, 0);
      emitf("{\"t\":\"ack\",\"m\":\"motor stopped\"}");
    }
    if (!g_live) continue;
    int ir[16];
    readAllIr(ir);
    // IMU
    float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
    bool imuOk = false;
    if (g_imuDev) {
      uint8_t b[14];
      hal::I2cLock lock(10);
      if (lock.ok && hal::i2cReadRegs(g_imuDev, 0x3B, b, 14, 10)) {
        int16_t r[7];
        for (int k = 0; k < 7; ++k) r[k] = (int16_t)((b[2 * k] << 8) | b[2 * k + 1]);
        ax = r[0] / 16384.0f; ay = r[1] / 16384.0f; az = r[2] / 16384.0f;
        gx = r[4] / 65.5f; gy = r[5] / 65.5f; gz = r[6] / 65.5f;
        imuOk = true;
      }
    }
    const float pitch = imuOk ? atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.2958f : 0;
    const float roll = imuOk ? atan2f(ay, az) * 57.2958f : 0;
    // compass
    int16_t m[3] = {0, 0, 0};
    bool magOk = false;
    if (g_magDev) {
      uint8_t b[6];
      hal::I2cLock lock(10);
      if (lock.ok && hal::i2cReadRegs(g_magDev, 0x01, b, 6, 10)) {
        for (int k = 0; k < 3; ++k) m[k] = (int16_t)(b[2 * k] | (b[2 * k + 1] << 8));
        magOk = true;
      }
    }
    const float field = sqrtf((float)m[0] * m[0] + (float)m[1] * m[1] + (float)m[2] * m[2]);
    float heading = atan2f((float)m[1], (float)m[0]) * 57.2958f;  // raw, uncalibrated
    if (heading < 0) heading += 360;
    // ultrasonic: one ping every 200 ms when no test is using it
    if (millis() - lastPing >= 200) {
      SenseLock lock(5);
      if (lock.ok) {
        lastPing = millis();
        g_echoNew = false;
        hal::pinWrite(PIN_US_TRIG, 1);
        esp_rom_delay_us(10);
        hal::pinWrite(PIN_US_TRIG, 0);
        delayMs(30);  // echoes up to ~5 m
        g_lastCm = g_echoNew ? g_echoWidth / 58.0f : -1;
      }
    }
    const float bv = batteryVolts();
    float bp = (bv - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
    bp = constrain(bp, 0.0f, 100.0f);
    // remote
    int jx = 0, jy = 0, btn = 0, rlen;
    uint8_t d[8];
    portENTER_CRITICAL(&g_rmux);
    rlen = g_rxLen;
    for (int i = 0; i < rlen; ++i) d[i] = g_rxData[i];
    const int rxCount = g_rxCount, rxOurs = g_rxOurs;
    portEXIT_CRITICAL(&g_rmux);
    if (!decodeRemote(d, rlen, &jx, &jy, &btn)) jx = jy = btn = 0;
    emitf("{\"t\":\"live\",\"ms\":%lu,\"run\":%d,"
          "\"ir\":[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d],"
          "\"imu\":%d,\"a\":[%.2f,%.2f,%.2f],\"g\":[%.1f,%.1f,%.1f],\"pitch\":%.1f,\"roll\":%.1f,"
          "\"mag\":%d,\"m\":[%d,%d,%d],\"field\":%.0f,\"hd\":%.0f,"
          "\"cm\":%.1f,\"touch\":%d,\"bv\":%.2f,\"bp\":%.0f,"
          "\"rx\":%d,\"rxo\":%d,\"jx\":%d,\"jy\":%d,\"btn\":%d,"
          "\"ml\":%.0f,\"mr\":%.0f}",
          (unsigned long)millis(), g_running ? 1 : 0, ir[0], ir[1], ir[2], ir[3], ir[4], ir[5], ir[6], ir[7], ir[8],
          ir[9], ir[10], ir[11], ir[12], ir[13], ir[14], ir[15], imuOk ? 1 : 0, ax, ay, az, gx, gy, gz, pitch, roll,
          magOk ? 1 : 0, m[0], m[1], m[2], field, heading, (float)g_lastCm, touchActive() ? 1 : 0, bv, bp, rxCount,
          rxOurs, jx, jy, btn, g_manualL, g_manualR);
  }
}

// ============================================================== commands from the dashboard
static char g_pendingRun[16] = "";  // "all" or a test id, picked up by the main loop
static portMUX_TYPE g_cmdMux = portMUX_INITIALIZER_UNLOCKED;

static void handleCommand(char* line) {
  char* argv[6];
  int argc = 0;
  for (char* tok = strtok(line, " \t"); tok && argc < 6; tok = strtok(nullptr, " \t")) argv[argc++] = tok;
  if (argc == 0) return;
  const char* cmd = argv[0];
  if (strcmp(cmd, "run") == 0) {
    const char* what = argc > 1 ? argv[1] : "all";
    if (g_running) { emitf("{\"t\":\"err\",\"m\":\"busy: a test is running\"}"); return; }
    bool known = strcmp(what, "all") == 0;
    for (const TestDef& t : kTests) known = known || strcmp(t.id, what) == 0;
    if (!known) { emitf("{\"t\":\"err\",\"m\":\"unknown test\"}"); return; }
    portENTER_CRITICAL(&g_cmdMux);
    snprintf(g_pendingRun, sizeof g_pendingRun, "%s", what);
    portEXIT_CRITICAL(&g_cmdMux);
    emitf("{\"t\":\"ack\",\"m\":\"run %s\"}", what);
  } else if (strcmp(cmd, "live") == 0 && argc > 1) {
    g_live = atoi(argv[1]) != 0;
    emitf("{\"t\":\"ack\",\"m\":\"live %d\"}", g_live ? 1 : 0);
  } else if (strcmp(cmd, "stop") == 0) {
    g_manualMotorUntil = 0;
    g_manualL = 0; g_manualR = 0;
    motors::write(0, 0);
    hal::pwmWrite(LEDC_CH_BUZZER, 0);
    if (g_strip) led_strip_clear(g_strip);
    emitf("{\"t\":\"ack\",\"m\":\"stopped\"}");
  } else if (g_running) {
    emitf("{\"t\":\"err\",\"m\":\"busy: wait for the test to finish\"}");
  } else if (strcmp(cmd, "motor") == 0 && argc >= 4) {
    const float l = constrain((float)atof(argv[1]), -100.0f, 100.0f);
    const float r = constrain((float)atof(argv[2]), -100.0f, 100.0f);
    const uint32_t ms = (uint32_t)constrain(atoi(argv[3]), 0, 3000);  // never more than 3 s per command
    g_manualL = l;
    g_manualR = r;
    motors::write(l, r);
    g_manualMotorUntil = ms ? millis() + ms : 0;
    if (!ms) { motors::write(0, 0); g_manualL = 0; g_manualR = 0; }
    emitf("{\"t\":\"ack\",\"m\":\"motor %.0f %.0f for %lu ms\"}", l, r, (unsigned long)ms);
  } else if (strcmp(cmd, "led") == 0 && argc >= 4) {
    stripAll((uint8_t)constrain(atoi(argv[1]), 0, 255), (uint8_t)constrain(atoi(argv[2]), 0, 255),
             (uint8_t)constrain(atoi(argv[3]), 0, 255));
    emitf("{\"t\":\"ack\",\"m\":\"lights set\"}");
  } else if (strcmp(cmd, "buzz") == 0 && argc >= 3) {
    tone((uint32_t)constrain(atoi(argv[1]), 50, 8000), (uint32_t)constrain(atoi(argv[2]), 10, 2000));
    emitf("{\"t\":\"ack\",\"m\":\"beep\"}");
  } else {
    emitf("{\"t\":\"err\",\"m\":\"unknown command\"}");
  }
}

static void commandTask(void*) {
  char line[64];
  size_t n = 0;
  for (;;) {
    uint8_t c;
    if (uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(100)) != 1) continue;
    if (c == '\r' || c == '\n') {
      if (n) {
        line[n] = 0;
        handleCommand(line);
        n = 0;
      }
    } else if (n + 1 < sizeof line) {
      line[n++] = (char)c;
    }
  }
}

static void hello() {
  char ids[160] = "";
  for (int i = 0; i < kNumTests; ++i) {
    char one[20];
    snprintf(one, sizeof one, "%s\"%s\"", i ? "," : "", kTests[i].id);
    strncat(ids, one, sizeof ids - strlen(ids) - 1);
  }
  emitf("{\"t\":\"hello\",\"v\":\"%s\",\"leds\":%d,\"tests\":[%s]}", TB_VERSION, NEOPIXEL_COUNT, ids);
}

extern "C" void app_main(void) {
  esp_err_t err = nvs_flash_init();  // needed by Wi-Fi; never erases the main firmware's data
  if (err != ESP_OK) printf("nvs_flash_init: %s (remote test may fail)\n", esp_err_to_name(err));
  else loadPairedRemote();
  // UART0 driver: lets the dashboard send commands; printf keeps working through it
  uart_driver_install(UART_NUM_0, 1024, 4096, 0, nullptr, 0);
  uart_vfs_dev_use_driver(UART_NUM_0);
  g_sense = xSemaphoreCreateMutex();
  setupHardware();
  testOledStart();
  sensorsInit();
  radioInit();
  xTaskCreatePinnedToCore(liveTask, "live", 6144, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(commandTask, "cmd", 4096, nullptr, 4, nullptr, 1);
  delayMs(1500);  // give the PC log window / dashboard time to attach after the reset
  hello();
  runAll();
  logf("Hold the touch pad for 2 s, or press Run in the dashboard, to run the tests again.");
  uint32_t lastHello = millis();
  for (;;) {
    char what[16];
    portENTER_CRITICAL(&g_cmdMux);
    snprintf(what, sizeof what, "%s", g_pendingRun);
    g_pendingRun[0] = 0;
    portEXIT_CRITICAL(&g_cmdMux);
    if (what[0]) {
      if (strcmp(what, "all") == 0) runAll();
      else runOne(what);
    }
    uint32_t held = 0;
    while (!g_running && touchActive() && held < 2000) {
      delayMs(20);
      held += 20;
    }
    if (held >= 2000) {
      while (touchActive()) delayMs(20);
      runAll();
    }
    if (millis() - lastHello > 5000) {  // lets a dashboard that connects later learn the test list
      lastHello = millis();
      hello();
    }
    delayMs(50);
  }
}
