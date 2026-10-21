// Robot battery level, measured by BatteryManager (games, Blocks and the app all use it).
#pragma once
#include <stdint.h>

namespace battery {
uint16_t packMv();   // pack voltage in millivolts (smoothed), 0 before the first reading
uint8_t percent();   // 0..100, Li-ion curve on the per-cell voltage
}  // namespace battery
