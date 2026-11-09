// Round fourteen group R3E (docs/rest_3e.md): the 50 functions of
// analysis/round14_cut.tsv's group R3E, 0x46A320..0x4801ED, each read with
// capstone to its last instruction (2026-10-04). All effect code: three kinds'
// dispatchers, the helpers five kinds of round twelve and thirteen call, the
// pools kinds 0x48 / 0x49 / 0x52 draw with, and the states of kinds 0x5D, 0x5E
// and 0x5F.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies, with one
// exception: EffectGlowTrail_Update is EffectKind52_TrailUpdate (E2F's)
// instruction for instruction but the call targets, so it calls that function
// directly (its callees go through the harness inside it). Sprite_Current is
// read again wherever the original reads [0x937F88] again after a call. No
// divergence: each is a faithful replacement. Where the original jumps through
// a state table past its end, or indexes the marks past their room, ours
// aborts with a message (docs/rest_3e.md section 7).
//
// One divergence rides on EffectKind5E_DrawPanel, its own switch
// (DockLegend, DIV-0137 of docs/button-unification-round.md): the dock's
// legend shows the buttons the Config has on its three tests now (section
// "the dock's legend" below). A second on EffectKind5E_DrawList (DIV-0198):
// the list's first line follows the release the mapped image's list box is
// in - the Western PSX twin's y + 4 over the West's own box, Japan's y + 0x14
// over Japan's.
#include "game/rest_3e.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/rest_3e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/exe_image.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_3e::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetWord;

unsigned char* S() { return Sprite_Current; }
U ULRest3e(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U ULRest3e(U a) { return ULRest3e(move_script::At(a)); }
U AddressOfRest3e(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next table.
void Dispatch(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3e.md section 7)",
                    who, state, entries, (unsigned)::bof3::VaOf(AddressOfRest3e(table)));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(ULRest3e(static_cast<const unsigned char*>(table) + 4 * state)))();
}

}  // namespace

// ===========================================================================
// Three kinds' dispatchers (Effect_KindHandlers[0x37], [0x17], [0x1B]; their
// state tables FC1's, docs/field_c1.md)
// ===========================================================================

// ===========================================================================
// Helpers of kinds 0x41, 0x30, 0x1D, 0x21, 0x24
// ===========================================================================

// ===========================================================================
// The glow sparks: 8 records of 0x1C at EffectKind30_Shards (+0 in use, +1 the
// state, +2 the count, +3 the shade, +4 the reload, +8 the rise speed, +0xC
// the point). Kinds 0x48 and 0x49 (E2D, E2E) and kind 0x52 (E2F) use them.
// ===========================================================================

// ===========================================================================
// The glow trail (0x92C060), the spiral (0x92C4A4), the ring
// ===========================================================================

// The spiral record (0xD20 bytes): +0..+8 its centre; eight rings of 32 screen
// points (12 bytes each) from +0x10, 0x180 a ring; eight rings of 32 shade
// bytes from +0xC10; the tilt word +0xD10; a light vector of three longs at
// +0xD14.
// original 0x479EE0 (E2E's kind 0x48 state 7 calls it): a ring of 32
// semi-transparent POLY_G4 round the ring record's point (+0, +4, +8): the
// rim at angle t is (x + cos t << 5, z + sin t << 5) at the height and at the
// height + 0x7000000, each projected; quad i joins the rim at t = 0x80 i to t
// = 0x80 (i + 1). Before each quad a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 1) linked at the new rim point (MapView_LinkPrimAt(x, z, 0, 0xC)); the
// quad linked there too (0x44). The shade: the old pair's the last quad's new
// one (the first: min(+0x10, 0xFF)); the new pair's min((s * +0x10, 16 bits)
// >> 7, 0xFF), s a running byte from 0x80 down 4 for quads 4..19, else up 4.
// ===========================================================================
// The dust: 64 records of 0x20 at 0x92D1DC (+0 in use, +2 the count, +4 x, +8
// z, +0xC the height, +0x14 the spread, +0x18 the rise, +0x1C the ground).
// Kind 0x49's variant 3 (E2D) uses them.
// ===========================================================================

