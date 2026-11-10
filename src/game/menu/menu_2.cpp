// Moved here by tools/move_funcs.py from battle_window_draw.cpp, menu_windows.cpp, rest_2b.cpp (docs/source-layout-round.md).
#include "game/menu/menu_2.h"

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/battle_window_draw.h"
#include "game/battle_window_draw_callees.h"
#include "game/cheats.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_verbs.h"
#include "game/menu_windows.h"
#include "game/menu_windows_callees.h"
#include "game/move_script_bytes.h"
#include "game/msgbox.h"
#include "game/rdata_consts.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2b_callees.h"
#include "game/scenario_harness.h"
#include "game/shoulder_glyphs.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

using namespace battle_window_draw;

using namespace menu_windows;

namespace {

using move_script::At;

using move_script::Word;

char* PrintBuf() { return reinterpret_cast<char*>(move_script::At(::bof3::Img(battle_window_draw::at::kPrintBuf.v))); }

const char* Format(std::uint32_t at) { return reinterpret_cast<const char*>(move_script::At(at)); }

}  // namespace

namespace menu_windows {

using move_script::At;

}  // namespace menu_windows

namespace {

using move_script::SetWord;

std::uint32_t Addr(const void* p) { return Low32(p); }

unsigned char* Next() { return Gfx_PacketNext; }

char* PrintBufMenuWindows() { return reinterpret_cast<char*>(menu_windows::At(::bof3::Img(menu_windows::at::kPrintBuf.v))); }

const char* FormatMenuWindows(std::uint32_t at) { return reinterpret_cast<const char*>(menu_windows::At(at)); }

std::int16_t S16MenuWindows(std::uint32_t v) { return static_cast<std::int16_t>(v); }

std::int32_t U16(std::uint32_t v) { return static_cast<std::int32_t>(v & 0xFFFF); }

// `fild dword` then `fstp dword`: an int rounded once to a float.
float F(std::int32_t v) { return static_cast<float>(v); }

// `fld dword`, `fadd` / `fsub` of a float constant, `fstp dword`: exact in
// the 53-bit precision the game runs x87 at, then rounded once.
float Add(float a, double b) { return static_cast<float>(static_cast<double>(a) + b); }

void Put(unsigned char* at, float f) { std::memcpy(at, &f, sizeof f); }

void Put32(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }

// `cdq; idiv` as the originals have it: a zero divisor faults here as it does
// there (Menu_DrawScrollBar with a total of 0, docs/menu-windows.md).
std::int32_t IDiv(std::int32_t num, std::int32_t den) {
    std::int32_t q;
    __asm__ volatile("cltd\n\tidivl %2" : "=a"(q) : "a"(num), "r"(den) : "edx", "cc");
    return q;
}

// The 8-byte RECT some draws put in the packet pool for a draw mode's texture
// window: Gfx_PacketNext moves past it, no commit.
unsigned char* PoolRect(unsigned x, unsigned y, unsigned w, unsigned h) {
    unsigned char* const r = Next();
    Gfx_PacketNext = r + 8;
    SetWord(r, x);
    SetWord(r + 2, y);
    SetWord(r + 4, w);
    SetWord(r + 6, h);
    return r;
}

// The draw mode with the page GetTPage(0, 0, page_x, 0x100) that the piece
// draws open with.
void PiecePage(int page_x) {
    const unsigned tpage = menu_windows::g.get_tpage(0, 0, page_x, 0x100);
    menu_windows::g.draw_mode(Next(), 0, 0, tpage & 0xFFFF, 0);
    menu_windows::g.commit(1, 0xC);
}

// A SPRT as Menu_DrawPiece writes its one, of the page rectangle (u, v, w,
// h) given here rather than a piece's (DIV-0138's steps).
void PieceSprite(int x, int y, unsigned u, unsigned v, unsigned w, unsigned h, unsigned char f) {
    unsigned char* const p = Next();
    menu_windows::g.set_sprt(p);
    menu_windows::g.set_semi(p, 0);
    Put(p + 8, F(S16MenuWindows(static_cast<std::uint32_t>(x))));
    Put(p + 0xC, F(S16MenuWindows(static_cast<std::uint32_t>(y))));
    p[0x14] = static_cast<unsigned char>(u);
    p[0x15] = static_cast<unsigned char>(v);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, w);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, h);
    const unsigned clut = (f & 2) ? menu_windows::g.get_clut(0x80, 0x1E2) : menu_windows::g.get_clut(0xB0, 0x1E1);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, clut);
    p[4] = p[5] = p[6] = 0x80;
    menu_windows::g.commit(1, 0x1C);
}

