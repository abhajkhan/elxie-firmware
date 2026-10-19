// Sensor hub: IR array (16 ch via mux), ultrasonic, IMU, touch, microphone.
// update() is called from the 100 Hz control loop and produces one consistent
// SensorState snapshot that the VM host, events and telemetry all read.
#pragma once
#include <stdint.h>

#include "esp_attr.h"
#include "platform.h"

#include "blocks_config.h"

struct SensorState {
  uint16_t irRaw[16];
  uint8_t line[8];      // 0..100 how strongly each sensor sees the line (polarity handled)
  uint8_t edge[4];      // 0..100 normalized, high = no floor      (FL, FR, BL, BR)
  uint8_t depth[4];     // 0..100 normalized, high = floor far away (FL, FR, BL, BR)
  float linePosition;   // -100 (far left) .. +100 (far right)
  bool lineSeen;
  float distanceCm;
  float pitch, roll;    // degrees
  float yaw;            // integrated gyro heading, degrees (not wrapped)
  float gyroZ;          // deg/s
  float accelX, accelY; // g, raw sensor axes (filtered), for the tilt test
  float accelZ;         // g (about +1 when upright, -1 upside down)
  bool shaken;
  bool imuOk;
  bool touch[TOUCH_PAD_COUNT];
  uint32_t touchHeldMs[TOUCH_PAD_COUNT];
  uint8_t soundLevel;   // 0..100 (0 when no microphone is fitted)
  uint8_t batteryPct;   // 0..100
  float heading;        // 0..360, 0 = magnetic north, clockwise. Gyro-based if no magnetometer.
  uint8_t magStrength;  // 0..100 how strong a nearby magnet is
  bool magOk;
  bool cornerObject[4]; // corner IR sees an object (FL, FR, BL, BR)
  int16_t magRaw[3];
};

struct IrCalibration {
  uint16_t white[16];
  uint16_t black[16];
};

struct MagCalibration {
  int16_t min[3];
  int16_t max[3];
  float baseline;  // Earth-field magnitude after calibration
  uint8_t valid;
};

class SensorHub {
 public:
  void begin();
  void calibrateGyro(uint16_t ms = 1000);  // robot must be still
  void update(uint32_t dtMs);
  const SensorState& state() const { return s_; }

  bool lineAt(uint8_t i) const { return s_.line[i] >= LINE_THRESHOLD; }
  bool lineInZone(uint8_t zone) const;       // 0 any, 1 left, 2 center, 3 right
  bool edgeAt(uint8_t corner) const;         // 0 any, 1..4
  bool frontEdge() const { return edgeAt(1) || edgeAt(2); }
  bool backEdge() const { return edgeAt(3) || edgeAt(4); }
  bool touched(uint8_t pad) const;           // 0 any, 1..N
  bool objectNear(uint8_t corner) const;     // 0 any, 1..4
  bool allEdges() const { return edgeAt(1) && edgeAt(2) && edgeAt(3) && edgeAt(4); }

  // Line calibration: slide the robot over line + floor while active.
  void beginIrCalibration();
  void endIrCalibration();
  bool calibrating() const { return calibrating_; }
  IrCalibration& calibration() { return cal_; }

  // Compass calibration: the robot spins while min/max per axis are recorded.
  void beginMagCalibration();
  bool endMagCalibration();  // false if the sweep looked too small
  void cancelMagCalibration() { magCalibrating_ = false; }
  bool magCalibrating() const { return magCalibrating_; }
  MagCalibration& magCalibration() { return magCal_; }

 private:
  void readIr();
  void readImu(uint32_t dtMs);
  void readTouch(uint32_t dtMs);
  void readMag();
  void initMag();
  void readMic();
  void pollUltrasonic();
  void pushDistance(float cm);
  static void IRAM_ATTR echoIsr(void* arg);
  uint8_t normalize(uint8_t ch, uint16_t raw) const;

  SensorState s_{};
  IrCalibration cal_{};
  IrCalibration calScan_{};
  bool calibrating_ = false;
  float gyroBias_[3] = {0, 0, 0};
  uint32_t shakeUntil_ = 0;
  uint32_t lastTrig_ = 0;
  float usHistory_[3] = {US_MAX_CM, US_MAX_CM, US_MAX_CM};
  uint8_t usIdx_ = 0;
  bool echoSeen_ = false;
  bool touchRaw_ = false;
  uint32_t touchStableMs_ = 0;
  uint8_t magType_ = 0;  // 0 none, 1 QMC5883L, 2 HMC5883L, 3 QMC5883P
  hal::I2cDev imuDev_ = 0;
  hal::I2cDev magDev_ = 0;
  uint32_t lastImuUs_ = 0;
  uint8_t magDiv_ = 0;
  MagCalibration magCal_{};
  MagCalibration magScan_{};
  bool magCalibrating_ = false;
  float magSum_ = 0;
  uint32_t magN_ = 0;
  float soundSmooth_ = 0;
  uint8_t battDiv_ = 0;
  float battSmooth_ = -1;
  float lastLinePos_ = 0;
};

extern SensorHub sensors;
