// The boss band's shared helpers: the 20 functions of 0x437CA0..0x440829 that
// three or more boss units reach (tools/boss_rows.py --unit H) - the common
// no-op and "answer 0" hooks, the battle-end and exit hooks, the state
// helpers several kinds' tables share, the death sequence kinds 8..11
// (Torast, Kassen, Galtel, Doksen) share, and the map helpers of the area-79
// fights. Round eleven group BH, through the boss harness (boss_harness.h).
// docs/boss_h.md.
#pragma once

void BossH_Inject();

namespace boss_h {
// BOF3X_SHADOW=boss_h: the start-up fuzz, boss_h_fuzz.cpp - three
// boss_harness::Run (the kinds 8..11 chain, the state helpers and callees, the
// hooks). Clones every original before BossH_Inject patches it.
void SelfTest();
}  // namespace boss_h
