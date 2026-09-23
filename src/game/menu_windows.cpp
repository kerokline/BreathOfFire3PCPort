// The menu and shop windows (the sixth round's group Y). docs/menu-windows.md.
//
// Thirty-seven functions the owner's shop route reaches: the menu box
// Menu_DrawBox and the pieces it is framed with, the two small fonts, the
// button row and the Yes / No chooser, the item list, the equipment, money
// and shop panels, the backdrop, and the few helpers under them. Each was
// read to its last instruction (2026-09-23); the PSX twins are STATUS.EMI's
// and the boot EXE's (docs/menu-windows.md, section 1).
//
// Every call goes through menu_windows::g (menu_windows_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Where the original reads memory after a call, ours
// reads it after the same call; a nested call is hoisted to where the
// original makes it.
//
// Upper halves. The originals push many values built with 8- and 16-bit
// operations, whose upper bits are whatever the register held; every callee
// here reads only the low 16 bits of a coordinate and the low byte of a
// colour, flag or id (each callee's read is in the fuzz's stand-ins), so ours
// passes the value with its upper bits as C++ computes them.
//
// Two divergences have their patch sites inside these bodies and survive in
// ours: DIV-0018 (Menu_DrawButtonRow's label draw, BOF3X_ORIGINAL=MenuVerbs)
// and DIV-0027 (Menu_YesNo's line and hand stops, BOF3X_ORIGINAL=YesNoLayout).
// Their modules patch Capcom's bytes as before; MenuWindows_Inject asks them
// what went in and ours does the same.
#include "game/menu_windows.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/menu_verbs.h"
#include "game/menu_windows_callees.h"
#include "game/move_script_bytes.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace menu_windows {

using move_script::At;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Text_DrawAt,
    Text_DrawAt,
    Text_DrawImmediate,
    Msg_SystemPtr,
    Msg_SystemPtr,
    MsgBox_Reset,
    reinterpret_cast<void (__cdecl*)(unsigned)>(reinterpret_cast<void*>(&Sound_PlayEffect)),
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_SetSprt,
    Gpu_SetSemiTrans,
    Gpu_GetTPage,
    Gpu_GetClut,
    Gpu_SetPolyFT4,
    Gpu_SetLineF2,
    Fn<void (__cdecl*)(unsigned char*)>(kSetLineF3),
    Gpu_SetLineF4,
    Gpu_SetTile,
    reinterpret_cast<unsigned char (__cdecl*)(int, int)>(reinterpret_cast<void*>(&AreaMap_ByteAt)),
    Crt_sprintf,
    Fn<void (__cdecl*)(unsigned, int, int, unsigned, unsigned, unsigned)>(kStatIcon),
    Fn<void (__cdecl*)(int, int, int)>(kDrawHand),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned char*, unsigned short*)>(kEquipCompare),
    Fn<const unsigned char* (__cdecl*)(unsigned, unsigned)>(kItemName),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(kItemKind),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(kEquipMask),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(kItemFlagsOf),
    Fn<unsigned (__cdecl*)(unsigned)>(kHasKeyItem),
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(kCountOwned),
    Fn<unsigned (__cdecl*)(unsigned)>(kCountCategory),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(kPriceScale),
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(kSellPrice),
    Text_DrawFont12,
    Text_DrawFont8,
    Menu_DrawPanel,
    Menu_DrawBox,
    Menu_DrawIcon8,
    Text_CharCount,
    Menu_DrawPieces,
    Menu_DrawPiece,
    Menu_PieceRect,
    Menu_DrawLine,
    Menu_DrawOutline,
    Menu_DrawItemIcon,
    Menu_DrawBigIcon,
    Char_ExpForLevel,
    Item_BasePrice,
    Item_CanUse,
    Menu_DrawItemRow,
    Menu_DrawScrollBar,
    Menu_ListScroll,
    Menu_DrawBorder,
    Gpu_SetSprt8,
};
Callees g = kOriginals;
bool g_yes_no_div = false;

}  // namespace menu_windows

using namespace menu_windows;

namespace {

using move_script::Long;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Ptr(std::uint32_t at) { return At(static_cast<std::uint32_t>(Long(At(at)))); }
unsigned char* Next() { return Gfx_PacketNext; }
char* PrintBuf() { return reinterpret_cast<char*>(At(at::kPrintBuf)); }
const char* Format(std::uint32_t at) { return reinterpret_cast<const char*>(At(at)); }

std::int16_t S16(std::uint32_t v) { return static_cast<std::int16_t>(v); }
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
    const unsigned tpage = g.get_tpage(0, 0, page_x, 0x100);
    g.draw_mode(Next(), 0, 0, tpage & 0xFFFF, 0);
    g.commit(1, 0xC);
}

}  // namespace

// ===========================================================================
// The message box's two small helpers

// original 0x497710 (PSX 0x801503AC, the sibling's Msg_OpenSystem): the
// string of system message id, as the box's base and stepper pointer, the
// message index its low 16 bits, and MsgBox_Reset.
extern "C" void __cdecl Msg_OpenSystem(unsigned id) {
    const unsigned char* const text = g.msg_system_ptr(id);
    Put32(At(at::kBoxText), Addr(text));
    Put32(At(at::kListText), Addr(text));
    SetWord(At(at::kMessage), id);
    g.msgbox_reset();
}

// original 0x498D20: the "there is more" arrow under the message box, an 8 x 8
// SPRT at (u 0xD0, v 0xF8) of page (0x3C0, 0) under abr 1, shown only while
// bit 5 of Frame_Counter is set - so it blinks, 32 frames on and 32 off. x and
// y are read as 16-bit (movsx).
extern "C" void __cdecl MsgBox_DrawArrow(int x, int y) {
    if (!(At(at::kFrameCounter)[0] & 0x20)) return;
    const unsigned tpage = g.get_tpage(0, 1, 0x3C0, 0);
    g.draw_mode(Next(), 0, 0, tpage & 0xFFFF, 0);
    g.commit(1, 0xC);
    unsigned char* const p = Next();
    g.set_sprt(p);
    SetWord(p + 0x16, g.get_clut(0xC0, 0x1E0));
    p[4] = p[5] = p[6] = 0x80;
    Put(p + 8, F(S16(x)));
    Put(p + 0xC, F(S16(y)));
    SetWord(p + 0x18, 8);
    SetWord(p + 0x1A, 8);
    p[0x14] = 0xD0;
    p[0x15] = 0xF8;
    g.set_semi(p, 1);
    g.commit(1, 0x1C);
}

