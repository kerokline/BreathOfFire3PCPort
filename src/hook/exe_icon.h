// DIV-0095: bof3x.exe's icon, generated on the player's machine from the
// game's own data (docs/exe-icon.md). Exe host only: the hosted build keeps
// Capcom's icon from BOF3.exe's .rsrc.
//
// The picture is the one tools/make_icon.py renders offline: icon 10 of
// Menu_DrawBigIcon's record table (the whelp, the owner's choice 2026-10-11),
// 40 x 48 at 4 bits a pixel from texture page 0x1E with its sixteen-halfword
// CLUT, cropped to its centre 40 x 40. The menu's CLUTs arrive by the
// compressed upload path, not as image chunks, so the picture exists only
// once the game has put the sheet and the CLUT into the VRAM shadow (the
// title's load screen, the field menu).
#pragma once

namespace bof3 {

// Once a drawn frame, on the thread that owns the window (Game_WinMain's
// loop). Until the VRAM shadow holds the sheet and CLUT the record names -
// the same on two drawn frames running - it returns after reading ~500
// halfwords; then it sets the window's big and small icons and the window
// class's (Alt-Tab), and starts a worker thread that writes bof3x.exe.new
// with the icon in its resources and the bof3x.icon stamp, once per install.
// Nothing in the hosted build. Any failure is one log line; none stops the game.
void ExeIcon_Frame(void* hwnd);

// The self-test (BOF3X_SELFTEST_ONLY with BOF3X_ICON_VRAM=<a dump of the
// VRAM shadow, 1 MB> and BOF3X_ICON_REF=<the expected .ico>): renders the
// icon from the dump, writes bof3x.icon-test.ico and bof3x.icon-test.exe
// beside the log, verifies the executable's resources, decodes both .ico
// files and compares every pixel of the five images with the reference. A
// mismatch is a Fatal. Called by Engine_Main after the data are copied.
void ExeIcon_SelfTest();

}  // namespace bof3