// ===========================================================================
// Kinds 0x5D and 0x5E (EffectKind5D_States 0..3, EffectKind5E_States 0..2;
// their dispatchers E2G's). Three windows (+7 bit j hides window j) open,
// show the item +6 and its count and a board of marks, and close; kind 0x5E
// picks one of the eight items 0x4E..0x55 (+2) and adds it to the counts at
// 0x903A10.
// ===========================================================================

// ===========================================================================
// The dock's legend (DIVERGENCE: DIV-0137, docs/button-unification-
// round.md section 3 "IC-D"; docs/rest_3e.md section 11)
//
// The panel EffectKind5E_DrawPanel draws is baked art of the area page (tag
// 0x0A081000, AREA128.DAT): three rows, each a button icon and its word -
// place the part (EffectKind5E_Pick 0x47F610 tests circle 0x20), quit (cross
// 0x40), the memo board (triangle 0x10). Each release bakes its own default
// buttons there: Japan's page (the PC's, the JP disc's, PSP-JP's, the same
// bytes) circle / cross / triangle; the PlayStation West's (US, EU, FR, DE)
// cross / triangle / square, P of Japan's; PSP-EU's cross / circle / square,
// S of Japan's (read on all seven builds' pages, 2026-10-17, section 11).
//
// With DockLegend on, each row shows the button the Config has its test on now:
// Sigma of the row's bit (input/sigma.h; Japan's terms, so the identity on a
// JP-based install at its defaults, P on a West install at the West's, S on
// PSP-EU's). Where that is the page's own icon nothing changes: one quad, the
// original's. Where it is not, that row's icon cell is left out of the panel
// (the panel drawn in pieces, the cell's 11 x 10 texels not drawn) and the
// right icon drawn there instead:
//
//   1. the page's own icon of that button, when another row of the page has
//      it (the disc's shapes, or Capcom's on the PC's own page) - an 11 x 10
//      FT4 of that row's cell;
//   2. else, under a Latin overlay, DIV-0051's icon of the button (the disc's
//      12 x 12 set, loc_build.py's ICONS_AT), as the Config panel draws it;
//   3. else the port's own stand-in, the glyph the Config panel's original
//      0x461C00 draws for the button (0x66A2FC's strings, circled numerals
//      and an X), in the same colour.
//
// The page's icons are read from the VRAM shadow, not assumed from the
// install: a cell is told by a rule on its bright texels (index 8 and up -
// the icons' ink; the words and the shadows are below 8), so a page from any
// source, or a later mod's, says itself what it shows. A page whose cells do
// not read as three of the four shapes is drawn as it is (logged once).
// ===========================================================================

