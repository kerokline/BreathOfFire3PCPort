// Moved here by tools/move_funcs.py from effect_2g.cpp, rest_3e.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_5d.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect/effect_kind_5d_callees.h"
#include "game/effect_2g.h"
#include "game/effect_2g_callees.h"
#include "game/effect_gte.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3e.h"
#include "game/rest_3e_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/exe_image.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_kind_5d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

U ULEffect2g(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect2g(U a) { return ULEffect2g(At(a)); }

U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2g.md section 6)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect2g(table + 4 * state)))();
}

}  // namespace

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using LD = long double;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U ULRest3e(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U ULRest3e(U a) { return ULRest3e(move_script::At(a)); }

unsigned W(U a) { return Word(At(a)); }

unsigned char& B(U a) { return At(a)[0]; }

U AddressOfRest3e(const void* p) { return Low32(p); }

LD I(std::int32_t v) { return static_cast<LD>(v); }

void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// fild of a dword then fstp: an integer stored as a single.
void StI(unsigned char* p, std::int32_t v) { StF(p, I(v)); }

unsigned char* Window(unsigned j) { return At(::bof3::Img(Rest3e_Windows_addr) + 8 * j); }

// The three windows' outlines at the size the step +9 gives (n = +9 * 16, at
// most each window's w and h); counted: how many of the six sides were at
// their full size.
unsigned Outlines() {
    unsigned full = 0;
    for (unsigned j = 0; j < 3; ++j) {
        const unsigned char* const w = Window(j);
        const unsigned n = (static_cast<unsigned>(S()[9]) << 4) & 0xFFFFu;
        unsigned width = n, height = n;
        if (Word(w + 4) <= n) {
            width = Word(w + 4);
            ++full;
        }
        if (Word(w + 6) <= n) {
            height = Word(w + 6);
            ++full;
        }
        if (((S()[7] >> j) & 1) == 0) SH_CALL(Window_DrawOutline)(Word(w), Word(w + 2), static_cast<int>(width), static_cast<int>(height));
    }
    return full;
}

// The FT4 the board and the marks are: page (0, 0, 0x2C0, 0x100), CLUT (clut
// x, 0x1E3), colour 0x80, the corners (x0, y0) .. (x1, y1) as singles.
unsigned char* Quad(int clut_x) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    rv::As<records::GpuPolyFT4>(p)->tpage = SH_CALL(Gpu_GetTPage)(0, 0, 0x2C0, 0x100);
    rv::As<records::GpuPolyFT4>(p)->clut = SH_CALL(Gpu_GetClut)(clut_x, 0x1E3);
    return p;
}

void Corners(unsigned char* p, std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1) {
    StI(p + 8, x0);
    StI(p + 0xC, y0);
    StI(p + 0x18, x1);
    StI(p + 0x1C, y0);
    StI(p + 0x28, x0);
    StI(p + 0x2C, y1);
    StI(p + 0x38, x1);
    StI(p + 0x3C, y1);
}

// DIVERGENCE DIV-0198 (issue #178): the list's first line below the box's y.
// Japan's code (the PC's, and PSP-EU's twin) adds 0x14 over a list box of y
// 80 / h 144; the Western PSX twin (psx-us SCENA10.EMI#0 0x801F7B50, `addiu
// $s3, $v1, 4`) adds 4 over the West's own box of y 96 / h 128 - both put
// the first part at y 100, the hand's row 0 (0x654894). A Western PSX
// disc-built image carries the West's box (DIV-0195's family, the map run
// 0x654876..0x654898 from the disc), so it takes the West's 4; the PC's
// .data, a Japanese or a PSP image keep 0x14. Read once, logged once.
unsigned ListFirstLine() {
    static int cached = -1;
    if (cached < 0) {
        const char* const build = bof3::ExeImage_MappedBuild();
        std::string error;
        const auto release = build == nullptr ? bof3x::input::sigma::Release::kJapan
                                              : bof3x::input::sigma::ReleaseOfSources(std::vector<std::string>{build}, error);
        if (!error.empty()) bof3::Fatal("DIV-0198 EffectKind5E_DrawList: the mapped image's build: %s", error.c_str());
        cached = release == bof3x::input::sigma::Release::kWest ? 4 : 0x14;
        bof3::Log("DIV-0198    EffectKind5E_DrawList: the parts list's first line at the box's y + %d (%s)", cached,
                  build == nullptr ? "the running .data is BOF3.exe's" : build);
    }
    return static_cast<unsigned>(cached);
}

