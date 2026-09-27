// Chapter 5's scenario bank, 0x546390..0x54A910: the chapter's four vtable
// slots of its own (the frame, the object trigger, the step and cell hooks;
// slot 3 is Scenario_NoHook), its enter-area state, its run dispatcher and
// the eighteen runs (the scenes), two helpers, the object handlers and the
// cell hooks' two entries. Round ten group SC5, taken with the scenario
// harness (scenario_harness.h). docs/scena_sc5.md.
#pragma once

void ScenaSc5_Inject();

namespace scena_sc5 {
// BOF3X_SHADOW=scena_sc5: the start-up fuzz, scena_sc5_fuzz.cpp. Clones every
// original before ScenaSc5_Inject patches it.
void SelfTest();
}  // namespace scena_sc5
