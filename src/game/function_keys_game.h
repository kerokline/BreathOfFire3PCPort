// The function keys in the game (input/function_keys.h; docs/controls.md
// section 4.3): the table the key messages and the pad's presses are looked
// up in, read once from BOF3X_RUBY_KEY and BOF3X_FKEYS.
#pragma once

#include <cstdint>

namespace bof3 {

// The table, once (later calls do nothing): the defaults (F1, F4, F5, F6, F7,
// F8, F9, F11, F12; no pad input), then BOF3X_RUBY_KEY's key for the row above
// the text (f1..f12 or none; any other value stops the game, as before), then
// BOF3X_FKEYS's items over it (input/function_keys.h FnParse; an item it does
// not understand is a log line and left out). Under an input recipe
// (BOF3X_INPUT) or the recorder (BOF3X_RECORD) BOF3X_FKEYS is not read - the
// recipes name the default keys - and a line says so. InjectAll calls it after
// MsgBox_ArmAfterBinds; the lookups below call it too, so a host that skips
// InjectAll's order still reads it before the first key.
void FunctionKeys_Load();

// The action (input::FnAction, as int) the key message (`vk` and `lparam` as
// a WM_KEYDOWN's) starts, or -1.
int FunctionKeys_ActionOfKey(unsigned vk, std::int32_t lparam);

// Once a pass of WinMain's loop: the actions whose pad input went down since
// the last pass, a bit for each (1 << FnAction), first-in-order where two
// actions hold one input. `allowed` false (the window not in front): nothing,
// and an input held then counts only once it has been released. Polls the
// pad only when an action holds a pad input: with none, this returns 0 and
// touches nothing.
unsigned FunctionKeys_PadPressed(bool allowed);

}  // namespace bof3
