// Moved here by tools/move_funcs.py from battle_misc.cpp, menu_windows.cpp, mode_states.cpp, msgbox.cpp, text_draw.cpp (docs/source-layout-round.md).
#include "game/text/text.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/battle_misc.h"
#include "game/battle_misc_callees.h"
#include "game/battle_text.h"
#include "game/button_text.h"
#include "game/dat_load.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_verbs.h"
#include "game/menu_windows.h"
#include "game/menu_windows_callees.h"
#include "game/mode_states_callees.h"
#include "game/move_script_bytes.h"
#include "game/msgbox.h"
#include "game/msgbox_callees.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/shoulder_glyphs.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

using namespace battle_misc;

}  // namespace

using namespace menu_windows;

using namespace mode_states;

using namespace msgbox;

namespace {

unsigned char* AtBattleMisc(battle_misc::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

battle_misc::U Addr(const void* p) { return Low32(p); }

unsigned char* Next() { return Gfx_PacketNext; }

std::int16_t S16MenuWindows(std::uint32_t v) { return static_cast<std::int16_t>(v); }

// `fild dword` then `fstp dword`: an int rounded once to a float.
float F(std::int32_t v) { return static_cast<float>(v); }

void Put(unsigned char* at, float f) { std::memcpy(at, &f, sizeof f); }

}  // namespace

namespace mode_states {

using move_script::At;

}  // namespace mode_states

namespace {

constexpr int kCell = 12;

constexpr int kRecordBytes = 0x20;

const std::uint8_t* ClipPairs() { return reinterpret_cast<const std::uint8_t*>(::bof3::Img(bof3::addr::Text_ClipPairs)); }

const std::uint8_t* Records() { return reinterpret_cast<const std::uint8_t*>(::bof3::Img(bof3::addr::Text_Records)); }

// The original's answer to a glyph index above 0xA00: `mov dx, 0x1000` /
// `in al, dx` (0x516C9B), a privileged instruction, which in user mode is a
// fault. Ours is the same instruction, so the crash reporter sees what it
// would have seen. Should the fault ever be resumed, the original goes on to
// draw the glyph; so does the caller of this.
void GlyphTrap() { __asm__ volatile("mov $0x1000, %%dx\n\tin %%dx, %%al" : : : "eax", "edx"); }

unsigned ClutOf(unsigned color) { return 0x7800u | (color & 0xFu); }

constexpr unsigned kTextLogSlots = 4096;

std::uint64_t g_textlog_seen[kTextLogSlots];

unsigned g_textlog_count = 0;

void TextLog(std::uint32_t caller, const unsigned char* text) {
    const std::uint64_t key = (static_cast<std::uint64_t>(caller) << 32) | reinterpret_cast<std::uintptr_t>(text);
    unsigned h = static_cast<unsigned>((key * 0x9E3779B97F4A7C15ull) >> 52) % kTextLogSlots;
    for (unsigned probe = 0; probe < kTextLogSlots; ++probe, h = (h + 1) % kTextLogSlots) {
        if (g_textlog_seen[h] == key) return;
        if (g_textlog_seen[h] == 0) {
            if (g_textlog_count + 1 >= kTextLogSlots) return;   // full: stop logging, keep drawing
            g_textlog_seen[h] = key;
            ++g_textlog_count;
            char hex[3 * 16 + 1] = {};
            for (unsigned i = 0; i < 16 && text[i]; ++i) {
                static const char digits[] = "0123456789ABCDEF";
                hex[i * 3] = digits[text[i] >> 4];
                hex[i * 3 + 1] = digits[text[i] & 0xF];
                hex[i * 3 + 2] = ' ';
            }
            bof3::Log("textlog     caller 0x%08X text 0x%08X frame %u: %s", (unsigned)caller,
                      (unsigned)reinterpret_cast<std::uintptr_t>(text), (unsigned)Frame_Counter, hex);
            return;
        }
    }
}

}  // namespace

// original 0x516B30 (PSX 0x8014F6BC). The port's general text draw, 341 callers: it plants the
// pen and hands the string to Text_DrawString.
//
// As the original has it: x is taken as a WORD and stored to both the pen and
// the line start; y is incremented as a dword and stored as a word, so a y of
// 0xFFFF draws at 0; the colour and the count go on unchanged.
extern "C" const unsigned char* __cdecl Text_DrawAt(int x, int y, int color, int count,
                                                    const unsigned char* text) {
    Text_PenX = static_cast<short>(x);
    Text_LineX = static_cast<short>(x);
    Text_PenY = static_cast<short>(y + 1);
    return msgbox::g.draw_string(static_cast<unsigned>(color), static_cast<unsigned>(count), text);
}

// original 0x516B70. Draws `text` at the pen (Text_PenX, Text_PenY) until a
// NUL or until `count` characters - a byte - have gone by.
//
//   0x01        newline: pen y += 12, pen x = Text_LineX
//   0x05 nn     remember the colour byte, take nn
//   0x06        take the remembered colour byte back (0 if none was)
//   0x07 nn     draw the record Text_Records + nn * 32, up to 32 characters,
//               by recursion, in the current colour byte
//   0x20        advance, draw nothing
//   >= 0x80     two bytes: glyph ((b0 & 0x7F) << 8) + b1
//   the rest    glyph b - 0x26 as 16 bits - so 0x02..0x04, 0x08..0x1F and
//               0x21..0x25 wrap past 0xA00 and trap, as any glyph above it
//
// As the original has it:
//   - (u, v), the clip pair picked by the colour byte's high nibble, is read
//     once on entry; 0x05 and 0x06 change the CLUT and not the clip;
//   - the remembered colour is one byte per call, not a stack, and a record
//     drawn by 0x07 starts with none of its own;
//   - a control's argument counts as a character of its own against `count`
//     only in that the loop looks at the byte after it: every pass, glyph or
//     control, costs one;
//   - the loop ends on the byte AFTER the one it handled being NUL, so a
//     two-byte code or an argument whose last byte is 0 ends the string, and
//     the first byte is the only one tested before anything is drawn;
//   - the return is one past the byte the loop stopped at: past the NUL, or,
//     when `count` ran out, past the first character it did not draw.
//
// DIVERGENCE DIV-0006: the pen advances by the glyph's entry in the overlay
// font's advance table where the original adds 12. With no table loaded -
// every shipped file - TextAdvance_Of is 12 and this is the original.
//
// DIVERGENCE (DIV-0140, the button unification round's DT-2;
// docs/button-dialogue.md 4): a button mention in the area text block - a
// layer's button code, or the PC's colour cue with no layer - is drawn as the
// button the Config has put its role on (ButtonText_Code / _Cue, button_text.h):
// the `05 cc` takes that button's colour and the icon its glyph, the pen
// advancing by the drawn glyph's width, so the rest of the line re-flows as
// the layer's widths do. Nothing else changes: the same bytes are walked,
// every pass costs what it cost, the return is the same. When the drawn
// button is the one the text shows, and under BOF3X_ORIGINAL=ButtonText, the
// draw is the one above.
extern "C" const unsigned char* __cdecl Text_DrawString(unsigned color_arg, unsigned count_arg,
                                                        const unsigned char* text) {
    if (text_draw::g_textlog) {
        TextLog(Low32(__builtin_return_address(0)), text);
    }
    unsigned color = color_arg;                     // only the low byte is ever read
    auto count = static_cast<std::uint8_t>(count_arg);
    unsigned clut = ClutOf(color);
    const std::uint8_t* pair = ClipPairs() + ((color & 0xFF) >> 4) * 2;
    const int u = pair[0], v = pair[1];
    std::uint8_t remembered = 0;
    // DIV-0140: 1 after a button code that resolved (its `05 cc` next), 2 once
    // the drawn colour is taken (the glyph next); `icon` is what to draw.
    int button = 0;
    button_text::Icon icon;

    if (text[0] == 0) return text + 1;
    do {
        if (count == 0) break;
        const std::uint8_t c = text[0];
        bool glyph = c > 0x20;
        if (!glyph) {
            switch (c) {
            case 0x00:
                break;
            case 0x01:
                Text_PenY = static_cast<short>(Text_PenY + kCell);
                Text_PenX = Text_LineX;
                break;
            case 0x05: {
                const std::uint8_t* const control = text;
                ++text;
                remembered = static_cast<std::uint8_t>(color);
                color = (color & ~0xFFu) | text[0];
                clut = ClutOf(color);
                // DIV-0140: a button code's own colour stands as the text has
                // it (06 takes it back at once); the `05 cc` after it, or a
                // colour cue, takes the drawn button's colour instead.
                if (button == 1 || (button == 0 && ButtonText_Cue(control, icon))) {
                    color = (color & ~0xFFu) | icon.colour;
                    clut = ClutOf(color);
                    button = 2;
                } else if (button == 0 && ButtonText_Code(control, icon)) {
                    button = 1;
                }
                break;
            }
            case 0x06:
                color = (color & ~0xFFu) | remembered;
                clut = ClutOf(color);
                break;
            case 0x07:
                ++text;
                Text_DrawString(color, kRecordBytes, Records() + text[0] * kRecordBytes);
                break;
            case 0x20:
                Text_PenX = static_cast<short>(Text_PenX + TextAdvance_Of(text));  // DIV-0006
                break;
            default:
                glyph = true;
                break;
            }
        }
        if (glyph) {
            // DIV-0140: the drawn button's glyph in the fallback's place; the
            // walk still steps over the fallback's own bytes.
            const std::uint8_t* const first = button == 2 ? icon.glyph : text;
            button = 0;
            std::uint16_t index;
            if (first[0] & 0x80) {
                index = static_cast<std::uint16_t>(((first[0] & 0x7F) << 8) + first[1]);
            } else {
                index = static_cast<std::uint16_t>(first[0] - 0x26);
            }
            if (c & 0x80) ++text;
            if (index > text_draw::g_glyph_limit) GlyphTrap();
            std::uint32_t pa, pb;
            if (TextPair_Of(index, &pa, &pb)) {
                // DIV-0057: a pair code is its two glyphs, the second one the
                // first's advance on. It counts as two characters, but draws
                // whole even when one is left (the message box asks for 1).
                const auto a = static_cast<std::uint16_t>(pa), b = static_cast<std::uint16_t>(pb);
                if (a > text_draw::g_glyph_limit || b > text_draw::g_glyph_limit) GlyphTrap();
                std::memcpy(Gfx_PacketNext + 0x16, &a, sizeof a);
                text_draw::g_emit(static_cast<std::uint16_t>(Text_PenX), static_cast<std::uint16_t>(Text_PenY), kCell,
                       kCell - v, u, v, static_cast<int>(clut));
                std::memcpy(Gfx_PacketNext + 0x16, &b, sizeof b);
                text_draw::g_emit(static_cast<std::uint16_t>(Text_PenX + TextAdvance_OfGlyph(a)),
                       static_cast<std::uint16_t>(Text_PenY), kCell, kCell - v, u, v, static_cast<int>(clut));
                if (count > 1) --count;
            } else {
                std::memcpy(Gfx_PacketNext + 0x16, &index, sizeof index);
                text_draw::g_emit(static_cast<std::uint16_t>(Text_PenX), static_cast<std::uint16_t>(Text_PenY), kCell,
                       kCell - v, u, v, static_cast<int>(clut));
            }
            Text_PenX = static_cast<short>(Text_PenX + TextAdvance_Of(first));  // DIV-0006
        }
        ++text;
        --count;
    } while (text[0] != 0);
    return text + 1;
}

// original 0x516D50. One glyph: fills the POLY_FT4-shaped record at
// Gfx_PacketNext and hands it on. Its only caller is Text_DrawString, as
// (pen x, pen y, 12, 12 - v, u, v, clut).
//
// As the original has it: the texture window is a flat 12 units wide (0 and
// 12 in every u) and `u` is the row - what DIV-0010 named, in the config
// screen's words, "the texture extent fixed at 0xC whatever the quad"; the
// three shade bytes are 0x80; the v coordinates are u and (u - v + 12) as
// BYTES, so they wrap; the corners are (x, y + u), (x + w, y + u),
// (x, y + h + u), (x + w, y + h + u), each stored as a word; the primitive
// pointer is re-read from Gfx_PacketNext before every single store, which
// nothing between them can change; +0x16 (the glyph index) is the caller's.
//
// This is faithful - no divergence. DIV-0010 is the config screen's own
// stand-in for Text_DrawAt (src/game/config_text.cpp), not a change here.
extern "C" void __cdecl Text_EmitGlyph(int x, int y, int w, int h, int u, int v, int clut) {
    unsigned char* const p = Gfx_PacketNext;
    const auto row = static_cast<unsigned char>(u);
    const auto far_row = static_cast<unsigned char>(u - v + 12);
    const auto put = [](unsigned char* at, unsigned value) {
        const auto word = static_cast<std::uint16_t>(value);
        std::memcpy(at, &word, sizeof word);
    };
    put(p + 0x0E, static_cast<unsigned>(clut));
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    p[0x0C] = 0;
    p[0x0D] = row;
    p[0x14] = 0x0C;
    p[0x15] = row;
    p[0x1C] = 0;
    p[0x1D] = far_row;
    p[0x24] = 0x0C;
    p[0x25] = far_row;
    put(p + 0x08, static_cast<unsigned>(x));
    put(p + 0x0A, static_cast<unsigned>(y + u));
    put(p + 0x10, static_cast<unsigned>(x + w));
    put(p + 0x12, static_cast<unsigned>(y + u));
    put(p + 0x18, static_cast<unsigned>(x));
    put(p + 0x1A, static_cast<unsigned>(y + h + u));
    put(p + 0x20, static_cast<unsigned>(x + w));
    put(p + 0x22, static_cast<unsigned>(y + h + u));
    msgbox::g.set_code6c(p);
    msgbox::g.set_semitrans(p, 1);
    msgbox::g.commit(1, 0x28);
}

// original 0x516E70 (PSX 0x8014FD78, the catalogue's pairing): the small
// UI font - up to `count` characters of `text` at (x, y), each an 8 x 8
// quad through Text_EmitGlyph (x, y, 8, 8 - v, u, v, clut) with u and v from
// the byte pair 0x65F5A8[colour >> 4] and the CLUT 0x7800 | (colour & 0xF),
// the glyph index to word +0x16 of the primitive at Gfx_PacketNext first.
// 0x01 is a newline (x back, y + 9), 0x20 a blank 8 wide, a byte with bit 7
// the first of a two-byte code ((b0 & 0x7F) << 8 | b1), any other byte glyph
// b - 0x26. Returns one past the NUL that ended the text, or one past the
// next byte when the count ran out. This is the draw the shipped Chinese
// menus' small text goes through (docs/dialogue-localisation.md sections 4
// and 9, DIV-0016's 8 x 8 set).
//
// As the original has it: colour and count are the arguments' low bytes; x
// and y are whole ints, the pen x reset to the argument; a newline and a
// blank use up the count like a glyph; the text byte after each character
// and the primitive pointer are read afresh after the glyph's call; a text
// that starts with its NUL returns text + 1 whatever the count; there is no
// glyph bound here (Text_DrawString's 0xA00 test is not in this draw).
extern "C" const unsigned char* __cdecl Text_DrawSmall(int x, int y, unsigned colour, unsigned count, const unsigned char* text) {
    const unsigned c = colour & 0xFF;
    const unsigned char* const uv = mode_states::At(::bof3::Img(mode_states::at::kSmallUV) + (c >> 4) * 2);
    const unsigned u = uv[0], v = uv[1];
    const int clut = static_cast<int>(0x7800u | (c & 0xF));
    const unsigned char* p = text;
    unsigned char ch = p[0];
    if (ch == 0) return p + 1;
    int pen_x = x, pen_y = y;
    unsigned char left = static_cast<unsigned char>(count);
    for (;;) {
        if (left == 0) return p + 1;
        if (ch == 1) {
            pen_x = x;
            pen_y += 9;
        } else if (ch == 0x20) {
            pen_x += 8;
        } else {
            unsigned glyph;
            if ((ch & 0x80) != 0) {
                glyph = ((ch & 0x7Fu) << 8) + p[1];
                ++p;
            } else {
                glyph = (ch - 0x26u) & 0xFFFF;
                // DIVERGENCE DIV-0064: the status words under a language
                // overlay, from its 8 x 8 set.
                if (const unsigned small = Labels_SmallGlyph(p)) glyph = small;
            }
            rv::Store<unsigned>(rv::As<records::GpuGlyph>(Gfx_PacketNext)->glyph, glyph);
            mode_states::g.emit_glyph(pen_x, pen_y, 8, static_cast<int>(8 - v), static_cast<int>(u), static_cast<int>(v), clut);
            pen_x += 8;
        }
        ch = p[1];
        ++p;
        --left;
        if (ch == 0) return p + 1;
    }
}

// original 0x516F60 (PSX 0x8015002C): a string in the 12 x 12 font of page
// (0x2F), 21 cells to a row: u = (c % 21) * 12, v = (c / 21) * 12 as bytes, the
// CLUT word 0x7800 | (colour & 0x3F), semi-transparent. y is drawn one below
// the y given. A byte 0x0A is a newline: y + 12 and x back to the x given; 0x20
// and 0 draw nothing; every byte, drawn or not, moves the pen 12.
//
// As the original has it: after a newline the next character is drawn 12
// right of the x given, since the newline's own byte moves the pen too; the
// test for the end is on the byte AFTER the one just handled, so a string
// that starts with its NUL goes on to the byte after it; a byte of 0x80 or
// more is one cell like any other. x and y are drawn as 16-bit.
extern "C" void __cdecl Text_DrawFont12(int x, int y, int colour, const unsigned char* text) {
    std::int32_t pen_y = y + 1;
    menu_windows::g.draw_mode(Next(), 0, 0, 0x2F, 0);
    menu_windows::g.commit(1, 0xC);
    const unsigned clut = (colour & 0x3F) | 0x7800;
    std::uint32_t pen_x = static_cast<std::uint32_t>(x);
    unsigned char* p = Next();
    for (;;) {
        const unsigned char c = text[0];
        if (c == 0x0A) {
            pen_y += 0xC;
            pen_x = static_cast<std::uint32_t>(x);
        } else if (c != 0x20 && c != 0) {
            rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, clut);
            rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, 0xC);
            rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, 0xC);
            p[4] = p[5] = p[6] = 0x80;
            p[0x14] = static_cast<unsigned char>((c % 21) * 12);
            p[0x15] = static_cast<unsigned char>((c / 21) * 12);
            Put(p + 8, F(S16MenuWindows(pen_x)));
            Put(p + 0xC, F(S16MenuWindows(static_cast<std::uint32_t>(pen_y))));
            menu_windows::g.set_sprt(p);
            menu_windows::g.set_semi(p, 1);
            menu_windows::g.commit(1, 0x1C);
            p = Next();
        }
        const unsigned char after = text[1];
        pen_x += 0xC;
        ++text;
        if (after == 0) break;
    }
}

