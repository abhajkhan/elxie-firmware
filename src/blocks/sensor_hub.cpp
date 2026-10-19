#include "sensor_hub.h"

#include <math.h>

#include "blocks_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "platform.h"

static const char* TAG = "sensors";
static const uint8_t kLineCh[8] = IR_LINE_CHANNELS;
static const uint8_t kEdgeCh[4] = IR_EDGE_CHANNELS;
static const uint8_t kDepthCh[4] = IR_DEPTH_CHANNELS;

SensorHub sensors;

static volatile uint32_t g_echoRise = 0;
static volatile uint32_t g_echoWidth = 0;
static volatile bool g_echoNew = false;

void IRAM_ATTR SensorHub::echoIsr(void*) {
  uint32_t now = (uint32_t)esp_timer_get_time();
  if (gpio_get_level((gpio_num_t)PIN_US_ECHO)) {
    g_echoRise = now;
  } else {
    g_echoWidth = now - g_echoRise;
    g_echoNew = true;
  }
}

void SensorHub::begin() {
  hal::pinOutput(PIN_MUX_S0);
  hal::pinOutput(PIN_MUX_S1);
  hal::pinOutput(PIN_MUX_S2);
  hal::pinOutput(PIN_MUX_S3);
  if (PIN_IR_EMITTER >= 0) hal::pinOutput(PIN_IR_EMITTER, 1);
  hal::adcAttach(PIN_MUX_SIG);
  if (PIN_MIC >= 0) hal::adcAttach(PIN_MIC);
  if (PIN_BATTERY >= 0) hal::adcAttach(PIN_BATTERY);
  for (int i = 0; i < 16; ++i) {
    cal_.white[i] = IR_RAW_WHITE;
    cal_.black[i] = IR_RAW_BLACK;
  }

  hal::pinOutput(PIN_US_TRIG, 0);
  gpio_config_t echo = {};
  echo.pin_bit_mask = 1ULL << PIN_US_ECHO;
  echo.mode = GPIO_MODE_INPUT;
  echo.intr_type = GPIO_INTR_ANYEDGE;
  gpio_config(&echo);
  gpio_install_isr_service(0);  // harmless if already installed
  gpio_isr_handler_add((gpio_num_t)PIN_US_ECHO, echoIsr, nullptr);
  s_.distanceCm = US_MAX_CM;
  s_.accelZ = 1.0f;

  uint8_t imuAddr = hal::i2cProbe(IMU_I2C_ADDR) ? IMU_I2C_ADDR : (hal::i2cProbe(IMU_I2C_ADDR_ALT) ? IMU_I2C_ADDR_ALT : 0);
  s_.imuOk = imuAddr != 0;
  if (s_.imuOk) {
    imuDev_ = hal::addI2cDevice(imuAddr);
    BLK_LOG(TAG, "IMU at 0x%02x", imuAddr);
    hal::I2cLock lock(50);
    hal::i2cWriteReg(imuDev_, 0x6B, 0x00);  // wake, internal clock
    hal::i2cWriteReg(imuDev_, 0x1A, 0x03);  // DLPF ~44 Hz
    hal::i2cWriteReg(imuDev_, 0x1B, 0x08);  // gyro +-500 dps  (65.5 LSB per deg/s)
    hal::i2cWriteReg(imuDev_, 0x1C, 0x00);  // accel +-2 g     (16384 LSB per g)
  } else {
    BLK_LOG(TAG, "IMU not found at 0x%02x: turns fall back to timing", IMU_I2C_ADDR);
  }

  hal::pinInput(PIN_TOUCH, !TOUCH_ACTIVE_HIGH, TOUCH_ACTIVE_HIGH);
  initMag();
}

static bool readImuRaw(hal::I2cDev dev, int16_t out[7]) {
  hal::I2cLock lock(2);  // OLED busy? skip this sample rather than stall the loop
  if (!lock.ok) return false;
  uint8_t b[14];
  if (!hal::i2cReadRegs(dev, 0x3B, b, 14)) return false;
  for (int i = 0; i < 7; ++i) out[i] = (int16_t)((b[2 * i] << 8) | b[2 * i + 1]);
  return true;
}