namespace dock {

// The panel's texels: the 4-bit page at VRAM (0x2C0, 0x100), CLUT (0x20,
// 0x1E3); the panel box is 0x40 x 0x28 (0x654888's words 2, 3) and its quad
// is u 0xC0..0xFF, v 0x50..0x78. The icon column is u 0xC4..0xCE, the three
// rows' cells v 0x55 + 10 r, 10 high - the same places on every page read
// (the icons themselves sit one texel apart between pages, inside the cell).
constexpr unsigned kPageX = 0x2C0, kPageY = 0x100;

constexpr unsigned kU0 = 0xC0, kV0 = 0x50;

constexpr unsigned kBoxW = 0x40, kBoxH = 0x28;

constexpr unsigned kColU = 4, kColW = 11;          // u offsets 4..14 of the panel

constexpr unsigned kBandV = 5, kBandH = 10;        // row r's cell: v offsets 5 + 10 r ..

constexpr unsigned kRows = 3;

// The rows' buttons in Japan's terms: EffectKind5E_Pick's three tests.
constexpr std::uint16_t kRowBit[kRows] = {0x20, 0x40, 0x10};

// The six buttons in DIV-0051's order (config_text.cpp's kIcons), each with
// the colour 0x461C00 draws its stand-in in and the Config panel its icon.
enum Shape : unsigned char { kCircle, kCross, kTriangle, kSquare, kL1, kR1, kUnknown };

constexpr std::uint16_t kShapeBit[6] = {0x20, 0x40, 0x10, 0x80, 0x04, 0x08};

constexpr int kShapeColour[6] = {2, 1, 6, 5, 0, 0};

const char* const kShapeName[7] = {"circle", "cross", "triangle", "square", "L1", "R1", "?"};

// DIV-0051's icons: loc_build.py's ICONS_AT, after the UI 8 x 8 set (0xA00,
// 100 cells) and its blank, in kShapeBit's order (config_text.cpp's
// kIconCircle .. kIconR1; fishing_text.cpp names the same).
constexpr std::uint32_t kIconCircle = 0xA00 + 100 + 1;

// The port's stand-ins: 0x66A2FC's first six dwords, the one-byte strings
// 0x461C00 draws for circle, cross, triangle, square, L1, R1 (read with
// capstone, 2026-10-17: [0x66A2FC] for 0x20 .. [0x66A310] for 8).
constexpr ::bof3::image::Va kStandIns{0x66A2FC};

Shape ShapeOfBit(std::uint16_t bit) {
    for (unsigned k = 0; k < 6; ++k)
        if (kShapeBit[k] == bit) return static_cast<Shape>(k);
    return kUnknown;
}

// One cell's ink, row-major: true where the texel's index is 8 or more.
struct Cell {
    bool ink[kBandH][kColW];
};

// The rule (section 11 of docs/rest_3e.md gives it with the pages it was read
// on). The bright texels' bounding box, 5..11 wide and 5..10 high, then its top
// and bottom rows: both full and the centre empty - a square; both with ink at
// the two ends, the centre inked and the top not full - a cross; neither at the ends, each more than half the width -
// a circle; the top a third of the width at most and the bottom full - a
// triangle. Anything else is not one of the four.
Shape Classify(const Cell& c) {
    int x0 = kColW, x1 = -1, y0 = kBandH, y1 = -1;
    for (int r = 0; r < static_cast<int>(kBandH); ++r)
        for (int u = 0; u < static_cast<int>(kColW); ++u)
            if (c.ink[r][u]) {
                if (u < x0) x0 = u;
                if (u > x1) x1 = u;
                if (r < y0) y0 = r;
                if (r > y1) y1 = r;
            }
    if (x1 < 0) return kUnknown;
    const int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if (w < 5 || h < 5) return kUnknown;
    int top = 0, bottom = 0;
    for (int u = x0; u <= x1; ++u) {
        top += c.ink[y0][u];
        bottom += c.ink[y1][u];
    }
    const bool top_ends = c.ink[y0][x0] && c.ink[y0][x1], bottom_ends = c.ink[y1][x0] && c.ink[y1][x1];
    const bool centre = c.ink[(y0 + y1) / 2][(x0 + x1) / 2] || c.ink[(y0 + y1 + 1) / 2][(x0 + x1 + 1) / 2];
    if (top == w && bottom == w && !centre) return kSquare;
    if (top_ends && bottom_ends && centre && top < w) return kCross;
    if (!top_ends && !bottom_ends && 2 * top > w && 2 * bottom > w) return kCircle;
    if (3 * top <= w && bottom == w) return kTriangle;
    return kUnknown;
}

// A 4-bit texel of the panel's page in the VRAM shadow: the cell (0x2C0 + u /
// 4, 0x100 + v), its nibble u & 3 counted from the low one (D3d_PageTexel4's
// note: the converter reads the low nibble first).
unsigned PanelTexel(unsigned u, unsigned v) {
    const unsigned short cell = Gfx_VramShadow[((kPageY + v) << 10) + kPageX + (u >> 2)];
    return (cell >> ((u & 3) * 4)) & 0xFu;
}

Cell ReadCell(unsigned row) {
    Cell c;
    for (unsigned r = 0; r < kBandH; ++r)
        for (unsigned u = 0; u < kColW; ++u) c.ink[r][u] = PanelTexel(kU0 + kColU + u, kV0 + kBandV + kBandH * row + r) >= 8;
    return c;
}

// What a row shows: the page's cell as it is, another row's cell, or a glyph.
enum How : unsigned char { kKeep, kCellOf, kGlyph };

struct Row {
    How how;
    unsigned char from;   // kCellOf: the row whose cell; kGlyph: the Shape
};

// The plan for the three rows: `baked` the page's icons, `wanted` the buttons
// the rows' tests are on now. False when the page's icons are not three of
// the four shapes: then nothing is changed.
bool Plan(const Shape baked[kRows], const std::uint16_t wanted[kRows], Row out[kRows]) {
    for (unsigned r = 0; r < kRows; ++r)
        if (baked[r] == kUnknown) return false;
    for (unsigned r = 0; r < kRows; ++r) {
        const Shape want = ShapeOfBit(wanted[r]);
        if (want == kUnknown)
            bof3::Fatal("DockLegend: row %u's button is 0x%04X, not one of the six Sigma maps (docs/button-sigma.md 2)", r,
                        static_cast<unsigned>(wanted[r]));
        out[r] = {kKeep, 0};
        if (baked[r] == want) continue;
        out[r] = {kGlyph, static_cast<unsigned char>(want)};
        for (unsigned k = 0; k < kRows; ++k)
            if (baked[k] == want) {
                out[r] = {kCellOf, static_cast<unsigned char>(k)};
                break;
            }
    }
    return true;
}

// The panel's quad, or a piece of it: (x, y) w x h on screen, u0..u1 x v0..v1
// of the page, as the original's (CLUT x 0x20, colour 0x80, slot 1).
void PanelQuad(std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h, unsigned u0, unsigned v0, unsigned u1,
               unsigned v1) {
    unsigned char* const p = Quad(0x20);
    p[4] = p[5] = p[6] = 0x80;
    Corners(p, x, y, x + w, y + h);
    p[0x14] = static_cast<unsigned char>(u0);
    p[0x15] = static_cast<unsigned char>(v0);
    p[0x24] = static_cast<unsigned char>(u1);
    p[0x25] = static_cast<unsigned char>(v0);
    p[0x34] = static_cast<unsigned char>(u0);
    p[0x35] = static_cast<unsigned char>(v1);
    p[0x44] = static_cast<unsigned char>(u1);
    p[0x45] = static_cast<unsigned char>(v1);
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// The glyph of `s` with its top-left at (x, y), as the Config panel draws its
// cell: DIV-0051's icon under a Latin overlay, else the port's stand-in.
void Glyph(int x, int y, Shape s) {
    if (Lang_Latin()) {
        const std::uint32_t g = kIconCircle + s;
        const unsigned char code[3] = {static_cast<unsigned char>(0x80 | (g >> 8)), static_cast<unsigned char>(g), 0};
        SH_CALL(Text_DrawAt)(x, y, kShapeColour[s], 3, code);
        return;
    }
    const auto* text = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(ULRest3e(::bof3::Img(kStandIns.v) + 4 * s)));
    SH_CALL(Text_DrawAt)(x, y, kShapeColour[s], 3, text);
}

// The plan logged when it changes (a row's source, or the page read).
void LogPlan(bool readable, const Shape baked[kRows], const std::uint16_t wanted[kRows], const Row rows[kRows]) {
    static U last = 0xFFFFFFFFu;
    U key = readable ? 1u : 0u;
    for (unsigned r = 0; r < kRows; ++r)
        key = key * 512 + baked[r] * 64 + (readable ? rows[r].how * 8 + rows[r].from : 0);
    if (key == last) return;
    last = key;
    if (!readable) {
        bof3::Log("DockLegend the page's icons read %s / %s / %s, not three of the four shapes: drawn as the page has them",
                  kShapeName[baked[0]], kShapeName[baked[1]], kShapeName[baked[2]]);
        return;
    }
    char what[kRows][24];
    for (unsigned r = 0; r < kRows; ++r) {
        const Shape want = ShapeOfBit(wanted[r]);
        if (rows[r].how == kKeep) std::snprintf(what[r], sizeof what[r], "%s", kShapeName[want]);
        else if (rows[r].how == kCellOf) std::snprintf(what[r], sizeof what[r], "%s (row %u's)", kShapeName[want], rows[r].from);
        else std::snprintf(what[r], sizeof what[r], "%s (%s)", kShapeName[want], Lang_Latin() ? "icon" : "stand-in");
    }
    bof3::Log("DockLegend page %s / %s / %s, shown %s / %s / %s", kShapeName[baked[0]], kShapeName[baked[1]],
              kShapeName[baked[2]], what[0], what[1], what[2]);
}

}  // namespace dock

}  // namespace