// original 0x517090 (PSX 0x801501C0): the same in the 8 x 8 font, SPRT_8
// (Gpu_SetSprt8), 32 cells to a row from 0x20: u = ((c - 0x20) % 32) * 8,
// v = ((c - 0x20) / 32) * 8, both as C divides (toward zero) and kept as
// bytes; y as given; newline y + 8; every byte moves the pen 8. The same
// quirks as Text_DrawFont12. Its the menus' numerals (every "%3d" here).
extern "C" void __cdecl Text_DrawFont8(int x, int y, int colour, const unsigned char* text) {
    std::int32_t pen_y = y;
    menu_windows::g.draw_mode(Next(), 0, 0, 0x2F, 0);
    menu_windows::g.commit(1, 0xC);
    const unsigned clut = (colour & 0x3F) | 0x7800;
    std::uint32_t pen_x = static_cast<std::uint32_t>(x);
    unsigned char* p = Next();
    for (;;) {
        const unsigned char c = text[0];
        if (c == 0x0A) {
            pen_y += 8;
            pen_x = static_cast<std::uint32_t>(x);
        } else if (c != 0x20 && c != 0) {
            rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, clut);
            p[4] = p[5] = p[6] = 0x80;
            const int cell = static_cast<int>(c) - 0x20;
            p[0x14] = static_cast<unsigned char>((cell % 32) * 8);
            p[0x15] = static_cast<unsigned char>((cell / 32) * 8);
            Put(p + 8, F(S16MenuWindows(pen_x)));
            Put(p + 0xC, F(S16MenuWindows(static_cast<std::uint32_t>(pen_y))));
            menu_windows::g.set_sprt8(p);
            menu_windows::g.set_semi(p, 1);
            menu_windows::g.commit(1, 0x18);
            p = Next();
        }
        const unsigned char after = text[1];
        pen_x += 8;
        ++text;
        if (after == 0) break;
    }
}

// original 0x5171A0 (PSX 0x8015030C): at most (n & 0xFF) bytes of src to dst,
// stopping at a NUL (not copied), then a NUL stored; byte by byte, forward.
// eax is where the NUL went (four callers tail-return it).
extern "C" char* __cdecl Str_CopyN(char* dst, const char* src, unsigned n) {
    const U limit = n & 0xFF;
    U d = Addr(dst), s = Addr(src);
    for (U k = 0; k < limit; ++k) {
        const unsigned char c = AtBattleMisc(s)[0];
        if (c == 0) break;
        AtBattleMisc(d)[0] = c;
        ++d;
        ++s;
    }
    AtBattleMisc(d)[0] = 0;
    return reinterpret_cast<char*>(AtBattleMisc(d));
}

void Text_Inject() {
    BOF3_INJECT(Text_DrawAt);
    BOF3_INJECT(Text_DrawString);
    BOF3_INJECT(Text_EmitGlyph);
    BOF3_INJECT(Text_DrawSmall);
    BOF3_INJECT(Text_DrawFont12);
    BOF3_INJECT(Text_DrawFont8);
    BOF3_INJECT(Str_CopyN);
}
