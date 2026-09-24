// The window and the frame loop: WinMain 0x4FCB00, WndProc 0x4FC6F0 and three
// helpers, replaced for the display overhaul's window modes
// (docs/window-modes.md, DIV-0032..0034).
#pragma once

void WinMain_Inject();

// DIV-0033: false while the game runs without being the foreground
// application. The pad words are zeroed while it is false, because the
// DirectInput keyboard is opened DISCL_BACKGROUND and would otherwise read
// what the player types into other windows. True whenever the loop is not
// ours (BOF3X_ORIGINAL=Game_WinMain), and always under Capcom's WndProc
// (BOF3X_ORIGINAL=Game_WndProc), which freezes the loop itself, so the
// original behaviour is unchanged.
bool WinMain_InputAllowed();
