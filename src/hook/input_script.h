// Scripted pad input: play a recipe of button presses into the game, counted
// in frames, so an unattended session can walk to a screen and have it
// captured. Diagnostic only, off unless BOF3X_INPUT is set (docs/input-script.md).
//
// Instrumentation, not a replacement: Input_Latch runs as Capcom built it,
// and then, while a recipe is playing, pad 1's three words are overwritten
// with the recipe's. With BOF3X_INPUT unset nothing is patched at all, so it
// needs no DIVERGENCE.md entry and cannot move the frame hash.
#pragma once

namespace bof3 {

// BOF3X_INPUT names a recipe file. Call after InjectAll.
void InputScript_Start();

// The latch our WinMain calls once per pass of its loop where Capcom's called
// Input_Latch (0x4FCDDE): the recipe's or the recorder's latch when one is
// on, else Input_Latch itself - and, in every case, DIV-0033's zeroing of the
// device words while the window is not in front. Capcom's WinMain
// (BOF3X_ORIGINAL=Game_WinMain) keeps its RetargetCall'd site instead.
void InputScript_Latch();

}  // namespace bof3
