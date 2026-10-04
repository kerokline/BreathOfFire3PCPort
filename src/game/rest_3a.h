// Group R3A of round fourteen (wave three): 45 functions in 0x404180..0x4378AA
// - the cut's 44 rows for R3A (analysis/round14_cut.tsv) and one start no list
// had (0x4041B0, WorldMap33_FrameStates[2]). docs/rest_3a.md.
//
// What they are, by the code:
//   - area 33's world-map frame states 1..3 (the case blocks WorldMap_FrameStep
//     carries inline; WorldMap33_FrameStates still holds them);
//   - BATE's root (game mode 9: BattleExtra_Dispatch by 0x929F00 and its
//     tally's two sub-dispatchers, the start, the way out, the transition
//     step) and three helpers of its equipment screen;
//   - the battle's end: BattleEnd_Steps[2] (the loss) and [3] with their
//     steps, BattleEnd_ExitSteps[3];
//   - battle-task kind 0's slots 2, 4, 5, 11 and 12 and their states, the
//     actor watch's states 1 and 4, kind 3's BattleBossFx_Dispatch;
//   - BattleEnemy_SetAnimation on an enemy by its battle index (two forms);
//   - New Game's character records.
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest3A_Inject();

namespace rest_3a {
// BOF3X_SHADOW=rest_3a: the start-up fuzz, rest_3a_fuzz.cpp - two
// boss_harness::Runs (the engine frame's for the battle engine's 36, a boss
// frame's for the nine outside the engine's runs). Clones the 45 originals
// before Rest3A_Inject patches them.
void SelfTest();
}  // namespace rest_3a