// original 0x47F2B0 (Effect_KindHandlers[0x5D], hidden in 0x47F040):
// jmp [EffectKind5D_States + +1 * 4], unbounded. Its states are catalog part 6
// rows (0x47F2D0..), no group's this round.
extern "C" void __cdecl EffectKind5D_Run(void) {
    Dispatch("EffectKind5D_Run", AddressOf(EffectKind5D_States), EffectKind5D_States_count, 1);
}

// original 0x47F2D0 (EffectKind5D_States[0]; PSX twin 0x801F6FAC):
// Sprite_SetAnimationBank(0x1EA); +0x3E = 0x64 (a word), +0x48, +0x24, +0x2A,
// +9 = 0, +0xA = 0x1E, +0x29 = 1; Sprite_SetAnimation(+6 - 0x4E); +1 = 1.
extern "C" void __cdecl EffectKind5D_Start(void) {
    SH_CALL(Sprite_SetAnimationBank)(0x1EA);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, 0x64);
    S()[0x48] = 0;
    S()[0x24] = 0;
    S()[0x2A] = 0;
    S()[9] = 0;
    S()[0xA] = 0x1E;
    S()[0x29] = 1;
    SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(S()[6] - 0x4E));
    S()[1] = 1;
}

// original 0x47F340 (EffectKind5D_States[1]; PSX twin 0x801F7068): the three
// windows' outlines growing; +9 up; when all six sides were at full size, +1 =
// 2.
extern "C" void __cdecl EffectKind5D_Open(void) {
    const unsigned full = Outlines();
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (full == 6) S()[1] = 2;
}

