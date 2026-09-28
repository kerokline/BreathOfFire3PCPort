// Scenario chapters 3 and 4: the chapter code behind Scena03_Hooks 0x660F50
// and Scena04_Hooks 0x660FD0, compiled into the exe at 0x5428C0..0x54638D -
// each chapter's frame and state machine (start, area entry, the run and its
// scenes), its object, step, arrive and cell hooks. Round ten group SC3, taken
// with the scenario harness (scenario_harness.h). docs/scena_sc3.md.
#pragma once

void ScenaSc3_Inject();

namespace scena_sc3 {
// BOF3X_SHADOW=scena_sc3: the start-up fuzz, scena_sc3_fuzz.cpp. Clones every
// original before ScenaSc3_Inject patches it.
void SelfTest();
}  // namespace scena_sc3
