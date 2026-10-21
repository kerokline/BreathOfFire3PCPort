// Widescreen (DIV-0041, docs/widescreen.md). See widescreen.h.
//
// The backend does the picture: the render target is (320 + 2 c) k wide for c
// columns a side (53 at 16:9, 120 at 21:9; docs/ultrawide-frame.md), the scene
// shader adds ck to every x (render_d3d11.cpp), so the game keeps
// drawing in its own 0..320 space with the projection centre at (160, 120)
// and anything it already draws past its edges - 3D geometry, scrolling
// layers - lands in the side bands. What this file holds is the game side:
// the culls that were tuned to a 320-wide view and now pop at the new edges,
// and the menu boxes that slid off the old edges (g_slides, below).
//
// The survey build (widescreen.md section 3e) widens the two the PSP widened:
//   - the terrain cull in MapView_Build (ours): [-50, 370] to [-150, 470] at
//     53 columns, wider than the PSP's [-96, 416] (widescreen_rule::TerrainMargin
//     says why), [-217, 537] at 120;
//   - the area-map frame pass AreaMapBD_BuildView 0x510780 (ours, R3G's; it reads them back): the
//     wide range [-200, 520] to [-252, 572] and the narrow [-50, 370] to
//     [-102, 422] - the PSP widened both by 31 for 32 columns; 52 for 53, 119
//     for 120 (widescreen_rule::RangeLo / RangeHi).
// The function reads them through `fcomp dword ptr [0x5C42xx]`: the .rdata
// floats are shared with MapView_Build's own compares (0x5C4230 / 0x5C4234
// with 0x56EE14 / 0x56EDFA, both ours now), so the floats stay and the four
// operand addresses are re-aimed at copies in this dll, as sprt_draw.cpp does
// with its far-edge table. Disassembly 2026-09-23 (widescreen.md section 3b).
#include "game/widescreen.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/snapshot.h"

using U = std::uint32_t;

float Widescreen_TerrainLo = -50.0f, Widescreen_TerrainHi = 370.0f;
unsigned g_live;
unsigned g_inset_columns;
unsigned g_backdrop_pairs;

unsigned Widescreen_Live() { return g_live; }
unsigned Widescreen_InsetColumns() { return g_inset_columns; }
unsigned Widescreen_BackdropPairs() { return g_backdrop_pairs; }

bool g_fills_armed;
unsigned Widescreen_Fill() { return g_fills_armed ? g_live : 0; }
bool Widescreen_FillsArmed() { return g_fills_armed; }
float Widescreen_FillX() { return 0.0f - static_cast<float>(Widescreen_Fill()); }
float Widescreen_FillWidth() { return 320.0f + 2.0f * static_cast<float>(Widescreen_Fill()); }
float Widescreen_FillRight(float edge) { return edge + static_cast<float>(Widescreen_Fill()); }
int Widescreen_Anchor() { return static_cast<int>(Widescreen_Fill()); }
void Widescreen_BattleTile(unsigned size, int& x, int& w) {
    if (size != kWidescreenFullFrameTile) return;
    const int c = static_cast<int>(Widescreen_Fill());
    if (x == 0) x = -c;
    w += 2 * c;
}

