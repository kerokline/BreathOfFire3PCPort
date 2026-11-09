// The window and the frame loop: WinMain 0x4FCB00, WndProc 0x4FC6F0 and three
// helpers, replaced for the display overhaul's window modes
// (docs/window-modes.md, DIV-0032..0034).
#pragma once

#include <cstdint>

void WinMain_Inject();

// DIV-0033: false while the game runs without being the foreground
// application. The pad words are zeroed while it is false, because the
// DirectInput keyboard is opened DISCL_BACKGROUND and would otherwise read
// what the player types into other windows. True whenever the loop is not
// ours (BOF3X_ORIGINAL=Game_WinMain), and always under Capcom's WndProc
// (BOF3X_ORIGINAL=Game_WndProc), which freezes the loop itself, so the
// original behaviour is unchanged.
bool WinMain_InputAllowed();

// DIV-0033's switch: true unless BOF3X_BACKGROUND=0. Read by Fmv_WndProc
// (DIV-0049), whose pause on deactivation follows it.
bool WinMain_Background();

namespace win_main {
extern std::uint8_t g_capcom_movie_only;   // defined in win_main.cpp (source-layout B0: promoted for a moved reader)
extern std::uint8_t g_pad_reset;   // defined in win_main.cpp (source-layout B0: promoted for a moved reader)
extern bool g_loop_ours;   // defined in win_main.cpp (source-layout B0: promoted for a moved reader)
}  // namespace win_main