// ===========================================================================
// The two small fonts (known-defects.md D1 names both)

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
    g.draw_mode(Next(), 0, 0, 0x2F, 0);
    g.commit(1, 0xC);
    const unsigned clut = (colour & 0x3F) | 0x7800;
    std::uint32_t pen_x = static_cast<std::uint32_t>(x);
    unsigned char* p = Next();
    for (;;) {
        const unsigned char c = text[0];
        if (c == 0x0A) {
            pen_y += 0xC;
            pen_x = static_cast<std::uint32_t>(x);
        } else if (c != 0x20 && c != 0) {
            SetWord(p + 0x16, clut);
            SetWord(p + 0x18, 0xC);
            SetWord(p + 0x1A, 0xC);
            p[4] = p[5] = p[6] = 0x80;
            p[0x14] = static_cast<unsigned char>((c % 21) * 12);
            p[0x15] = static_cast<unsigned char>((c / 21) * 12);
            Put(p + 8, F(S16(pen_x)));
            Put(p + 0xC, F(S16(static_cast<std::uint32_t>(pen_y))));
            g.set_sprt(p);
            g.set_semi(p, 1);
            g.commit(1, 0x1C);
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
    g.draw_mode(Next(), 0, 0, 0x2F, 0);
    g.commit(1, 0xC);
    const unsigned clut = (colour & 0x3F) | 0x7800;
    std::uint32_t pen_x = static_cast<std::uint32_t>(x);
    unsigned char* p = Next();
    for (;;) {
        const unsigned char c = text[0];
        if (c == 0x0A) {
            pen_y += 8;
            pen_x = static_cast<std::uint32_t>(x);
        } else if (c != 0x20 && c != 0) {
            SetWord(p + 0x16, clut);
            p[4] = p[5] = p[6] = 0x80;
            const int cell = static_cast<int>(c) - 0x20;
            p[0x14] = static_cast<unsigned char>((cell % 32) * 8);
            p[0x15] = static_cast<unsigned char>((cell / 32) * 8);
            Put(p + 8, F(S16(pen_x)));
            Put(p + 0xC, F(S16(static_cast<std::uint32_t>(pen_y))));
            g.set_sprt8(p);
            g.set_semi(p, 1);
            g.commit(1, 0x18);
            p = Next();
        }
        const unsigned char after = text[1];
        pen_x += 8;
        ++text;
        if (after == 0) break;
    }
}

// original 0x596020 (PSX 0x8015AA88): window kind 2's list (Window_Kind2Frame,
// docs/window-task.md): items 0 .. the s8 at 0x7DEE66 inclusive, each through
// Text_DrawImmediate at the current window record's (x >> 4, y >> 4) - s16
// words +4 / +6, shifted arithmetically - plus the set's offsets 0x66AE2C and
// a per-item stride, all 16-bit. The first item's text is the dword at
// 0x7DEE50; each next item's is where Text_DrawImmediate stopped. The
// record, the set and the count are read again for every item.
extern "C" void __cdecl Window_Kind2List() {
    const unsigned char* text = Ptr(at::kListText);
    if (static_cast<signed char>(At(at::kListCount)[0]) < 0) return;
    for (int i = 0;;) {
        const unsigned set = At(at::kListSet)[0];
        const unsigned char* const record = Ptr(at::kCurrent);
        const unsigned char* const xy = At(at::kSetXY + set * 6);
        const auto dy = static_cast<std::uint16_t>(Word(xy + 4) * static_cast<std::uint16_t>(i));
        const auto y = static_cast<std::uint16_t>(dy + (S16(Word(record + 6)) >> 4) + Word(xy + 2));
        const auto x = static_cast<std::uint16_t>((S16(Word(record + 4)) >> 4) + Word(xy));
        text = g.text_immediate(x, y, text);
        ++i;
        if (i > static_cast<signed char>(At(at::kListCount)[0])) break;
    }
}

// ===========================================================================
// The menu box and its pieces

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
extern "C" void __cdecl Menu_DrawBox(int x, int y, int w, int h, int flags, int colour) {
    const auto kind = static_cast<unsigned char>(flags & 0xF);
    const unsigned char shade = kind == 1 ? 0x48 : 0xAC;
    const unsigned char* const window = PoolRect(0, 0xF0, 0x10, 0x10);
    g.draw_mode(Next(), 0, 1, 0xF, Addr(window));
    g.commit(1, 0xC);
    unsigned char* p = Next();
    g.set_poly_ft4(p);
    std::uint32_t clut_x = static_cast<std::uint32_t>(colour & 0xFF) << 5;
    if (kind == 2) {
        g.set_semi(p, 1);
        clut_x += 0x10;
    }
    p[6] = p[5] = p[4] = shade;
    const std::int32_t xs = U16(static_cast<std::uint32_t>(x)), ys = U16(static_cast<std::uint32_t>(y));
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
    SetWord(p + 0x16, g.get_clut(static_cast<int>(clut_x & 0xFFFF), 0x1E1));
    SetWord(p + 0x26, 0xF);
    g.commit(1, 0x48);

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
        g.commit(1, 0x48);
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
        g.commit(1, 0x48);
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
        g.commit(1, 0x48);
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
    g.commit(1, 0x48);

    const unsigned char* const whole = PoolRect(0, 0, 0x100, 0x100);
    g.draw_mode(Next(), 0, 0, 0xF, Addr(whole));
    g.commit(1, 0xC);
}

// original 0x57D830: piece id's rectangle (u, v, w, h), from 0x663B94, or from
// 0x663C8C when the flag byte is non-zero. Both read as bytes.
extern "C" const unsigned char* __cdecl Menu_PieceRect(unsigned id, unsigned flags) {
    return At(((flags & 0xFF) ? at::kPieceRectsAlt : at::kPieceRects) + (id & 0xFF) * 4);
}

// original 0x57D860 (PSX 0x801B02A0): one piece of a panel - a SPRT at (x, y),
// 16-bit, of piece id's rectangle (Menu_PieceRect with flag bit 0), opaque,
// CLUT (0xB0, 0x1E1), or (0x80, 0x1E2) with flag bit 1, shade 0x80. Sets no
// draw mode (its callers do).
extern "C" void __cdecl Menu_DrawPiece(int x, int y, unsigned id, unsigned flags) {
    const auto f = static_cast<unsigned char>(flags);
    const unsigned char* const rect = g.piece_rect(id, f & 1u);
    unsigned char* const p = Next();
    g.set_sprt(p);
    g.set_semi(p, 0);
    Put(p + 8, F(S16(static_cast<std::uint32_t>(x))));
    Put(p + 0xC, F(S16(static_cast<std::uint32_t>(y))));
    p[0x14] = rect[0];
    p[0x15] = rect[1];
    SetWord(p + 0x18, rect[2]);
    SetWord(p + 0x1A, rect[3]);
    const unsigned clut = (f & 2) ? g.get_clut(0x80, 0x1E2) : g.get_clut(0xB0, 0x1E1);
    SetWord(p + 0x16, clut);
    p[4] = p[5] = p[6] = 0x80;
    g.commit(1, 0x1C);
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
        g.piece(x + static_cast<signed char>(list[0]) * 8, y + static_cast<signed char>(list[1]) * 8, id,
                static_cast<unsigned>(flags));
        id = list[5];
        list += 3;
    } while (id != 0xFF);
}

// original 0x5762D0 (PSX 0x801DD304): the shop windows' border - pieces 0x29
// along the top and 0x2E along the bottom (w of them), 0x2B and 0x2C down the
// sides (h of them), corners 0x28 0x2A 0x2D 0x2F - on page (0x380, 0x100). w
// and h are counts of 8-unit cells, read as bytes.
extern "C" void __cdecl Menu_DrawBorder(int x, int y, int w, int h) {
    PiecePage(0x380);
    const unsigned wb = static_cast<unsigned>(w) & 0xFF, hb = static_cast<unsigned>(h) & 0xFF;
    if (wb) {
        const int bottom = y + static_cast<int>(hb) * 8 + 8;
        for (unsigned char i = 0; i < wb; ++i) {
            g.piece(x + i * 8 + 8, y, 0x29, 0);
            g.piece(x + i * 8 + 8, bottom, 0x2E, 0);
        }
    }
    if (hb) {
        const int right = x + static_cast<int>(wb) * 8 + 8;
        for (unsigned char j = 0; j < hb; ++j) {
            g.piece(x, y + j * 8 + 8, 0x2B, 0);
            g.piece(right, y + j * 8 + 8, 0x2C, 0);
        }
    }
    g.piece(x, y, 0x28, 0);
    const int right = x + static_cast<int>(wb) * 8 + 8;
    g.piece(right, y, 0x2A, 0);
    const int bottom = y + static_cast<int>(hb) * 8 + 8;
    g.piece(x, bottom, 0x2D, 0);
    g.piece(right, bottom, 0x2F, 0);
}

// original 0x575830 (PSX 0x801DBEB0 and three more overlays' copies): a menu
// panel - the box (x + 3, y + 3, w * 8 + 0x2D, h * 8 + 0x18) in the window
// colour, then its frame of pieces 0..8 on page (0x340, 0x100) with flag 1: a
// top of w pieces between two corner pieces, a bottom of w + 5, the sides h
// deep. w and h are read as bytes.
//
// As the original has it: the bottom's counter is a byte compared with
// w + 5, so a w of 251 or more never ends (no caller passes one).
extern "C" void __cdecl Menu_DrawPanel(int x, int y, int w, int h) {
    const unsigned wb = static_cast<unsigned>(w) & 0xFF, hb = static_cast<unsigned>(h) & 0xFF;
    const int height = static_cast<int>(hb) * 8 + 0x18;
    g.box(x + 3, y + 3, static_cast<int>(wb) * 8 + 0x2D, height, 0, At(at::kColour)[0]);
    PiecePage(0x340);
    g.piece(x, y, 0, 1);
    for (unsigned char c = 0; c < wb; ++c) g.piece(x + c * 8 + 0x20, y, 1, 1);
    g.piece(x + static_cast<int>(wb) * 8 + 0x20, y, 2, 1);
    const int right = x + static_cast<int>(wb) * 8 + 0x30;
    g.piece(right, y, 3, 1);
    const int n = static_cast<int>(wb) + 5;
    unsigned char c = 0;
    do {
        g.piece(x + c * 8 + 8, y + height, 6, 1);
        ++c;
    } while (static_cast<int>(c) < n);
    for (unsigned char r = 0; r < hb; ++r) {
        g.piece(x, y + r * 8 + 0x18, 4, 1);
        g.piece(right, y + r * 8 + 0x18, 8, 1);
    }
    g.piece(x, y + height, 5, 1);
    g.piece(right, y + height, 7, 1);
}

// original 0x5A7720 (PSX 0x8017B36C, the sibling's SetSprt8): code 0x74, SPRT_8,
// and the float 0.01 to +0x10.
extern "C" void __cdecl Gpu_SetSprt8(unsigned char* prim) {
    prim[7] = 0x74;
    Put32(prim + 0x10, 0x3C23D70Au);
}

