// Round eleven group BSC: the boss band's fights 11, 12, 14, 15, 16 and 46 and
// kinds 12..17 and 53 (chapters 2 and 3 by the plan's section 5): the set-ups
// and their end / exit / event hooks, and the kinds' dispatchers, state
// handlers and +0xF4 hook tables - Amalgam's (kind 12) death, the Balio /
// Sunder / Nina scripts of fights 13 and 16 (kinds 13, 14, 17), Rocky, Pooch
// and Sample 8's entrances (15, 16, 53). 53 functions of 0x439410..0x43A589,
// through the boss harness (boss_harness.h). docs/boss_sc.md.
#pragma once

void BossSc_Inject();

namespace boss_sc {
// BOF3X_SHADOW=boss_sc: the start-up fuzz, boss_sc_fuzz.cpp - one
// boss_harness::Run per set-up and per kind (13). Clones every original
// before BossSc_Inject patches it.
void SelfTest();
}  // namespace boss_sc
