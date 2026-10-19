// Platform layer on ESP-IDF: time, I2C bus, ADC, LEDC PWM, GPIO helpers.
// Everything above this layer is written against these functions only.
#pragma once
#include <stdint.h>
#include <stddef.h>

#include <algorithm>

#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

using std::max;
using std::min;

inline uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }
inline uint32_t micros() { return (uint32_t)esp_timer_get_time(); }
inline void delayMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1)); }

template <class T, class L, class H>
constexpr T constrain(T v, L lo, H hi) {
  return v < (T)lo ? (T)lo : (v > (T)hi ? (T)hi : v);
}

namespace hal {

// ---- I2C (one bus shared by IMU, magnetometer and OLED)
void initI2c();
i2c_master_dev_handle_t addI2cDevice(uint8_t addr);
bool i2cProbe(uint8_t addr);
bool i2cWriteReg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val);
bool i2cReadRegs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t* out, size_t n, int timeoutMs = 5);

// Held for each I2C transaction group. The OLED takes it per transfer, so a
// full frame never blocks the IMU for more than one short chunk.
extern SemaphoreHandle_t i2cMutex;
struct I2cLock {
  bool ok;
  explicit I2cLock(uint32_t waitMs = 5) { ok = xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(waitMs)) == pdTRUE; }
  ~I2cLock() {
    if (ok) xSemaphoreGive(i2cMutex);
  }
};

// ---- ADC1 one-shot reads (IR mux signal, microphone)
bool adcAttach(int gpio);  // returns false if the pin isn't on ADC1
int adcRead(int gpio);     // 0..4095, or 0 if not attached

// ---- LEDC PWM
void pwmTimer(int timer, uint32_t freqHz, int bits);
void pwmAttach(int gpio, int channel, int timer);
void pwmWrite(int channel, uint32_t duty);
void pwmFreq(int timer, uint32_t freqHz);

// ---- GPIO
void pinOutput(int gpio, int level = 0);
void pinInput(int gpio, bool pullUp, bool pullDown);
void pinWrite(int gpio, int level);
int pinRead(int gpio);

}  // namespace hal
