// The quick menu: F5's list of the sixteen quicksaves, newest first, three
// shown at a time, drawn as the title's load menu with its Yes / No (the
// quicksave round's QM, docs/quicksave.md, section QM; ledger draft D).
#pragma once

#include <cstdint>

namespace bof3 {

// Game_WndProc's WM_KEYDOWN, first (and a pad press bound to a function key's
// action, WinGame_PadActions): `its_key`, the key or press is the one the
// function keys' table gives the quick menu (F5 by default;
// game/function_keys_game.h), opens the menu (pausing the game, as F9 does)
// or closes it; while it is open every other key is the menu's and does
// nothing (the pad drives it, not WM_KEYDOWN). `repeat`: a held key's
// auto-repeat, which neither opens nor closes it. True when the key was
// taken; false leaves it to the rest of WndProc.
bool QuickMenu_Key(bool its_key, bool repeat);

// WinMain's loop, in the paused branch, once a frame: the menu's input and
// draw when it is open (true), nothing otherwise (false: the pause lines are
// drawn as ever).
bool QuickMenu_Frame();

// WinMain's loop, right after Snapshot_Pending: when the menu asked a load
// this frame, closes the menu if it ran (the game resumes in the restored
// state) or keeps it open with the refusal's reason drawn.
void QuickMenu_AfterPoint();

// LoadDatFile, after FIRST.DAT and its layers are walked: copies the load
// menu's four system messages (0xAC, 0xB9, 0xB8, 0xF) out of the title's pool,
// so the menu draws the title's text whatever scene's pool is loaded (a
// battle's has other messages at those ids).
void QuickMenu_TitlePoolLoaded();

// LoadDatFile, after START.DAT and its layers are walked (the title's menu):
// copies the cells Menu_DrawBackdrop draws from - its tiles on the page
// (0x340, 0x100), which START.DAT has just loaded, and its four CLUTs on row
// 0x1E2 - out of the VRAM shadow, so the menu draws the load menu's backdrop
// in a scene that keeps other pictures there (a battle; docs/quicksave.md QM.9).
void QuickMenu_MenuPageLoaded();

// Draft B (docs/state-by-name-tier0.md 2), the title's load path: a refused
// quicksave's save half, loaded through the title after the restart.
// TitleFlow_Begin: the save half's bytes when the title's menu begins on the
// way to that load (the menu's first step then goes to the load menu's read),
// nullptr otherwise; any came-from put-back still armed is dropped first
// (save_tail.h). LoadMenu_Read: the bytes to read in place of the slot's file,
// once, nullptr otherwise. The slot they came from, for the log.
const unsigned char* QuickMenu_TitleBegins();
const unsigned char* QuickMenu_SaveHalfForRead();
unsigned QuickMenu_SaveHalfSlot();

// InjectAll, after every module's self-test: the self-test of the two lists
// and the cursor (BOF3X_SELFTEST_ONLY in the exe host, BOF3X_SHADOW=quick_menu
// hosted), then SaveLists_Arm, then the save tail's self-test and switch
// (SaveTail_Inject).
void QuickMenu_Inject();

}  // namespace bof3