// DIVERGENCE (DIV-0138, docs/button-shoulder-corner.md): a piece of the
// UI page that carries the L1 or R1 label, drawn with the label as σ has it
// (shoulder_glyphs.h). False: draw it as the original does (the identity on
// a page with the L1 / R1 art).
bool ShoulderPiece(int x, int y, unsigned id, unsigned char f, std::uint16_t slot) {
    auto rect = [](unsigned piece) {
        const unsigned char* const r = menu_windows::g.piece_rect(piece, 1);
        return shoulder_glyphs::Rect{r[0], r[1], r[2], r[3]};
    };
    const std::uint16_t shown = bof3x::input::sigma::Sigma(slot);
    const shoulder_glyphs::Plan plan = shoulder_glyphs::Make(id, x, y, rect(id), rect(0x15), rect(0x16), rect(0x18),
                                                             shown, shoulder_glyphs::PageArt(), false);
    if (plan.original) return false;
    for (int i = 0; i < plan.n; ++i) {
        const shoulder_glyphs::Step& s = plan.s[i];
        if (s.kind == shoulder_glyphs::Step::kSprite) {
            PieceSprite(s.x, s.y, s.u, s.v, s.w, s.h, f);
        } else if (s.kind == shoulder_glyphs::Step::kGlyph) {
            const shoulder_glyphs::Code c = shoulder_glyphs::DrawCode(s.button);
            Text_DrawSmall(s.x, s.y, static_cast<unsigned>(c.colour), 1, c.text);
        } else {
            // The text's primitives name a glyph where a SPRT's page would
            // be; the pieces after this one go on with the page every caller
            // of a flag-1 piece opened with (Menu_DrawPieces' (0x340, 0x100)).
            PiecePage(0x340);
        }
    }
    return true;
}

using U = std::uint32_t;

template <class T> void PutRest2b(unsigned char* p, unsigned offset, T v) {
    if constexpr (std::is_pointer_v<T>) {   // the field is a 32-bit word on every width
        const std::uint32_t w = Low32(v);
        std::memcpy(p + offset, &w, sizeof w);
    } else {
        std::memcpy(p + offset, &v, sizeof v);
    }
}

}  // namespace