void SensorHub::calibrateGyro(uint16_t ms) {
  if (!s_.imuOk) return;
  double sum[3] = {0, 0, 0};
  int n = 0;
  uint32_t end = millis() + ms;
  while ((int32_t)(millis() - end) < 0) {
    int16_t r[7];
    if (readImuRaw(imuDev_, r)) {
      for (int k = 0; k < 3; ++k) sum[k] += r[4 + k] / 65.5;
      n++;
    }
    delayMs(4);
  }
  if (n > 0)
    for (int k = 0; k < 3; ++k) gyroBias_[k] = (float)(sum[k] / n);
  s_.yaw = 0;
}

uint8_t SensorHub::normalize(uint8_t ch, uint16_t raw) const {
  int32_t w = cal_.white[ch], b = cal_.black[ch];
  if (!IR_DARK_IS_HIGH) {  // flip so "dark / far" is always the high end
    raw = 4095 - raw;
    w = 4095 - w;
    b = 4095 - b;
  }
  if (b - w < 50) return raw > (w + b) / 2 ? 100 : 0;
  int32_t v = (int32_t)(raw - w) * 100 / (b - w);
  return (uint8_t)constrain(v, 0, 100);
}

void SensorHub::readIr() {
  for (uint8_t ch = 0; ch < 16; ++ch) {
    hal::pinWrite(PIN_MUX_S0, ch & 1);
    hal::pinWrite(PIN_MUX_S1, (ch >> 1) & 1);
    hal::pinWrite(PIN_MUX_S2, (ch >> 2) & 1);
    hal::pinWrite(PIN_MUX_S3, (ch >> 3) & 1);
    esp_rom_delay_us(MUX_SETTLE_US);
    s_.irRaw[ch] = (uint16_t)hal::adcRead(PIN_MUX_SIG);
    if (calibrating_) {
      calScan_.white[ch] = min(calScan_.white[ch], s_.irRaw[ch]);
      calScan_.black[ch] = max(calScan_.black[ch], s_.irRaw[ch]);
    }
  }
  float sum = 0, weighted = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t dark = normalize(kLineCh[i], s_.irRaw[kLineCh[i]]);
    s_.line[i] = LINE_IS_DARK ? dark : (uint8_t)(100 - dark);
    if (s_.line[i] > 20) {
      float x = -100.0f + i * (200.0f / 7.0f);
      sum += s_.line[i];
      weighted += s_.line[i] * x;
    }
  }
  s_.lineSeen = sum > 0;
  if (s_.lineSeen) {
    s_.linePosition = weighted / sum;
    lastLinePos_ = s_.linePosition;
  } else {
    s_.linePosition = lastLinePos_ < 0 ? -100.0f : (lastLinePos_ > 0 ? 100.0f : 0.0f);
  }
  for (uint8_t c = 0; c < 4; ++c) {
    s_.edge[c] = normalize(kEdgeCh[c], s_.irRaw[kEdgeCh[c]]);
    s_.depth[c] = normalize(kDepthCh[c], s_.irRaw[kDepthCh[c]]);
    const uint16_t raw = s_.irRaw[kDepthCh[c]];
    s_.cornerObject[c] = CORNER_ACTIVE_HIGH ? raw >= CORNER_RAW_THRESHOLD : raw <= (4095 - CORNER_RAW_THRESHOLD);
  }
}

void SensorHub::readImu(uint32_t dtMs) {
  if (!s_.imuOk) return;
  int16_t r[7];
  if (!readImuRaw(imuDev_, r)) return;
  uint32_t nowUs = micros();
  float dt = lastImuUs_ ? (nowUs - lastImuUs_) / 1e6f : 0.0f;
  lastImuUs_ = nowUs;
  if (dt <= 0 || dt > 0.2f) dt = dtMs / 1000.0f;

  float ax = r[0] / 16384.0f, ay = r[1] / 16384.0f, az = r[2] / 16384.0f;
  s_.accelZ += 0.3f * (az - s_.accelZ);
  s_.accelX += 0.3f * (ax - s_.accelX);
  s_.accelY += 0.3f * (ay - s_.accelY);
#if IMU_SWAP_PITCH_ROLL
  { float t = ax; ax = ay; ay = t; }  // sensor mounted turned 90 degrees
#endif
  float gz = r[6] / 65.5f - gyroBias_[2];
  s_.gyroZ = gz * IMU_YAW_SIGN;
  if (fabsf(s_.gyroZ) > 0.5f) s_.yaw += s_.gyroZ * dt;  // small dead band kills drift at rest

  float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.2958f * IMU_PITCH_SIGN;
  float roll = atan2f(ay, az) * 57.2958f * IMU_ROLL_SIGN;
  s_.pitch += 0.25f * (pitch - s_.pitch);
  s_.roll += 0.25f * (roll - s_.roll);

  float mag = sqrtf(ax * ax + ay * ay + az * az);
  if (fabsf(mag - 1.0f) > SHAKE_G) shakeUntil_ = millis() + SHAKE_HOLD_MS;
  s_.shaken = (int32_t)(shakeUntil_ - millis()) > 0;
}

