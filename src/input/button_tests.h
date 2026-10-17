// Group HT of the button round (docs/button-tests.md): the hard-coded button
// tests ask sigma (input/sigma.h). Each site is a one-line change in its own
// module; this file holds the stand-in check the round asks for.
#pragma once

namespace bof3x::input::button_tests {

// BOF3X_SHADOW=button_tests: run once, after sigma::Arm (it needs the live
// table). Part 1 is pure: every changed site's Japan mask under stand-in Config
// words - each release's defaults (identity for Japan: the original's mask;
// the release's own map for the West and PSP-EU) and Japan's with Speak <->
// Action and View <-> Change swapped (the mask with circle <-> cross and
// L1 <-> R1, stated here by hand, not through sigma). Group HT-2 adds the
// text's speed-up, the music room and the battle command cross, with
// Battle_CommandPadMasks' six masks read in place. Part 2 runs five of ours
// (LeaderPanel_S1Buttons, LeaderPanel_PressLatch, EffectKind8C_Ready,
// EffectKind8C_CountPress, and BattleMenu_CommandSelect held with nothing
// pressed: the five with a path that calls nothing) under the
// live table built from those stand-in words by sigma::Rebound, and checks that
// the swapped Config with the swapped press does what Japan's does with the
// press itself. The nine pad words and every cell touched are put back.
// A mismatch is a Fatal.
void Check();

}  // namespace bof3x::input::button_tests