// original 0x47F3E0 (EffectKind5D_States[2]; PSX twin 0x801F719C): the shown
// windows' frames (Window_DrawFrame); window 0 shown: the sprite placed at
// its corner (+0x2E = 0x654890's byte + x, +0x30 = 0x654891's + y),
// Sprite_ScriptTick, Sprite_QueueOverlay; window 1 shown: the item +6's name
// at (x + 4, y + 2) of window 1 and its count (Inventory_Count(0, +6, 0)) at
// (x + 0x48, y + 2); the board (EffectKind5D_DrawBoard at the board frame's
// corner + 1, 0x80 x 0x68). Then +0xA down when not 0; at 0, any held button
// sets +1 = 3.
extern "C" void __cdecl EffectKind5D_Show(void) {
    for (unsigned j = 0; j < 3; ++j) {
        const unsigned char* const w = Window(j);
        if (((S()[7] >> j) & 1) == 0) SH_CALL(Window_DrawFrame)(Word(w), Word(w + 2), w[4], w[6]);
    }
    if ((S()[7] & 1) == 0) {
        rv::As<records::EffectObject>(S())->screen_x = Rest3e_SpriteDx + Rest3e_Windows;
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->screen_y, Rest3e_SpriteDy + W(::bof3::Img(at::kWindows.v) + 2));
        SH_CALL(Sprite_ScriptTick)();
        SH_CALL(Sprite_QueueOverlay)();
    }
    if ((S()[7] & 2) == 0) {
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, S()[6]);
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(at::kText.v))), reinterpret_cast<const char*>(At(::bof3::Img(at::kStringFormat.v))), name);
        SH_CALL(Text_DrawAt)(static_cast<int>(ULRest3e(::bof3::Img(at::kWindows.v) + 8) + 4), static_cast<int>((W(::bof3::Img(at::kWindows.v) + 0xA) + 2) & 0xFFFFu), 0, 8, At(::bof3::Img(at::kText.v)));
        const unsigned count = SH_CALL(Inventory_Count)(0, S()[6], 0);
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(at::kText.v))), reinterpret_cast<const char*>(At(::bof3::Img(at::kCountFormat.v))), count & 0xFFFFu);
        SH_CALL(Text_DrawAt)(static_cast<int>(ULRest3e(::bof3::Img(at::kWindows.v) + 8) + 0x48), static_cast<int>((W(::bof3::Img(at::kWindows.v) + 0xA) + 2) & 0xFFFFu), 0, 2, At(::bof3::Img(at::kText.v)));
    }
    SH_CALL(EffectKind5D_DrawBoard)(static_cast<int>(ULRest3e(::bof3::Img(at::kBoard.v)) + 1), static_cast<int>((W(::bof3::Img(at::kBoard.v) + 2) + 1) & 0xFFFFu), 0x80, 0x68);
    unsigned char* const s = S();
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        return;
    }
    if (Input_Held != 0) s[1] = 3;
}

// original 0x47F550 (EffectKind5D_States[3]; PSX twin 0x801F73EC): the three
// windows' outlines shrinking; +9 down; at 0, +1 = 4 (Effect_StateRelease).
extern "C" void __cdecl EffectKind5D_Close(void) {
    Outlines();
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    if (S()[9] == 0) S()[1] = 4;
}

// original 0x47F5D0 (Effect_KindHandlers[0x5E]; a catalog part 6 row the cut
// does not list, taken as the band's dispatcher): jmp [EffectKind5E_States +
// +1 * 4], unbounded.
extern "C" void __cdecl EffectKind5E_Run(void) {
    Dispatch("EffectKind5E_Run", AddressOf(EffectKind5E_States), EffectKind5E_States_count, 1);
}

