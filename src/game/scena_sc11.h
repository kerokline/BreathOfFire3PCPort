// Scenario chapter 11 (the PSX's SCENA11.EMI), compiled into the exe at
// 0x55C040..0x55E4D2: the chapter's frame and its three states, eight scenes
// run by MoveScript_Var7, the object trigger and its fourteen handlers, the
// arrive hook and slot 4. Round ten group SC11, taken with the scenario
// harness (scenario_harness.h). docs/scena_sc11.md.
#pragma once

void ScenaSc11_Inject();

namespace scena_sc11 {
// BOF3X_SHADOW=scena_sc11: the start-up fuzz, scena_sc11_fuzz.cpp. Clones every
// original before ScenaSc11_Inject patches it.
void SelfTest();
}  // namespace scena_sc11
