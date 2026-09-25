// DIVERGENCE DIV-0038: the F9 pause lines in English.
//
// While Game_Paused is set, WinMain draws two lines through Text_DrawAt at
// (100, 100) and (0x70, 0x80) - Pause_LinesGame 0x66A418 in game,
// Pause_LinesTitle 0x66A448 while Title_LogoState or Title_Fade is set. They
// are the PC port's own text, in the exe's .data and on no disc, so the
// English overlays never reached them. Read 2026-09-23 by rendering their
// glyph codes from FIRST.DAT's table (docs/window-modes.md section 6):
//
//   0x66A3F0  再按一次F9回主画面   press F9 again: back to the title screen
//   0x66A404  按其他键继续游戏     press any other key: go on playing
//   0x66A420  再按一次F9结束游戏   press F9 again: quit the game
//   0x66A434  按其他键回主画面     press any other key: back to the title screen
//
// which is what WndProc does with the next key (F9: Game_RestartFlag in game,
// Game_QuitFlag on the title; anything else: unpause).
//
// The English is ours - the PlayStation has no such screen - kept under
// about 37 characters, since at the English font's 8 units a character a
// line must fit the 320-unit screen, and is written in the English overlay's
// encoding, which for letters, digits and the space
// is ASCII (tools/loc_build.py, ASCII_OF). The four pointers are re-aimed
// through PatchBytes under the name "PauseText", so BOF3X_ORIGINAL=PauseText
// keeps Capcom's lines; and only once the English glyphs are in the table,
// so a missing overlay never draws English bytes with Chinese glyphs.
#include "game/pause_text.h"

#include <cstring>

#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;

struct Line {
    U slot;       // the pointer in .data
    U original;   // what it holds in the shipped exe
    const char* english;
};

const Line kLines[] = {
    {0x66A418, 0x66A3F0, "Press F9 again for the title screen"},
    {0x66A41C, 0x66A404, "Press any other key to continue"},
    {0x66A448, 0x66A420, "Press F9 again to quit the game"},
    {0x66A44C, 0x66A434, "Any other key returns to the title"},
};

bool g_applied = false;

bool IsOurs(const unsigned char* line) {
    for (const Line& l : kLines)
        if (reinterpret_cast<const char*>(line) == l.english) return true;
    return false;
}

}  // namespace

void PauseText_Apply() {
    if (g_applied) return;
    g_applied = true;
    for (const Line& l : kLines) {
        const U ours = static_cast<U>(reinterpret_cast<std::uintptr_t>(l.english));
        std::uint8_t was[4], is[4];
        std::memcpy(was, &l.original, 4);
        std::memcpy(is, &ours, 4);
        bof3::PatchBytes("PauseText", l.slot, was, is, 4);
    }
}

int PauseText_X(const unsigned char* line, int original_x) {
    if (!IsOurs(line)) return original_x;
    int width = 0;
    for (const unsigned char* p = line; *p; p += (*p & 0x80) ? 2 : 1) width += TextAdvance_Of(p);
    return 160 - width / 2;
}