// original 0x47F5F0 (EffectKind5E_States[0]; PSX twin 0x801F755C): +6 = 0x4E,
// kind 0x5D's start (EffectKind5D_Start), +2 = 0.
extern "C" void __cdecl EffectKind5E_Start(void) {
    S()[6] = 0x4E;
    SH_CALL(EffectKind5D_Start)();
    S()[2] = 0;
}

// original 0x47F610 (EffectKind5E_States[1]; PSX twin 0x801F7590): the menu
// drawn (EffectKind5E_DrawMenu); Input_Pressed's 0x1000 / 0x4000: the pick +2
// down / up, & 7, sound 0x206, +6 = 0x4E + the pick, Sprite_SetAnimation(the
// pick). Then by the low byte (read again after a move): 0x10 sound 0x209,
// +0xA = 0x14, +1 = 2; else 0x40 sound 0x20C, +1 = 3; else 0x20: the item
// +6 held (Inventory_Count(0, +6, 0) not 0) - the count 0x903A10[+2] up, held
// at 100, Inventory_Remove(0, +6, 1), sound 0x20A - or sound 0x20D.
extern "C" void __cdecl EffectKind5E_Pick(void) {
    SH_CALL(EffectKind5E_DrawMenu)();
    U pressed = ULRest3e(At(AddressOfRest3e(&Input_Pressed)));
    if (pressed & 0x5000u) {
        unsigned char* s = S();
        s[2] = static_cast<unsigned char>((pressed & 0x1000u) ? s[2] - 1 : s[2] + 1);
        S()[2] &= 7;
        SH_CALL(Sound_PlayEffect)(0x206);
        s = S();
        s[6] = static_cast<unsigned char>((s[2] & 7) + 0x4E);
        SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(S()[6] - 0x4E));
        pressed = ULRest3e(At(AddressOfRest3e(&Input_Pressed)));
    }
    if (pressed & Sigma(0x10)) {
        SH_CALL(Sound_PlayEffect)(0x209);
        S()[0xA] = 0x14;
        S()[1] = 2;
        return;
    }
    if (pressed & Sigma(0x40)) {
        SH_CALL(Sound_PlayEffect)(0x20C);
        S()[1] = 3;
        return;
    }
    if ((pressed & Sigma(0x20)) == 0) return;
    if ((SH_CALL(Inventory_Count)(0, S()[6], 0) & 0xFFFFu) == 0) {
        SH_CALL(Sound_PlayEffect)(0x20D);
        return;
    }
    const unsigned char* const s = S();
    unsigned char& count = B(::bof3::Img(at::kCounts.v) + s[2]);
    if (count < 100) count = static_cast<unsigned char>(count + 1);
    SH_CALL(Inventory_Remove)(0, s[6], 1);
    SH_CALL(Sound_PlayEffect)(0x20A);
}

// original 0x47F720 (EffectKind5E_States[2]; PSX twin 0x801F7758): the board's
// frame (Window_DrawFrame) and the board (EffectKind5D_DrawBoard at its corner
// + 1, 0x80 x 0x68); +0xA down when not 0; at 0, any pressed button: sound
// 0x20C, +1 = 1.
extern "C" void __cdecl EffectKind5E_Wait(void) {
    SH_CALL(Window_DrawFrame)(W(::bof3::Img(at::kBoard.v)), W(::bof3::Img(at::kBoard.v) + 2), B(::bof3::Img(at::kBoard.v) + 4), B(::bof3::Img(at::kBoard.v) + 6));
    SH_CALL(EffectKind5D_DrawBoard)(static_cast<int>(ULRest3e(::bof3::Img(at::kBoard.v)) + 1), static_cast<int>((W(::bof3::Img(at::kBoard.v) + 2) + 1) & 0xFFFFu), 0x80, 0x68);
    unsigned char* const s = S();
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        return;
    }
    if (Input_Pressed == 0) return;
    SH_CALL(Sound_PlayEffect)(0x20C);
    S()[1] = 1;
}