// original 0x57CEF0: a horizontal LINE_F2 at the packet cursor from (x, y) to
// (x + w, y) - x and y the arguments' low words signed, w the third's low word
// unsigned, the floats built by fild - grey 0x28 when the fourth argument's
// byte is non-zero, else 0x8C; Gfx_CommitPrim(1, 0x20). The original also
// writes into its own argument slots (scratch for fild), which its two callers
// (community code) pop unread.
extern "C" void __cdecl Menu_DrawGreyHLine(int x, int y, unsigned w, unsigned bright) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    const unsigned char shade = (bright & 0xFF) != 0 ? 0x28 : 0x8C;
    p[4] = shade;
    p[5] = shade;
    p[6] = shade;
    const int x0 = static_cast<short>(x);
    const int y0 = static_cast<short>(y);
    const int width = static_cast<int>(w & 0xFFFF);
    PutRest2b<float>(p, 8, static_cast<float>(x0));
    PutRest2b<float>(p, 0x14, static_cast<float>(width + x0));
    PutRest2b<float>(p, 0x18, static_cast<float>(y0));
    PutRest2b<float>(p, 0xC, static_cast<float>(y0));
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x57CF60 (PSX 0x801AF3F0): the menu box - a window of x, y, w, h (16
// bits each) in POLY_FT4s of the 16 x 16 tile at (0, 0xF0), repeated through
// a draw mode whose texture window is that tile (a RECT in the packet pool),
// then a draw mode with the window back to the whole page.
//
// Three quads - a left end 4 wide, a middle, a right end 4 wide - or four when
// w is 0x100 or more (the middle split at w / 2). The shade is 0x48 for kind
// (flags & 0xF) 1, else 0xAC; kind 2 is semi-transparent with the CLUT half a
// row on. Flag 0x80 moves the left end's top vertex down 4, 0x20 its bottom
// up 4; 0x40 and 0x10 the right end's likewise. The CLUT is
// GetClut(colour * 32 (+ 16), 0x1E1).
//
// Each quad after the first is a byte copy of the one before it, then
// changed - so it carries what the pool's commit wrote into the first one.
// And the quads are placed at 0x48-byte steps from the first, not where
// Gfx_PacketNext is after each commit (the same when the pool has room).
//
// As the original has it: the middle quads' bottom v is h's low byte where
// the ends' is (y + h)'s - the texture's rows are out of phase with the ends
// when y is not a multiple of 16 (D39, docs/known-defects.md).
//
// DIVERGENCE DIV-0165 (known-defects D348): the original takes x and y as
// unsigned words (`and esi, 0xffff` / `and eax, 0xffff` at 0x57D009 /
// 0x57D00F before the `fild`), so a box slid past the left or top edge is
// placed 65,536 pixels away and its fill vanishes while Menu_DrawPiece's
// frame (`movsx`) still draws. The PSX (0x801AF3F0) stores into signed
// vertex halfwords and the GPU clips. With g_signed_xy (1 through
// PatchBytes("MenuSignedXY") in MenuWindows_Inject, after the self-test) x
// and y are sign-extended words; w and h stay unsigned, as on both.
namespace menu_windows { std::uint8_t g_signed_xy = 0; }

extern "C" void __cdecl Menu_DrawBox(int x, int y, int w, int h, int flags, int colour) {
    const auto kind = static_cast<unsigned char>(flags & 0xF);
    const unsigned char shade = kind == 1 ? 0x48 : 0xAC;
    const unsigned char* const window = PoolRect(0, 0xF0, 0x10, 0x10);
    menu_windows::g.draw_mode(Next(), 0, 1, 0xF, Addr(window));
    menu_windows::g.commit(1, 0xC);
    unsigned char* p = Next();
    menu_windows::g.set_poly_ft4(p);
    std::uint32_t clut_x = static_cast<std::uint32_t>(colour & 0xFF) << 5;
    if (kind == 2) {
        menu_windows::g.set_semi(p, 1);
        clut_x += 0x10;
    }
    p[6] = p[5] = p[4] = shade;
    const std::int32_t xs = menu_windows::g_signed_xy ? S16MenuWindows(static_cast<std::uint32_t>(x)) : U16(static_cast<std::uint32_t>(x));
    const std::int32_t ys = menu_windows::g_signed_xy ? S16MenuWindows(static_cast<std::uint32_t>(y)) : U16(static_cast<std::uint32_t>(y));
    const auto xb = static_cast<unsigned char>(x), yb = static_cast<unsigned char>(y);
    p[0x14] = xb;
    p[0x15] = yb;
    p[0x25] = yb;
    p[0x34] = xb;
    const float fx = F(xs), fy = F(ys), fx4 = F(xs + 4);
    Put(p + 8, fx);
    Put(p + 0xC, fy);
    Put(p + 0x1C, fy);
    Put(p + 0x18, fx4);
    Put(p + 0x28, fx);
    const auto yhb = static_cast<unsigned char>(h + yb);
    p[0x35] = yhb;
    p[0x45] = yhb;
    const float fyh = F(U16(static_cast<std::uint32_t>(h)) + ys);
    Put(p + 0x2C, fyh);
    Put(p + 0x38, fx4);
    Put(p + 0x3C, fyh);
    p[0x24] = static_cast<unsigned char>(xb + 4);
    p[0x44] = static_cast<unsigned char>(xb + 4);
    const auto fl = static_cast<unsigned char>(flags);
    if (fl & 0x80) {
        Put(p + 0xC, Add(fy, 4.0));
        p[0x15] = static_cast<unsigned char>(yb + 4);
    }
    if (fl & 0x20) {
        Put(p + 0x2C, Add(fyh, -4.0));
        p[0x35] = static_cast<unsigned char>(yhb - 4);
    }
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, menu_windows::g.get_clut(static_cast<int>(clut_x & 0xFFFF), 0x1E1));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, 0xF);
    menu_windows::g.commit(1, 0x48);

    const auto wv = static_cast<std::uint32_t>(w);
    const std::int32_t ws = U16(wv);
    const auto hb = static_cast<unsigned char>(h);
    p += 0x48;
    std::memcpy(p, p - 0x48, 0x48);
    Put(p + 0xC, fy);
    Put(p + 8, fx4);
    if (ws < 0x100) {
        const float fr = F(ws + xs - 4);
        const auto u = static_cast<unsigned char>(static_cast<unsigned char>(wv) - 8);
        Put(p + 0x1C, fy);
        Put(p + 0x28, fx4);
        Put(p + 0x2C, fyh);
        Put(p + 0x18, fr);
        Put(p + 0x38, fr);
        Put(p + 0x3C, fyh);
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x24] = u;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x35] = hb;
        p[0x44] = u;
        p[0x45] = hb;
        menu_windows::g.commit(1, 0x48);
    } else {
        const std::int32_t half = ws >> 1;
        const float fm = F(half + xs);
        const auto u = static_cast<unsigned char>(static_cast<unsigned char>(wv >> 1) - 4);
        Put(p + 0x28, fx4);
        Put(p + 0x1C, fy);
        Put(p + 0x18, fm);
        Put(p + 0x38, fm);
        Put(p + 0x2C, fyh);
        Put(p + 0x3C, fyh);
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x24] = u;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x35] = hb;
        p[0x44] = u;
        p[0x45] = hb;
        menu_windows::g.commit(1, 0x48);
        p += 0x48;
        std::memcpy(p, p - 0x48, 0x48);
        const float fr = F(xs + half * 2 - 4);
        Put(p + 8, fm);
        Put(p + 0xC, fy);
        Put(p + 0x1C, fy);
        Put(p + 0x28, fm);
        Put(p + 0x18, fr);
        Put(p + 0x38, fr);
        Put(p + 0x2C, fyh);
        Put(p + 0x3C, fyh);
        menu_windows::g.commit(1, 0x48);
    }

    p += 0x48;
    const std::int32_t xe = ws + xs;
    std::memcpy(p, p - 0x48, 0x48);
    const float fe4 = F(xe - 4), fe = F(xe);
    Put(p + 8, fe4);
    Put(p + 0x18, fe);
    Put(p + 0x28, fe4);
    Put(p + 0x2C, fyh);
    Put(p + 0x1C, fy);
    Put(p + 0xC, fy);
    Put(p + 0x38, fe);
    Put(p + 0x3C, fyh);
    const auto ue = static_cast<unsigned char>(static_cast<unsigned char>(wv) + xb);
    p[0x24] = ue;
    p[0x44] = ue;
    p[0x14] = static_cast<unsigned char>(ue - 4);
    p[0x34] = static_cast<unsigned char>(ue - 4);
    p[0x15] = yb;
    p[0x25] = yb;
    p[0x35] = yhb;
    p[0x45] = yhb;
    if (fl & 0x40) {
        float v;
        std::memcpy(&v, p + 0x1C, sizeof v);
        Put(p + 0x1C, Add(v, 4.0));
        p[0x25] = static_cast<unsigned char>(p[0x25] + 4);
    }
    if (fl & 0x10) {
        float v;
        std::memcpy(&v, p + 0x3C, sizeof v);
        Put(p + 0x3C, Add(v, -4.0));
        p[0x45] = static_cast<unsigned char>(p[0x45] - 4);
    }
    menu_windows::g.commit(1, 0x48);

    const unsigned char* const whole = PoolRect(0, 0, 0x100, 0x100);
    menu_windows::g.draw_mode(Next(), 0, 0, 0xF, Addr(whole));
    menu_windows::g.commit(1, 0xC);
}

