// DIVERGENCE DIV-0006: the dialogue box's pen advance, per glyph.
//
// The original advances a flat 12 px a character, in two places that do not
// share the constant: the string draw 0x516B70 (pen word 0x8034F4) and
// MsgBox_Step, which calls Text_DrawAt for one character at a time and keeps
// its own pen, MsgBox_PenX - reload, add 12, store (0x497A34..0x497A4C). That
// is right for the port's 24 x 24 Chinese glyphs and wrong for the Western
// builds' font, whose cells are 8 x 12 and whose stepper adds 8
// (SLUS-00422 0x80150770).
//
// Nothing of MsgBox_Step is replaced. Its one call of Text_DrawAt, at
// 0x497A22, is re-aimed here; we make the same call and then move
// MsgBox_PenX by (advance - 12), so that the stepper's own `+ 12` lands the
// pen where the glyph's advance says. With no table loaded - every shipped
// file - the adjustment is zero and the box behaves as the original.
//
// Not covered, and still 12 px: the stepper's other draw, 0x4987E0, taken
// while flag 8 of 0x7DEE44 is set (unread; the PSX grow/shrink text effect by
// position). The string draw's own pen - every other caller of Text_DrawAt -
// takes the same table in src/game/text_draw.cpp.
#include "game/text_advance.h"

#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr int kOriginalAdvance = 12;       // 0x497A44: add bp, 0xC
constexpr std::uint32_t kCallSite = 0x497A22;  // MsgBox_Step: call Text_DrawAt
constexpr std::uint32_t kCallee = 0x516B30;    // Text_DrawAt; its name is a macro here

std::uint8_t* g_advances;
std::uint32_t g_count;
int g_space = kOriginalAdvance;

}  // namespace

// The glyph index as Text_DrawString derives it (src/game/text_draw.cpp).
int TextAdvance_Of(const std::uint8_t* text) {
    if (!g_advances) return kOriginalAdvance;
    const std::uint8_t b = text[0];
    if (b == 0x20) return g_space;
    std::uint32_t glyph;
    if (b & 0x80) {
        glyph = (static_cast<std::uint32_t>(b & 0x7F) << 8) + text[1];
    } else if (b >= 0x26) {
        glyph = b - 0x26u;
    } else {
        return kOriginalAdvance;  // 0x516B70's control bytes: no glyph
    }
    return glyph < g_count ? g_advances[glyph] : kOriginalAdvance;
}

// Declared in game/text_advance.h: MsgBox_Step (src/game/msgbox.cpp) calls it
// directly now that the stepper is ours.
int TextAdvance_OfGlyph(std::uint32_t glyph) {
    return g_advances && glyph < g_count ? g_advances[glyph] : kOriginalAdvance;
}

extern "C" const unsigned char* __cdecl MsgBox_DrawChar(int x, int y, int color, int count,
                                                        const unsigned char* text) {
    if (!g_advances) return Text_DrawAt(x, y, color, count, text);
    const int advance = TextAdvance_Of(text);  // DIV-0006
    // The stepper has just hung this character into the margin if it is 0x2A
    // or 0x3C at the start of a line: pen = line start - 12 (0x4979A0..B9).
    // The hang should be the glyph's width, as the US stepper's is (8, for its
    // double quote): draw it that much left of the line start, and leave the
    // stepper's + 12 to bring the pen back to the line start.
    if ((text[0] == 0x2A || text[0] == 0x3C) &&
        MsgBox_PenX == static_cast<short>(MsgBox_LineX - kOriginalAdvance))
        return Text_DrawAt(x + kOriginalAdvance - advance, y, color, count, text);
    const unsigned char* end = Text_DrawAt(x, y, color, count, text);
    MsgBox_PenX = static_cast<short>(MsgBox_PenX + advance - kOriginalAdvance);
    return end;
}

void TextAdvance_Set(const std::uint8_t* advances, std::uint32_t count, std::uint32_t space) {
    if (g_advances) Crt_free(g_advances);
    g_advances = static_cast<std::uint8_t*>(Crt_malloc(count));
    std::memcpy(g_advances, advances, count);
    g_count = count;
    g_space = static_cast<int>(space);
    bof3::Log("DIV-0006: advance table, %u glyphs, space %u px", (unsigned)count, (unsigned)space);
}

void TextAdvance_Inject() {
    bof3::RetargetCall("MsgBox_DrawChar", kCallSite, kCallee,
                       reinterpret_cast<void*>(&MsgBox_DrawChar));
}
