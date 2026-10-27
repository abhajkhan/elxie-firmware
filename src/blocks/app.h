// Program control, the "My Programs" list on the robot, and safety stops.
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
uint32_t lastInputMs();          // last touch / remote input in the "My Programs" list
bool takeLeaveRequest();         // remote B pressed in the list (leave Blocks mode)

void startIrCalibration();
bool finishIrCalibration();
bool startMagCalibration();  // robot spins for MAG_CAL_MS, then saves
bool magCalibrating();
}  // namespace app
