// Round twelve group BE1 (docs/takeover-queue-field-battle.md section 3,
// analysis/round12_cut.tsv's rows BE1): the battle engine's first run,
// 0x42D7A0..0x432B6A - BATE's tally and equipment screens (game mode 9),
// the command menu's held-button steps and auto battle, action kind 3's two
// steps, the random ability pick and the ability notice, the end's party
// restore, the result's counts, level-up notice and the level gain, and the
// loss screen with its draws. 41 functions (39 of the cut and two it lacks,
// 0x432430 and 0x432440), through the boss harness's engine frame
// (boss_harness.h). docs/battle_e1.md.
#pragma once

void BattleE1_Inject();

namespace battle_e1 {
// BOF3X_SHADOW=battle_e1: the start-up fuzz, battle_e1_fuzz.cpp - one
// boss_harness::Run with Group::engine. Clones every original before
// BattleE1_Inject patches it.
void SelfTest();
}  // namespace battle_e1