// original 0x57D360 (PSX 0x801AFA80): an item kind's 8 x 8 icon - SPRT_8 at
// (x, y) of cell (icon + 7) * 8 (a byte) in row 0xF8, CLUT word 0x780E for
// icons 1, 3 and 15 and 0x780C for the rest, shade 0x40 when dim, else 0x80;
// after a draw mode of page 0xF.
extern "C" void __cdecl Menu_DrawIcon8(int x, int y, int icon, int dim) {
    g.draw_mode(Next(), 0, 0, 0xF, 0);
    g.commit(1, 0xC);
    unsigned char* const p = Next();
    g.set_sprt8(p);
    const unsigned char shade = (dim & 0xFF) ? 0x40 : 0x80;
    p[4] = p[5] = p[6] = shade;
    Put(p + 8, F(U16(static_cast<std::uint32_t>(x))));
    Put(p + 0xC, F(U16(static_cast<std::uint32_t>(y))));
    p[0x15] = 0xF8;
    p[0x14] = static_cast<unsigned char>((icon + 7) << 3);
    const unsigned i = static_cast<unsigned>(icon) & 0xFF;
    SetWord(p + 0x16, (i == 1 || i == 3 || i == 15) ? 0x780E : 0x780C);
    g.commit(1, 0x18);
}

// original 0x57D760: a LINE_F2 from (x0, y0) to (x1, y1), 16-bit, colour (r, g,
// b), semi-transparent under abr & 3 - after a draw mode of page
// ((abr & 3) << 5) | 0x1E.
extern "C" void __cdecl Menu_DrawLine(int x0, int y0, int x1, int y1, int r, int gr, int b, int abr) {
    g.draw_mode(Next(), 0, 0, (static_cast<unsigned>(abr & 3) << 5) | 0x1E, 0);
    g.commit(1, 0xC);
    unsigned char* const p = Next();
    g.set_line_f2(p);
    Put(p + 8, F(S16(static_cast<std::uint32_t>(x0))));
    Put(p + 0xC, F(S16(static_cast<std::uint32_t>(y0))));
    p[4] = static_cast<unsigned char>(r);
    Put(p + 0x14, F(S16(static_cast<std::uint32_t>(x1))));
    p[5] = static_cast<unsigned char>(gr);
    p[6] = static_cast<unsigned char>(b);
    Put(p + 0x18, F(S16(static_cast<std::uint32_t>(y1))));
    g.set_semi(p, 1);
    g.commit(1, 0x20);
}

// original 0x57D420: a rectangle's outline in four Menu_DrawLines of the
// window colour - entry 0 of CLUT row (s8) 0x903A5A in the shadow 0x80B7A8 -
// the left and top edges under abr 2 with flag bit 0 (else 1), the bottom and
// right under the other: a bevel.
extern "C" void __cdecl Menu_DrawOutline(int x, int y, int w, int h, int flags) {
    const bool bit0 = (flags & 1) != 0;
    const unsigned char abr_a = bit0 ? 2 : 1, abr_b = bit0 ? 1 : 2;
    const int row = static_cast<signed char>(At(at::kColour)[0]);
    const unsigned char* const entry = At(at::kClutShadow + static_cast<std::uint32_t>(row * 64));
    const unsigned c = Word(entry);
    const auto r = static_cast<unsigned char>((entry[0] & 0x1F) << 3);
    const auto gr = static_cast<unsigned char>(((c >> 5) & 0x1F) << 3);
    const auto b = static_cast<unsigned char>(((c >> 10) & 0x1F) << 3);
    const int yh = y + h, xw = x + w;
    g.line(x, y, x, yh - 1, r, gr, b, abr_a);
    g.line(x, yh, xw - 1, yh, r, gr, b, abr_b);
    g.line(xw, yh, xw, y + 1, r, gr, b, abr_b);
    g.line(xw, y, x + 1, y, r, gr, b, abr_a);
}

// original 0x574AB0 (PSX 0x801DAB48): the screen title's box, the owner's
// "Ability has no box" lead (docs/menu-screens.md section 3): four
// semi-transparent POLY_FT4s of the 16 x 16 tile at (0, 0xF0) under its
// texture window - a left end 2 wide with slanted corners, two middles of
// (w + 1 - 4) / 2 each (the second a unit wider when w + 1 is odd), a right
// end 2 wide - shade 0xAC, CLUT GetClut(colour * 32 + 16, 0x1E1); then the
// window back to the whole page and Menu_DrawOutline(x + 2, y + 2, w - 4,
// h - 4, 0) inside it. The box spans w + 1 by h + 1.
extern "C" void __cdecl Menu_DrawTitleBox(int x, int y, int w, int h, int colour) {
    const unsigned char* const window = PoolRect(0, 0xF0, 0x10, 0x10);
    g.draw_mode(Next(), 0, 1, 0xF, Addr(window));
    g.commit(1, 0xC);
    const auto w1 = static_cast<std::uint32_t>(w) + 1, h1 = static_cast<std::uint32_t>(h) + 1;
    unsigned char* p = Next();
    g.set_poly_ft4(p);
    g.set_semi(p, 1);
    const std::int32_t xs = U16(static_cast<std::uint32_t>(x)), ys = U16(static_cast<std::uint32_t>(y));
    const std::int32_t hs = U16(h1);
    const auto hb = static_cast<unsigned char>(h1);
    const float fx = F(xs), fy = F(ys), fx2 = F(xs + 2), fyh = F(hs + ys);
    const float fy2 = Add(fy, 2.0), fyh3 = Add(fyh, -3.0);
    p[0x14] = 0;
    p[0x24] = 2;
    p[0x25] = 0;
    Put(p + 8, fx);
    p[0x34] = 0;
    Put(p + 0x1C, fy);
    Put(p + 0x18, fx2);
    p[0x44] = 2;
    Put(p + 0x28, fx);
    Put(p + 0x38, fx2);
    p[0x45] = hb;
    p[4] = p[5] = 0xAC;
    p[6] = 0xAC;
    Put(p + 0x3C, fyh);
    p[0x15] = 2;
    Put(p + 0xC, fy2);
    Put(p + 0x2C, fyh3);
    const auto hb3 = static_cast<unsigned char>(hb - 3);
    p[0x35] = hb3;
    const int clut_x = static_cast<int>((static_cast<unsigned>(colour) & 0xFF) << 5) + 0x10;
    SetWord(p + 0x16, g.get_clut(clut_x, 0x1E1));
    SetWord(p + 0x26, 0xF);
    g.commit(1, 0x48);

    const std::int32_t ws = U16(w1);
    const std::int32_t half = (ws - 4) >> 1;
    const std::int32_t parity = static_cast<std::int32_t>(w1 & 1);
    p = Next();
    g.set_poly_ft4(p);
    g.set_semi(p, 1);
    const std::int32_t half16 = S16(static_cast<std::uint32_t>(half));
    const float fmid = F(half16 + xs + 2);
    Put(p + 0xC, fy);
    Put(p + 8, fx2);
    Put(p + 0x1C, fy);
    Put(p + 0x28, fx2);
    Put(p + 0x18, fmid);
    Put(p + 0x38, fmid);
    Put(p + 0x2C, fyh);
    Put(p + 0x3C, fyh);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x24] = static_cast<unsigned char>(half);
    p[0x25] = 0;
    p[0x34] = 0;
    p[0x35] = hb;
    p[0x44] = static_cast<unsigned char>(half);
    p[0x45] = hb;
    p[4] = p[5] = p[6] = 0xAC;
    SetWord(p + 0x16, g.get_clut(clut_x, 0x1E1));
    SetWord(p + 0x26, 0xF);
    g.commit(1, 0x48);

    p = Next();
    g.set_poly_ft4(p);
    g.set_semi(p, 1);
    const float fright = F(xs + parity + half16 * 2 + 2);
    const auto u3 = static_cast<unsigned char>(parity + half);
    Put(p + 8, fmid);
    Put(p + 0xC, fy);
    p[0x14] = 0;
    Put(p + 0x1C, fy);
    Put(p + 0x28, fmid);
    Put(p + 0x2C, fyh);
    Put(p + 0x18, fright);
    Put(p + 0x38, fright);
    Put(p + 0x3C, fyh);
    p[0x15] = 0;
    p[0x24] = u3;
    p[0x25] = 0;
    p[0x34] = 0;
    p[0x35] = hb;
    p[0x44] = u3;
    p[0x45] = hb;
    p[4] = p[5] = p[6] = 0xAC;
    SetWord(p + 0x16, g.get_clut(clut_x, 0x1E1));
    SetWord(p + 0x26, 0xF);
    g.commit(1, 0x48);

    p = Next();
    g.set_poly_ft4(p);
    g.set_semi(p, 1);
    const std::int32_t xe = xs + ws;
    const float fe2 = F(xe - 2), fe = F(xe);
    Put(p + 0xC, fy);
    Put(p + 0x1C, fy2);
    p[0x14] = 0;
    Put(p + 8, fe2);
    p[0x15] = 0;
    p[0x24] = 2;
    p[0x34] = 0;
    p[0x44] = 2;
    Put(p + 0x18, fe);
    p[4] = 0xAC;
    Put(p + 0x28, fe2);
    Put(p + 0x38, fe);
    p[5] = p[6] = 0xAC;
    Put(p + 0x3C, fyh3);
    Put(p + 0x2C, Add(fyh, -1.0));
    p[0x25] = 2;
    p[0x35] = static_cast<unsigned char>(hb - 1);
    p[0x45] = hb3;
    SetWord(p + 0x16, g.get_clut(clut_x, 0x1E1));
    SetWord(p + 0x26, 0xF);
    g.commit(1, 0x48);

    const unsigned char* const whole = PoolRect(0, 0, 0x100, 0x100);
    g.draw_mode(Next(), 0, 1, 0xF, Addr(whole));
    g.commit(1, 0xC);
    g.outline(x + 2, y + 2, static_cast<int>(w1 - 5), static_cast<int>(h1 - 5), 0);
}

