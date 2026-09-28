// Chapter 6's bank of scenario code, 0x54A910..0x54F080: the chapter's four
// vtable slots of its own (the frame, the object trigger, the step and cell
// hooks), its three states, its seventeen runs (the scenes) and their helpers,
// the object handlers and the cell hook's three entries, and a leap (a jump
// arc of the current sprite) area 77 calls. Round ten group SC6, taken with
// the scenario harness (scenario_harness.h). docs/scena_sc6.md.
#pragma once

void ScenaSc6_Inject();

namespace scena_sc6 {
// BOF3X_SHADOW=scena_sc6: the start-up fuzz, scena_sc6_fuzz.cpp. Clones every
// original before ScenaSc6_Inject patches it.
void SelfTest();
}  // namespace scena_sc6
