// Group BE3 of round twelve, wave one, stage B: 49 functions of the battle
// engine's resident code in 0x437030..0x442F97 - the enemy ops round eleven
// left (EnemyOp_Steps 7..9 and their sub-tables, the action's end, the roll,
// the cue), the party objects' states 6, 10, 11 and 26 with state 6's five
// sub-trees, two party helpers and a fixed-point helper - through the boss
// harness's engine frame (boss_harness.h, Group::engine). docs/battle_e3.md.
#pragma once

void BattleE3_Inject();

namespace battle_e3 {
// BOF3X_SHADOW=battle_e3: the start-up fuzz, battle_e3_fuzz.cpp - one
// boss_harness::Run for the enemy side and one for the party objects
// (BOF3X_BE3_RUN=EO or OBJ runs one). Clones every original before
// BattleE3_Inject patches it.
void SelfTest();
}  // namespace battle_e3
