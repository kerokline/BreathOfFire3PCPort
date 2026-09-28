// Chapters 13 and 14 of the scenario code, 0x561DB0..0x567DC0: each chapter's
// vtable slots (the frame, the object trigger, the step and arrive hooks), its
// states, its run dispatcher and runs, its object handlers and the helpers the
// runs share; and the state-0 body eight chapters' state tables hold
// (0x5646B0). Round ten group SC13, taken with the scenario harness
// (scenario_harness.h). docs/scena_sc13.md.
#pragma once

void ScenaSc13_Inject();

namespace scena_sc13 {
// BOF3X_SHADOW=scena_sc13: the start-up fuzz, scena_sc13_fuzz.cpp - two runs
// of the harness, chapter 13's functions with Cond_ByteFA 13 and chapter 14's
// with 14. Clones every original before ScenaSc13_Inject patches it.
void SelfTest();
}  // namespace scena_sc13
