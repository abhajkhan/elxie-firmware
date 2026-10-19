// Program control, on-robot launcher menu and safety stops.
#pragma once
#include <stdint.h>

#include "vm.h"

namespace app {
void begin();
void tick(uint32_t dtMs);  // launcher input + emergency-stop checks (every control tick)

bool runSlot(uint8_t slot, elx::LoadError* err = nullptr);
void stopProgram();
bool running();
uint8_t runningSlot();  // 0xFF when idle
elx::VM& vm();

uint8_t autorunSlot();
bool setAutorun(uint8_t slot);  // 0xFF = none
void slotsChanged();            // refresh launcher after upload / delete

void startIrCalibration();
bool finishIrCalibration();
bool startMagCalibration();  // robot spins for MAG_CAL_MS, then saves
bool magCalibrating();
}  // namespace app