// original 0x47F7A0 (PSX twin 0x801F781C): the board - an FT4 at (x, y) of w x
// h (the low words; CLUT x 0x40), u 0 / 0x80, v 0x98 / 0xFF, committed to slot
// 1 (0x48); then for each of the 8 items its count (0x903A10[i], at most 3)
// marks from its row of the table 0x6548BC (3 bytes a row, 0xA none), mark m
// drawn (EffectKind5D_DrawMark) at (x + its dx, y + its dy) from the table
// 0x6548D4.
extern "C" void __cdecl EffectKind5D_DrawBoard(int x, int y, int w, int h) {
    unsigned char* const p = Quad(0x40);
    const std::int32_t x0 = static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFFu);
    const std::int32_t y0 = static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFFu);
    Corners(p, x0, y0, x0 + static_cast<std::int32_t>(static_cast<U>(w) & 0xFFFFu), y0 + static_cast<std::int32_t>(static_cast<U>(h) & 0xFFFFu));
    p[4] = p[5] = p[6] = 0x80;
    p[0x14] = 0;
    p[0x15] = 0x98;
    p[0x24] = 0x80;
    p[0x25] = 0x98;
    p[0x34] = 0;
    p[0x35] = 0xFF;
    p[0x44] = 0x80;
    p[0x45] = 0xFF;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
    for (unsigned i = 0; i < at::kItems; ++i) {
        unsigned n = B(::bof3::Img(at::kCounts.v) + i);
        if (n > 3) n = 3;
        for (unsigned k = 0; k < n; ++k) {
            const unsigned m = B(::bof3::Img(at::kRows.v) + 3 * i + k);
            if (m == 0xA) continue;
            if (m >= at::kMarkCount)
                bof3::Fatal("EffectKind5D_DrawBoard: mark %u of row %u is past the %u the table 0x6548D4 has room for - the "
                            "original reads what follows (docs/rest_3e.md section 7)",
                            m, i, at::kMarkCount);
            SH_CALL(EffectKind5D_DrawMark)(static_cast<int>((W(::bof3::Img(at::kMarks.v) + 4 * m) + static_cast<U>(x)) & 0xFFFFu),
                                           static_cast<int>((W(::bof3::Img(at::kMarks.v) + 4 * m + 2) + static_cast<U>(y)) & 0xFFFFu));
        }
    }
}

// original 0x47F910: a mark - a 16 x 16 FT4 at (x, y) (the low words; CLUT x
// 0x30), u 0xB0 / 0xC0, v 0x50 / 0x60, colour 0x80, committed to slot 1
// (0x48).
extern "C" void __cdecl EffectKind5D_DrawMark(int x, int y) {
    unsigned char* const p = Quad(0x30);
    p[4] = p[5] = p[6] = 0x80;
    const std::int32_t x0 = static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFFu);
    const std::int32_t y0 = static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFFu);
    Corners(p, x0, y0, x0 + 0x10, y0 + 0x10);
    p[0x14] = 0xB0;
    p[0x34] = 0xB0;
    p[0x24] = 0xC0;
    p[0x44] = 0xC0;
    p[0x15] = 0x50;
    p[0x25] = 0x50;
    p[0x35] = 0x60;
    p[0x45] = 0x60;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x47F9E0: kind 0x5E's menu - its box (Menu_DrawBox, flags 2, the
