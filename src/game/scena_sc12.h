// Chapter 12's first block of scenario code, 0x55E4E0..0x561DB0: the
// chapter's five vtable slots (the frame, the object trigger, the step,
// arrive and cell hooks), its enter-area state, its run dispatcher and the
// eight runs (the scenes), the object handlers and the cell hooks' two
// entries. Round ten group SC12, taken with the scenario harness
// (scenario_harness.h). docs/scena_sc12.md.
#pragma once

void ScenaSc12_Inject();

namespace scena_sc12 {
// BOF3X_SHADOW=scena_sc12: the start-up fuzz, scena_sc12_fuzz.cpp. Clones every
// original before ScenaSc12_Inject patches it.
void SelfTest();
}  // namespace scena_sc12
