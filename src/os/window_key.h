// bof3::os, a key posted to a window of the system's own window system, as a
// keyboard's press or release is queued for it: the input recipe's `key` step
// (hook/input_script.cpp) when the game's window is the Win32 one - our
// WinMain under BOF3X_OFF=WindowSdl or BOF3X_ORIGINAL=Game_WndProc, or
// Capcom's WinMain - whose procedure takes keys only as messages. The SDL
// window takes a recipe's key through its own queue instead
// (game_window::QueueKey), on every system.
//
// Windows: PostMessageA of WM_KEYDOWN with lParam 1 (one press, the key was
// up), or of WM_KEYUP with lParam 0xC0000001 (the key was down, going up), the
// virtual-key number as wParam - the two messages the recipe posted before the
// seam. POSIX: there are no such messages; false, with LastError ENOSYS.
// Part of the operating-system seam (docs/linux-round.md, LB1): no Windows or
// POSIX type appears here, and nothing logs or stops: the caller owns the
// message (bof3::Fatal).
#pragma once

namespace bof3::os {

// `window`: the window's handle (an HWND), as the game holds it. True when the
// message is queued; false when it is not (LastError says why).
bool PostWindowKey(void* window, unsigned vk, bool down) noexcept;

}  // namespace bof3::os