namespace {

namespace rule = widescreen_rule;

// The two widths (BOF3X_WIDE=1 and 2), each by its aspect at 240 rows.
constexpr U kColumns16x9 = rule::ColumnsForAspect(16, 9);   // 426 = 240 x 16 / 9 rounded down to even
constexpr U kColumns21x9 = rule::ColumnsForAspect(21, 9);   // 560 = 240 x 21 / 9 exactly (the owner, 2026-10-13)

// The 16:9 build's constants, which the rules must give at 53 (the property
// check: these are the values the rules replaced, kept as the test's oracle).
// The terrain cull's margin past the original's [-50, 370]: the PSP's [-96,
// 416] kept 14 beyond its 32 columns; 53 + 14 = 67 here still let the map's
// far corners pop about 20 px in at the top left and right while the attract
// sequence rotates the map (the owner, 2026-09-23); 100 was the next try, and
// held. The cell inset was lowered by 3 (a ring column is two map cells
// across, about 20 screen px at the field camera's usual distance; the owner's
// bridge, 2026-10-06). The menu backdrop drew one more pair a side.
static_assert(kColumns16x9 == 53 && kColumns21x9 == 120, "the two widths");
static_assert(rule::TerrainMargin(53) == 100, "the terrain margin tuned by eye at 53");
static_assert(rule::InsetColumns(53) == 3, "the cell inset lowered by 3 at 53");
static_assert(rule::BackdropPairs(53) == 1, "one backdrop pair a side at 53");
static_assert(rule::RangeLo(-200.0f, 53) == -252.0f && rule::RangeHi(520.0f, 53) == 572.0f, "the wide frame range at 53");
static_assert(rule::RangeLo(-50.0f, 53) == -102.0f && rule::RangeHi(370.0f, 53) == 422.0f, "the narrow frame range at 53");
// The two sweeps' maps (I38): the ends moved out by the columns, the fixed points kept.
static_assert(rule::SlideMap(390, 53) == 443 && rule::SlideMap(-70, 53) == -123 && rule::SlideMap(160, 120) == 160 &&
                  rule::SlideMap(390, 0) == 390 && rule::SlideMap(-50, 0) == -50, "kind 0xD's slide map");
static_assert(rule::ShadeMap(0, 53) == -53 && rule::ShadeMap(220, 53) == 273 && rule::ShadeMap(400, 120) == 520 &&
                  rule::ShadeMap(10, 0) == 10 && rule::ShadeMap(410, 0) == 410, "area 172's shade map");

float g_wide_lo = -200.0f, g_wide_hi = 520.0f;     // set by Widescreen_Inject when wide; named only once patched
float g_narrow_lo = -50.0f, g_narrow_hi = 370.0f;

struct Site {
    U at;          // the 4-byte operand of an fcomp dword ptr [mem] in 0x510780
    U rdata;       // the .rdata float it names in the original
    float* ours;   // the copy it names when the view is wide
    U now;         // the address it names in this build (Widescreen_RangeOperand): rdata, or ours once patched
};
Site g_sites[] = {
    {0x51097E, 0x5C4240, &g_wide_lo, 0x5C4240},       // fcomp [-200.0], wide x range, low
    {0x510991, 0x5C423C, &g_wide_hi, 0x5C423C},       // fcomp [520.0], wide x range, high
    {0x5109BB, 0x5C4234, &g_narrow_lo, 0x5C4234},     // fcomp [-50.0], narrow x range, low
    {0x5109D2, 0x5C4230, &g_narrow_hi, 0x5C4230},     // fcomp [370.0], narrow x range, high
};

// The menu boxes that slide off the screen's edge - the field menu's time
// and money boxes when a sub-menu opens, the shop's panels, and their way
// back in - are window-task states in 0x596000..0x59D000, each stepping
// the window's x by 0x20 a frame until a bound that lies just past the 320
// view's edge (then holding, or freeing the window through 0x59E310). In
// the wide view those bounds left the boxes hanging in the bands (the
// owner's screenshots, 2026-09-23). The bound is an immediate: `mov ecx,
// imm32` (9 sites) or `cmp cx / ax, imm16` (5). Scan and disassembly
// 2026-09-23 (widescreen.md section 5).
// From 2026-09-23 each bound was patched outward by the columns, which kept
// the box alive longer: 2 frames more at 16:9, 4 at 21:9, and the game waits
// on the window (docs/ultrawide-census-2.md section 3.2). Since 2026-10-13
// (the faster slide, docs/ultrawide-slides.md) the immediates stay Capcom's,
// every state decides by them, and the wide bound - widescreen_rule::
// SlideBound, the same rule - is only where the box is drawn on the frame it
// holds or is freed (Widescreen_SlideHeld / _SlideDraw below).
// Every one of the fourteen is in a function that is ours, and ours reads
// the bound from `now` (Widescreen_SlideBound) - never the operand
// (docs/operand-store.md); hosted, Operands_Check compares the two.
struct Slide {
    U at;               // the immediate's first byte
    U size;             // 2 or 4
    int bound;          // the original's; negative slides off the left, positive off the right
    std::int32_t now;   // the bound in this build: `bound` (an imm16's in its low word)
};
Slide g_slides[] = {
    {0X596926, 4, -170, -170},
    {0X599CC6, 4, -300, -300},
    {0X599DF6, 4, -100, -100},
    {0X59A136, 4, -180, -180},
    {0X59A2B6, 4, -110, -110},
    {0X59A586, 4, -200, -200},
    {0X59B446, 4, -150, -150},   // MenuWin_SlideOutLeft's (menu_draw_helpers.cpp)
    {0X59C136, 4, -120, -120},
    {0X59A5E6, 4, 320, 320},
    {0X598A08, 2, 322, 322},
    {0X598A1C, 2, -190, -190},
    {0X599061, 2, -165, -165},
    {0X599451, 2, 347, 347},
    {0X599551, 2, 323, 323},
};

// ---------------------------------------------------------------------------
// The faster slide (widescreen.h; docs/ultrawide-slides.md).

// The switch (PatchBytes "Widescreen" sets it when the view is wide) and the
// arming (Widescreen_ArmFills, after every module's self-test, while
// Field_RunTaskRecords is ours: it is what puts Capcom's x back).
std::uint8_t g_slide_switch = 0;
bool g_slide_armed = false;

// The window records (symbols.toml block WindowRecords) and the current one.
// kCurrent: now BattleWinStates_Current (symbols.toml).
// kRecords: now WindowRecords (symbols.toml).
constexpr U kRecordSize = 0x24, kRecordCount = 22;

// One record's split: Capcom's x (`L`) and the drawn one (`D`) as this file
// last left them, and the leg the drawn x is on - from (aL, aD), where it
// started, to (endL, endD), Capcom's bound and the wide one. `parks`: left by
// a holding state, so it outlives the slide while the box parks.
struct Split {
    bool on = false, parks = false;
    std::uint8_t handler = 0, kind = 0;
    int L = 0, D = 0, aL = 0, aD = 0, endL = 0, endD = 0;
};
Split g_split[kRecordCount];

// The property check's stand-ins: a record outside the array, its split, and
// the columns to use.
unsigned char* g_test_rec = nullptr;
Split* g_test_split = nullptr;
int g_test_columns = -1;

unsigned SlideColumns() {
    if (g_test_columns >= 0) return static_cast<unsigned>(g_test_columns);
    return g_slide_switch != 0 && g_slide_armed ? g_live : 0;
}

Split* SplitOf(unsigned char* rec) {
    if (g_test_rec != nullptr) return rec == g_test_rec ? g_test_split : nullptr;
    const U a = static_cast<U>(reinterpret_cast<std::uintptr_t>(rec));
    if (a >= WindowRecords_addr && a < WindowRecords_addr + kRecordSize * kRecordCount && (a - WindowRecords_addr) % kRecordSize == 0)
        return &g_split[(a - WindowRecords_addr) / kRecordSize];
    // Not a window record: the box is drawn at Capcom's x (the original's
    // picture), and said once.
    static bool said = false;
    if (!said) {
        said = true;
        bof3::Log("DIV-0041    the faster slide: a slide state ran on 0x%X, not one of the 22 window records; drawn at "
                  "Capcom's x",
                  a);
    }
    return nullptr;
}

short Sx(const unsigned char* rec) { return static_cast<short>(rec[4] | rec[5] << 8); }
void SetX(unsigned char* rec, int x) {
    rec[4] = static_cast<std::uint8_t>(x);
    rec[5] = static_cast<std::uint8_t>(static_cast<unsigned>(x) >> 8);
}
int Sat16(int v) { return v < -0x8000 ? -0x8000 : v > 0x7FFF ? 0x7FFF : v; }

// The drawn x for Capcom's x `L` on the split's leg. The slide's way is its
// bound's side, as widescreen_rule::SlideBound reads it (off the left for a
// negative bound, off the right for a positive one). At or past the end that
// way, the end moved as L moved; at or behind the start, the start moved as L
// moved; between, on the line from (aL, aD) to (endL, endD). Saturated to an
// s16.
int Drawn(const Split& e, int L) {
    const bool right = e.endL > 0;
    int d;
    if (right ? L >= e.endL : L <= e.endL) d = e.endD + (L - e.endL);
    else if (right ? L <= e.aL : L >= e.aL) d = e.aD + (L - e.aL);
    else d = e.aD + static_cast<int>(static_cast<long long>(L - e.aL) * (e.endD - e.aD) / (e.endL - e.aL));
    return Sat16(d);
}

}  // namespace