void SensorHub::readTouch(uint32_t dtMs) {
  // TTP223: digital output, debounced here.
  bool raw = hal::pinRead(PIN_TOUCH) == (TOUCH_ACTIVE_HIGH ? 1 : 0);
  if (raw != touchRaw_) {
    touchRaw_ = raw;
    touchStableMs_ = 0;
  } else if (touchStableMs_ < TOUCH_DEBOUNCE_MS) {
    touchStableMs_ += dtMs;
    if (touchStableMs_ >= TOUCH_DEBOUNCE_MS) s_.touch[0] = raw;
  }
}

// ------------------------------------------------------------------ magnetometer
void SensorHub::initMag() {
  if (hal::i2cProbe(MAG_QMCP_ADDR)) {
    // QMC5883P (same setup as the working qmc5883p_xiao test sketch)
    magType_ = 3;
    magDev_ = hal::addI2cDevice(MAG_QMCP_ADDR);
    hal::I2cLock lock(50);
    hal::i2cWriteReg(magDev_, 0x0B, 0x08);  // set/reset on, +-8 G range
    delayMs(5);
    hal::i2cWriteReg(magDev_, 0x0A, 0xCF);  // continuous, 200 Hz, OSR 512
    BLK_LOG(TAG, "magnetometer: QMC5883P at 0x%02x", MAG_QMCP_ADDR);
  } else if (hal::i2cProbe(MAG_QMC_ADDR)) {
    magType_ = 1;
    magDev_ = hal::addI2cDevice(MAG_QMC_ADDR);
    hal::I2cLock lock(50);
    hal::i2cWriteReg(magDev_, 0x0B, 0x01);  // SET/RESET period
    hal::i2cWriteReg(magDev_, 0x09, 0x1D);  // continuous, 200 Hz, 8 G, OSR 512
  } else if (hal::i2cProbe(MAG_HMC_ADDR)) {
    magType_ = 2;
    magDev_ = hal::addI2cDevice(MAG_HMC_ADDR);
    hal::I2cLock lock(50);
    hal::i2cWriteReg(magDev_, 0x00, 0x78);  // 8 samples avg, 75 Hz
    hal::i2cWriteReg(magDev_, 0x01, 0x20);  // gain 1.3 Ga
    hal::i2cWriteReg(magDev_, 0x02, 0x00);  // continuous
  } else {
    BLK_LOG(TAG, "no magnetometer found: heading uses the gyro");
  }
  s_.magOk = magType_ != 0;
  // Uncalibrated defaults: symmetric range, baseline found on first reads.
  for (int i = 0; i < 3; ++i) {
    magCal_.min[i] = -1000;
    magCal_.max[i] = 1000;
  }
  magCal_.baseline = 0;
  magCal_.valid = 0;
}