// original 0x57D360 (PSX 0x801AFA80): an item kind's 8 x 8 icon - SPRT_8 at
// (x, y) of cell (icon + 7) * 8 (a byte) in row 0xF8, CLUT word 0x780E for
// icons 1, 3 and 15 and 0x780C for the rest, shade 0x40 when dim, else 0x80;
// after a draw mode of page 0xF.
//
// DIVERGENCE DIV-0165 (known-defects D348): x and y unsigned words in the
// original (`and 0xffff` at 0x57D3A0 / 0x57D3BB), signed halfwords on the
// PSX (0x801AFA80); sign-extended with g_signed_xy, as Menu_DrawBox.
extern "C" void __cdecl Menu_DrawIcon8(int x, int y, int icon, int dim) {
    menu_windows::g.draw_mode(Next(), 0, 0, 0xF, 0);
    menu_windows::g.commit(1, 0xC);
    unsigned char* const p = Next();
    menu_windows::g.set_sprt8(p);
    const unsigned char shade = (dim & 0xFF) ? 0x40 : 0x80;
    p[4] = p[5] = p[6] = shade;
    Put(p + 8, F(menu_windows::g_signed_xy ? S16MenuWindows(static_cast<std::uint32_t>(x)) : U16(static_cast<std::uint32_t>(x))));
    Put(p + 0xC, F(menu_windows::g_signed_xy ? S16MenuWindows(static_cast<std::uint32_t>(y)) : U16(static_cast<std::uint32_t>(y))));
    p[0x15] = 0xF8;
    p[0x14] = static_cast<unsigned char>((icon + 7) << 3);
    const unsigned i = static_cast<unsigned>(icon) & 0xFF;
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, (i == 1 || i == 3 || i == 15) ? 0x780E : 0x780C);
    menu_windows::g.commit(1, 0x18);
}

// original 0x57D420: a rectangle's outline in four Menu_DrawLines of the
// window colour - entry 0 of CLUT row (s8) 0x903A5A in the shadow 0x80B7A8 -
// the left and top edges under abr 2 with flag bit 0 (else 1), the bottom and
// right under the other: a bevel.
extern "C" void __cdecl Menu_DrawOutline(int x, int y, int w, int h, int flags) {
    const bool bit0 = (flags & 1) != 0;
    const unsigned char abr_a = bit0 ? 2 : 1, abr_b = bit0 ? 1 : 2;
    const int row = Config_WindowColour;
    const unsigned char* const entry = At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x228 + static_cast<std::uint32_t>(row * 64));
    const unsigned c = Word(entry);
    const auto r = static_cast<unsigned char>((entry[0] & 0x1F) << 3);
    const auto gr = static_cast<unsigned char>(((c >> 5) & 0x1F) << 3);
    const auto b = static_cast<unsigned char>(((c >> 10) & 0x1F) << 3);
    const int yh = y + h, xw = x + w;
    menu_windows::g.line(x, y, x, yh - 1, r, gr, b, abr_a);
    menu_windows::g.line(x, yh, xw - 1, yh, r, gr, b, abr_b);
    menu_windows::g.line(xw, yh, xw, y + 1, r, gr, b, abr_b);
    menu_windows::g.line(xw, y, x + 1, y, r, gr, b, abr_a);
}