// ===========================================================================
// Icons, the button row, the chooser

// original 0x573E50 (PSX 0x801D8F9C): an icon of 0x28 x 0x30 - rectangle icon
// of 0x66367C: (u, v, clut-x byte, clut-y byte), the CLUT word
// ((b3 + 0x1E0) << 6) | (b2 >> 4) - after a draw mode of page 0x1E with dtd;
// shade 0x80 for 0, 0x30 for 1, else (0x40, 0x40, 0x80).
extern "C" void __cdecl Menu_DrawBigIcon(int x, int y, int icon, int shade) {
    const unsigned char* const r = At(at::kIconRects + (static_cast<unsigned>(icon) & 0xFF) * 4);
    g.draw_mode(Next(), 0, 1, 0x1E, 0);
    g.commit(1, 0xC);
    unsigned char* const p = Next();
    g.set_sprt(p);
    const auto s = static_cast<unsigned char>(shade);
    if (s == 0) {
        p[6] = p[5] = p[4] = 0x80;
    } else if (s == 1) {
        p[6] = p[5] = p[4] = 0x30;
    } else {
        p[6] = 0x80;
        p[5] = p[4] = 0x40;
    }
    Put(p + 8, F(U16(static_cast<std::uint32_t>(x))));
    Put(p + 0xC, F(U16(static_cast<std::uint32_t>(y))));
    p[0x14] = r[0];
    p[0x15] = r[1];
    SetWord(p + 0x18, 0x28);
    SetWord(p + 0x1A, 0x30);
    SetWord(p + 0x16, ((r[3] + 0x1E0u) << 6) | static_cast<unsigned>(r[2] >> 4));
    g.commit(1, 0x1C);
}

// original 0x573F30 (PSX 0x801D90E8): Menu_DrawBigIcon, with icon 4 drawn as
// icon 0xB once the s8 at 0x8034E0 (Cond_ByteFA) is 8 or more.
extern "C" void __cdecl Menu_DrawItemIcon(int x, int y, int icon, int shade) {
    if ((icon & 0xFF) == 4 && static_cast<signed char>(At(at::kCondFA)[0]) >= 8)
        icon = static_cast<int>((static_cast<unsigned>(icon) & ~0xFFu) | 0xB);
    g.icon(x, y, icon, shade);
}

// original 0x5747D0 (PSX 0x801DA628): the Yes / No chooser (DIV-0027,
// docs/glyph-draw.md section 7). System message 0xF at (0x1C, 0x16), the hand
// at x 0xFE - 36 * s8 selection (0x929F0B), y 0x18. Then on this frame's
// Input_Pressed: a cancel button - sound 0x106, selection 0, answer 1; a
// confirm button - sound 0x104 on Yes, 0x106 on No, answer 1; bit 13 or 15
// (left, right) - sound 0x101, the selection's bit 0 flipped; else 0.
//
// DIV-0027 (under a language overlay, BOF3X_ORIGINAL=YesNoLayout): the line
// through YesNo_Line and the hand at 0x112 - 56 * selection, as the patched
// original has them.
extern "C" unsigned char __cdecl Menu_YesNo() {
    const unsigned char* const line = g.yes_no_line(0xF);
    g.text_draw_at(0x1C, 0x16, 0, 0xFF, line);
    const auto sel = static_cast<std::uint32_t>(static_cast<int>(static_cast<signed char>(At(at::kYesNo)[0])));
    const std::uint32_t hand = g_yes_no_div ? 0x112u - 56u * sel : 0xFEu - 36u * sel;
    g.draw_hand(static_cast<int>(hand), 0x18, 0);
    const auto pressed = static_cast<std::uint32_t>(Long(At(at::kInputPressed)));
    if (Word(At(at::kCancelButtons)) & pressed) {
        g.sound(0x106);
        At(at::kYesNo)[0] = 0;
        return 1;
    }
    if (Word(At(at::kConfirmButtons)) & pressed) {
        if (At(at::kYesNo)[0]) {
            g.sound(0x104);
            return 1;
        }
        g.sound(0x106);
        return 1;
    }
    if ((pressed >> 8) & 0xA0) {
        g.sound(0x101);
        At(at::kYesNo)[0] = static_cast<unsigned char>(At(at::kYesNo)[0] ^ 1);
    }
    return 0;
}

// original 0x574890 (PSX 0x801DA738 and three more overlays' copies): the
// buttons above a menu panel (DIV-0018, docs/config-screen.md section 8). For
// each of Menu_ButtonSets[set].count: the box (x + 48 i, y, 0x2D, 0x14) in the
// window colour; the verb through Text_DrawAt, centred as
// x + 0x16 + 48 i - 6 n (n from Text_CharCount), count 0x10, colour 7 -
// greyed - for every button but the selected one when the selection is below
// the count, for verb 0x12 unless bit 0 of 0x905BA2, and for verb 8 when key
// item 0xF is not held and no item 0x58 is; then the pieces 0x663440 on the
// selected button, 0x663428 on the rest. The count is read again after each
// button; the fifth argument is not read.
//
// DIV-0018 (under a language overlay, BOF3X_ORIGINAL=MenuVerbs): the label
// through MenuVerbs_DrawLabel, as the patched original has it.
extern "C" void __cdecl Menu_DrawButtonRow(int x, int y, int set, int sel, int) {
    const unsigned s5 = (static_cast<unsigned>(set) & 0xFF) * 5;
    const unsigned char* const sets = At(at::kButtonSets);
    if (sets[s5] == 0) return;
    const auto selected = static_cast<unsigned char>(sel);
    unsigned char i = 0;
    do {
        const int bx = x + 48 * i;
        g.box(bx, y, 0x2D, 0x14, 0, At(at::kColour)[0]);
        unsigned char colour;
        if (selected >= sets[s5]) colour = 0;
        else colour = (i == selected) ? 0 : 7;
        const unsigned char verb = sets[s5 + 1 + i];
        if (verb == 0x12 && !(At(at::kInputFlags)[0] & 1)) colour = 7;
        if (verb == 8) {
            if ((g.has_key_item(0xF) & 0xFF) == 0) {
                if ((g.count_owned(0, 0x58, 0) & 0xFFFF) == 0) colour = 7;
            }
        }
        const unsigned char* const text = Ptr(at::kVerbPointers + verb * 4u);
        const unsigned n = g.char_count(text);
        g.verb_label(x + 0x16 + 6 * (8 * i - static_cast<int>(n)), y + 3, colour, 0x10, text);
        g.pieces(bx, y, At(i == selected ? 0x663440 : 0x663428), 0);
        ++i;
    } while (i < sets[s5]);
}

