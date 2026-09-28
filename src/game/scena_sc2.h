// Scenario chapter 2's bank (the PSX's SCENA02.EMI, compiled into the exe at
// 0x53DDA0..0x5428B2): the chapter's frame, start, area entry and run
// dispatcher, the twenty-four scenes it runs and their helpers, the object
// hook and its twenty-two handlers, and the step hook. Round ten's third
// wave, group SC2, taken with the scenario harness (scenario_harness.h).
// docs/scena_sc2.md.
#pragma once

void ScenaSc2_Inject();

namespace scena_sc2 {
// BOF3X_SHADOW=scena_sc2: the start-up fuzz, scena_sc2_fuzz.cpp. Clones every
// original before ScenaSc2_Inject patches it.
void SelfTest();
}  // namespace scena_sc2
