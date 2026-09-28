// Chapter 9's first block of scenario code, 0x553B30..0x557170: the
// chapter's frame (vtable slot 0) and object trigger (slot 1), its
// enter-area state, its run dispatcher and fourteen runs (the scenes), and
// four object handlers. Round ten group SC9a, taken with the scenario harness
// (scenario_harness.h). docs/scena_sc9a.md.
#pragma once

void ScenaSc9a_Inject();

namespace scena_sc9a {
// BOF3X_SHADOW=scena_sc9a: the start-up fuzz, scena_sc9a_fuzz.cpp. Clones every
// original before ScenaSc9a_Inject patches it.
void SelfTest();
}  // namespace scena_sc9a
