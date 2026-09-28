// Group BSI of the boss round: fights 37, 38, 40, 42, 44, 45, 49, 50, 51, 53
// and enemy kinds 45, 47, 49, 51, 52, 56, 57, 58, 60 (the tool's Ammonite,
// Sample 2, Sample 4, Sample 6, Sample 7, Manmo, Chimera, Arwan, HugeSlug)
// and the kind-3 effect dispatcher's slot 7 (Arwan's task) - their set-ups
// and end hooks, the kinds' dispatchers, entrances and +0xF4 hooks, kind 58's
// state 4 and 5 and its task. Round eleven, through the boss harness
// (boss_harness.h). docs/boss_si.md.
#pragma once

void BossSi_Inject();

namespace boss_si {
// BOF3X_SHADOW=boss_si: the start-up fuzz, boss_si_fuzz.cpp - one
// boss_harness::Run per kind, per fight and for the task. Clones every
// original before BossSi_Inject patches it.
void SelfTest();
}  // namespace boss_si
