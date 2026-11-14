#pragma once

// The New Game sequence, ours (docs/new-game-round.md, stage B group TF): the
// PlayStation's flow back between the title's New Game and the game - the name
// screen for character record 0 (START.EMI's New Game sub-steps, section 3a
// M1), then the Config screen on the title's Game_Step 4 path, then
// TitleFlow_EnterGame. Two switches, both code names of PatchBytes and so in
// BOF3X_OFF:
//
//   NewGameNameEntry  the name screen, where name_grid::NameGrid_Available()
//                     (a kind-22 chunk in the language layer; none under the
//                     original Chinese)
//   NewGameOptions    the Config screen after New Game (the round doc's call 1:
//                     in every language, the original Chinese too)
//
// With both off, or the name entry on where no grid is and the options off,
// TitleFlow_NewGame is the port's whole (sound.cpp).
//
// The screens are sub-steps of Game_Step 2 on SaveMenu_FlowState 0x6BDF86, as
// the PlayStation's are on its sub-step byte 0x801ED7E0, so a recipe can wait
// on them (Game_Step 0x66C7EA == 2 and 0x6BDF86): 0 the open (the records),
// 1..4 the name screen (open, fade in, input, fade out). The step list
// kScreens in new_game.cpp is where a later screen joins (IDEAS I56's
// champion screen): its sub-steps after the last, its own "wanted" test.
//
// Nothing here writes SaveMenu_Cursor 0x6BDF85: it stays 0 (New Game's row) to
// TitleFlow_EnterGame, whose cursor-2 test is then the title's third row alone
// (M1, "The PC's test already tells New Game's options from the third row").

namespace new_game {

// TitleFlow_NewGame's first call. True when a screen of ours or the options
// step is on: this frame's work is done here (the port's body must not run).
// False when neither is: the caller runs the port's body unchanged.
bool NewGame_Sequence();

// TitleFlow_Begin's: the title task has started - the PlayStation's reload
// of START, which is what resets its name buffer and page (M1).
void NewGame_TitleBegins();

// InjectAll, after every module's self-test (save_menu's fuzz compares
// Capcom's TitleFlow_NewGame with both switches 0): the two PatchBytes and
// the snapshot participant "NEWGAME". BOF3X_SHADOW=new_game runs the self-test
// first.
void Arm();

}  // namespace new_game

// sound.cpp, the port's TitleFlow_NewGame 0x5880E0 cut at the one place the
// sequence enters it: the records (NewGame_InitCharacters and DIV-0228's
// stamp), and the rest - 0x9039F2, Game_AreaNumber, Scenario_Start(0), the
// Cond_Flags words - then Transition_Start(0) when `fade`, then Game_Step =
// `step` (5 the port's, 4 the Config screen's title path).
void TitleFlow_NewGameRecords();
void TitleFlow_NewGameRest(bool fade, unsigned step);
