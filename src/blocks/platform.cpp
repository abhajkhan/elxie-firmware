#include "platform.h"

#include <Wire.h>

namespace hal {

SemaphoreHandle_t i2cMutex = nullptr;

void initI2c() {
  if (!i2cMutex) i2cMutex = xSemaphoreCreateMutex();
}

I2cDev addI2cDevice(uint8_t addr) { return addr; }

bool i2cProbe(uint8_t addr) {
  I2cLock lock(50);
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool i2cWriteReg(I2cDev dev, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(dev);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool i2cReadRegs(I2cDev dev, uint8_t reg, uint8_t* out, size_t n, int) {
  Wire.beginTransmission(dev);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)dev, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; ++i) out[i] = (uint8_t)Wire.read();
  return true;
}

bool adcAttach(int gpio) {
  if (gpio < 0) return false;
  analogReadResolution(12);  // analogRead() sets the pin up itself
  return true;
}

int adcRead(int gpio) { return gpio < 0 ? 0 : analogRead(gpio); }

void pinOutput(int gpio, int level) {
  if (gpio < 0) return;
  pinMode(gpio, OUTPUT);
  digitalWrite(gpio, level ? HIGH : LOW);
}

void pinInput(int gpio, bool pullUp, bool pullDown) {
  if (gpio < 0) return;
  pinMode(gpio, pullUp ? INPUT_PULLUP : (pullDown ? INPUT_PULLDOWN : INPUT));
}

void pinWrite(int gpio, int level) {
  if (gpio >= 0) digitalWrite(gpio, level ? HIGH : LOW);
}

int pinRead(int gpio) { return gpio < 0 ? 0 : digitalRead(gpio); }

}  // namespace hal