void SensorHub::readMag() {
  if (!magType_) return;
  int16_t v[3];
  {
    hal::I2cLock lock(2);
    if (!lock.ok) return;
    uint8_t b[6];
    const uint8_t dataReg = magType_ == 1 ? 0x00 : (magType_ == 2 ? 0x03 : 0x01);
    if (!hal::i2cReadRegs(magDev_, dataReg, b, 6)) return;
    if (magType_ == 1 || magType_ == 3) {  // QMC5883L / QMC5883P: X Y Z, little-endian
      v[0] = (int16_t)(b[0] | (b[1] << 8));
      v[1] = (int16_t)(b[2] | (b[3] << 8));
      v[2] = (int16_t)(b[4] | (b[5] << 8));
    } else {  // HMC: X Z Y, big-endian
      v[0] = (int16_t)((b[0] << 8) | b[1]);
      v[2] = (int16_t)((b[2] << 8) | b[3]);
      v[1] = (int16_t)((b[4] << 8) | b[5]);
    }
  }
  for (int i = 0; i < 3; ++i) {
    s_.magRaw[i] = v[i];
    if (magCalibrating_) {
      magScan_.min[i] = min(magScan_.min[i], v[i]);
      magScan_.max[i] = max(magScan_.max[i], v[i]);
    }
  }
  // Hard-iron offset + per-axis scale (simple soft-iron correction)
  float c[3], r[3];
  for (int i = 0; i < 3; ++i) {
    c[i] = (magCal_.max[i] + magCal_.min[i]) * 0.5f;
    r[i] = max(1.0f, (magCal_.max[i] - magCal_.min[i]) * 0.5f);
  }
  float x = (v[0] - c[0]) / r[0] * MAG_X_SIGN;
  float y = (v[1] - c[1]) / r[1] * MAG_Y_SIGN;
  float z = (v[2] - c[2]) / r[2];
  float h = atan2f(y, x) * 57.2958f + MAG_HEADING_OFFSET;  // robot assumed level
  while (h < 0) h += 360.0f;
  while (h >= 360.0f) h -= 360.0f;
  s_.heading = h;

  // Magnet detection: how far the field magnitude is from the normal Earth field.
  float mag = sqrtf((float)v[0] * v[0] + (float)v[1] * v[1] + (float)v[2] * v[2]);
  if (magCal_.baseline <= 0) magCal_.baseline = mag;  // first reading at boot
  if (magCalibrating_) {
    magSum_ += mag;
    magN_++;
  }
  float dev = fabsf(mag / magCal_.baseline - 1.0f);
  float strength = constrain(dev / (MAGNET_FULL_RATIO - 1.0f) * 100.0f, 0.0f, 100.0f);
  s_.magStrength = (uint8_t)strength;
  (void)z;
}

void SensorHub::beginMagCalibration() {
  for (int i = 0; i < 3; ++i) {
    magScan_.min[i] = 32767;
    magScan_.max[i] = -32768;
  }
  magSum_ = 0;
  magN_ = 0;
  magCalibrating_ = true;
}

bool SensorHub::endMagCalibration() {
  magCalibrating_ = false;
  // Need a real sweep on X and Y (the robot spins flat)
  for (int i = 0; i < 2; ++i)
    if (magScan_.max[i] - magScan_.min[i] < 60) return false;
  for (int i = 0; i < 3; ++i) {
    magCal_.min[i] = magScan_.min[i];
    magCal_.max[i] = magScan_.max[i];
  }
  if (magScan_.max[2] - magScan_.min[2] < 20) {  // Z barely changes when spinning flat
    magCal_.min[2] = (int16_t)(s_.magRaw[2] - 1000);
    magCal_.max[2] = (int16_t)(s_.magRaw[2] + 1000);
  }
  if (magN_ > 0) magCal_.baseline = magSum_ / magN_;  // normal field strength, no magnet near
  magCal_.valid = 1;
  return true;
}

void SensorHub::readMic() {
  if (PIN_MIC < 0) {
    s_.soundLevel = 0;
    return;
  }
  uint16_t lo = 4095, hi = 0;
  for (int i = 0; i < MIC_SAMPLES; ++i) {
    uint16_t v = (uint16_t)hal::adcRead(PIN_MIC);
    lo = min(lo, v);
    hi = max(hi, v);
  }
  float level = (float)((int)(hi - lo) - MIC_PP_QUIET) * 100.0f / (MIC_PP_LOUD - MIC_PP_QUIET);
  level = constrain(level, 0.0f, 100.0f);
  soundSmooth_ = level > soundSmooth_ ? level : soundSmooth_ * 0.9f + level * 0.1f;  // fast attack, slow release
  s_.soundLevel = (uint8_t)soundSmooth_;
}

void SensorHub::pushDistance(float cm) {
  usHistory_[usIdx_] = cm;
  usIdx_ = (uint8_t)((usIdx_ + 1) % 3);
  float a = usHistory_[0], b = usHistory_[1], c = usHistory_[2];  // median of 3
  s_.distanceCm = max(min(a, b), min(max(a, b), c));
}