// original 0x57D520 (PSX 0x801AFD54): Menu_DrawOutline's sibling with notched
// corners - the window colour (entry 0 of CLUT row (s8) 0x903A5A in the shadow
// 0x80B7A8, 5:5:5 shifted up by 3) in eight Menu_DrawLines at most, abr 2 / 1
// by flag bit 0 as Menu_DrawOutline's. Flag bits 4..7 make the notch words
// n0..n3 3 (else 0): the left edge from y + n0 to y + h - 1 - n2, the bottom
// from x + n2 to x + w - 1 - n3, the right from y + h - n3 up to y + n1 + 1,
// the top from x + w - n1 to x + n0 + 1; a corner's diagonal is drawn when
// the word BEFORE its own is set (bottom-left n2 by n0, bottom-right n3 by
// n1, top-right n1 by n2, top-left n0 by n3). The original builds the
// coordinates from dwords spanning two notch words (n0 | n1 << 16, ...) and
// Menu_DrawLine reads their low words: the same lines.
extern "C" void __cdecl Menu_DrawOutlineNotched(int x, int y, int w, int h, int flags) {
    const int row = Config_WindowColour;
    const unsigned char* const entry = At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x228 + static_cast<U>(row * 64));
    const U c = Word(entry);
    const auto r = static_cast<unsigned char>((entry[0] & 0x1F) << 3);
    const auto gr = static_cast<unsigned char>(((c >> 5) & 0x1F) << 3);
    const auto b = static_cast<unsigned char>(((c >> 10) & 0x1F) << 3);
    const bool bit0 = (flags & 1) != 0;
    const int abr1 = bit0 ? 2 : 1, abr2 = bit0 ? 1 : 2;
    const unsigned n = (static_cast<unsigned>(flags) & 0xFF) >> 4;
    const int n0 = n & 1 ? 3 : 0, n1 = n & 2 ? 3 : 0, n2 = n & 4 ? 3 : 0, n3 = n & 8 ? 3 : 0;
    auto line = [&](int x0, int y0, int x1, int y1, int abr) { SH_CALL(Menu_DrawLine)(x0, y0, x1, y1, r, gr, b, abr); };
    const int left_end = y + h - 1 - n2;
    line(x, y + n0, x, left_end, abr1);
    if (n0 != 0) line(x, left_end, x + n2, y + h, abr2);
    const int bottom_end = x + w - 1 - n3;
    line(x + n2, y + h, bottom_end, y + h, abr2);
    if (n1 != 0) line(bottom_end, y + h, x + w, y + h - n3, abr2);
    line(x + w, y + h - n3, x + w, y + n1 + 1, abr2);
    if (n2 != 0) line(x + w, y + n1 + 1, x + w - n1, y, abr1);
    line(x + w - n1, y, x + n0 + 1, y, abr1);
    if (n3 != 0) line(x + n0 + 1, y, x, y + n0, abr1);
}

// original 0x57D760: a LINE_F2 from (x0, y0) to (x1, y1), 16-bit, colour (r, g,
// b), semi-transparent under abr & 3 - after a draw mode of page
// ((abr & 3) << 5) | 0x1E.
extern "C" void __cdecl Menu_DrawLine(int x0, int y0, int x1, int y1, int r, int gr, int b, int abr) {
    menu_windows::g.draw_mode(Next(), 0, 0, (static_cast<unsigned>(abr & 3) << 5) | 0x1E, 0);
    menu_windows::g.commit(1, 0xC);
    unsigned char* const p = Next();
    menu_windows::g.set_line_f2(p);
    Put(p + 8, F(S16MenuWindows(static_cast<std::uint32_t>(x0))));
    Put(p + 0xC, F(S16MenuWindows(static_cast<std::uint32_t>(y0))));
    p[4] = static_cast<unsigned char>(r);
    Put(p + 0x14, F(S16MenuWindows(static_cast<std::uint32_t>(x1))));
    p[5] = static_cast<unsigned char>(gr);
    p[6] = static_cast<unsigned char>(b);
    Put(p + 0x18, F(S16MenuWindows(static_cast<std::uint32_t>(y1))));
    menu_windows::g.set_semi(p, 1);
    menu_windows::g.commit(1, 0x20);
}

// original 0x57D800 (PSX 0x801B020C): the characters in text to its NUL - a byte with bit 7 and
// the byte after it are one - looking at no more than 16 bytes (the count of
// bytes may step from 15 to 17 over a two-byte character).
extern "C" unsigned char __cdecl Text_CharCount(const unsigned char* text) {
    unsigned char n = 0, bytes = 0;
    while (text[0]) {
        ++n;
        if (text[0] & 0x80) {
            if (TextPair_At(text)) ++n;   // DIV-0057: a pair is two characters wide
            text += 2;
            ++bytes;
        } else {
            ++text;
        }
        ++bytes;
        if (bytes >= 0x10) break;
    }
    return n;
}

// original 0x57D830: piece id's rectangle (u, v, w, h), from 0x663B94, or from
// 0x663C8C when the flag byte is non-zero. Both read as bytes.
extern "C" const unsigned char* __cdecl Menu_PieceRect(unsigned id, unsigned flags) {
    return At(((flags & 0xFF) ? ::bof3::Img(menu_windows::at::kPieceRectsAlt.v) : ::bof3::Img(menu_windows::at::kPieceRects.v)) + (id & 0xFF) * 4);
}