short Widescreen_SlideLogic(short bound) {
    static const bool control = [] {
        char text[8];
        const DWORD n = GetEnvironmentVariableA("BOF3X_SL_CONTROL", text, sizeof text);
        const bool on = n != 0 && n < sizeof text && std::strcmp(text, "1") == 0;
        if (on)
            bof3::Log("DIV-0041    BOF3X_SL_CONTROL=1: the slide-off states decide by the bound moved out by the columns "
                      "(the late free before the faster slide) - a fuzz at BOF3X_WIDE=1 or 2 must refuse it");
        return on;
    }();
    if (!control) return bound;
    return static_cast<short>(rule::SlideBound(bound, Widescreen_Columns()));
}

void Widescreen_SlideHeld(unsigned char* rec, short x_read, short bound) {
    const unsigned c = SlideColumns();
    if (c == 0) return;
    Split* const e = SplitOf(rec);
    if (e == nullptr) return;
    // The same leg goes on while this state keeps running from where it left
    // the record; otherwise a new leg starts at the x read - drawn where the
    // record was drawn if it was parked there, else where Capcom's is.
    const bool here = e->on && e->parks && e->handler == rec[1] && e->kind == rec[2] && e->L == x_read;
    if (!(here && e->endL == bound)) {
        e->aL = x_read;
        e->aD = here ? e->D : x_read;
    }
    e->on = e->parks = true;
    e->handler = rec[1];
    e->kind = rec[2];
    e->endL = bound;
    e->endD = rule::SlideBound(bound, c);
    e->L = Sx(rec);
    e->D = Drawn(*e, e->L);
    SetX(rec, e->D);
}

void Widescreen_SlideDraw(unsigned char* rec, short bound, int step) {
    const unsigned c = SlideColumns();
    if (c == 0) return;
    Split* const e = SplitOf(rec);
    if (e == nullptr) return;
    const int x = Sx(rec);
    const bool goes_on = e->on && !e->parks && e->handler == rec[1] && e->kind == rec[2] && e->endL == bound &&
                         x == static_cast<short>(e->L + step);
    if (!goes_on) e->aL = e->aD = x;
    e->on = true;
    e->parks = false;
    e->handler = rec[1];
    e->kind = rec[2];
    e->endL = bound;
    e->endD = rule::SlideBound(bound, c);
    e->L = x;
    e->D = Drawn(*e, x);
    SetX(rec, e->D);
}

void Widescreen_SlideUndraw(unsigned char* rec) {
    if (SlideColumns() == 0) return;
    Split* const e = SplitOf(rec);
    if (e == nullptr || !e->on || e->parks) return;
    if (Sx(rec) == static_cast<short>(e->D)) SetX(rec, e->L);
}

void Widescreen_SlideBefore(unsigned char* rec) {
    if (SlideColumns() == 0) return;
    Split* const e = SplitOf(rec);
    if (e == nullptr || !e->on || !e->parks) return;
    // Parked, and nothing else has moved or reused the record: drawn where it
    // parked. State 0 of every table a holding state is in is a bare ret
    // (docs/ultrawide-slides.md section 3), so nothing but the draw reads it.
    if (rec[0] == 0 || rec[1] != e->handler || rec[2] != e->kind || Sx(rec) != static_cast<short>(e->L)) {
        e->on = false;
        return;
    }
    if (rec[3] == 0) SetX(rec, e->D);
}

void Widescreen_SlideAfter(unsigned char* rec) {
    if (SlideColumns() == 0) return;
    Split* const e = SplitOf(rec);
    if (e == nullptr || !e->on) return;
    if (e->parks) {
        // Capcom's x back; a record its handler moved some other way is
        // Capcom's already, and the split ends.
        if (Sx(rec) == static_cast<short>(e->D)) SetX(rec, e->L);
        else if (Sx(rec) != static_cast<short>(e->L)) e->on = false;
    }
    if (rec[0] == 0) e->on = false;
}

void Widescreen_SlideIdle(unsigned char* rec) {
    if (SlideColumns() == 0) return;
    if (Split* const e = SplitOf(rec)) e->on = false;
}

const std::int32_t* Widescreen_SlideOperand(unsigned at) {
    for (const Slide& s : g_slides)
        if (s.at == at) return &s.now;
    bof3::Fatal("widescreen: 0x%X is not one of DIV-0041's slide bounds", at);
}

U Widescreen_RangeOperand(unsigned at) {
    for (const Site& s : g_sites)
        if (s.at == at) return s.now;
    bof3::Fatal("widescreen: 0x%X is not one of DIV-0041's area-map range operands", at);
}

