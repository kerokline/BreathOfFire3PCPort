// Scenario chapters 15, 17, 18 and 19 and chapter 16's object hook (the PSX's
// SCENA15..SCENA19.EMI), compiled into the exe at 0x567DC0..0x56B2A0 (with
// 0x537580), 0x56C080 and 0x56C130..0x56D5E0: chapter 15's frame, its
// area set-ups and six runs, its object, step and arrive hooks; chapter 17's
// staff roll, its draws and the task that follows it; chapters 18 and 19's
// frames. Round ten group SC15, taken with the scenario harness
// (scenario_harness.h). docs/scena_sc15.md.
#pragma once

void ScenaSc15_Inject();

namespace scena_sc15 {
// BOF3X_SHADOW=scena_sc15: the start-up fuzz, scena_sc15_fuzz.cpp. Clones every
// original before ScenaSc15_Inject patches it.
void SelfTest();
}  // namespace scena_sc15