void SensorHub::pollUltrasonic() {
  uint32_t now = millis();
  if (g_echoNew) {
    g_echoNew = false;
    float cm = g_echoWidth / 58.0f;
    pushDistance((cm < 2 || cm > US_MAX_CM) ? (float)US_MAX_CM : cm);
    echoSeen_ = true;
  }
  if (now - lastTrig_ >= US_PERIOD_MS) {
    if (lastTrig_ != 0 && !echoSeen_) pushDistance(US_MAX_CM);  // no echo: nothing in range
    echoSeen_ = false;
    lastTrig_ = now;
    hal::pinWrite(PIN_US_TRIG, 1);
    esp_rom_delay_us(10);
    hal::pinWrite(PIN_US_TRIG, 0);
  }
}

void SensorHub::update(uint32_t dtMs) {
  readIr();
  readImu(dtMs);
  readTouch(dtMs);
  if (++magDiv_ >= MAG_READ_EVERY) {
    magDiv_ = 0;
    readMag();
  }
  if (!s_.magOk) {  // no magnetometer: heading from the gyro (0 = direction at boot)
    float h = fmodf(-s_.yaw, 360.0f);
    s_.heading = h < 0 ? h + 360.0f : h;
  }
  readMic();
  pollUltrasonic();
  if (PIN_BATTERY >= 0 && ++battDiv_ >= 50) {  // twice a second
    battDiv_ = 0;
    float v = hal::adcRead(PIN_BATTERY) / 4095.0f * 3.1f * BATT_DIVIDER;  // ~3.1 V full scale at 12 dB
    float pct = (v - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
    float clamped = constrain(pct, 0.0f, 100.0f);
    battSmooth_ = battSmooth_ < 0 ? clamped : battSmooth_ * 0.9f + clamped * 0.1f;
    s_.batteryPct = (uint8_t)battSmooth_;
  }
  for (uint8_t p = 0; p < TOUCH_PAD_COUNT; ++p) s_.touchHeldMs[p] = s_.touch[p] ? s_.touchHeldMs[p] + dtMs : 0;
}

bool SensorHub::lineInZone(uint8_t zone) const {
  switch (zone) {
    case 1: return lineAt(0) || lineAt(1) || lineAt(2);
    case 2: return lineAt(3) || lineAt(4);
    case 3: return lineAt(5) || lineAt(6) || lineAt(7);
    default:
      for (uint8_t i = 0; i < 8; ++i)
        if (lineAt(i)) return true;
      return false;
  }
}

bool SensorHub::edgeAt(uint8_t corner) const {
  if (corner == 0) return edgeAt(1) || edgeAt(2) || edgeAt(3) || edgeAt(4);
  if (corner > 4) return false;
  uint8_t c = corner - 1;
  return s_.edge[c] >= EDGE_THRESHOLD || (EDGE_USE_DEPTH && s_.depth[c] >= DEPTH_THRESHOLD);
}

bool SensorHub::objectNear(uint8_t corner) const {
  if (corner == 0) return s_.cornerObject[0] || s_.cornerObject[1] || s_.cornerObject[2] || s_.cornerObject[3];
  return corner <= 4 && s_.cornerObject[corner - 1];
}

bool SensorHub::touched(uint8_t pad) const {
  if (pad == 0) {
    for (uint8_t p = 0; p < TOUCH_PAD_COUNT; ++p)
      if (s_.touch[p]) return true;
    return false;
  }
  return pad <= TOUCH_PAD_COUNT && s_.touch[pad - 1];
}

void SensorHub::beginIrCalibration() {
  for (int i = 0; i < 16; ++i) {
    calScan_.white[i] = 4095;
    calScan_.black[i] = 0;
  }
  calibrating_ = true;
}

void SensorHub::endIrCalibration() {
  calibrating_ = false;
  // Only accept channels that actually saw contrast; keep old values otherwise.
  for (int i = 0; i < 16; ++i) {
    if (calScan_.black[i] > calScan_.white[i] + 200) {
      cal_.white[i] = calScan_.white[i];
      cal_.black[i] = calScan_.black[i];
    }
  }
}
