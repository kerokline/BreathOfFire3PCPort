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
#include "game/widescreen.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_verbs.h"
#include "game/msgbox.h"
#include "game/menu_windows_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/shoulder_glyphs.h"
#include "game/text_pairs.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace menu_windows {

using move_script::At;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

bool g_yes_no_div = false;

}  // namespace menu_windows

using namespace menu_windows;

namespace {

using move_script::Long;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Addr(const void* p) { return Low32(p); }
unsigned char* Ptr(std::uint32_t at) { return menu_windows::At(static_cast<std::uint32_t>(move_script::Long(menu_windows::At(at)))); }

// rv::As<records::WindowRecord>(p) views a window record
// (docs/typed-records.md; WindowRecords
// 0x803160, the record each window handler is given). Only +0..+7 are
// typed: +8..+0x1F mean something else in each handler (typed-records 3).

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

}  // namespace

// ===========================================================================
// The message box's two small helpers

// ===========================================================================
// The two small fonts (known-defects.md D1 names both)

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

// original 0x57D860 (PSX 0x801B02A0): one piece of a panel - a SPRT at (x, y),
// 16-bit, of piece id's rectangle (Menu_PieceRect with flag bit 0), opaque,
// CLUT (0xB0, 0x1E1), or (0x80, 0x1E2) with flag bit 1, shade 0x80. Sets no
// draw mode (its callers do).
// ===========================================================================
// Icons, the button row, the chooser

// ===========================================================================
// The stat, item and price helpers

// ===========================================================================
// The panels

// ===========================================================================
// The shop's windows

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
    {
        // DIV-0165: after the self-test, which compared Capcom's unsigned words.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("MenuSignedXY", Low32(&menu_windows::g_signed_xy), &was, &is, 1);
        bof3::Log("DIV-0165    Menu_DrawBox and Menu_DrawIcon8 take x and y as signed words (on unless the line above says OFF)");
    }

}