extern "C" void __cdecl Menu_DrawPiece(int x, int y, unsigned id, unsigned flags) {
    const auto f = static_cast<unsigned char>(flags);
    if ((f & 1) && shoulder_glyphs::On()) {
        if (const std::uint16_t slot = shoulder_glyphs::SlotOf(id)) {
            if (ShoulderPiece(x, y, id, f, slot)) return;
        }
    }
    const unsigned char* const rect = menu_windows::g.piece_rect(id, f & 1u);
    unsigned char* const p = Next();
    menu_windows::g.set_sprt(p);
    menu_windows::g.set_semi(p, 0);
    Put(p + 8, F(S16MenuWindows(static_cast<std::uint32_t>(x))));
    Put(p + 0xC, F(S16MenuWindows(static_cast<std::uint32_t>(y))));
    p[0x14] = rect[0];
    p[0x15] = rect[1];
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, rect[2]);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, rect[3]);
    const unsigned clut = (f & 2) ? menu_windows::g.get_clut(0x80, 0x1E2) : menu_windows::g.get_clut(0xB0, 0x1E1);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, clut);
    p[4] = p[5] = p[6] = 0x80;
    menu_windows::g.commit(1, 0x1C);
}

// original 0x57D910 (PSX 0x801B0390): a list of pieces - 3-byte records (s8
// column, s8 row, id) in 8-unit cells from (x, y), to an id of 0xFF - after the
// draw mode of page (0x380, 0x100), or (0x340, 0x100) with flag bit 0. Each
// piece gets the flags as they came.
extern "C" void __cdecl Menu_DrawPieces(int x, int y, const unsigned char* list, int flags) {
    PiecePage((flags & 1) ? 0x340 : 0x380);
    unsigned char id = list[2];
    if (id == 0xFF) return;
    do {
        menu_windows::g.piece(x + static_cast<signed char>(list[0]) * 8, y + static_cast<signed char>(list[1]) * 8, id,
                static_cast<unsigned>(flags));
        id = list[5];
        list += 3;
    } while (id != 0xFF);
}

// original 0x57D9A0 (PSX 0x801B0494): whether a list of mode shows item as
// usable, all four arguments bytes. Item 0: no. Mode 1 (the field menu's
// use): a consumable (category 0) whose flags byte has bit 0, and the Faerie
// Tiara (0x57) only where AreaMap_ByteAt at the leader's (x, z) is 0xAE; mode
// 2: flags bit 1; mode 3: not flags bit 3; mode 4: member's bit in the
// category's equip mask; any other mode: yes. (item-use.md calls it the
// field menu's gate.)
extern "C" unsigned char __cdecl Item_CanUse(unsigned mode, unsigned member, unsigned category, unsigned item) {
    const auto it = static_cast<unsigned char>(item);
    if (it == 0) return 0;
    switch (mode & 0xFF) {
    case 1:
        if (category & 0xFF) return 0;
        if (!(At(::bof3::Img(menu_windows::at::kItemFlags.v) + it * 22u)[0] & 1)) return 0;
        if (it != 0x57) return 1;
        return menu_windows::g.area_byte(ObjTrio[0].x_whole, ObjTrio[0].z_whole) == 0xAE ? 1 : 0;
    case 2: return (menu_windows::g.item_flags(category, item) & 2) ? 1 : 0;
    case 3: return (menu_windows::g.item_flags(category, item) & 8) ? 0 : 1;
    case 4: {
        const unsigned mask = menu_windows::g.equip_mask(category, item);
        return ((1u << (member & 31)) & mask & 0xFF) ? 1 : 0;
    }
    default: return 1;
    }
}

// original 0x57DA70 (GAME.EMI 0x801B05B0): whether member `member` may use
// ability `id` in list mode `mode`. Id 0: no. Mode 1 (the field's lists):
// the member's record through the party list 0x904062 and 0x66972C; no when
// its AP (+0x1A) is below Skill_ApCost or the ability's b0 lacks bit 0. Mode
// 2 (battle): the working record 0x802DC0 + member * 0x14C; no when its AP
// is below the cost, b0 lacks bit 1, or the member's +0x10 has bit 4 while
// the ability's u16_4 has 0x400; then five abilities have tests of their own
// - 0x14 only while +0x1E is below 5, 0x15 only while the four bytes at
// 0x904660 are all 0, 0x3E the same for 0x90465C, 0x8C only at 0 AP, 0x97
// not while 0x904AAA is 0x25. Any other mode: yes.
//
// As the original has it: in battle the AP is read before the cost call and
// the rest after it; in mode 1 the record is pushed as the member dword with
// its low byte replaced (the argument's own stack slot), and the AP is read
// after the call. The PSX tests the four bytes as one word each.
extern "C" unsigned char __cdecl Skill_CanUse(unsigned mode, unsigned member, unsigned id) {
    const unsigned ability = id & 0xFF;
    if (ability == 0) return 0;
    const unsigned char* const rec = At(::bof3::Img(battle_window_draw::at::kAbilities.v) + ability * battle_window_draw::at::kAbilityStride);
    const unsigned m = mode & 0xFF;
    if (m == 1) {
        const unsigned character = At(::bof3::Img(Cond_Flags_addr) + 0xd2 + (member & 0xFF))[0];
        const unsigned record = At(::bof3::Img(Character_IndexById_addr) + character)[0];
        const unsigned cost = battle_window_draw::g.ap_cost((member & 0xFFFFFF00u) | record, id, 0) & 0xFF;
        if (Word(At(::bof3::Img(CharacterRecords_addr) + record * battle_window_draw::at::kCharStride + 0x1A)) < cost) return 0;
        return (rec[0x10] & 1) ? 1 : 0;
    }
    if (m != 2) return 1;
    const unsigned char* const w = At(::bof3::Img(battle_window_draw::at::kWorking.v) + (member & 0xFF) * battle_window_draw::at::kWorkingStride);
    const unsigned ap = Word(w + 0x1A);
    const unsigned cost = battle_window_draw::g.ap_cost(member, id, 1) & 0xFF;
    if (ap < cost) return 0;
    if ((rec[0x10] & 2) == 0) return 0;
    if ((w[0x10] & 0x10) != 0 && (rec[0x15] & 4) != 0) return 0;
    switch (ability) {
    case 0x14: return w[0x1E] < 5 ? 1 : 0;
    case 0x15: {
        const unsigned char* const f = At(::bof3::Img(Rest3c_Byte660_addr));
        return (f[0] | f[1] | f[2] | f[3]) == 0 ? 1 : 0;
    }
    case 0x3E: {
        const unsigned char* const f = At(::bof3::Img(ModeFlow_CountdownA_addr));
        return (f[0] | f[1] | f[2] | f[3]) == 0 ? 1 : 0;
    }
    case 0x8C: return ap == 0 ? 1 : 0;
    case 0x97: return EventBattle_Id == 0x25 ? 0 : 1;
    default: return 1;
    }
}

