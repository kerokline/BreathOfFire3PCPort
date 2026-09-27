// Scenario chapter 1's bank (the PSX's SCENA01.EMI, compiled into the exe at
// 0x539AE0..0x53DD92): the chapter's start, its run dispatcher and the
// eighteen scenes it runs, the effect helper one scene calls, the object hook
// and its seventeen handlers, and the cell hook and its handler. Round ten,
// group SC1, taken with the scenario harness (scenario_harness.h).
// docs/scena_sc1.md.
#pragma once

void ScenaSc1_Inject();

namespace scena_sc1 {
// BOF3X_SHADOW=scena_sc1: the start-up fuzz, scena_sc1_fuzz.cpp. Clones every
// original before ScenaSc1_Inject patches it.
void SelfTest();
}  // namespace scena_sc1
