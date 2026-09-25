// Scripted pad input: play a recipe of button presses into the game, counted
// in frames, so an unattended session can walk to a screen and have it
// captured - or, with BOF3X_RECORD, write one from the player's own play.
// Diagnostic only, off unless one of the two is set (docs/input-script.md).
//
// Instrumentation, not a replacement: Input_Latch runs as Capcom built it,
// and then, while a recipe is playing, pad 1's three words are overwritten
// with the recipe's. With both variables unset nothing is patched at all, so
// it needs no DIVERGENCE.md entry and cannot move the frame hash.
#pragma once

namespace bof3 {

// BOF3X_INPUT names a recipe file to play; BOF3X_RECORD a file to record to
// (both set is fatal). BOF3X_SHOT_DIR and BOF3X_SHOT_WAIT shape a recipe's
// `shot` steps (input_script.cpp). Call after InjectAll.
void InputScript_Start();

// Writes the recorder's open run - the hold or wait still going when the
// process ends - and closes the recording. Nothing when not recording.
// Called from DLL_PROCESS_DETACH.
void InputScript_Stop();

// The latch our WinMain calls once per pass of its loop where Capcom's called
// Input_Latch (0x4FCDDE): the recipe's or the recorder's latch when one is
// on, else Input_Latch itself - and, in every case, DIV-0033's zeroing of the
// device words while the window is not in front. Capcom's WinMain
// (BOF3X_ORIGINAL=Game_WinMain) keeps its RetargetCall'd site instead.
void InputScript_Latch();

}  // namespace bof3
