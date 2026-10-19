// The quick menu: F5's list of the sixteen quicksaves, newest first, three
// shown at a time as the save menu shows them; Load or Back per slot (the
// quicksave round's QM, docs/quicksave.md, section QM; ledger draft D).
#pragma once

#include <cstdint>

namespace bof3 {

// Game_WndProc's WM_KEYDOWN, first: F5 opens the menu (pausing the game, as
// F9 does) or closes it; while it is open every other key is the menu's and
// does nothing (the pad drives it, not WM_KEYDOWN). True when the key was
// taken; false leaves it to the rest of WndProc.
bool QuickMenu_Key(unsigned vk, long lparam);

// WinMain's loop, in the paused branch, once a frame: the menu's input and
// draw when it is open (true), nothing otherwise (false: the pause lines are
// drawn as ever).
bool QuickMenu_Frame();

// WinMain's loop, right after Snapshot_Pending: when the menu asked a load
// this frame, closes the menu if it ran (the game resumes in the restored
// state) or keeps it open with the refusal's reason drawn.
void QuickMenu_AfterPoint();

// InjectAll, after every module's self-test: the self-test of the two lists
// and the cursor (BOF3X_SELFTEST_ONLY in the exe host, BOF3X_SHADOW=quick_menu
// hosted), then SaveLists_Arm.
void QuickMenu_Inject();

}  // namespace bof3
