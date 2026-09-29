// Round twelve group BE7 (docs/takeover-queue-field-battle.md section 3; the
// cut table analysis/round12_cut.tsv, group BE7): the battle windows of
// 0x597FC0..0x59DB61 - the result screen's level-up and drops windows and
// their frame, the gene windows of the window-kind handler 0x598890 (the
// grid's draw, the three-choice box, the two lists and their parts), and the
// battle menu's equipment window 0x59D640. 31 functions through the boss
// harness as an engine group (boss_harness.h, docs/boss_harness.md section
// 10). docs/battle_e7.md.
#pragma once

void BattleE7_Inject();

namespace battle_e7 {
// BOF3X_SHADOW=battle_e7: the start-up fuzz, battle_e7_fuzz.cpp - one
// boss_harness::Run over the 31. Clones every original before
// BattleE7_Inject patches it.
void SelfTest();
}  // namespace battle_e7
