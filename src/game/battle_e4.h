// Round twelve group BE4: the battle engine's 56 functions of
// 0x444660..0x44AAC9 (analysis/round12_cut.tsv) - the screen tiles, the turn
// and damage helpers, the item command's states 5..9 with the equipment
// window, the escape, three text helpers - through the boss harness as an
// engine group (boss_harness.h, docs/boss_harness.md section 10).
// docs/battle_e4.md.
#pragma once

void BattleE4_Inject();

namespace battle_e4 {
// BOF3X_SHADOW=battle_e4: the start-up fuzz, battle_e4_fuzz.cpp - one
// boss_harness::Run over the group. Clones every original before
// BattleE4_Inject patches it.
void SelfTest();
}  // namespace battle_e4
