// Chapter 9's tail and chapter 10 of the scenario code, 0x557170..0x55C040:
// chapter 9's step hook (slot 2), cell hook (slot 4) and its fourteen cell
// handlers, and eight of its object handlers; chapter 10 whole - its frame,
// object trigger, step and arrive hooks, enter-area state, run dispatcher and
// eleven runs, its object handlers and helpers, and two handlers areas 75 and
// 86 name. Round ten group SC9b, taken with the scenario harness
// (scenario_harness.h). docs/scena_sc9b.md.
#pragma once

void ScenaSc9b_Inject();

namespace scena_sc9b {
// BOF3X_SHADOW=scena_sc9b: the start-up fuzz, scena_sc9b_fuzz.cpp. Clones every
// original before ScenaSc9b_Inject patches it.
void SelfTest();
}  // namespace scena_sc9b
