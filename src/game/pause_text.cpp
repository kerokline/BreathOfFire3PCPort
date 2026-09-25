// DIVERGENCE DIV-0038: the F9 pause lines in the overlay's language.
//
// While Game_Paused is set, WinMain draws two lines through Text_DrawAt at
// (100, 100) and (0x70, 0x80) - Pause_LinesGame 0x66A418 in game,
// Pause_LinesTitle 0x66A448 while Title_LogoState or Title_Fade is set. They
// are the PC port's own text, in the exe's .data and on no disc, so the
// language overlays never reached them. Read 2026-09-23 by rendering their
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
// The lines are ours - the PlayStation has no such screen - one set per
// language, written and encoded by tools/loc_build.py (PAUSE_LINES) into the
// overlay's chunk kind 14, after its glyphs: an overlay without one (or no
// overlay) leaves Capcom's. Before 2026-09-25 the English lines were built in
// here and applied on any overlay's advance table, so the Japanese, French
// and German overlays showed them too (docs/new-code-audit.md A2). The four
// pointers are re-aimed through PatchBytes under the name "PauseText", so
// BOF3X_ORIGINAL=PauseText keeps Capcom's lines.
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
};

// In the chunk's order: in game (F9, other key), then the title's.
const Line kLines[] = {
    {0x66A418, 0x66A3F0},
    {0x66A41C, 0x66A404},
    {0x66A448, 0x66A420},
    {0x66A44C, 0x66A434},
};
constexpr U kRoom = 96;   // bytes a line, its NUL included; a 26-glyph Japanese line is 53

char g_text[4][kRoom];
bool g_applied = false;

bool IsOurs(const unsigned char* line) {
    for (const char* t : g_text)
        if (reinterpret_cast<const char*>(line) == t) return true;
    return false;
}

}  // namespace

void PauseText_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    if (tag != 0) bof3::Fatal("DIV-0038: pause chunk tag is 0x%X, expected 0", (unsigned)tag);
    // FIRST.DAT's overlay is walked again on a reload; its lines are the same.
    if (g_applied) return;
    const std::uint8_t* p = payload;
    const std::uint8_t* const end = payload + size;
    for (U i = 0; i < 4; ++i) {
        const std::uint8_t* nul = static_cast<const std::uint8_t*>(std::memchr(p, 0, static_cast<U>(end - p)));
        if (!nul) bof3::Fatal("DIV-0038: pause chunk ran out inside line %u", (unsigned)i);
        const U n = static_cast<U>(nul - p) + 1;
        if (n == 1 || n > kRoom) bof3::Fatal("DIV-0038: pause line %u is %u bytes, room is %u", (unsigned)i, (unsigned)n, (unsigned)kRoom);
        std::memcpy(g_text[i], p, n);
        p = nul + 1;
    }
    if (p != end) bof3::Fatal("DIV-0038: pause chunk has %u bytes past its four lines", (unsigned)(end - p));
    g_applied = true;
    for (U i = 0; i < 4; ++i) {
        const U ours = static_cast<U>(reinterpret_cast<std::uintptr_t>(g_text[i]));
        std::uint8_t was[4], is[4];
        std::memcpy(was, &kLines[i].original, 4);
        std::memcpy(is, &ours, 4);
        bof3::PatchBytes("PauseText", kLines[i].slot, was, is, 4);
    }
}

int PauseText_X(const unsigned char* line, int original_x) {
    if (!IsOurs(line)) return original_x;
    int width = 0;
    for (const unsigned char* p = line; *p; p += (*p & 0x80) ? 2 : 1) width += TextAdvance_Of(p);
    return 160 - width / 2;
}
