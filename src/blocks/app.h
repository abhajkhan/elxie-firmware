// Program control, idle screen and safety stops (Home firmware: no on-robot launcher).
#pragma once
#include <stdint.h>

#include "vm/vm.h"

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
void slotsChanged();            // after upload / delete
void showIdle();                // redraw the idle screen (entering Blocks mode)

void startIrCalibration();
bool finishIrCalibration();
bool startMagCalibration();  // robot spins for MAG_CAL_MS, then saves
bool magCalibrating();
}  // namespace app