// window style 0x903A5A) and title (the text 0x66A098 at the box's corner + (4,
// 2), colour 0xF); the list (EffectKind5E_DrawList); the hand at the pick
// (Menu_DrawHand(the low byte of 0x654894, its high byte + 15 * (+2 & 7)));
// the panel (EffectKind5E_DrawPanel); the sprite placed at the panel's corner
// (+0x2E, +0x30, as kind 0x5D's), the panel's frame (Window_DrawFrame);
// Sprite_ScriptTick, then a tail jump to Sprite_QueueOverlay.
//
// DIVERGENCE DIV-0064 (group 20, docs/install-gap-labels.md): once a language
// overlay has written the title (the disc's own, SCENA10.EMI's: the French
// seventeen letters past the original's count of 15), it is drawn whole, to
// its NUL (Labels_Written's rule). Since D280's ja-JP layer the title is
// printed from the group's buffer (Labels_Slot: the slot 0x66A098 itself
// while no layer wrote it), which holds the Japan disc's fifteen characters.
extern "C" void __cdecl EffectKind5E_DrawMenu(void) {
    static_assert(at::kTitle.v == 0x66A098, "Labels_Slot(20, 0) stands for the title slot");
    SH_CALL(Menu_DrawBox)(W(::bof3::Img(at::kMenuBox.v)), W(::bof3::Img(at::kMenuBox.v) + 2), W(::bof3::Img(at::kMenuBox.v) + 4), W(::bof3::Img(at::kMenuBox.v) + 6), 2, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(at::kText.v))), reinterpret_cast<const char*>(At(::bof3::Img(at::kStringFormat.v))), Labels_Slot(20, 0));
    const int count = Labels_Written(20) ? 0xFF : 0xF;
    SH_CALL(Text_DrawAt)(static_cast<int>(ULRest3e(::bof3::Img(at::kMenuBox.v)) + 4), static_cast<int>((W(::bof3::Img(at::kMenuBox.v) + 2) + 2) & 0xFFFFu), 0, count, At(::bof3::Img(at::kText.v)));
    SH_CALL(EffectKind5E_DrawList)();
    const unsigned pick = S()[2] & 7u;
    const unsigned hand = Rest3e_Hand;
    SH_CALL(Menu_DrawHand)(static_cast<int>(hand & 0xFFu), static_cast<int>(15 * pick + (hand >> 8)), 0);
    SH_CALL(EffectKind5E_DrawPanel)();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->screen_x, Rest3e_SpriteDx + ULRest3e(::bof3::Img(at::kPanel.v)));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->screen_y, Rest3e_SpriteDy + W(::bof3::Img(at::kPanel.v) + 2));
    SH_CALL(Window_DrawFrame)(W(::bof3::Img(at::kPanel.v)), W(::bof3::Img(at::kPanel.v) + 2), B(::bof3::Img(at::kPanel.v) + 4), B(::bof3::Img(at::kPanel.v) + 6));
    SH_CALL(Sprite_ScriptTick)();
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x47FAF0: the list - its box (Menu_DrawBox, flags 0, the style);
// for each item 0x4E..0x55 a line 0xF lower from (x + 8, y + 0x14): its name
// (Item_NamePtr(0, item)) in colour 0 when held, else 7 (Inventory_Count(0,
// item, 0)'s low byte), its length Text_CharCount; the count (that byte)
// through the format 0x653EC0 at (x + 8 + 0x5A, the line + 3) in the same
// colour (Text_DrawFont8). The 0x14 is ListFirstLine() above (DIV-0198).
extern "C" void __cdecl EffectKind5E_DrawList(void) {
    SH_CALL(Menu_DrawBox)(W(::bof3::Img(at::kListBox.v)), W(::bof3::Img(at::kListBox.v) + 2), W(::bof3::Img(at::kListBox.v) + 4), W(::bof3::Img(at::kListBox.v) + 6), 0, static_cast<unsigned char>(Config_WindowColour));
    const U x = ULRest3e(::bof3::Img(at::kListBox.v)) + 8;
    U line = W(::bof3::Img(at::kListBox.v) + 2) + ListFirstLine();
    for (unsigned item = at::kFirstItem; item < at::kFirstItem + at::kItems; ++item, line += 0xF) {
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, item);
        const auto count = static_cast<unsigned char>(SH_CALL(Inventory_Count)(0, item, 0));
        const unsigned colour = count != 0 ? 0u : 7u;
        const unsigned length = SH_CALL(Text_CharCount)(name);
        SH_CALL(Text_DrawAt)(static_cast<int>(x), static_cast<int>(line), static_cast<int>(colour), static_cast<int>(length), name);
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(at::kText.v))), reinterpret_cast<const char*>(At(::bof3::Img(at::kListFormat.v))), static_cast<unsigned>(count));
        SH_CALL(Text_DrawFont8)(static_cast<int>(x + 0x5A), static_cast<int>(line + 3), static_cast<int>(colour), At(::bof3::Img(at::kText.v)));
    }
}

