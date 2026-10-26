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
// `shot` steps; BOF3X_SHOT_EVERY=N adds a frame every N recipe frames
// (input_script.cpp, docs/abc-harness.md). Call after InjectAll.
void InputScript_Start();

// Writes the recorder's open run - the hold or wait still going when the
// process ends - and closes the recording; and, when a recipe is still
// playing, logs its `done` (a `quit` step was current) or `FAILED` (any
// other step was). Called from DLL_PROCESS_DETACH.
void InputScript_Stop();

// WinMain's loop while Game_Paused (F9), where Capcom's loop called nothing:
// no device latch, no pad words; a playing recipe advances one step per
// frame with its words dropped, so a recipe can time a pause. While
// BOF3X_RECORD records, each paused frame is written as a waited one.
void InputScript_PausedLatch();

// Game_WndProc's WM_KEYDOWN, before anything else reads the key: while
// BOF3X_RECORD records, a press of F1..F11, or of any key while the game is
// paused (any key ends a pause), is written as `key KEY 0` ahead of the frame
// whose message pass took it - the step a replay posts at that same point.
// F12 is the recorder's shot, read from the key bytes as before.
void InputScript_RecordKey(unsigned vk, long lparam);

// The quick menu (quick_menu.cpp, the quicksave round's QM), which reads a
// pad while paused: true while a recipe plays, with this frame's word (the
// one InputScript_PausedLatch stepped to, in the pad's terms) - so a recipe
// drives the menu as it would the game. False otherwise: the menu reads the
// devices.
bool InputScript_PausedWord(unsigned short* word);

// The latch our WinMain calls once per pass of its loop where Capcom's called
// Input_Latch (0x4FCDDE): the recipe's or the recorder's latch when one is
// on, else Input_Latch itself - and, in every case, DIV-0033's zeroing of the
// device words while the window is not in front. Capcom's WinMain
// (BOF3X_ORIGINAL=Game_WinMain) keeps its RetargetCall'd site instead.
void InputScript_Latch();

// The recipe frame the scripted latch has reached (the number the log's
// `recipe frame` lines and the recipe's `shot` lines count), 0 when no recipe
// is playing. Read by the draw-order log (BOF3X_DRAWORDER).
unsigned InputScript_Frame();

// True from two latches before a recipe's `shot` step until the shot is
// taken: WinMain's loop draws those frames even when it is running late
// (BOF3X_SPEED, a stall), so the frame a shot saves is the one it names.
// Also true on the three passes before each frame BOF3X_SHOT_EVERY writes:
// one forced pass let a late pass before it change the picture (a fade step,
// docs/button-routes.md 9.2).
bool InputScript_ShotPending();

}  // namespace bof3
