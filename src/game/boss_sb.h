// Group BSB of the boss round: fights 4..10 and 13 and enemy kinds 3, 4, 5
// and 8..11 (chapters 0 and 2; the tool's names Engineer / Foreman / Miner,
// Worker, Operator, Torast, Kassen, Galtel, Doksen) - their set-ups, event,
// end and exit hooks, and the kinds' dispatchers, entrances, hooks and kind
// 3's hit sequence. Round eleven, through the boss harness (boss_harness.h).
// docs/boss_sb.md.
#pragma once

void BossSb_Inject();

namespace boss_sb {
// BOF3X_SHADOW=boss_sb: the start-up fuzz, boss_sb_fuzz.cpp - one
// boss_harness::Run per kind and per fight. Clones every original before
// BossSb_Inject patches it.
void SelfTest();
}  // namespace boss_sb
