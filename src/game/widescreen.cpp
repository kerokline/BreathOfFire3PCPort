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

#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

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
void Widescreen_ArmFills() {
    g_fills_armed = true;
    if (g_live) bof3::Log("DIV-0041    full-frame fills at (%.0f, 0) %.0f x 240", Widescreen_FillX(), Widescreen_FillWidth());
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
// owner's screenshots, 2026-09-23); each moves outward by the columns
// added, so the box clears the new edge as it cleared the old. The bound is
// an immediate: `mov ecx, imm32` (9 sites) or `cmp cx / ax, imm16` (5).
// Scan and disassembly 2026-09-23 (widescreen.md section 5).
// Every one of the fourteen is in a function that is ours now, and ours reads
// the bound from `now` (Widescreen_SlideBound), the value this file writes -
// never the operand (docs/operand-store.md); hosted, Operands_Check compares
// the two.
struct Slide {
    U at;               // the immediate's first byte
    U size;             // 2 or 4
    int bound;          // the original's; negative slides off the left, positive off the right
    std::int32_t now;   // the bound in this build: `bound`, or the widened one once patched (an imm16's in its low word)
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

}  // namespace

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
        // 21:9: every bound still fits the imm16 its site holds. A box that
        // steps 0x20 a frame from inside the view reaches it: each reader steps
        // x monotonically and frees past it or clamps at it (widescreen.md
        // section 5, docs/ultrawide-frame.md), so it is passed after at most
        // (|bound| + 320) / 0x20 frames.
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
    for (Slide& s : g_slides) {
        std::uint8_t expected[4], replacement[4];
        const int wide = rule::SlideBound(s.bound, columns);
        if (s.size == 4) {
            std::memcpy(expected, &s.bound, 4);
            std::memcpy(replacement, &wide, 4);
        } else {
            const std::int16_t e = static_cast<std::int16_t>(s.bound), w = static_cast<std::int16_t>(wide);
            std::memcpy(expected, &e, 2);
            std::memcpy(replacement, &w, 2);
        }
        if (bof3::PatchBytes("Widescreen", s.at, expected, replacement, s.size)) s.now = wide;
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