namespace {

namespace dock {

constexpr unsigned kColU = 4, kColW = 11;          // u offsets 4..14 of the panel
constexpr unsigned kBandV = 5, kBandH = 10;        // row r's cell: v offsets 5 + 10 r ..
constexpr unsigned kRows = 3;

// The rows' buttons in Japan's terms: EffectKind5E_Pick's three tests.
constexpr std::uint16_t kRowBit[kRows] = {0x20, 0x40, 0x10};

// The six buttons in DIV-0051's order (config_text.cpp's kIcons), each with
// the colour 0x461C00 draws its stand-in in and the Config panel its icon.
enum Shape : unsigned char { kCircle, kCross, kTriangle, kSquare, kL1, kR1, kUnknown };
constexpr std::uint16_t kShapeBit[6] = {0x20, 0x40, 0x10, 0x80, 0x04, 0x08};

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

// DIVERGENCE: DIV-0137 - 1 after this module's self-test (PatchBytes
// "DockLegend", BOF3X_ORIGINAL=DockLegend for the page as it is).
}  }  namespace rest_3e::dock { unsigned char g_on = 0; }  namespace { namespace dock {

// The self-test (BOF3X_SHADOW=rest_3e, after the fuzz): the rule on shapes
// drawn here by rule - no page's texels are copied - at every place in the
// cell; then the plan on each release's page order (docs/rest_3e.md section
// 11) with stand-in Configs built by sigma::Build from sigma::DefaultWords: at
// each release's defaults every row kept (the original's one quad), under the
// stand-in swap (Speak <-> Action, View <-> Change) rows 0 and 1 trading cells,
// and over all 720 assignments of the six lines every row showing its test's
// button.
unsigned g_bad = 0;
void Check(bool ok, const char* what, unsigned a = 0, unsigned b = 0) {
    if (ok) return;
    if (g_bad < 20) bof3::Log("shadow    rest_3e DockLegend MISMATCH: %s (%u, %u)", what, a, b);
    ++g_bad;
}

// A 7 x 7 shape by rule: the square's ring, the circle's ring with its four
// corners cut, the cross's two diagonals, the triangle's rows widening by one
// texel each side every second row.
bool Drawn(Shape s, int x, int y) {
    const bool edge = x == 0 || x == 6 || y == 0 || y == 6;
    switch (s) {
    case kSquare: return edge;
    case kCircle: return edge && !((x == 0 || x == 6) && (y == 0 || y == 6));
    case kCross: return x == y || x == 6 - y;
    case kTriangle: return y == 6 || x == 3 - y / 2 || x == 3 + y / 2;
    default: return false;
    }
}

void SelfTest() {
    namespace sg = bof3x::input::sigma;
    g_bad = 0;
    unsigned cells = 0, plans = 0;
    // 1. The rule.
    for (unsigned s = kCircle; s <= kSquare; ++s)
        for (unsigned ox = 0; ox + 7 <= kColW; ++ox)
            for (unsigned oy = 0; oy + 7 <= kBandH; ++oy) {
                Cell c = {};
                for (int y = 0; y < 7; ++y)
                    for (int x = 0; x < 7; ++x) c.ink[oy + y][ox + x] = Drawn(static_cast<Shape>(s), x, y);
                Check(Classify(c) == s, "a drawn shape read as another", s, Classify(c));
                ++cells;
            }
    {
        Cell empty = {}, block = {}, line = {}, small = {};
        for (unsigned y = 0; y < 7; ++y)
            for (unsigned x = 0; x < 7; ++x) block.ink[y][x] = true;
        for (unsigned x = 0; x < kColW; ++x) line.ink[4][x] = true;
        for (unsigned y = 0; y < 3; ++y)
            for (unsigned x = 0; x < 3; ++x) small.ink[y][x] = x != 1 || y != 1;
        Check(Classify(empty) == kUnknown, "an empty cell read as a shape");
        Check(Classify(block) == kUnknown, "a filled block read as a shape", Classify(block));
        Check(Classify(line) == kUnknown, "a line read as a shape", Classify(line));
        Check(Classify(small) == kUnknown, "a 3 x 3 ring read as a shape", Classify(small));
        cells += 4;
    }
    // 2. The plan. Each release's page order: its defaults' buttons on the
    // three rows (M_R of circle, cross, triangle).
    const sg::Release releases[3] = {sg::Release::kJapan, sg::Release::kWest, sg::Release::kPspEu};
    for (const sg::Release rel : releases) {
        const unsigned rn = static_cast<unsigned>(rel);
        Shape page[kRows];
        for (unsigned r = 0; r < kRows; ++r) page[r] = ShapeOfBit(sg::ReleaseMap(rel, kRowBit[r]));
        std::uint16_t words[9];
        sg::DefaultWords(rel, false, words);
        std::string why;
        const sg::Table t = sg::Build(words, why);
        Check(t.valid, "a release's defaults refused", rn);
        std::uint16_t wanted[kRows];
        Row rows[kRows];
        for (unsigned r = 0; r < kRows; ++r) wanted[r] = sg::Apply(t, kRowBit[r]);
        Check(Plan(page, wanted, rows), "a release's page unreadable", rn);
        for (unsigned r = 0; r < kRows; ++r) Check(rows[r].how == kKeep, "a row changed at the release's defaults", rn, r);
        ++plans;
        // The stand-in swap: rows 0 and 1 trade cells, row 2 kept, on all three.
        std::uint16_t swapped[9];
        std::memcpy(swapped, words, sizeof swapped);
        const std::uint16_t l2r2 = static_cast<std::uint16_t>(words[0] & sg::kL2R2);
        swapped[0] = static_cast<std::uint16_t>(words[6] | l2r2);
        swapped[6] = static_cast<std::uint16_t>(words[0] & ~sg::kL2R2);
        std::swap(swapped[3], swapped[4]);
        const sg::Table ts = sg::Build(swapped, why);
        Check(ts.valid, "the stand-in swap refused", rn);
        for (unsigned r = 0; r < kRows; ++r) wanted[r] = sg::Apply(ts, kRowBit[r]);
        Check(Plan(page, wanted, rows), "a release's page unreadable", rn);
        Check(rows[0].how == kCellOf && rows[0].from == 1, "swap: row 0 not row 1's cell", rn, rows[0].how);
        Check(rows[1].how == kCellOf && rows[1].from == 0, "swap: row 1 not row 0's cell", rn, rows[1].how);
        Check(rows[2].how == kKeep, "swap: row 2 changed", rn, rows[2].how);
        ++plans;
        // Every assignment of the six lines to the six buttons: each row shows
        // its test's button - kept only where the page has it there, a cell
        // only where the page has it in another row, a glyph only where the
        // page has it nowhere.
        unsigned perm[6] = {0, 1, 2, 3, 4, 5};
        const unsigned char line_word[6] = {0, 1, 6, 2, 3, 4};
        do {
            std::uint16_t w[9];
            std::memcpy(w, words, sizeof w);
            for (unsigned l = 0; l < 6; ++l) w[line_word[l]] = kShapeBit[perm[l]];
            const sg::Table tp = sg::Build(w, why);
            Check(tp.valid, "a permutation refused", perm[0], perm[1]);
            for (unsigned r = 0; r < kRows; ++r) wanted[r] = sg::Apply(tp, kRowBit[r]);
            Check(Plan(page, wanted, rows), "a release's page unreadable", rn);
            for (unsigned r = 0; r < kRows; ++r) {
                const Shape want = ShapeOfBit(wanted[r]);
                bool on_page = false;
                for (unsigned k = 0; k < kRows; ++k) on_page = on_page || page[k] == want;
                const bool ok = rows[r].how == kKeep     ? page[r] == want
                                : rows[r].how == kCellOf ? page[r] != want && page[rows[r].from] == want
                                                         : !on_page && rows[r].from == want;
                Check(ok, "a row not showing its button", r, want);
            }
            ++plans;
        } while (std::next_permutation(perm, perm + 6));
    }
    {
        const Shape page[kRows] = {kCircle, kUnknown, kTriangle};
        const std::uint16_t wanted[kRows] = {0x40, 0x20, 0x10};
        Row rows[kRows];
        Check(!Plan(page, wanted, rows), "an unreadable page planned");
        ++plans;
    }
    bof3::Log("shadow    rest_3e DockLegend self-test: %u cells read by the rule, %u plans (3 page orders at their "
              "defaults, the stand-in swap and 720 assignments each, an unreadable page); %u MISMATCHES",
              cells, plans, g_bad);
    if (g_bad) bof3::Fatal("rest_3e DockLegend: %u mismatches against docs/rest_3e.md section 11", g_bad);
    if (cells != 4 * 5 * 4 + 4 || plans != 3 * (2 + 720) + 1)
        bof3::Fatal("rest_3e DockLegend self-test: %u cells, %u plans (84, 2167 wanted)", cells, plans);
}

}  // namespace dock

}  // namespace

// ===========================================================================
// Kind 0x5F's states 1..9 (EffectKind5F_States; state 0 is Task_StartHold60).
// The record's point +0x34 / +0x38 / +0x3C, its velocity +0xC / +0x10 / +0x14.
// ===========================================================================

void Rest3E_Inject() {
    if (bof3::WantsShadow("rest_3e")) {
        rest_3e::SelfTest();
        dock::SelfTest();
    }
    // DIVERGENCE: DIV-0137, the dock's legend from the Config (after the
    // fuzz above, which compares Capcom's one quad).
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("DockLegend", Low32(&rest_3e::dock::g_on), &was, &is, 1);
    bof3::Log("DockLegend the dock's legend shows the buttons the Config has its three tests on: %s",
              rest_3e::dock::g_on ? "on" : "OFF (BOF3X_ORIGINAL=DockLegend)");
}
