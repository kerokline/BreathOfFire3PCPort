// What the drawn launcher's bind capture makes of a key (docs/launcher-portable.md,
// stage P3): the key's SDL scancode in, the number a Controls cell stores out.
// It is config_dialog.cpp's CaptureKeyHook, with SDL's scancode in place of
// the keyboard hook's scan code and extended flag:
//
//   the key's number is its DirectInput number (input/key_sdl.h), Pause and
//   NumLock too: the Win32 capture crosses the pair from how Windows sends
//   them (input/function_keys.h FnKeyOfScan; docs/known-defects.md D427,
//   fixed 2026-11-20), so both captures store DirectInput's numbers;
//   Escape cancels; a Function keys cell refuses F10 and either Alt, which
//   the game's window never gives to its key handler (WM_SYSKEYDOWN).
//
// Plain C++ over input/key_sdl.h alone, so the host test that holds it against
// the Win32 capture's numbers (tools/key_host capture_tests) builds it with no
// window and no SDL library.
#pragma once

namespace bof3x::ui {

enum class CaptureTake {
    kNothing,   // a key with no number (no PC scan code): the capture waits on
    kCancel,    // Escape
    kRefused,   // F10 or Alt on a Function keys cell: the capture says why and waits on
    kKey,       // the cell takes `number`
};

// `function_keys`: a Function keys cell asked. `number` is set for kKey only.
CaptureTake CaptureTakeKey(int scancode, bool function_keys, int* number);

}  // namespace bof3x::ui