// original 0x57D800: the characters in text to its NUL - a byte with bit 7 and
// the byte after it are one - looking at no more than 16 bytes (the count of
// bytes may step from 15 to 17 over a two-byte character).
extern "C" unsigned char __cdecl Text_CharCount(const unsigned char* text) {
    unsigned char n = 0, bytes = 0;
    while (text[0]) {
        ++n;
        if (text[0] & 0x80) {
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

// ===========================================================================
// The stat, item and price helpers

// original 0x5749F0 (PSX 0x801DA988): an item's price, the u16 of its record -
// weapons (category 1) 0x65746A + 28 id, armour (2) 0x657D80 + 26 id,
// accessories (3) 0x658466 + 24 id, any other category a consumable, 0x656B3C
// + 22 id - both arguments read as bytes, the answer zero-extended.
extern "C" unsigned __cdecl Item_BasePrice(unsigned category, unsigned id) {
    const unsigned i = id & 0xFF;
    switch (category & 0xFF) {
    case 1: return Word(At(0x65746A + i * 28));
    case 2: return Word(At(0x657D80 + i * 26));
    case 3: return Word(At(0x658466 + i * 24));
    default: return Word(At(0x656B3C + i * 22));
    }
}

// original 0x574A60 (PSX 0x801DAA30): the experience member needs for a level:
// the sum of the first `level` u16s of its table (0x658F48 + member * 0x318,
// stride 8); -1 for a level above 99. Both read as bytes.
extern "C" int __cdecl Char_ExpForLevel(unsigned member, unsigned level) {
    const auto n = static_cast<unsigned char>(level);
    if (n > 0x63) return -1;
    std::uint32_t sum = 0;
    const unsigned char* const table = At(at::kExpTable + (member & 0xFF) * 0x318);
    for (unsigned k = 0; k < n; ++k) sum += Word(table + 8 * k);
    return static_cast<int>(sum);
}

// original 0x574530 (PSX 0x801DA18C): the experience bar - an opaque LINE_F2
// in red (0x80, 0, 0) from (x, y) to (x + 57 (exp - need) / (next - need), y),
// need and next Char_ExpForLevel of level and level + 1 (a byte). All 32-bit
// unsigned, as the original has it: the product wraps and the division is
// unsigned. With next equal to need the line has no length. x and y 16-bit.
extern "C" void __cdecl Menu_DrawExpBar(int x, int y, unsigned member, unsigned level, unsigned exp) {
    unsigned char* const p = Next();
    g.set_line_f2(p);
    g.set_semi(p, 0);
    const std::int32_t xs = U16(static_cast<std::uint32_t>(x));
    const float fx = F(xs), fy = F(U16(static_cast<std::uint32_t>(y)));
    Put(p + 8, fx);
    Put(p + 0xC, fy);
    const auto need = static_cast<std::uint32_t>(g.exp_for_level(member, level));
    const auto next = static_cast<std::uint32_t>(
        g.exp_for_level(member, (level & ~0xFFu) | ((level + 1) & 0xFFu)));
    const std::uint32_t span = next - need;
    if (span != 0) {
        const std::uint32_t end = (57u * exp - 57u * need) / span + static_cast<std::uint32_t>(xs);
        Put(p + 0x14, static_cast<float>(static_cast<double>(end)));   // fild qword of (0, end)
    } else {
        Put(p + 0x14, fx);
    }
    Put(p + 0x18, fy);
    p[4] = 0x80;
    p[5] = 0;
    p[6] = 0;
    g.commit(1, 0x20);
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
        if (!(At(at::kItemFlags + it * 22u)[0] & 1)) return 0;
        if (it != 0x57) return 1;
        return g.area_byte(Word(At(at::kLeaderX)), Word(At(at::kLeaderZ))) == 0xAE ? 1 : 0;
    case 2: return (g.item_flags(category, item) & 2) ? 1 : 0;
    case 3: return (g.item_flags(category, item) & 8) ? 0 : 1;
    case 4: {
        const unsigned mask = g.equip_mask(category, item);
        return ((1u << (member & 31)) & mask & 0xFF) ? 1 : 0;
    }
    default: return 1;
    }
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

// ===========================================================================
// The panels

// original 0x573A80 (PSX 0x801D8A10 and three more overlays' copies): a
// member's equipment - the panel (x, y, 9, 0xA), "EQUIP" at (x + 0x26, y + 7),
// the stat icon 0x5903F0(0, x + 7, y + 5, 0x10, 0x10, 0x80), then the six
// slots, 13 apart: the slot icon of 0x663660 and the name of record bytes
// +0x12 (a weapon), +0x13..+0x15 (armour), +0x16, +0x17 (accessories). The
// six names are looked up once, after the stat icon.
extern "C" void __cdecl Menu_DrawEquipPanel(int x, int y, unsigned member) {
    g.panel(x, y, 9, 0xA);
    const unsigned char* const record = At(at::kCharRecords + (member & 0xFF) * at::kCharStride);
    g.text_draw_at(x + 0x26, y + 7, 0, 0xFF, At(at::kEquipLabel));
    g.stat_icon(0, x + 7, y + 5, 0x10, 0x10, 0x80);
    const unsigned char* const names[6] = {
        At(0x657450 + record[0x12] * 28u), At(0x657D68 + record[0x13] * 26u), At(0x657D68 + record[0x14] * 26u),
        At(0x657D68 + record[0x15] * 26u), At(0x658450 + record[0x16] * 24u), At(0x658450 + record[0x17] * 24u),
    };
    for (unsigned i = 0; i < 6; ++i) {
        g.icon8(x + 5, y + static_cast<int>(13 * i) + 0x1C, At(0x663660)[i], 0);
        const unsigned n = g.char_count(names[i]);
        g.text_draw_at(x + 0x11, y + static_cast<int>(13 * i) + 0x1A, 0, static_cast<int>(n), names[i]);
    }
}

// original 0x573CE0 (PSX 0x801D8E08): a cursor box - a LINE_F3 (0x5A7670) from
// (x + 4, y) down the left, (x, y + 4) to (x, y + h - 1), then a LINE_F4 round
// the rest, (x + 4, y) (x + w - 1, y) (x + w - 1, y + h - 1) (x, y + h - 1), x y
// w h as 16-bit. Its colour: shade in the channels flags bits 2, 1, 0 name
// (red, green, blue), the rest 0; shade 0xFF, or blinking with Frame_Counter
// bits 1..3 when blink's low byte is set.
extern "C" void __cdecl Menu_DrawCursorBox(int x, int y, int w, int h, int blink, int flags) {
    unsigned char shade;
    if (blink & 0xFF) {
        const unsigned char fc = At(at::kFrameCounter)[0];
        shade = (fc & 8) ? static_cast<unsigned char>(((fc & 6) << 5) + 0x3F)
                         : static_cast<unsigned char>(((~fc & 6) << 5) + 0x3F);
    } else {
        shade = 0xFF;
    }
    const auto f = static_cast<unsigned char>(flags);
    const unsigned char c0 = (f & 1) ? shade : 0, c1 = (f & 2) ? shade : 0, c2 = (f & 4) ? shade : 0;
    unsigned char* p = Next();
    g.set_line_f3(p);
    const std::int32_t xs = U16(static_cast<std::uint32_t>(x)), ys = U16(static_cast<std::uint32_t>(y));
    const std::int32_t hs = U16(static_cast<std::uint32_t>(h));
    const float fx4 = F(xs + 4), fy = F(ys), fx = F(xs), fyh = F(hs + ys - 1);
    p[4] = c2;
    Put(p + 8, fx4);
    Put(p + 0xC, fy);
    p[5] = c1;
    p[6] = c0;
    Put(p + 0x14, fx);
    Put(p + 0x18, F(ys + 4));
    Put(p + 0x20, fx);
    Put(p + 0x24, fyh);
    g.commit(1, 0x2C);
    p = Next();
    g.set_line_f4(p);
    const float fxw = F(U16(static_cast<std::uint32_t>(w)) + xs - 1);
    Put(p + 8, fx4);
    Put(p + 0x2C, fx);
    Put(p + 0x30, fyh);
    Put(p + 0xC, fy);
    Put(p + 0x18, fy);
    Put(p + 0x24, fyh);
    p[4] = c2;
    p[5] = c1;
    p[6] = c0;
    Put(p + 0x20, fxw);
    Put(p + 0x14, fxw);
    g.commit(1, 0x38);
}

// original 0x574610 (PSX 0x801DA2A8): the money box - the box (x + 3, y + 3,
// 0x65, 0x10), value in "%7d" in the 12 x 12 font at (x + 4, y + 4), the
// label 0x66A31C at (x + 0x58, y + 4), then three piece lists across it. The
// third argument is not read (every caller passes 0).
extern "C" void __cdecl Menu_DrawMoneyBox(int x, int y, int, unsigned value) {
    g.box(x + 3, y + 3, 0x65, 0x10, 0, At(at::kColour)[0]);
    g.sprintf_(PrintBuf(), Format(at::kFmt7d), value);
    g.font12(x + 4, y + 4, 0, reinterpret_cast<const unsigned char*>(PrintBuf()));
    g.text_draw_at(x + 0x58, y + 4, 0, 0xFF, At(at::kMoneyLabel));
    g.pieces(x, y, At(0x6633A8), 0);
    for (unsigned i = 0; i < 11; ++i) g.pieces(x + static_cast<int>(i * 8), y, At(0x6633B4), 0);
    g.pieces(x, y, At(0x6633C0), 0);
}

// original 0x575430 (PSX 0x801DB988 and three more overlays' copies): a
// member's panel in the weapon shop - the box (x + 3, y + 3, 0x6C, 0x30), kind
// 1 (dark) when the member cannot equip the item (its bit in the equip mask of
// the slot's category: slot 1 weapons, 2..4 armour, 5, 10, 11 accessories,
// else consumables); three numbers in the 8 x 8 font 15 apart - when it can,
// what 0x590960 answers the stats would become, each in its colour (0, 2 or
// 3), from its first, second and FOURTH outputs; when it cannot, the record's
// u16s +0x24, +0x26, +0x28 in colour 7; the member's icon (record +9), dark
// when it cannot; the three stat labels, greyed (7) when it cannot; and the
// piece list 0x663528.
extern "C" void __cdecl Shop_DrawMemberStats(int x, int y, unsigned member, unsigned slot, unsigned item) {
    static const unsigned char kCategory[11] = {1, 2, 2, 2, 3, 0, 0, 0, 0, 3, 3};   // 0x575684 by way of 0x575674
    const unsigned m = member & 0xFF;
    const unsigned char* const record = At(at::kCharRecords + m * at::kCharStride);
    unsigned char colour = 0;
    const unsigned s1 = (slot & 0xFF) - 1;
    const unsigned char category = s1 > 10 ? 0 : kCategory[s1];
    const unsigned mask = g.equip_mask(category, item);
    const unsigned char cannot = ((1u << (m & 31)) & mask & 0xFF) == 0 ? 1 : 0;
    g.box(x + 3, y + 3, 0x6C, 0x30, cannot, At(at::kColour)[0]);
    if (!cannot) {
        unsigned char colours[4] = {0, 0, 0, 0};
        unsigned short stats[4] = {0, 0, 0, 0};
        g.equip_compare(member, (slot & ~0xFFu) | ((slot - 1) & 0xFF), item, colours, stats);
        stats[2] = stats[3];
        colours[2] = colours[3];
        for (unsigned i = 0; i < 3; ++i) {
            g.sprintf_(PrintBuf(), Format(at::kFmt3d), static_cast<unsigned>(stats[i]));
            g.font8(x + 0x2C, y + static_cast<int>(15 * i) + 8, colours[i],
                    reinterpret_cast<const unsigned char*>(PrintBuf()));
        }
    } else {
        colour = 7;
        static const unsigned kStat[3] = {0x24, 0x26, 0x28};
        static const int kY[3] = {8, 0x17, 0x26};
        for (unsigned i = 0; i < 3; ++i) {
            g.sprintf_(PrintBuf(), Format(at::kFmt3d), static_cast<unsigned>(Word(record + kStat[i])));
            g.font8(x + 0x2C, y + kY[i], 7, reinterpret_cast<const unsigned char*>(PrintBuf()));
        }
    }
    g.item_icon(x + 0x46, y + 2, record[9], cannot);
    g.text_draw_at(x + 5, y + 6, colour, 0xFF, At(at::kStatLabels[0]));
    g.text_draw_at(x + 5, y + 0x15, colour, 0xFF, At(at::kStatLabels[1]));
    g.text_draw_at(x + 5, y + 0x24, colour, 0xFF, At(at::kStatLabels[2]));
    g.pieces(x, y, At(0x663528), 0);
}

// original 0x575690 (PSX 0x801DBCBC): the menu backdrop (docs/menu-screens.md
// section 2) - ten columns 0x20 apart, each tiled down with SPRTs of the
// rectangles 0x663874 in the order of the pattern 0x663920 from kind's start
// (0x66396C) until 0xF0 deep, the pattern carrying on across each pair of
// columns; CLUT (0x30 / 0x50 / 0x40 / 0x60, 0x1E2) by kind; page (0x340,
// 0x100). kind is Config's "Background" byte 0x903A5B, which Config keeps in
// 0..3 (0x461239 / 0x46126D).
//
// Not as the original: a kind of 4 or more reads the four CLUTs' array past
// its end - into the original's own stack frame and its caller's - which C++
// cannot reproduce; ours aborts loudly there (CLAUDE.md rule 4). Only a save
// with that byte corrupted would reach it.
extern "C" void __cdecl Menu_DrawBackdrop(unsigned kind) {
    PiecePage(0x340);
    std::uint16_t cluts[4];
    cluts[0] = static_cast<std::uint16_t>(g.get_clut(0x30, 0x1E2));
    cluts[1] = static_cast<std::uint16_t>(g.get_clut(0x50, 0x1E2));
    cluts[2] = static_cast<std::uint16_t>(g.get_clut(0x40, 0x1E2));
    cluts[3] = static_cast<std::uint16_t>(g.get_clut(0x60, 0x1E2));
    const unsigned k = kind & 0xFF;
    if (k >= 4)
        bof3::Fatal("Menu_DrawBackdrop: kind %u - the original reads its stack past the four CLUTs here "
                    "(Config keeps 0x903A5B in 0..3)",
                    k);
    const std::uint16_t clut = cluts[k];
    std::int32_t column = 0;
    for (int outer = 0; outer < 5; ++outer) {
        const unsigned char* pattern = At(at::kBackdropPattern + At(at::kBackdropStart)[k]);
        for (int inner = 0; inner < 2; ++inner) {
            const float fcolumn = F(S16(static_cast<std::uint32_t>(column)));
            std::uint32_t row = 0;
            do {
                unsigned char* const p = Next();
                g.set_sprt(p);
                g.set_semi(p, 0);
                Put(p + 8, fcolumn);
                SetWord(p + 0x16, clut);
                Put(p + 0xC, F(S16(row)));
                const unsigned char* const r = At(at::kBackdropRects + *pattern++ * 4u);
                p[0x14] = r[0];
                p[0x15] = r[1];
                SetWord(p + 0x18, r[2]);
                SetWord(p + 0x1A, r[3]);
                p[4] = p[5] = p[6] = 0x80;
                g.commit(1, 0x1C);
                row += r[3];
            } while (S16(row) < 0xF0);
            column += 0x20;
        }
    }
}

// original 0x5759C0: the item list window (the field menu's Items, a shop's
// sell list). The window record: +4 / +6 x and y, +8 the Item_CanUse mode, +9
// greyed, +0xA the category, +0xB the top index, +0xC a marked index, +0xD the
// cursor, +0x10 a word of two arrow flags and a countdown, +0x12 the scroll
// state (Menu_ListScroll).
//
// The box (x + 3, y + 3, 0x99, 0x9A); the scroll step, whose offset (s8)
// shifts the rows and whose moving flag draws a tenth row; then each row, 13
// apart, whose item is not 0: Menu_DrawItemRow, colour 7 (and dim) when
// greyed or not usable, 2 on the cursor; the cursor's and the marked row drawn
// 2 higher over a dim shadow (none for a greyed row). The count is 1 for
// category 4 (key items), else the counts array's. While the scroll state is
// 0xFn the marks are not shifted with the offset. Then the title and footer
// boxes, the category's label centred, "%3d/%3d" of 0x591A80 and the
// category's room (0x20 key items, else 0x80), the arrows by +0x10's bits 1
// and 0, the countdown in +0x10's high nibble stepped down (the word zeroed
// when it runs out), the frame's pieces and the scroll bar.
extern "C" void __cdecl Menu_DrawItemList(unsigned char* window) {
    unsigned char* const w = window;
    g.box(Word(w + 4) + 3, Word(w + 6) + 3, 0x99, 0x9A, w[9], At(at::kColour)[0]);
    unsigned char offset = 0, moving = 0;
    const unsigned char top = g.list_scroll(w + 0xB, &offset, &moving, w + 0x12);
    const unsigned category = w[0xA];
    const unsigned char* items = Ptr(at::kInventory + category * 4) + top;
    const unsigned char* counts = Ptr(at::kCounts + category * 4) + top;
    std::uint32_t row = static_cast<std::uint16_t>(static_cast<signed char>(offset) + Word(w + 6)) + 0x1Au;
    const unsigned rows = moving + 9u;
    for (unsigned char i = 0; i < rows; ++i) {
        if (items[0] != 0) {
            unsigned char dim;
            if (w[9] == 0) {
                const unsigned char member = At(at::kMemberRecord)[At(at::kPartyLists)[static_cast<signed char>(
                    At(at::kMenuMember)[0])]];
                dim = g.can_use(w[8], member, w[0xA], items[0]) ? 0 : 1;
            } else {
                dim = 1;
            }
            unsigned char colour = dim ? 7 : 0;
            const unsigned char adj = (Word(w + 0x12) & 0xFFF0) != 0xF0 ? moving : 0;
            const unsigned mark = w[0xD] + static_cast<unsigned>(adj);
            const unsigned here = w[0xB] + static_cast<unsigned>(i);
            if (here == mark) colour = 2;
            const unsigned char row_category = w[0xA];
            const unsigned char count = row_category == 4 ? 1 : counts[0];
            if (here == mark || here == w[0xC] + static_cast<unsigned>(adj)) {
                if (colour != 7)
                    g.item_row(Word(w + 4) + 7, static_cast<int>(row), 7, row_category, items[0], count, 1);
                g.item_row(Word(w + 4) + 7, static_cast<int>(row) - 2, colour, w[0xA], items[0], count, dim);
            } else {
                g.item_row(Word(w + 4) + 7, static_cast<int>(row), colour, row_category, items[0], count, dim);
            }
        }
        ++items;
        ++counts;
        row += 0xD;
    }

    g.box(Word(w + 4) + 3, Word(w + 6) + 3, 0x99, 0x14, w[9], At(at::kColour)[0]);
    g.box(Word(w + 4) + 3, Word(w + 6) + 0x92, 0x99, 8, w[9], At(at::kColour)[0]);
    {
        const unsigned char grey = w[9] ? 7 : 0;
        const int label_y = Word(w + 6) + 7;
        const unsigned char* const label = Ptr(at::kCategoryLabels + w[0xA] * 4u);
        const unsigned n = g.char_count(label);
        g.text_draw_at(static_cast<std::uint16_t>(6 * (13 - static_cast<int>(n)) + Word(w + 4)), label_y, grey, 0x10,
                       label);
    }
    {
        const unsigned char cat = w[0xA];
        const unsigned room = cat != 4 ? 0x80 : 0x20;
        const unsigned have = g.count_category(cat) & 0xFF;
        g.sprintf_(PrintBuf(), Format(at::kFmt3d3d), have, room);
        const unsigned char grey = w[9] ? 7 : 0;
        g.font8(Word(w + 4) + 0x55, Word(w + 6) + 0x92, grey, reinterpret_cast<const unsigned char*>(PrintBuf()));
    }
    g.pieces(Word(w + 4), Word(w + 6), At((w[0x10] & 2) ? 0x66349C : 0x663484), 1);
    g.pieces(Word(w + 4), Word(w + 6), At((w[0x10] & 1) ? 0x6634A8 : 0x663490), 1);
    {
        const unsigned t = Word(w + 0x10);
        SetWord(w + 0x10, (t & 0xF0) ? t - 0x10 : 0);
    }
    for (unsigned b = 0; b < 10; ++b) g.piece(Word(w + 4) + static_cast<int>(b * 8) + 0x28, Word(w + 6), 1, 1);
    for (unsigned b = 0; b < 15; ++b) g.piece(Word(w + 4), Word(w + 6) + static_cast<int>(b * 8) + 0x18, 4, 1);
    g.piece(Word(w + 4) + 0x90, Word(w + 6) + 0x18, 0x16, 1);
    for (unsigned b = 0; b < 12; ++b) g.piece(Word(w + 4) + 0x90, Word(w + 6) + static_cast<int>(b * 8) + 0x28, 0x17, 1);
    g.piece(Word(w + 4) + 0x90, Word(w + 6) + 0x88, 0x18, 1);
    g.piece(Word(w + 4), Word(w + 6) + 0x90, 0x19, 1);
    for (unsigned b = 0; b < 9; ++b) g.piece(Word(w + 4) + static_cast<int>(b * 8) + 8, Word(w + 6) + 0x90, 0x1A, 1);
    g.piece(Word(w + 4) + 0x50, Word(w + 6) + 0x90, 0x1B, 1);
    for (unsigned b = 0; b < 6; ++b) g.piece(Word(w + 4) + static_cast<int>(b * 8) + 0x58, Word(w + 6) + 0x90, 0x1C, 1);
    g.piece(Word(w + 4) + 0x88, Word(w + 6) + 0x90, 0x1D, 1);
    if (w[0xA] == 4) {
        g.piece(Word(w + 4) + 8, Word(w + 6), 0x32, 1);
        g.piece(Word(w + 4) + 0x90, Word(w + 6), 0x33, 1);
    }
    const unsigned char cat = w[0xA];
    g.scroll_bar(Ptr(at::kInventory + cat * 4u), w[0xB], Word(w + 4) + 0x90, Word(w + 6) + 0x18, 9,
                 cat == 4 ? 0x20 : 0x80, 0x74);
}

// original 0x57DBF0: one row of an item list - the kind's icon (0x591720 into
// 0x663D60) at (x, y + 2), dim as given; the name (0x591680) at (x + 10, y) in
// colour; and "*%2d" of count at (x + 0x6D, y + 2) when count (a byte) is 2
// or more.
extern "C" void __cdecl Menu_DrawItemRow(int x, int y, int colour, unsigned category, unsigned id, unsigned count,
                                         int dim) {
    const unsigned kind = g.item_kind(category, id) & 0xFF;
    g.icon8(x, y + 2, At(at::kRowIcons)[kind], dim);
    const unsigned char* const name = g.item_name(category, id);
    const unsigned n = g.char_count(name);
    g.text_draw_at(x + 0xA, y, colour, static_cast<int>(n), name);
    const auto c = static_cast<unsigned char>(count);
    if (c > 1) {
        g.sprintf_(PrintBuf(), Format(at::kFmtCount), static_cast<unsigned>(c));
        g.font8(x + 0x6D, y + 2, colour, reinterpret_cast<const unsigned char*>(PrintBuf()));
    }
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
    const std::int32_t sx = S16(static_cast<std::uint32_t>(x)), sy = S16(static_cast<std::uint32_t>(y));
    for (std::int32_t i = 1; i <= t; ++i) {
        if (items[i - 1] == 0) continue;
        unsigned char* const p = Next();
        g.set_tile(p);
        Put(p + 8, F(sx + 4));
        const std::int32_t a = IDiv((i - 1) * hgt, t) + sy + 3;
        Put32(p + 0x14, 0x40400000u);
        p[4] = 0x80;
        p[5] = 0;
        p[6] = 0;
        const std::int32_t b = IDiv(i * hgt, t) + sy + 3;
        Put(p + 0xC, F(a));
        Put(p + 0x18, static_cast<float>(static_cast<double>(b) - static_cast<double>(a)));
        g.commit(1, 0x1C);
    }
    unsigned char* const p = Next();
    g.set_poly_ft4(p);
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
    SetWord(p + 0x26, g.get_tpage(0, 0, 0x340, 0x100));
    SetWord(p + 0x16, g.get_clut(0xB0, 0x1E1));
    p[4] = p[5] = p[6] = 0x80;
    g.commit(1, 0x48);
}

// ===========================================================================
// The shop's windows

// original 0x59B580 (PSX 0x801E2DBC): a shop's buy list. The window record:
// +4 / +6 x and y, +0xA greyed, +0xB the price percent, +0xC the cursor, dword
// +0x20 the shop's list (a count, then (category, id) pairs). The box (x + 3,
// y + 3, 0xAD, 0x9F) greyed as +0xA; each entry 13 apart: its icon, name and
// "%6dZ" of the price scaled (0x5830D0), colour 7 (and a dim icon) when greyed
// or the price is more than the money 0x904058; the cursor's row 2 higher
// over a shadow in colour 7 (none when greyed or dear). The count is read
// again after each entry. Then the border (x, y, 0x14, 0x13).
extern "C" void __cdecl Shop_DrawBuyList(unsigned char* window) {
    unsigned char* const w = window;
    g.box(Word(w + 4) + 3, Word(w + 6) + 3, 0xAD, 0x9F, w[0xA], At(at::kColour)[0]);
    const unsigned char* const list = Ptr(Addr(w + 0x20));
    std::uint32_t text_y = static_cast<std::uint16_t>(Word(w + 6) + 8);
    std::uint32_t row_y = text_y + 3;
    if (list[0] != 0) {
        unsigned char i = 0;
        do {
            const unsigned char category = list[1 + 2 * i], id = list[2 + 2 * i];
            const unsigned percent = w[0xB];   // pushed before Item_BasePrice, for the scale after it
            const unsigned price = g.item_price(category, id) & 0xFFFF;
            const std::uint32_t cost = g.price_scale(price, percent);
            const unsigned char kind = static_cast<unsigned char>(g.item_kind(category, id));
            const unsigned char* const name = g.item_name(category, id);
            unsigned char colour, dim;
            if (w[0xA] == 0 && static_cast<std::uint32_t>(Long(At(at::kGold))) >= cost) {
                colour = 0;
                dim = 0;
            } else {
                colour = 7;
                dim = 1;
            }
            g.sprintf_(PrintBuf(), Format(at::kFmt6dZ), cost);
            if (i == w[0xC]) {
                if (colour == 0) {
                    g.icon8(Word(w + 4) + 4, static_cast<int>(row_y), At(at::kBuyIcons)[kind], 1);
                    const unsigned n = g.char_count(name);
                    g.text_draw_at(Word(w + 4) + 0xE, static_cast<int>(text_y), 7, static_cast<int>(n), name);
                    g.font8(Word(w + 4) + 0x71, static_cast<int>(row_y), 7,
                            reinterpret_cast<const unsigned char*>(PrintBuf()));
                }
                text_y -= 2;
                row_y -= 2;
            }
            g.icon8(Word(w + 4) + 4, static_cast<int>(row_y), At(at::kBuyIcons)[kind], dim);
            const unsigned n = g.char_count(name);
            g.text_draw_at(Word(w + 4) + 0xE, static_cast<int>(text_y), colour, static_cast<int>(n), name);
            g.font8(Word(w + 4) + 0x71, static_cast<int>(row_y), colour,
                    reinterpret_cast<const unsigned char*>(PrintBuf()));
            if (i == w[0xC]) {
                text_y += 2;
                row_y += 2;
            }
            text_y += 0xD;
            row_y += 0xD;
            ++i;
        } while (i < list[0]);
    }
    g.border(Word(w + 4), Word(w + 6), 0x14, 0x13);
}

namespace {

// The detail windows' frame: page (0x380, 0x100); 20 pieces 0x29 along the top
// and 0x3B along y + bottom; `rows` pieces 0x2B and 0x2C down the sides at x
// and x + 0xA8; the corners 0x28, 0x2A, 0x3A, 0x3C.
void DetailFrame(const unsigned char* w, int bottom, unsigned rows) {
    PiecePage(0x380);
    for (unsigned b = 0; b < 0x14; ++b) {
        g.piece(Word(w + 4) + static_cast<int>(b * 8) + 8, Word(w + 6), 0x29, 0);
        g.piece(Word(w + 4) + static_cast<int>(b * 8) + 8, Word(w + 6) + bottom, 0x3B, 0);
    }
    for (unsigned b = 0; b < rows; ++b) {
        g.piece(Word(w + 4), Word(w + 6) + static_cast<int>(b * 8) + 8, 0x2B, 0);
        g.piece(Word(w + 4) + 0xA8, Word(w + 6) + static_cast<int>(b * 8) + 8, 0x2C, 0);
    }
    g.piece(Word(w + 4), Word(w + 6), 0x28, 0);
    g.piece(Word(w + 4) + 0xA8, Word(w + 6), 0x2A, 0);
    g.piece(Word(w + 4), Word(w + 6) + bottom, 0x3A, 0);
    g.piece(Word(w + 4) + 0xA8, Word(w + 6) + bottom, 0x3C, 0);
}

}  // namespace

// original 0x59B820 (PSX 0x801E313C): the buy list's detail - the entry at
// +0xB of the list: the box (x + 3, y + 3, 0xAA, 0x3C), its icon, name,
// "%7dZ" of quantity (+0xC) times the price scaled by +0xA percent, "*%2d" of
// the quantity, system messages 0x27 and 0x28 with "*%2d" of the count held
// and equipped (0x5919B0 where 0 and 1), then the frame 7 cells deep.
extern "C" void __cdecl Shop_DrawBuyDetail(unsigned char* window) {
    unsigned char* const w = window;
    const unsigned char* const list = Ptr(Addr(w + 0x20));
    const unsigned sel = w[0xB];
    const unsigned char category = list[1 + 2 * sel], id = list[2 + 2 * sel];
    const unsigned percent = w[0xA];
    const unsigned price = g.item_price(category, id) & 0xFFFF;
    const std::uint32_t cost = g.price_scale(price, percent);
    const unsigned char* const name = g.item_name(category, id);
    g.box(Word(w + 4) + 3, Word(w + 6) + 3, 0xAA, 0x3C, 0, At(at::kColour)[0]);
    const unsigned kind = g.item_kind(category, id) & 0xFF;
    g.icon8(Word(w + 4) + 4, Word(w + 6) + 7, At(at::kBuyDetailIcons)[kind], 0);
    const unsigned n = g.char_count(name);
    g.text_draw_at(Word(w + 4) + 0xE, Word(w + 6) + 4, 0, static_cast<int>(n), name);
    g.sprintf_(PrintBuf(), Format(at::kFmt7dZ), w[0xC] * cost);
    g.font8(Word(w + 4) + 0x69, Word(w + 6) + 0x12, 0, reinterpret_cast<const unsigned char*>(PrintBuf()));
    g.sprintf_(PrintBuf(), Format(at::kFmtCount), static_cast<unsigned>(w[0xC]));
    g.font8(Word(w + 4) + 0x89, Word(w + 6) + 7, 0, reinterpret_cast<const unsigned char*>(PrintBuf()));
    const unsigned char* const held = g.msg_system_ptr(0x27);
    g.text_draw_at(Word(w + 4) + 0xE, Word(w + 6) + 0x20, 0, 0xFF, held);
    const unsigned owned = g.count_owned(category, id, 0) & 0xFFFF;
    g.sprintf_(PrintBuf(), Format(at::kFmtCount), owned);
    g.font8(Word(w + 4) + 0x89, Word(w + 6) + 0x23, 0, reinterpret_cast<const unsigned char*>(PrintBuf()));
    const unsigned char* const equipped = g.msg_system_ptr(0x28);
    g.text_draw_at(Word(w + 4) + 0xE, Word(w + 6) + 0x2D, 0, 0xFF, equipped);
    const unsigned worn = g.count_owned(category, id, 1) & 0xFFFF;
    g.sprintf_(PrintBuf(), Format(at::kFmtCount), worn);
    g.font8(Word(w + 4) + 0x89, Word(w + 6) + 0x30, 0, reinterpret_cast<const unsigned char*>(PrintBuf()));
    DetailFrame(w, 0x38, 6);
}

// original 0x59BBC0: the sell detail - item (+0xA category, +0xB id): the box
// (x + 3, y + 3, 0xAA, 0x1C), its icon, name, "%6dZ" of the price a shop pays
// (0x583100 with +0xD), "%7dZ" of it times the quantity +0xC and "*%2d" of the
// quantity, then the frame 3 cells deep.
extern "C" void __cdecl Shop_DrawSellDetail(unsigned char* window) {
    unsigned char* const w = window;
    const unsigned char id = w[0xB], category = w[0xA], flag = w[0xD];
    const std::uint32_t unit = g.sell_price(category, id, flag);
    const unsigned char* const name = g.item_name(category, id);
    g.box(Word(w + 4) + 3, Word(w + 6) + 3, 0xAA, 0x1C, 0, At(at::kColour)[0]);
    const unsigned kind = g.item_kind(category, id) & 0xFF;
    g.icon8(Word(w + 4) + 7, Word(w + 6) + 6, At(at::kSellIcons)[kind], 0);
    const unsigned n = g.char_count(name);
    g.text_draw_at(Word(w + 4) + 0x11, Word(w + 6) + 4, 0, static_cast<int>(n), name);
    g.sprintf_(PrintBuf(), Format(at::kFmt6dZ), unit);
    g.font8(Word(w + 4) + 0x71, Word(w + 6) + 7, 0, reinterpret_cast<const unsigned char*>(PrintBuf()));
    g.sprintf_(PrintBuf(), Format(at::kFmt7dZ), w[0xC] * unit);
    g.font8(Word(w + 4) + 0x69, Word(w + 6) + 0x12, 0, reinterpret_cast<const unsigned char*>(PrintBuf()));
    g.sprintf_(PrintBuf(), Format(at::kFmtCount), static_cast<unsigned>(w[0xC]));
    g.font8(Word(w + 4) + 0xE, Word(w + 6) + 0x12, 0, reinterpret_cast<const unsigned char*>(PrintBuf()));
    DetailFrame(w, 0x18, 2);
}

// ===========================================================================

void MenuWindows_Inject() {
    // The two divergences whose patch sites are inside these bodies: what
    // their modules put in, ours does the same (see the top of this file).
    g = kOriginals;
    if (const auto label = MenuVerbs_ActiveLabel()) g.verb_label = label;
    if (const auto line = YesNoLayout_ActiveLine()) g.yes_no_line = line;
    g_yes_no_div = YesNoLayout_StopsMoved();
    bof3::Log("menu_windows: DIV-0018 label %s, DIV-0027 line %s, hand stops %s",
              g.verb_label == kOriginals.verb_label ? "original" : "MenuVerbs_DrawLabel",
              g.yes_no_line == kOriginals.yes_no_line ? "original" : "YesNo_Line", g_yes_no_div ? "moved" : "original");

    if (bof3::WantsShadow("menu_windows")) SelfTest();

    BOF3_INJECT(Msg_OpenSystem);
    BOF3_INJECT(MsgBox_DrawArrow);
    BOF3_INJECT(Text_DrawFont12);
    BOF3_INJECT(Text_DrawFont8);
    BOF3_INJECT(Window_Kind2List);
    BOF3_INJECT(Menu_DrawEquipPanel);
    BOF3_INJECT(Menu_DrawCursorBox);
    BOF3_INJECT(Menu_DrawBigIcon);
    BOF3_INJECT(Menu_DrawItemIcon);
    BOF3_INJECT(Menu_DrawExpBar);
    BOF3_INJECT(Menu_DrawMoneyBox);
    BOF3_INJECT(Menu_YesNo);
    BOF3_INJECT(Menu_DrawButtonRow);
    BOF3_INJECT(Item_BasePrice);
    BOF3_INJECT(Char_ExpForLevel);
    BOF3_INJECT(Menu_DrawTitleBox);
    BOF3_INJECT(Shop_DrawMemberStats);
    BOF3_INJECT(Menu_DrawBackdrop);
    BOF3_INJECT(Menu_DrawPanel);
    BOF3_INJECT(Menu_DrawItemList);
    BOF3_INJECT(Menu_DrawBorder);
    BOF3_INJECT(Menu_DrawBox);
    BOF3_INJECT(Menu_DrawIcon8);
    BOF3_INJECT(Menu_DrawOutline);
    BOF3_INJECT(Menu_DrawLine);
    BOF3_INJECT(Text_CharCount);
    BOF3_INJECT(Menu_PieceRect);
    BOF3_INJECT(Menu_DrawPiece);
    BOF3_INJECT(Menu_DrawPieces);
    BOF3_INJECT(Item_CanUse);
    BOF3_INJECT(Menu_DrawItemRow);
    BOF3_INJECT(Menu_DrawScrollBar);
    BOF3_INJECT(Menu_ListScroll);
    BOF3_INJECT(Shop_DrawBuyList);
    BOF3_INJECT(Shop_DrawBuyDetail);
    BOF3_INJECT(Shop_DrawSellDetail);
    BOF3_INJECT(Gpu_SetSprt8);
}