// original 0x57DBF0: one row of an item list - the kind's icon (0x591720 into
// 0x663D60) at (x, y + 2), dim as given; the name (0x591680) at (x + 10, y) in
// colour; and "*%2d" of count at (x + 0x6D, y + 2) when count (a byte) is 2
// or more.
extern "C" void __cdecl Menu_DrawItemRow(int x, int y, int colour, unsigned category, unsigned id, unsigned count,
                                         int dim) {
    const unsigned kind = menu_windows::g.item_kind(category, id) & 0xFF;
    menu_windows::g.icon8(x, y + 2, At(::bof3::Img(menu_windows::at::kRowIcons.v))[kind], dim);
    const unsigned char* const name = menu_windows::g.item_name(category, id);
    const unsigned n = menu_windows::g.char_count(name);
    menu_windows::g.text_draw_at(x + 0xA, y, colour, static_cast<int>(n), name);
    const auto c = static_cast<unsigned char>(count);
    if (c > 1) {
        menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmtCount.v)), static_cast<unsigned>(c));
        menu_windows::g.font8(x + 0x6D, y + 2, colour, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
    }
}

// original 0x57DC90 (GAME.EMI 0x801B0928): one row of the skill list - the
// kind's 8 x 8 icon (table 0x663D70) at (x, y + 2), the name at (x + 10, y)
// for Text_CharCount characters, "%2d" of the cost's low byte in the 8 px font
// at (x + 0x6D, y + 2). The battle's skill list 0x59D200 hands it the ability
// record itself as the name (its 16-byte GBK name field), Skill_FlagIndex as
// the kind and Skill_ApCost as the cost.
extern "C" void __cdecl Menu_DrawSkillRow(int x, int y, int colour, unsigned kind, const unsigned char* name,
                                          unsigned cost, int dim) {
    const unsigned char icon = At(::bof3::Img(battle_window_draw::at::kSkillIcons.v) + (kind & 0xFF))[0];
    battle_window_draw::g.icon8(x, y + 2, icon, dim);
    const unsigned char n = battle_window_draw::g.char_count(name);
    battle_window_draw::g.text_draw_at(x + 10, y, colour, n, name);
    battle_window_draw::g.sprintf_(PrintBuf(), Format(::bof3::Img(Boss26Fx_CountFormat_addr)), cost & 0xFF);
    battle_window_draw::g.font8(x + 0x6D, y + 2, colour, reinterpret_cast<const unsigned char*>(PrintBuf()));
}

