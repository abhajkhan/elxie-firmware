// Robot battery level, measured by BatteryManager (games, Blocks and the app all use it).
#pragma once
#include <stdint.h>

namespace battery {
uint16_t packMv();   // pack voltage in millivolts (smoothed), 0 before the first reading
uint8_t percent();   // 0..100, straight line between the BoardConfig pack points
uint16_t pinMv();    // raw ADC reading of GPIO7 in mV (before the divider and BATT_CAL)
}  // namespace battery