unsigned Widescreen_Columns() {
    static int columns = -1;
    if (columns >= 0) return static_cast<unsigned>(columns);
    char text[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_WIDE", text, sizeof text);
    if (n == 0 || std::strcmp(text, "0") == 0) columns = 0;
    else if (n < sizeof text && std::strcmp(text, "1") == 0) columns = kColumns16x9;
    else if (n < sizeof text && std::strcmp(text, "2") == 0) columns = kColumns21x9;
    else bof3::Fatal("BOF3X_WIDE=%s: 0, 1 (16:9) or 2 (21:9)", n < sizeof text ? text : "...");
    return static_cast<unsigned>(columns);
}

// The property check (widescreen.h). The 16:9 values are the constants the
// rules replaced: kTerrainMargin 100, kInsetColumns 3, the backdrop's one pair,
// the frame ranges [-252, 572] / [-102, 422], and the fourteen slide bounds as
// Widescreen_Inject patched them with 53 folded in (widescreen.md section 5).
void Widescreen_CheckRules() {
    auto bad = [](const char* what, double got, double want) {
        bof3::Fatal("widescreen: the rule for %s gives %g at 53 columns, the 16:9 build had %g", what, got, want);
    };
    if (kColumns16x9 != 53) bad("the 16:9 columns", kColumns16x9, 53);
    if (kColumns21x9 != 120) bad("the 21:9 columns", kColumns21x9, 120);
    if (rule::TerrainMargin(53) != 100) bad("the terrain margin", rule::TerrainMargin(53), 100);
    if (rule::InsetColumns(53) != 3) bad("the cell inset", rule::InsetColumns(53), 3);
    if (rule::BackdropPairs(53) != 1) bad("the backdrop pairs", rule::BackdropPairs(53), 1);
    if (rule::RangeLo(-200.0f, 53) != -252.0f) bad("the wide range's low", rule::RangeLo(-200.0f, 53), -252);
    if (rule::RangeHi(520.0f, 53) != 572.0f) bad("the wide range's high", rule::RangeHi(520.0f, 53), 572);
    if (rule::RangeLo(-50.0f, 53) != -102.0f) bad("the narrow range's low", rule::RangeLo(-50.0f, 53), -102);
    if (rule::RangeHi(370.0f, 53) != 422.0f) bad("the narrow range's high", rule::RangeHi(370.0f, 53), 422);
    // the slide bounds as the 16:9 build wrote them: the original's, 53 out
    static const int k16x9[] = {-223, -353, -153, -233, -163, -253, -203, -173, 373, 375, -243, -218, 400, 376};
    static_assert(sizeof k16x9 / sizeof k16x9[0] == sizeof g_slides / sizeof g_slides[0], "one per slide bound");
    int lowest = 0, highest = 0;
    for (unsigned i = 0; i < sizeof k16x9 / sizeof k16x9[0]; ++i) {
        const int got = rule::SlideBound(g_slides[i].bound, 53);
        if (got != k16x9[i]) bad("a slide bound", got, k16x9[i]);
        // 21:9: every drawn bound still fits the s16 word +4 holds. Since the
        // faster slide (docs/ultrawide-slides.md) it is only where the box is
        // drawn on the frame Capcom's x reaches Capcom's bound;
        // Widescreen_CheckSlides proves the frame counts.
        const int wide = rule::SlideBound(g_slides[i].bound, kColumns21x9);
        if (wide < -0x8000 || wide > 0x7FFF) bad("a 21:9 slide bound's s16", wide, 0);
        if (wide < lowest) lowest = wide;
        if (wide > highest) highest = wide;
    }
    bof3::Log("DIV-0041    the width rules: at 53 columns the 16:9 constants (terrain margin 100, inset 3, backdrop "
              "pair 1, ranges [-252, 572] / [-102, 422], 14 slide bounds); at 120: terrain margin %u, inset %u, backdrop "
              "pairs %u, ranges [%.0f, %.0f] / [%.0f, %.0f], slide bounds %d..%d",
              rule::TerrainMargin(kColumns21x9), rule::InsetColumns(kColumns21x9), rule::BackdropPairs(kColumns21x9),
              rule::RangeLo(-200.0f, kColumns21x9), rule::RangeHi(520.0f, kColumns21x9),
              rule::RangeLo(-50.0f, kColumns21x9), rule::RangeHi(370.0f, kColumns21x9), lowest, highest);
}

void Widescreen_Inject() {
    // The values of ours, against the operands they stand for (the hosted
    // check, code_bytes.h): narrow or wide, patched or not.
    for (const Site& s : g_sites) bof3::Operand("DIV-0041 area-map range", s.at, &s.now, 4);
    for (const Slide& s : g_slides) bof3::Operand("DIV-0041 slide bound", s.at, &s.now, s.size);
    Widescreen_CheckRules();
    const U columns = Widescreen_Columns();
    if (columns == 0) return;
    g_wide_lo = rule::RangeLo(-200.0f, columns);
    g_wide_hi = rule::RangeHi(520.0f, columns);
    g_narrow_lo = rule::RangeLo(-50.0f, columns);
    g_narrow_hi = rule::RangeHi(370.0f, columns);
    for (Site& s : g_sites) {
        std::uint8_t expected[4], replacement[4];
        const U ours = static_cast<U>(reinterpret_cast<std::uintptr_t>(s.ours));
        std::memcpy(expected, &s.rdata, 4);
        std::memcpy(replacement, &ours, 4);
        if (bof3::PatchBytes("Widescreen", s.at, expected, replacement, 4)) s.now = ours;
    }
    {
        // The faster slide's switch (docs/ultrawide-slides.md): a byte of ours,
        // so BOF3X_ORIGINAL=Widescreen leaves it off and every box slides and
        // parks as Capcom's does, at Capcom's bounds.
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("Widescreen", static_cast<U>(reinterpret_cast<std::uintptr_t>(&g_slide_switch)), &was, &is, 1);
    }
    g_live = columns;
    g_backdrop_pairs = rule::BackdropPairs(columns);
    U terrain_margin = rule::TerrainMargin(columns);
    {
        // BOF3X_WIDE_TERRAIN=N: the terrain cull's margin past [-50, 370], for
        // experiments (2026-10-07, the owner's waterfall); the rule's unless set
        // (DIV-0041: 100 at 53 columns, 167 at 120).
        char text[16];
        const DWORD n = GetEnvironmentVariableA("BOF3X_WIDE_TERRAIN", text, sizeof text);
        if (n != 0) {
            char* end = nullptr;
            const long v = n < sizeof text ? std::strtol(text, &end, 10) : -1;
            if (end == nullptr || end == text || *end != 0 || v < 0 || v > 400)
                bof3::Fatal("BOF3X_WIDE_TERRAIN=%s: 0..400", n < sizeof text ? text : "...");
            terrain_margin = static_cast<U>(v);
        }
    }
    Widescreen_TerrainLo = -50.0f - static_cast<float>(terrain_margin);
    Widescreen_TerrainHi = 370.0f + static_cast<float>(terrain_margin);
    const U inset_asked = rule::InsetColumns(columns);
    g_inset_columns = inset_asked;
    {
        char text[16];
        const DWORD n = GetEnvironmentVariableA("BOF3X_WIDE_INSET", text, sizeof text);
        if (n != 0) {
            char* end = nullptr;
            const long v = n < sizeof text ? std::strtol(text, &end, 10) : -1;
            if (end == nullptr || end == text || *end != 0 || v < 0 || v > 14)
                bof3::Fatal("BOF3X_WIDE_INSET=%s: 0..14", n < sizeof text ? text : "...");
            g_inset_columns = static_cast<U>(v);
        }
    }
    // The inset cannot go below 0 (map_layers.cpp Inset()), and the original's
    // largest is 6, so a lowering of 6 or more walks the whole 28-column ring
    // whatever the camera: past that, what the ring covers is UC's measurement
    // and RG's to grow (IDEAS I37).
    constexpr U kLargestInset = (0x160 - 0) / 50 - 1;   // Inset() head-on (Cond_AngleFB 0x200) at distance 0
    bof3::Log("DIV-0041    widescreen: %u columns a side (%u x 240); terrain cull [%.0f, %.0f]; area-map frame ranges "
              "[%.0f, %.0f] and [%.0f, %.0f]; menu backdrop %u pair(s) more a side; cell inset asked %u lower a side "
              "(ceil(columns / 20)), applied %u%s (BOF3X_WIDE, BOF3X_WIDE_TERRAIN, BOF3X_WIDE_INSET)",
              columns, 320 + 2 * columns, Widescreen_TerrainLo, Widescreen_TerrainHi, g_wide_lo, g_wide_hi, g_narrow_lo,
              g_narrow_hi, g_backdrop_pairs, inset_asked, g_inset_columns,
              g_inset_columns >= kLargestInset ? " - the original's largest inset is 6, so the whole 28-column ring is walked"
                                               : "");
}

// The faster slide's property check (widescreen.h). For each of the fourteen
// states, at 0, 53 and 120 columns, from every start x in [-640, 640) and every
// 257th across the s16 range, the record is run through the cycle
// Field_RunTaskRecords gives it - Widescreen_SlideBefore, the state, the draw
// (here: +4 read as the drawn x), Widescreen_SlideAfter - and three parked
// frames after a hold. The nine holding states are ours, called through
// 0x905B84 (Capcom's step as read where BOF3X_ORIGINAL left one Capcom's);
// the five freeing ones are Capcom's step as read (they call draws), with
// Widescreen_SlideDraw / _SlideUndraw around the draw as ours have them.
// Checked every frame: +4 and +3 (and the free) outside the handler equal
// Capcom's step model's, so the frame count is the narrow run's; narrow, the
// drawn x is Capcom's x; wide, it moves one way and on the hold or free frame
// is at or past the wide bound, and a parked box is drawn there.
void Widescreen_CheckSlides() {
    enum Cmp { kLt, kLe, kGt, kGe };
    struct State {
        const char* name;
        U pc;
        void (__cdecl* ours)();   // null for a freeing state
        U at;                     // its bound's operand (g_slides)
        int step;
        Cmp cmp;                  // hold when the stepped x compares so; free when the x before the step does
    };
    static const State kStates[] = {
        {"MenuSlide_LeftOff170", 0x596920, &::MenuSlide_LeftOff170, 0x596926, -0x20, kLt},
        {"MenuList_SlideLeftOff300", 0x599CC0, &::MenuList_SlideLeftOff300, 0x599CC6, -0x20, kLe},
        {"MenuList_SlideLeftOff100", 0x599DF0, &::MenuList_SlideLeftOff100, 0x599DF6, -0x20, kLt},
        {"MenuList_SlideLeftOff180", 0x59A130, &::MenuList_SlideLeftOff180, 0x59A136, -0x20, kLe},
        {"MenuList_SlideLeftOff110", 0x59A2B0, &::MenuList_SlideLeftOff110, 0x59A2B6, -0x20, kLt},
        {"MenuSlide_LeftOff", 0x59A580, &::MenuSlide_LeftOff, 0x59A586, -0x20, kLt},
        {"MenuWin_SlideOutLeft", 0x59B440, &::MenuWin_SlideOutLeft, 0x59B446, -0x20, kLt},
        {"MasterWin_SlideOut", 0x59C130, &::MasterWin_SlideOut, 0x59C136, -0x20, kLt},
        {"MenuSlide_RightOff", 0x59A5E0, &::MenuSlide_RightOff, 0x59A5E6, 0x20, kGt},
        {"GeneWin_GridSlideOut (right)", 0x5989F0, nullptr, 0x598A08, 0x20, kGe},
        {"GeneWin_GridSlideOut (left)", 0x5989F0, nullptr, 0x598A1C, -0x20, kLe},
        {"GeneWin_ListSlideOut", 0x599050, nullptr, 0x599061, -0x20, kLe},
        {"GeneWin_List2SlideOut", 0x599440, nullptr, 0x599451, 0x20, kGe},
        {"GeneWin_List3SlideOut", 0x599540, nullptr, 0x599551, 0x20, kGe},
    };
    auto past = [](int x, int bound, Cmp c) {
        return c == kLt ? x < bound : c == kLe ? x <= bound : c == kGt ? x > bound : x >= bound;
    };
    static const unsigned kWidths[] = {0, kColumns16x9, kColumns21x9};
    constexpr int kMaxFrames = 0x10000 / 0x20 + 8;

    volatile U* const current = reinterpret_cast<volatile U*>(static_cast<std::uintptr_t>(BattleWinStates_Current_addr));
    const U current_was = *current;
    alignas(4) unsigned char rec[kRecordSize];
    Split split;
    g_test_rec = rec;
    g_test_split = &split;

    unsigned long long frames_run = 0;
    unsigned starts = 0, called = 0, stepped = 0;
    int longest = 0;
    for (const State& s : kStates) {
        const short bound = Widescreen_SlideBound(s.at);
        const bool holds = s.ours != nullptr;
        const bool call = holds && bof3::IsEnabled(s.pc);
        (call ? called : stepped) += 1;
        for (int x0 = -0x8000; x0 <= 0x7FFF; x0 += (x0 >= -640 && x0 < 640) ? 1 : 257) {
            int narrow_frames = -1;
            for (unsigned w = 0; w < 3; ++w) {
                const unsigned c = kWidths[w];
                g_test_columns = static_cast<int>(c);
                split = Split{};
                std::memset(rec, 0, sizeof rec);
                rec[0] = 1;
                rec[1] = 6;
                rec[2] = 5;
                rec[3] = 1;
                SetX(rec, x0);
                *current = static_cast<U>(reinterpret_cast<std::uintptr_t>(rec));
                const int end_drawn = rule::SlideBound(bound, c);
                // a start the box slides toward its bound from, with no wrap on the way
                const bool regular = s.step > 0 ? x0 <= bound : x0 >= bound;
                int model = x0, frames = 0, last_drawn = x0;
                bool done = false;
                auto bad = [&](const char* what, int got, int want) {
                    *current = current_was;
                    bof3::Fatal("widescreen: the faster slide's check: %s from x %d at %u columns, frame %d: %s %d, want %d",
                                s.name, x0, c, frames, what, got, want);
                };
                while (!done) {
                    if (++frames > kMaxFrames) bad("no hold or free after", frames, kMaxFrames);
                    Widescreen_SlideBefore(rec);
                    int drawn;
                    if (holds) {
                        const short x_read = Sx(rec);
                        if (call) {
                            s.ours();
                        } else {
                            SetX(rec, x_read + s.step);
                            if (past(Sx(rec), bound, s.cmp)) {
                                SetX(rec, bound);
                                rec[3] = 0;
                            }
                            Widescreen_SlideHeld(rec, x_read, bound);
                        }
                        drawn = Sx(rec);
                    } else {
                        Widescreen_SlideDraw(rec, bound, s.step);
                        drawn = Sx(rec);
                        Widescreen_SlideUndraw(rec);
                        const short x = Sx(rec);
                        if (past(x, bound, s.cmp)) rec[0] = rec[2] = rec[3] = 0;   // Window_FreeCurrent
                        else SetX(rec, x + s.step);
                    }
                    Widescreen_SlideAfter(rec);
                    // Capcom's step, as read; `seen` the x Capcom's draw reads this frame
                    bool model_done;
                    int seen = model;
                    if (holds) {
                        model = static_cast<short>(model + s.step);
                        model_done = past(model, bound, s.cmp);
                        if (model_done) model = bound;
                        seen = model;
                        if (Sx(rec) != model) bad("x outside the handler", Sx(rec), model);
                        if ((rec[3] == 0) != model_done) bad("+3 0 (held)", rec[3] == 0, model_done);
                    } else {
                        model_done = past(model, bound, s.cmp);
                        if ((rec[0] == 0) != model_done) bad("freed", rec[0] == 0, model_done);
                        if (!model_done) {
                            model = static_cast<short>(model + s.step);
                            if (Sx(rec) != model) bad("x outside the handler", Sx(rec), model);
                        }
                    }
                    if (c == 0) {
                        if (drawn != seen) bad("drawn x, narrow", drawn, seen);
                    } else if (regular) {
                        if (s.step > 0 ? drawn < last_drawn : drawn > last_drawn) bad("drawn x went back", drawn, last_drawn);
                    }
                    last_drawn = drawn;
                    if (model_done) {
                        if (c != 0 && (s.step > 0 ? drawn < end_drawn : drawn > end_drawn))
                            bad("drawn x short of the wide bound on the last frame", drawn, end_drawn);
                        if (holds && c != 0 && drawn != end_drawn) bad("drawn x on the hold", drawn, end_drawn);
                        done = true;
                    }
                    ++frames_run;
                }
                if (holds) {
                    // parked: state 0 is a bare ret; drawn at the wide bound, Capcom's x outside
                    const int park = c == 0 ? bound : end_drawn;
                    for (int p = 0; p < 3; ++p) {
                        Widescreen_SlideBefore(rec);
                        const int drawn = Sx(rec);
                        Widescreen_SlideAfter(rec);
                        if (drawn != park) bad("drawn x parked", drawn, park);
                        if (Sx(rec) != bound) bad("x parked, outside the handler", Sx(rec), bound);
                        ++frames_run;
                    }
                }
                if (w == 0) narrow_frames = frames;
                else if (frames != narrow_frames) bad("frames to hold or free, against narrow's", frames, narrow_frames);
                if (frames > longest) longest = frames;
            }
            ++starts;
        }
    }
    *current = current_was;
    g_test_rec = nullptr;
    g_test_split = nullptr;
    g_test_columns = -1;
    bof3::Log("DIV-0041    the faster slide's check: 14 states (%u of ours called, %u stepped as read), %u starts each, "
              "at 0, 53 and 120 columns, %llu frames: the hold / free frame and every x outside the handler as narrow's, "
              "the drawn x at the wide bound on that frame; longest slide %d frames",
              called, stepped, starts / 14, frames_run, longest);
}

void Widescreen_ArmFills() {
    g_fills_armed = true;
    if (g_live) bof3::Log("DIV-0041    full-frame fills at (%.0f, 0) %.0f x 240", Widescreen_FillX(), Widescreen_FillWidth());
    Widescreen_CheckSlides();
    // The faster slide: Field_RunTaskRecords (ours) puts Capcom's x back after
    // each handler, so the split is armed only while it is ours.
    const bool loop_ours = bof3::IsEnabled(bof3::addr::Field_RunTaskRecords);
    g_slide_armed = loop_ours;
    if (g_live)
        bof3::Log("DIV-0041    the faster slide: %s (the 14 slide-off states decide by Capcom's bounds; the box is drawn "
                  "to the bound moved out by %u columns on the frame it holds or is freed)",
                  g_slide_switch == 0 ? "off (BOF3X_ORIGINAL=Widescreen)"
                  : loop_ours         ? "on"
                                      : "off (Field_RunTaskRecords is Capcom's, which would not put Capcom's x back)",
                  g_live);
    Widescreen_CheckSweeps();   // DIV-0041 (I38): the two sweeps' map, below
}

// ---------------------------------------------------------------------------
// The two edge sweeps drawn mapped (widescreen.h; DIV-0041 amended, wide-ring
// round group SW, docs/wide-sweeps.md).

namespace {

// Kind 0xD's records marked this frame: the record and the x +0x2E held when
// its state queued it, so a record the frame reused or moved some other way
// is drawn as Capcom's. Thirty entries, as the overlay list has; emptied with
// it (Field_DrawFrame). Marks are made only while the map is on, so the table
// stays empty through every self-test and in narrow play.
struct SweepMark {
    unsigned char* rec;
    short x;
};
SweepMark g_sweep_marks[30];
unsigned g_sweep_count = 0;
// The draw in progress: the overlay whose +0x74 holds the mapped float, and
// Capcom's float to put back.
unsigned char* g_sweep_drawn = nullptr;
std::uint32_t g_sweep_kept = 0;
// Kind 0xD's half wants Sprite_DrawOverlays (which maps and puts back) and
// Field_DrawFrame (which empties the marks) ours: set by Widescreen_CheckSweeps.
bool g_sweep_overlays_ours = false;

// The columns the sweeps map by: the fill columns (0 until Widescreen_ArmFills,
// so 0 in every self-test, and 0 when narrow), and 0 under
// BOF3X_ORIGINAL=Widescreen (the faster slide's switch, which the same
// PatchBytes name sets).
unsigned SweepColumns() { return g_slide_switch != 0 ? Widescreen_Fill() : 0; }
unsigned MarkColumns() { return g_sweep_overlays_ours ? SweepColumns() : 0; }

short Word2E(const unsigned char* rec) { return static_cast<short>(rec[0x2E] | rec[0x2F] << 8); }

std::uint32_t FloatOfShort(int v) {
    const float f = static_cast<float>(static_cast<short>(v));
    std::uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    return bits;
}

// A log line for a sweep's first and last drawn x (the evidence a run reached
// it), at most 32 a run.
void SweepNote(const char* what, int x, int d, int first, int last) {
    static unsigned logged = 0;
    if ((x != first && x != last) || logged >= 32) return;
    ++logged;
    bof3::Log("DIV-0041    %s drawn mapped: logic x %d drawn at %d (%s)", what, x, d,
              x == first ? "its first frame" : "its last frame");
}

}  // namespace

void Widescreen_SweepMark(unsigned char* rec) {
    if (MarkColumns() == 0) return;
    for (unsigned i = 0; i < g_sweep_count; ++i)
        if (g_sweep_marks[i].rec == rec) {
            g_sweep_marks[i].x = Word2E(rec);
            return;
        }
    if (g_sweep_count >= sizeof g_sweep_marks / sizeof g_sweep_marks[0]) return;   // the list's own limit: drawn as Capcom's
    g_sweep_marks[g_sweep_count++] = {rec, Word2E(rec)};
}

void Widescreen_SweepBefore(unsigned char* sprite) {
    g_sweep_drawn = nullptr;
    const unsigned c = MarkColumns();
    if (c == 0) return;
    for (unsigned i = 0; i < g_sweep_count; ++i) {
        if (g_sweep_marks[i].rec != sprite) continue;
        const short x = Word2E(sprite);
        if (x != g_sweep_marks[i].x) return;
        const int d = rule::SlideMap(x, c);
        std::memcpy(&g_sweep_kept, sprite + 0x74, 4);
        const std::uint32_t bits = FloatOfShort(Sat16(d));
        std::memcpy(sprite + 0x74, &bits, 4);
        g_sweep_drawn = sprite;
        SweepNote("kind 0xD's slide", x, d, 360, -50);
        return;
    }
}

void Widescreen_SweepAfter(unsigned char* sprite) {
    if (g_sweep_drawn == nullptr) return;
    if (g_sweep_drawn == sprite) std::memcpy(sprite + 0x74, &g_sweep_kept, 4);
    g_sweep_drawn = nullptr;
}

void Widescreen_SweepClear() { g_sweep_count = 0; }

int Widescreen_SweepShadeX(int x) {
    const unsigned c = SweepColumns();
    if (c == 0) return x;
    const int d = rule::ShadeMap(static_cast<short>(x), c);
    SweepNote("area 172's shade", static_cast<short>(x), d, 10, 410);
    return static_cast<int>(static_cast<std::uint16_t>(Sat16(d)));
}

void Widescreen_CheckSweeps() {
    // The slide draws 360..160 and 130..-50 (390 less 30 a frame, held at
    // 160; idle once at -70 or less); the shade 10..410 (0 plus 10 a frame,
    // the script on above 400). Checked over a wider sweep, by ones: the
    // identity at 0 columns, the ends and fixed points where the rule puts
    // them, never decreasing, and the slide never drawn nearer the centre
    // than Capcom's x.
    static const unsigned kColumns[] = {0, rule::ColumnsForAspect(16, 9), rule::ColumnsForAspect(21, 9)};
    unsigned checks = 0;
    for (int which = 0; which < 2; ++which)
        for (const unsigned c : kColumns) {
            const char* const what = which == 0 ? "kind 0xD's slide" : "area 172's shade";
            const auto map = [&](int x) { return which == 0 ? rule::SlideMap(x, c) : rule::ShadeMap(x, c); };
            const int ci = static_cast<int>(c);
            const auto want = [&](const char* at, int x, int got, int expect) {
                ++checks;
                if (got != expect)
                    bof3::Fatal("widescreen: the sweep map's check: %s at %u columns, %s x %d: %d, want %d", what, c, at, x,
                                got, expect);
            };
            if (which == 0) {
                want("the start", rule::kSlideStart, map(rule::kSlideStart), rule::kSlideStart + ci);
                want("the end", rule::kSlideEnd, map(rule::kSlideEnd), rule::kSlideEnd - ci);
                want("the hold", rule::kSlideHold, map(rule::kSlideHold), rule::kSlideHold);
            } else {
                want("the start", rule::kShadeStart, map(rule::kShadeStart), rule::kShadeStart - ci);
                want("the panel's edge", rule::kShadePanel, map(rule::kShadePanel), rule::kShadePanel + ci);
                want("the end", rule::kShadeEnd, map(rule::kShadeEnd), rule::kShadeEnd + ci);
            }
            const int lo = which == 0 ? -100 : -70, hi = which == 0 ? 420 : 430;
            int last = -0x10000;
            for (int x = lo; x <= hi; ++x) {
                const int d = map(x);
                if (c == 0) want("the identity", x, d, x);
                if (d < last) want("monotonic", x, d, last);
                if (which == 0 && x > rule::kSlideHold) want("outward", x, d >= x ? 1 : 0, 1);
                if (which == 0 && x < rule::kSlideHold) want("outward", x, d <= x ? 1 : 0, 1);
                if (which == 1 && x >= rule::kShadePanel) want("anchored", x, d, x + ci);
                last = d;
            }
        }
    // The shade's entry as wired: the same int back while the map is off
    // (which it is here unless the view is wide and the switch on).
    const unsigned live = SweepColumns();
    for (int x = -0x8000; x <= 0xFFFF; x += 7) {
        const int got = Widescreen_SweepShadeX(x);
        const int expect =
            live == 0 ? x : static_cast<int>(static_cast<std::uint16_t>(Sat16(rule::ShadeMap(static_cast<short>(x), live))));
        ++checks;
        if (got != expect) bof3::Fatal("widescreen: the sweep map's check: the shade's entry at x %d: %d, want %d", x, got, expect);
    }
    if (g_sweep_count != 0) bof3::Fatal("widescreen: the sweep map's check: %u kind-0xD marks before play", g_sweep_count);
    g_sweep_overlays_ours =
        bof3::IsEnabled(bof3::addr::Sprite_DrawOverlays) && bof3::IsEnabled(bof3::addr::Field_DrawFrame);
    if (g_live)
        bof3::Log("DIV-0041    the two sweeps drawn mapped: shade %s, slide %s; kind 0xD's slide [-70, 390] onto [%d, %d] "
                  "(160 fixed), area 172's shade [0, 400] onto [%d, %d] (anchored right from 220) (the map's check: %u, 0 "
                  "differences at 0, 53 and 120 columns)",
                  live != 0 ? "on" : "off (BOF3X_ORIGINAL=Widescreen)",
                  MarkColumns() != 0 ? "on"
                  : live == 0        ? "off (BOF3X_ORIGINAL=Widescreen)"
                                     : "off (Sprite_DrawOverlays or Field_DrawFrame is Capcom's)",
                  rule::SlideMap(-70, g_live), rule::SlideMap(390, g_live), rule::ShadeMap(0, g_live),
                  rule::ShadeMap(400, g_live), checks);
    else
        bof3::Log("DIV-0041    the two sweeps' map check: %u, 0 differences at 0, 53 and 120 columns (the view is narrow: off)",
                  checks);
}

// The quicksave (SP-G, docs/quicksave.md, the graphics participant): the faster slide's split
// of the 22 window records - where each box is drawn against Capcom's x, and
// its leg - is the DLL's, not the span's, and changes in play: carried whole.
void Widescreen_QuickSave(bof3::SnapshotWriter& w) {
    w.Put(static_cast<std::uint32_t>(sizeof g_split));
    w.Put(g_split, sizeof g_split);
}

void Widescreen_QuickRestore(bof3::SnapshotReader& r) {
    const std::uint32_t n = r.Get<std::uint32_t>();
    if (n != sizeof g_split)
        bof3::Fatal("widescreen: a quicksave's slide split is %u bytes, this build's %u", n, static_cast<unsigned>(sizeof g_split));
    r.Get(g_split, sizeof g_split);
}
