// Scenario chapters 7 and 8: the chapter code behind Scena07_Hooks 0x6611B8
// and Scena08_Hooks 0x661368, compiled into the exe at 0x54F080..0x553B21 -
// each chapter's frame and state machine (start, area entry, the run and its
// scenes), its object, step, arrive and cell hooks and their helpers. Round
// ten group SC7, taken with the scenario harness (scenario_harness.h).
// docs/scena_sc7.md.
#pragma once

void ScenaSc7_Inject();

namespace scena_sc7 {
// BOF3X_SHADOW=scena_sc7: the start-up fuzz, scena_sc7_fuzz.cpp. Clones every
// original before ScenaSc7_Inject patches it.
void SelfTest();
}  // namespace scena_sc7