// original 0x47FBE0: the panel - its box (Menu_DrawBox, flags 0, the style),
// then an FT4 over it (CLUT x 0x20; the corners the box's, the low words) with
// u 0xC0 / (w byte - 0x41), v 0x50 / (h byte + 0x50), colour 0x80, committed
// to slot 1 (0x48). With DockLegend on and a row whose button is not the
// page's icon, the quad in pieces and that row's icon drawn (the section
// above); else exactly the original's.
extern "C" void __cdecl EffectKind5E_DrawPanel(void) {
    SH_CALL(Menu_DrawBox)(static_cast<std::uint32_t>(Rest3e_PanelBox), W(::bof3::Img(at::kPanelBox.v) + 2), W(::bof3::Img(at::kPanelBox.v) + 4), W(::bof3::Img(at::kPanelBox.v) + 6), 0, static_cast<unsigned char>(Config_WindowColour));
    if (rest_3e::dock::g_on) {
        if (W(::bof3::Img(at::kPanelBox.v) + 4) != dock::kBoxW || W(::bof3::Img(at::kPanelBox.v) + 6) != dock::kBoxH)
            bof3::Fatal("DockLegend: the panel box 0x654888 is 0x%X x 0x%X, not the 0x40 x 0x28 its rows were read for", W(::bof3::Img(at::kPanelBox.v) + 4),
                        W(::bof3::Img(at::kPanelBox.v) + 6));
        dock::Shape baked[dock::kRows];
        std::uint16_t wanted[dock::kRows];
        for (unsigned r = 0; r < dock::kRows; ++r) {
            baked[r] = dock::Classify(dock::ReadCell(r));
            wanted[r] = bof3x::input::sigma::Sigma(dock::kRowBit[r]);
        }
        dock::Row rows[dock::kRows];
        const bool readable = dock::Plan(baked, wanted, rows);
        dock::LogPlan(readable, baked, wanted, rows);
        bool changed = false;
        for (unsigned r = 0; readable && r < dock::kRows; ++r) changed = changed || rows[r].how != dock::kKeep;
        if (changed) {
            const std::int32_t x0 = Rest3e_PanelBox;
            const std::int32_t y0 = static_cast<std::int32_t>(W(::bof3::Img(at::kPanelBox.v) + 2));
            const unsigned u1 = dock::kU0 + dock::kBoxW - 1;   // 0xFF, as w - 0x41
            // Whole-width runs of texel rows, 1:1 in v as the original's quad;
            // a changed row's cell left out of its band (u 4..14 not drawn).
            unsigned run = 0;
            auto Whole = [&](unsigned to) {
                if (to > run)
                    dock::PanelQuad(x0, y0 + static_cast<std::int32_t>(run), static_cast<std::int32_t>(dock::kBoxW),
                                    static_cast<std::int32_t>(to - run), dock::kU0, dock::kV0 + run, u1, dock::kV0 + to);
            };
            for (unsigned r = 0; r < dock::kRows; ++r) {
                if (rows[r].how == dock::kKeep) continue;
                const unsigned top = dock::kBandV + dock::kBandH * r;
                Whole(top);
                const std::int32_t y = y0 + static_cast<std::int32_t>(top), h = static_cast<std::int32_t>(dock::kBandH);
                const unsigned right = dock::kColU + dock::kColW;
                dock::PanelQuad(x0, y, static_cast<std::int32_t>(dock::kColU), h, dock::kU0, dock::kV0 + top,
                                dock::kU0 + dock::kColU, dock::kV0 + top + dock::kBandH);
                dock::PanelQuad(x0 + static_cast<std::int32_t>(right), y, static_cast<std::int32_t>(dock::kBoxW - right), h,
                                dock::kU0 + right, dock::kV0 + top, u1, dock::kV0 + top + dock::kBandH);
                run = top + dock::kBandH;
            }
            Whole(dock::kBoxH);
            for (unsigned r = 0; r < dock::kRows; ++r) {
                const unsigned top = dock::kBandV + dock::kBandH * r;
                if (rows[r].how == dock::kCellOf) {
                    const unsigned from = dock::kBandV + dock::kBandH * rows[r].from;
                    dock::PanelQuad(x0 + static_cast<std::int32_t>(dock::kColU), y0 + static_cast<std::int32_t>(top),
                                    static_cast<std::int32_t>(dock::kColW), static_cast<std::int32_t>(dock::kBandH),
                                    dock::kU0 + dock::kColU, dock::kV0 + from, dock::kU0 + dock::kColU + dock::kColW,
                                    dock::kV0 + from + dock::kBandH);
                } else if (rows[r].how == dock::kGlyph) {
                    // The 12-unit glyph centred on the 10-texel cell: Text_DrawAt's
                    // y + 1 is the glyph's top, so top - 2 puts it at top - 1 ..
                    // top + 11. (top + 3 until 2026-11-01 - issue #178's capture had
                    // the circle's lower half over the next row's icon.)
                    dock::Glyph(x0 + 4, y0 + static_cast<int>(top) - 2, static_cast<dock::Shape>(rows[r].from));
                }
            }
            return;
        }
    }
    unsigned char* const p = Quad(0x20);
    p[4] = p[5] = p[6] = 0x80;
    const std::int32_t x0 = Rest3e_PanelBox;
    const std::int32_t y0 = static_cast<std::int32_t>(W(::bof3::Img(at::kPanelBox.v) + 2));
    Corners(p, x0, y0, x0 + static_cast<std::int32_t>(W(::bof3::Img(at::kPanelBox.v) + 4)), y0 + static_cast<std::int32_t>(W(::bof3::Img(at::kPanelBox.v) + 6)));
    p[0x14] = 0xC0;
    p[0x15] = 0x50;
    p[0x25] = 0x50;
    p[0x34] = 0xC0;
    p[0x24] = static_cast<unsigned char>(B(::bof3::Img(at::kPanelBox.v) + 4) - 0x41);
    p[0x35] = static_cast<unsigned char>(B(::bof3::Img(at::kPanelBox.v) + 6) + 0x50);
    p[0x44] = static_cast<unsigned char>(B(::bof3::Img(at::kPanelBox.v) + 4) - 0x41);
    p[0x45] = static_cast<unsigned char>(B(::bof3::Img(at::kPanelBox.v) + 6) + 0x50);
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

void EffectKind5d_Inject() {
    BOF3_INJECT(EffectKind5D_Run);
    BOF3_INJECT(EffectKind5D_Start);
    BOF3_INJECT(EffectKind5D_Open);
    BOF3_INJECT(EffectKind5D_Show);
    BOF3_INJECT(EffectKind5D_Close);
    BOF3_INJECT(EffectKind5E_Run);
    BOF3_INJECT(EffectKind5E_Start);
    BOF3_INJECT(EffectKind5E_Pick);
    BOF3_INJECT(EffectKind5E_Wait);
    BOF3_INJECT(EffectKind5D_DrawBoard);
    BOF3_INJECT(EffectKind5D_DrawMark);
    BOF3_INJECT(EffectKind5E_DrawMenu);
    BOF3_INJECT(EffectKind5E_DrawList);
    BOF3_INJECT(EffectKind5E_DrawPanel);
}