// original 0x57DD10 (PSX 0x801B0A14): a list's scroll bar at (x, y), 16-bit -
// for each of the list's `total` entries that is not 0, a red TILE 3 wide at
// x + 4 over its share of the bar's height; then the thumb, a POLY_FT4 8 wide
// of the piece (0x58..0x60, 0x98..0x9F) on page (0x340, 0x100), from
// (total + 2 top height) / (2 total) down for rows * height / total. All five
// counts bytes, the divisions signed. As the original has it: a total of 0
// divides by zero.
extern "C" void __cdecl Menu_DrawScrollBar(const unsigned char* items, unsigned top, int x, int y, unsigned rows,
                                           unsigned total, unsigned height) {
    const std::int32_t t = static_cast<std::int32_t>(total & 0xFF), hgt = static_cast<std::int32_t>(height & 0xFF);
    const std::int32_t sx = S16MenuWindows(static_cast<std::uint32_t>(x)), sy = S16MenuWindows(static_cast<std::uint32_t>(y));
    for (std::int32_t i = 1; i <= t; ++i) {
        if (items[i - 1] == 0) continue;
        unsigned char* const p = Next();
        menu_windows::g.set_tile(p);
        Put(p + 8, F(sx + 4));
        const std::int32_t a = IDiv((i - 1) * hgt, t) + sy + 3;
        Put32(p + 0x14, 0x40400000u);
        p[4] = 0x80;
        p[5] = 0;
        p[6] = 0;
        const std::int32_t b = IDiv(i * hgt, t) + sy + 3;
        Put(p + 0xC, F(a));
        Put(p + 0x18, static_cast<float>(static_cast<double>(b) - static_cast<double>(a)));
        menu_windows::g.commit(1, 0x1C);
    }
    unsigned char* const p = Next();
    menu_windows::g.set_poly_ft4(p);
    const std::int32_t thumb = IDiv(t + 2 * static_cast<std::int32_t>(top & 0xFF) * hgt, 2 * t) & 0xFF;
    const float ftop = F(thumb + sy + 3), fx8 = F(sx + 8);
    Put(p + 8, F(sx));
    Put(p + 0xC, ftop);
    Put(p + 0x18, fx8);
    Put(p + 0x1C, ftop);
    Put(p + 0x28, F(sx));
    const std::int32_t length = IDiv(static_cast<std::int32_t>(rows & 0xFF) * hgt, t);
    Put(p + 0x38, fx8);
    p[0x14] = 0x58;
    p[0x24] = 0x60;
    p[0x34] = 0x58;
    p[0x44] = 0x60;
    const float fbottom = F(length + thumb + sy + 3);
    p[0x15] = 0x98;
    p[0x25] = 0x98;
    Put(p + 0x2C, fbottom);
    Put(p + 0x3C, fbottom);
    p[0x35] = 0x9F;
    p[0x45] = 0x9F;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, menu_windows::g.get_tpage(0, 0, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, menu_windows::g.get_clut(0xB0, 0x1E1));
    p[4] = p[5] = p[6] = 0x80;
    menu_windows::g.commit(1, 0x48);
}

// original 0x57DF00: a list's scroll step, on its state byte (high nibble the
// direction, low the phase). 0x1n: down - the top index up one at phase 0, the
// offset -((n + 4) % 12) with moving 1 and the answer the top less one, or the
// scroll ended (all 0, the top); 0xFn: up - the top down one and phase 8 at
// phase 0, else the offset -(n - 4) (a byte) with moving 1, or ended at phase
// 4; any other state: offset and moving 0, the state kept. Answers the top
// index as read after the stores.
extern "C" unsigned char __cdecl Menu_ListScroll(unsigned char* top, unsigned char* offset, unsigned char* moving,
                                                 unsigned char* state) {
    const unsigned char s = *state;
    const auto hi = static_cast<unsigned char>(s >> 4), lo = static_cast<unsigned char>(s & 0xF);
    if (hi == 1) {
        if (lo == 0) *top = static_cast<unsigned char>(*top + 1);
        const auto r = static_cast<unsigned char>((lo + 4) % 12);
        if (r == 0) {
            *offset = 0;
            *moving = 0;
            const unsigned char v = *top;
            *state = 0;
            return v;
        }
        *offset = static_cast<unsigned char>(-r);
        *moving = 1;
        const auto v = static_cast<unsigned char>(*top - 1);
        *state = static_cast<unsigned char>(0x10 + r);
        return v;
    }
    if (hi == 0xF) {
        unsigned char d;
        if (lo == 0) {
            *top = static_cast<unsigned char>(*top - 1);
            d = 8;
        } else {
            d = static_cast<unsigned char>(lo - 4);
            if (d == 0) {
                *offset = 0;
                *moving = 0;
                const unsigned char v = *top;
                *state = 0;
                return v;
            }
        }
        *offset = static_cast<unsigned char>(-d);
        *moving = 1;
        const unsigned char v = *top;
        *state = static_cast<unsigned char>(0xF0 + d);
        return v;
    }
    *offset = 0;
    *moving = 0;
    const unsigned char v = *top;
    *state = static_cast<unsigned char>((hi << 4) + lo);
    return v;
}

void Menu2FromMenuWindows_Inject() {
    {
        // DIV-0165: after the self-test, which compared Capcom's unsigned words.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("MenuSignedXY", Low32(&menu_windows::g_signed_xy), &was, &is, 1);
        bof3::Log("DIV-0165    Menu_DrawBox and Menu_DrawIcon8 take x and y as signed words (on unless the line above says OFF)");
    }
}

void Menu2_Inject() {
    BOF3_INJECT(Menu_DrawGreyHLine);
    BOF3_INJECT(Menu_DrawBox);
    BOF3_INJECT(Menu_DrawIcon8);
    BOF3_INJECT(Menu_DrawOutline);
    BOF3_INJECT(Menu_DrawOutlineNotched);
    BOF3_INJECT(Menu_DrawLine);
    BOF3_INJECT(Text_CharCount);
    BOF3_INJECT(Menu_PieceRect);
    BOF3_INJECT(Menu_DrawPiece);
    BOF3_INJECT(Menu_DrawPieces);
    BOF3_INJECT(Item_CanUse);
    BOF3_INJECT(Skill_CanUse);
    BOF3_INJECT(Menu_DrawItemRow);
    BOF3_INJECT(Menu_DrawSkillRow);
    BOF3_INJECT(Menu_DrawScrollBar);
    BOF3_INJECT(Menu_ListScroll);
}
