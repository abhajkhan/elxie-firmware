// Platform layer for the block engine on Arduino-ESP32 (Home firmware).
// Same API as firmware/main/hal/platform.h (ESP-IDF), so the block services
// are shared code. I2C goes through Arduino Wire, which the game code also uses.
#pragma once
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

using std::max;
using std::min;

// Log lines go to the same serial console as the game firmware's "[Event] ..." lines.
#define BLK_LOG(tag, fmt, ...) Serial.printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)

inline void delayMs(uint32_t ms) { delay(ms ? ms : 1); }

namespace hal {

// ---- I2C (one bus shared by IMU, magnetometer and OLED; Wire is begun by SensorManager)
typedef uint8_t I2cDev;  // a device is just its 7-bit address on Wire
void initI2c();
I2cDev addI2cDevice(uint8_t addr);
bool i2cProbe(uint8_t addr);
bool i2cWriteReg(I2cDev dev, uint8_t reg, uint8_t val);
bool i2cReadRegs(I2cDev dev, uint8_t reg, uint8_t* out, size_t n, int timeoutMs = 5);

// Held for each I2C transaction group by the block engine (sensors and face).
extern SemaphoreHandle_t i2cMutex;
struct I2cLock {
  bool ok;
  explicit I2cLock(uint32_t waitMs = 5) { ok = xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(waitMs)) == pdTRUE; }
  ~I2cLock() {
    if (ok) xSemaphoreGive(i2cMutex);
  }
};

// ---- ADC (IR mux signal, battery)
bool adcAttach(int gpio);
int adcRead(int gpio);  // 0..4095

// ---- GPIO
void pinOutput(int gpio, int level = 0);
void pinInput(int gpio, bool pullUp, bool pullDown);
void pinWrite(int gpio, int level);
int pinRead(int gpio);

}  // namespace hal
