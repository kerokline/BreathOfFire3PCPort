// Scenario chapter 0 (the PSX's SCENA00.EMI; compiled into the exe at
// 0x537F20..0x539AD0): its vtable's frame, object and step hooks, the three
// states, the ten runs and their helpers. Round ten group SCH, taken with the
// scenario harness (scenario_harness.h). docs/scena_sc0.md.
#pragma once

void ScenaSc0_Inject();

namespace scena_sc0 {
// BOF3X_SHADOW=scena_sc0: the start-up fuzz, scena_sc0_fuzz.cpp. Clones every
// original before ScenaSc0_Inject patches it.
void SelfTest();
}  // namespace scena_sc0
