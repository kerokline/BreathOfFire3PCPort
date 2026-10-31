#include "game/layering.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "hook/draw_order.h"
#include "hook/log.h"
#include "hook/snapshot.h"

namespace layering {

int g_mode = kOff;

namespace {

// How many layers later a sprite may be drawn, and how far above the feet a
// cell's lowest corner may stand and still be floor (in corner units: 16 world
// units, a cell being 128 wide). BOF3X_LAYERING_AHEAD and BOF3X_LAYERING_RISE
// move them for tuning.
int g_ahead = 3;
int g_rise = 2;
// The feet and the shadow, round the sprite's screen point; and another
// sprite's body, round its own.
constexpr float kFeetHalfWidth = 14.0f, kFeetAbove = 6.0f, kFeetBelow = 8.0f;
constexpr float kBodyHalfWidth = 14.0f, kBodyAbove = 44.0f, kBodyBelow = 6.0f;
// A cell record this many cells from the sprite's own is taken to be in the way.
constexpr int kRecordReach = 2;
bool g_log;
const char* g_why = "";   // BOF3X_LAYERING_LOG: what stopped the last sprite judged

constexpr unsigned kViewCells = 0x38 * 28;
std::uint16_t g_cell_of[0x1000];   // draw item index -> view cell + 1, rebuilt each pass
// The same for a cell's own quad and its two side triangles (the item's words
// +0x7E and +0x8E, docs/map-layers.md): the table faerie's test (TableFaerie).
std::uint16_t g_owner_of[0x1000];

constexpr unsigned kSprites = 0x28;   // Sprite_UpdateScreen's limit on the draw list
struct Moved { unsigned char* sprite; std::uint16_t key; };
Moved g_moved[kSprites];
unsigned g_moved_n;

template <typename T>
T Get(const void* p) {
    T v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Pointer(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

int EnvInt(const char* name, int fallback, int lo, int hi) {
    char text[16];
    const DWORD n = GetEnvironmentVariableA(name, text, sizeof text);
    if (n == 0 || n >= sizeof text) return fallback;
    char* end = nullptr;
    const long v = std::strtol(text, &end, 10);
    if (end == nullptr || *end != '\0' || v < lo || v > hi) bof3::Fatal("%s must be %d..%d", name, lo, hi);
    return static_cast<int>(v);
}

struct Box { float x0, y0, x1, y1; };
bool Overlap(const Box& a, const Box& b) { return a.x1 > b.x0 && a.x0 < b.x1 && a.y1 > b.y0 && a.y0 < b.y1; }

bool InPool(std::uint32_t prim) {
    const std::uint32_t base = Address(draw_pool::Items());
    return prim >= base && prim < base + draw_pool::Count() * 0x90u && (prim - base) % 0x48u == 0;
}

bool QuadReaches(const float* x, const float* y, const Box& target);
constexpr unsigned kRound[4] = {0x08u, 0x18u, 0x38u, 0x28u};   // a quad's perimeter: points 0, 1, 3, 2

// A draw item's quad or a POLY_FT4: four float points at +8, +0x18, +0x28 and
// +0x38, in strip order. Anything else has no shape here, and counts as in
// the way (`known` false, the result true).
bool Reaches(std::uint32_t prim, const Box& target, bool* known = nullptr) {
    const unsigned char* const p = Pointer(prim);
    if (known) *known = InPool(prim) || p[7] == 0x2C;
    if (!InPool(prim) && p[7] != 0x2C) return true;
    float x[4], y[4];
    for (unsigned k = 0; k < 4; ++k) {
        x[k] = Get<float>(p + kRound[k]);
        y[k] = Get<float>(p + kRound[k] + 4);
    }
    return QuadReaches(x, y, target);
}

// Four points in perimeter order against a box.
bool QuadReaches(const float* x, const float* y, const Box& target) {
    Box box = {1e9f, 1e9f, -1e9f, -1e9f};
    for (unsigned k = 0; k < 4; ++k) {
        if (x[k] < box.x0) box.x0 = x[k];
        if (x[k] > box.x1) box.x1 = x[k];
        if (y[k] < box.y0) box.y0 = y[k];
        if (y[k] > box.y1) box.y1 = y[k];
    }
    if (!Overlap(box, target)) return false;
    // The quad taken as convex: apart when all of the target's corners lie
    // outside one of its edges, whichever way round it winds.
    const float cx[4] = {target.x0, target.x1, target.x1, target.x0}, cy[4] = {target.y0, target.y0, target.y1, target.y1};
    float area = 0.0f;
    for (unsigned k = 0; k < 4; ++k) area += x[k] * y[(k + 1) & 3] - x[(k + 1) & 3] * y[k];
    if (area == 0.0f) return true;
    for (unsigned k = 0; k < 4; ++k) {
        const float ex = x[(k + 1) & 3] - x[k], ey = y[(k + 1) & 3] - y[k];
        if (ex == 0.0f && ey == 0.0f) continue;
        bool all_outside = true;
        for (unsigned c = 0; c < 4 && all_outside; ++c) {
            const float side = ex * (cy[c] - y[k]) - ey * (cx[c] - x[k]);
            if (area > 0.0f ? side >= 0.0f : side <= 0.0f) all_outside = false;
        }
        if (all_outside) return false;
    }
    return true;
}

// A cell's own quad (not a side item's) that can be walked on and whose lowest
// corner stands no more than g_rise above `feet` - the comparison the
// original's draw table makes between a cell and a sprite of one layer, here
// across layers.
bool IsFloor(std::uint32_t prim, int feet) {
    if (!InPool(prim)) return false;
    const unsigned index = (prim - Address(draw_pool::Items())) / 0x90u;
    if (g_cell_of[index] == 0) return false;
    const unsigned char* const cell = MapView_CellItems + (g_cell_of[index] - 1u) * 4u;
    const unsigned char* const corners =
        reinterpret_cast<const unsigned char*>(&AreaMap_Corners) + (cell[1] * AreaMap_Header[0] + cell[0]) * 4u;
    int lowest = static_cast<signed char>(corners[0]);
    for (unsigned k = 1; k < 4; ++k)
        if (static_cast<signed char>(corners[k]) < lowest) lowest = static_cast<signed char>(corners[k]);
    if (lowest > feet + g_rise) return false;
    // Ground the party cannot step onto - forest, water, a wall's foot - is
    // drawn as something standing there, and keeps covering what is behind it.
    return AreaMap_CellBlocked(cell[0], cell[1]) == 0;
}

enum Need { kNothing, kFloorOnly };

// The lowest and highest of a map cell's four corners.
struct Height { int lo, hi; };
Height CellHeight(unsigned x, unsigned z) {
    const signed char* const c = reinterpret_cast<const signed char*>(&AreaMap_Corners) + (z * AreaMap_Header[0] + x) * 4u;
    Height h = {c[0], c[0]};
    for (unsigned k = 1; k < 4; ++k) {
        if (c[k] < h.lo) h.lo = c[k];
        if (c[k] > h.hi) h.hi = c[k];
    }
    return h;
}

// A draw item that is a cell's top or one of its side triangles, the cell
// standing at exactly `height` (its lowest and highest corners).
bool SameHeightCell(std::uint32_t prim, const Height& height) {
    if (!InPool(prim)) return false;
    const unsigned index = (prim - Address(draw_pool::Items())) / 0x90u;
    if (g_owner_of[index] == 0) return false;
    const unsigned char* const cell = MapView_CellItems + (g_owner_of[index] - 1u) * 4u;
    const Height h = CellHeight(cell[0], cell[1]);
    return h.lo == height.lo && h.hi == height.hi;
}

// What stops the sprite at the layer before: a primitive that reaches the
// feet and is not floor allowed there, or one that is not floor and reaches
// the sprite anywhere (`whole_box`, the feet and the body) - a raised cell
// of a later layer stands in front of the sprite even where it covers only
// the body (the owner's Nina behind a crate, 2026-10-06: the crate's top
// reached her head, not her feet, and she was drawn through it). With
// `table` (the table faerie, the layer it stands in), the top or a side of
// a cell as high as the one it stands inside stops nothing.
bool Stops(std::uint32_t prim, const Box& feet_box, const Box& whole_box, int feet, Need need, const Height* table = nullptr) {
    if (table != nullptr && SameHeightCell(prim, *table)) return false;
    if (Reaches(prim, feet_box) && !(need == kFloorOnly && IsFloor(prim, feet))) return true;
    return Reaches(prim, whole_box) && !IsFloor(prim, feet);
}

// One of a layer's three lists (0, 1, 2: the first, the second, the frame
// nodes): false when a primitive of it stops the sprite (Stops).
bool ListClear(unsigned layer, unsigned which, const Box& feet_box, const Box& whole_box, int feet, Need need,
               const Height* table = nullptr) {
    const unsigned long* const list = DrawLayers + which * 4u + (Gfx_BufferIndex + layer * 6u) * 2u;   // first, last
    std::uint32_t at = list[0];
    for (unsigned steps = 0; at != 0; ++steps) {
        if (steps == 256) return false;
        if (Stops(at, feet_box, whole_box, feet, need, table)) {
            if (g_log && draw_order::Tagging() && InPool(at)) {
                const unsigned index = (at - Address(draw_pool::Items())) / 0x90u;
                const unsigned char* const cell = MapView_CellItems + (g_cell_of[index] ? g_cell_of[index] - 1u : 0u) * 4u;
                const signed char* const c = reinterpret_cast<const signed char*>(&AreaMap_Corners) +
                                             (cell[1] * AreaMap_Header[0] + cell[0]) * 4u;
                bof3::Log("DIV-0071      layer %u list %u: item %u (%s cell %u,%u corners %d %d %d %d, byte %02X)",
                          layer, which, index, g_cell_of[index] ? "own" : "no", static_cast<unsigned>(cell[0]),
                          static_cast<unsigned>(cell[1]), c[0], c[1], c[2], c[3],
                          static_cast<unsigned>(AreaMap_ByteAt(cell[0], cell[1])));
            }
            return false;
        }
        if (at == list[1]) break;
        at = Get<std::uint32_t>(Pointer(at)) & 0xFFFFFFu;
    }
    return true;
}

// The view row DrawLayer_Open walks for `layer`: false when one of its cells'
// record runs belongs to a map cell within kRecordReach of the sprite's.
bool RecordsClear(unsigned layer, int cell_x, int cell_z) {
    int row = MapView_Row + static_cast<int>(layer) + 1;
    if (row >= 0x38) row -= 0x38;
    if (row < 0 || row >= 0x38) return false;
    for (unsigned column = 0; column < 28; ++column) {
        const unsigned word = MapView_Cells[row * 28 + column];
        if (word == 0) continue;
        const std::uint32_t head = Get<std::uint32_t>(AreaMap_Header + (word + (AreaMap_CellBase & 0xFFFFu)) * 4u - 4u);
        const int dx = static_cast<int>((head >> 8) & 0xFF) - cell_x, dz = static_cast<int>(head & 0xFF) - cell_z;
        if (dx >= -kRecordReach && dx <= kRecordReach && dz >= -kRecordReach && dz <= kRecordReach) return false;
    }
    return true;
}

// What each record run drew, two generations: the pass being drawn fills
// g_runs[g_pass & 1]; Defer, before the pass draws anything, reads the other,
// stamped g_pass - 1. A stamp of 0 is never a pass's.
// A run keeps its quads (POLY_FT4 and POLY_G4, the shapes MapCell_DrawQuads,
// MapCell_DrawFrames and MapCell_DrawShaded commit), each in perimeter order,
// up to kQuads; past that, their union box. Any other primitive is not kept,
// nor a quad with a point off any screen (a vertex behind the eye projects to
// 1e24 and the like): for those the proximity test (RecordsClear) is all
// there is, as before this test existed.
constexpr unsigned kRuns = 2048;   // a power of two; a view names at most 0x38 * 28 = 1,568 runs
constexpr unsigned kQuads = 8;
constexpr float kFar = 4096.0f;
struct Run {
    std::uint32_t at, stamp;
    unsigned n;       // quads kept
    bool overflow;    // more than kQuads: `box` holds the rest
    Box box;
    float x[kQuads][4], y[kQuads][4];
};
Run g_runs[2][kRuns];
// The primitives the run being drawn has committed, read at RunEnd. A handler
// may write a primitive's points after committing it: MapCell_DrawUprights
// commits each pair of leaf quads, then writes their outer vertices 0 and 1
// (as Capcom's 0x570210 does). Read at the commit, those were whatever the
// packet pool held there from an earlier pass - on some passes a bush's six
// quads all collapsed onto its trunk, and a sprite behind it was moved in front
// of it one pass in three (known-defects D306, GitHub issue #79).
constexpr unsigned kPending = 64;
const unsigned char* g_pending[kPending];
unsigned g_pending_n;
std::uint32_t g_pass = 1;
unsigned g_runs_now, g_runs_before;   // runs drawn in this pass, and in the last
Run* g_run;                           // the run being drawn, or null
bool g_run_boxes = true;              // BOF3X_LAYERING_RECORDS=0: the proximity test alone (a control)

// What each sprite drew in the last pass (amended 2026-10-21, docs/garr-fire.md):
// the screen box of the cell sprite (code 0x84) Sprite_Draw committed for it,
// as D3d_BuildCellTexture lays its cells out (draw_order.cpp's box for the
// code). RunsClear tests the runs against it as well as the body box: Garr
// seated by the camp's fire is 104 px wide, and his arm and weapon reach over
// the fire's quads 20 px beyond the body box's side. Two generations, as the
// runs: the pass being drawn fills g_drawn[g_pass & 1]. A sprite not drawn in
// the last pass (culled, new, its packet skipped) has no box, and the body
// box alone is tested, as before this amendment.
constexpr unsigned kDrawn = 64;   // Sprite_UpdateScreen lists at most 0x28 a pass
struct Drawn { std::uint32_t at; Box box; };
Drawn g_drawn[2][kDrawn];
unsigned g_drawn_n[2];
std::uint32_t g_drawn_stamp[2];
unsigned char* g_sprite_mark;            // Gfx_PacketNext before the sprite being drawn, or null
bool g_sprite_boxes = true;              // BOF3X_LAYERING_SPRITEBOX=0: the body box alone (a control)

const Box* DrawnBox(const unsigned char* sprite) {
    const unsigned g = (g_pass - 1) & 1;
    if (!g_sprite_boxes || g_drawn_stamp[g] != g_pass - 1) return nullptr;
    for (unsigned i = 0; i < g_drawn_n[g]; ++i)
        if (g_drawn[g][i].at == Address(sprite)) return &g_drawn[g][i].box;
    return nullptr;
}

void Keep(const unsigned char* sprite, const Box& box);

// The cell sprite at `prim`, just committed for `sprite`: its box kept.
void KeepDrawn(const unsigned char* sprite, const unsigned char* prim) {
    const unsigned first = Get<std::uint16_t>(prim + 0x18), count = Get<std::uint16_t>(prim + 0x1A);
    if (count == 0 || count > 256) return;
    int l = 0, r = 0, t = 0, b = 0;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const rec = SpriteCell_Table + (first + i) * 8u;
        const int x = Get<std::int16_t>(rec), y = static_cast<signed char>(rec[2]);
        const int w = static_cast<int>(rec[3] & 0xF) * 8, h = static_cast<int>(rec[3] >> 4) * 8;
        if (x < l) l = x;
        if (y < t) t = y;
        if (x + w > r) r = x + w;
        if (y + h > b) b = y + h;
    }
    const float x = Get<float>(prim + 8), y = Get<float>(prim + 0xC);
    const float sx = Get<float>(prim + 0x10), sy = Get<float>(prim + 0x14);
    const bool flip = (Get<std::uint16_t>(prim + 0x1E) & 0x400) != 0;
    const Box box = {flip ? x - r * sx : x + l * sx, y + t * sy, flip ? x - l * sx : x + r * sx, y + b * sy};
    Keep(sprite, box);
}

// `box` kept as what `sprite` drew this pass (DrawnBox next pass); boxes off
// any screen are not (a vertex behind the eye projects to 1e24 and the like).
void Keep(const unsigned char* sprite, const Box& box) {
    if (!(box.x0 > -kFar && box.x1 < kFar && box.y0 > -kFar && box.y1 < kFar)) return;   // NaN included
    const unsigned g = g_pass & 1;
    if (g_drawn_stamp[g] != g_pass) {
        g_drawn_stamp[g] = g_pass;
        g_drawn_n[g] = 0;
    }
    for (unsigned i = 0; i < g_drawn_n[g]; ++i) {
        if (g_drawn[g][i].at != Address(sprite)) continue;
        Box& kept = g_drawn[g][i].box;   // drawn twice in a pass: both
        if (box.x0 < kept.x0) kept.x0 = box.x0;
        if (box.y0 < kept.y0) kept.y0 = box.y0;
        if (box.x1 > kept.x1) kept.x1 = box.x1;
        if (box.y1 > kept.y1) kept.y1 = box.y1;
        return;
    }
    if (g_drawn_n[g] == kDrawn) return;
    g_drawn[g][g_drawn_n[g]++] = {Address(sprite), box};
}

Run* FindRun(std::uint32_t at, std::uint32_t stamp, bool claim) {
    Run* const table = g_runs[stamp & 1];
    for (unsigned i = (at >> 2) * 2654435761u >> 21, n = 0; n < kRuns; ++n, i = (i + 1) & (kRuns - 1)) {
        if (table[i].stamp != stamp) {
            if (!claim) return nullptr;
            table[i].at = at;
            table[i].stamp = stamp;
            table[i].n = 0;
            table[i].overflow = false;
            table[i].box = {1e9f, 1e9f, -1e9f, -1e9f};
            ++g_runs_now;
            return &table[i];
        }
        if (table[i].at == at) return &table[i];
    }
    return nullptr;
}

// Whether what `run` drew in the last pass reaches `target`.
bool RunReaches(const Run& run, const Box& target) {
    for (unsigned q = 0; q < run.n; ++q)
        if (QuadReaches(run.x[q], run.y[q], target)) return true;
    return run.overflow && Overlap(run.box, target);
}

// The runs DrawLayer_Open walks for `layer` (its row and columns, computed as
// it computes them): false when one of them drew, in the last pass, a quad
// that reaches `whole_box` (the body and feet, grown to what the sprite drew
// last pass: LayerFor's reach_box), or was not drawn in the last pass at all.
bool RunsClear(unsigned layer, const Box& whole_box) {
    if (!g_run_boxes || g_runs_before == 0) return true;   // nothing recorded: DrawLayer_Open not ours, say
    int row = MapView_Row + static_cast<int>(layer) + 1;
    if (row >= 0x38) row -= 0x38;
    if (row < 0 || row >= 0x38) return false;
    int inset = MapView_Inset;
    if (Field_InputFlags & 1) inset = inset >= 2 ? inset - 2 : 0;
    int column = MapView_Column + inset + (~row & 1);
    if (column >= 0x1C) column -= 0x1C;
    for (int count = (14 - inset) * 2; count > 0; --count) {
        column = column == 0x1B ? 0 : column + 1;
        if (column < 0 || column >= 0x1C) return false;
        const unsigned word = MapView_Cells[row * 28 + column];
        if (word == 0) continue;
        const std::uint32_t at = Address(AreaMap_Header + (word + (AreaMap_CellBase & 0xFFFFu)) * 4u);
        const Run* const run = FindRun(at, g_pass - 1, false);
        if (run == nullptr || RunReaches(*run, whole_box)) {
            if (g_log && draw_order::Tagging()) {
                if (run == nullptr)
                    bof3::Log("DIV-0071      layer %u: the record run at %08X (view column %d) was not drawn last pass", layer, at, column);
                else
                    bof3::Log("DIV-0071      layer %u: the record run at %08X (view column %d) drew a quad over the sprite last "
                              "pass (%u kept%s; the sprite's box (%g, %g)-(%g, %g))", layer, at, column, run->n,
                              run->overflow ? ", more boxed" : "", whole_box.x0, whole_box.y0, whole_box.x1, whole_box.y1);
            }
            return false;
        }
    }
    return true;
}

Box BoxRound(const unsigned char* sprite, float half_width, float above, float below) {
    const float x = Get<float>(sprite + 0x74), y = Get<float>(sprite + 0x78);
    return {x - half_width, y - above, x + half_width, y + below};
}

// `box` grown to what `sprite` drew in the last pass (DrawnBox), when it was drawn.
Box Reach(const unsigned char* sprite, Box box) {
    if (const Box* const drawn = DrawnBox(sprite)) {
        if (drawn->x0 < box.x0) box.x0 = drawn->x0;
        if (drawn->y0 < box.y0) box.y0 = drawn->y0;
        if (drawn->x1 > box.x1) box.x1 = drawn->x1;
        if (drawn->y1 > box.y1) box.y1 = drawn->y1;
    }
    return box;
}

// The table faerie (DIV-0151, known-defects D300, docs/faerie-fix.md): in area 175, the
// faerie village's room where the faeries gather after the Faerie Tiara, the
// gold faerie stands at floor height inside the table's raised cell, and the
// next plank's top, in its layer's second list, is drawn over its skirt.
// Capcom's, on both platforms; the owner's decision (2026-10-15) is to fix
// this one scene. The faerie is known by what it is, never by a screen
// position: a field object (Sprite_Objects) of area 175 running the area's
// movement script kTableFaerieScript (object +0x83, the index into the area
// descriptor's +0x10 table that Field_ObjectUpdate steps,
// docs/movement-script.md section 1b). The cell condition is
// docs/faerie-over-table.md section 4's: its feet below its own map cell's
// lowest corner.
constexpr unsigned kTableArea = 175;
constexpr unsigned kTableFaerieScript = 1;

// Whether `sprite` is the table faerie standing inside its raised cell.
// `height` gets that cell's.
bool TableFaerie(const unsigned char* sprite, int feet, int cell_x, int cell_z, Height* height) {
    if (Game_AreaNumber != kTableArea) return false;
    const unsigned char* const objects = reinterpret_cast<const unsigned char*>(::bof3::Img(Sprite_Objects_addr));
    if (sprite < objects || sprite >= objects + 30u * 0xA4u || (sprite - objects) % 0xA4u != 0) return false;
    if (sprite[0x83] != kTableFaerieScript) return false;
    if (cell_x <= 0 || cell_z <= 0 || cell_x >= AreaMap_Header[0] || cell_z >= AreaMap_Header[1]) return false;
    const Height h = CellHeight(static_cast<unsigned>(cell_x), static_cast<unsigned>(cell_z));
    if (!(feet < h.lo)) return false;
    *height = h;
    return true;
}

// When the faerie is drawn a layer later, its table's tops no longer cover
// it, and neither would the sides: but the sides are what hid its shadow, a
// piece of the same sprite at its ground point on the floor inside the table
// (the black oval on the right plank's front, otherwise). So the side
// triangles of its table in the second list it passed, those within reach of
// it, are drawn again right after it (AfterSprite): copies, in the packet
// pool, of what the list drew - opaque quads, the same pixels again.
constexpr unsigned kFaerieSides = 8;
constexpr std::uint32_t kPoolLimit = 0x7F1BAC;   // Gfx_PacketPools 0x7E1C00 + 0x10000 - 0x54 (draw_emit.cpp's)
constexpr float kFaerieHalfWidth = 24.0f;   // the faerie's wings reach wider than a walking sprite's body
const unsigned char* g_faerie;               // the table faerie drawn a layer later this pass, or null
std::uint32_t g_faerie_sides[kFaerieSides];
unsigned g_faerie_sides_n;
Box g_faerie_reach;                          // their union box

// Each primitive of one of a layer's lists, as ListClear walks it.
template <typename F>
void ForEachInList(unsigned layer, unsigned which, F&& f) {
    const unsigned long* const list = DrawLayers + which * 4u + (Gfx_BufferIndex + layer * 6u) * 2u;   // first, last
    std::uint32_t at = list[0];
    for (unsigned steps = 0; at != 0 && steps < 256; ++steps) {
        f(at);
        if (at == list[1]) break;
        at = Get<std::uint32_t>(Pointer(at)) & 0xFFFFFFu;
    }
}

// The layer `sprite` (number `index` of the sorted draw list, in `layer`) is
// drawn in.
unsigned LayerFor(unsigned index, unsigned layer) {
    const unsigned char* const sprite = Sprite_DrawList[index];
    const float sx = Get<float>(sprite + 0x74), sy = Get<float>(sprite + 0x78);
    if (!(sx > -64.0f && sx < 1024.0f && sy > -64.0f && sy < 512.0f)) return layer;   // NaN included
    // The feet in corner units, as the sprite's own key has them.
    const int feet = static_cast<signed char>(Get<std::uint16_t>(sprite + 0x3E) >> 5);
    const int cell_x = Get<std::uint16_t>(sprite + 0x36), cell_z = Get<std::uint16_t>(sprite + 0x3A);
    Height table_height;
    const Height* const table = TableFaerie(sprite, feet, cell_x, cell_z, &table_height) ? &table_height : nullptr;
    // A model (+0x24 bit 0x40: its quads are records, Sprite_AddDrawRecords)
    // stays in its layer. The pass sorts the records of a layer's sprites and
    // models together by depth, so a model moved out of its layer leaves that
    // sort: the dragon gene drawn over its globe's facets instead of inside
    // them (known-defects D242, docs/faerie-over-table.md section 6).
    if (sprite[0x24] & 0x40) {
        if (g_log && draw_order::Tagging())
            bof3::Log("DIV-0071    frame %u: sprite %08X is a model: layer %u kept (key %04X, at (%g, %g), cell %d,%d, feet %d, pos %08X %08X)",
                      static_cast<unsigned>(Frame_Counter), Address(sprite), layer, Get<std::uint16_t>(sprite + 0x32), sx, sy,
                      cell_x, cell_z, feet, Get<std::uint32_t>(sprite + 0x34), Get<std::uint32_t>(sprite + 0x38));
        return layer;
    }
    const Box feet_box = BoxRound(sprite, kFeetHalfWidth, kFeetAbove, kFeetBelow);
    const Box whole_box = BoxRound(sprite, kBodyHalfWidth, kBodyAbove, kFeetBelow);   // the body and the feet
    // For the record runs and the later sprites: the body and the feet, and
    // what the sprite drew last pass (DrawnBox; docs/garr-fire.md).
    const Box reach_box = Reach(sprite, whole_box);
    // The same for a sprite beside a model of its own layer: moved, it would
    // leave the depth sort it shares with the model's quads.
    for (unsigned other = 0; other < Sprite_DrawListCount; ++other) {
        const unsigned char* const o = Sprite_DrawList[other];
        if (other == index || !(o[0x24] & 0x40) || static_cast<unsigned>(Get<std::uint16_t>(o + 0x32) >> 8) != layer) continue;
        if (Overlap(Reach(o, BoxRound(o, kBodyHalfWidth, kBodyAbove, kBodyBelow)), reach_box)) {
            if (g_log && draw_order::Tagging())
                bof3::Log("DIV-0071    frame %u: sprite %08X beside model %08X in layer %u: kept",
                          static_cast<unsigned>(Frame_Counter), Address(sprite), Address(o), layer);
            return layer;
        }
    }
    // What the sprite would newly be drawn over in layer `next`: the second
    // list and the later sprites and table items of the layer before, then
    // this layer's first list, cell records and frame nodes. `passed`: the
    // table faerie's table, whose tops and sides in the second list of the
    // layer before stop nothing.
    const auto clear_to = [&](unsigned next, const Height* passed) {
        if (!ListClear(next - 1, 1, feet_box, whole_box, feet, kNothing, passed)) { g_why = "the layer before's second list"; return false; }
        for (unsigned other = index + 1; other < Sprite_DrawListCount; ++other) {
            const unsigned char* const o = Sprite_DrawList[other];
            if (static_cast<unsigned>(Get<std::uint16_t>(o + 0x32) >> 8) > next - 1) break;
            // Each side grown to what it drew (issue 132): Garr's spear over
            // Ryu at the camp is 23 px clear of both body boxes.
            if (Overlap(Reach(o, BoxRound(o, kBodyHalfWidth, kBodyAbove, kBodyBelow)), reach_box)) {
                g_why = "a sprite or a table item of the layer before";
                return false;
            }
        }
        for (unsigned item = 0; item < DrawTable_Count; ++item) {
            const unsigned long entry = DrawTable[item];
            if ((entry >> 24) != next - 1) continue;
            const std::uint32_t prim =
                Address(draw_pool::Items() + (Gfx_BufferIndex + (entry & 0xFFFu) * 2u) * 0x48u);
            if (Stops(prim, feet_box, whole_box, feet, kFloorOnly)) {
                g_why = "a sprite or a table item of the layer before";
                return false;
            }
        }
        if (!ListClear(next, 0, feet_box, whole_box, feet, kFloorOnly)) { g_why = "the first list: not floor"; return false; }
        if (!RecordsClear(next, cell_x, cell_z)) { g_why = "a cell record nearby"; return false; }
        if (!RunsClear(next, reach_box)) { g_why = "what a cell record drew"; return false; }
        if (!ListClear(next, 2, feet_box, whole_box, feet, kNothing)) { g_why = "a frame node"; return false; }
        return true;
    };
    unsigned drawn_in = layer;
    // DIV-0191 (docs/mountain-passes.md section 2): a party member on the
    // Zublo walkway's strip, (0x38, 0x1C) or (0x38, 0x1B) of the Rhapala map,
    // is drawn as far ahead as the engine allows, with no stop test. The
    // strip's art is a block painted a step higher than its mesh (corners
    // 0x10 under a 0x16 block), so the painted face of the cell in front
    // covers the member's legs when the pass keeps them in their layer; and
    // nothing nearer the camera than the strip is anything but that block's
    // paint and ground (the hut stands behind, up-left). The owner's eye,
    // 2026-10-30 night: "render on top of the road".
    if (g_mountain_passes && Game_AreaNumber == 87 && (cell_z == 0x1C || cell_z == 0x1B) && cell_x == 0x38) {
        const unsigned char* const trio = reinterpret_cast<const unsigned char*>(::bof3::Img(ObjTrio_addr));
        if (sprite >= trio && sprite < trio + 3u * 0x14Cu) {
            drawn_in = layer + static_cast<unsigned>(g_ahead);
            if (drawn_in > 0x36) drawn_in = 0x36;
            if (g_log && draw_order::Tagging())
                bof3::Log("DIV-0191    frame %u: member %08X on the Zublo strip cell %d,%d: layer %u drawn in layer %u, no stop test",
                          static_cast<unsigned>(Frame_Counter), Address(sprite), cell_x, cell_z, layer, drawn_in);
            return drawn_in;
        }
    }
    for (unsigned next = layer + 1; next < 0x37 && next <= layer + static_cast<unsigned>(g_ahead); ++next) {
        if (!clear_to(next, nullptr)) break;
        drawn_in = next;
    }
    // The table faerie (TableFaerie), where DIV-0071 kept it in its layer:
    // one layer later, past its own table's tops and sides, if nothing else
    // stops it there.
    if (table != nullptr && drawn_in == layer) {
        const char* const why = g_why;
        // One faerie a pass: a second object running the same script stays.
        const bool moved = g_faerie == nullptr && layer + 1 < 0x37 && clear_to(layer + 1, table);
        if (moved) {
            drawn_in = layer + 1;
            g_faerie_sides_n = 0;
            const Box reach = BoxRound(sprite, kFaerieHalfWidth, kBodyAbove, kFeetBelow);
            g_faerie_reach = {1e9f, 1e9f, -1e9f, -1e9f};
            ForEachInList(layer, 1, [&](std::uint32_t prim) {
                const unsigned item = (prim - Address(draw_pool::Items())) / 0x90u;
                if (!InPool(prim) || g_cell_of[item] != 0 || !SameHeightCell(prim, *table) || !Reaches(prim, reach)) return;
                if (g_faerie_sides_n == kFaerieSides) {   // more than the table can have: not moved after all
                    drawn_in = layer;
                    return;
                }
                g_faerie_sides[g_faerie_sides_n++] = prim;
                const unsigned char* const p = Pointer(prim);
                for (unsigned k = 0; k < 4; ++k) {
                    const float x = Get<float>(p + kRound[k]), y = Get<float>(p + kRound[k] + 4);
                    if (x < g_faerie_reach.x0) g_faerie_reach.x0 = x;
                    if (x > g_faerie_reach.x1) g_faerie_reach.x1 = x;
                    if (y < g_faerie_reach.y0) g_faerie_reach.y0 = y;
                    if (y > g_faerie_reach.y1) g_faerie_reach.y1 = y;
                }
            });
            if (drawn_in != layer) g_faerie = sprite;
            else g_why = "more table sides than kept";
        } else {
            g_why = why;
        }
        if (g_log)
            bof3::Log("D300 fix    frame %u: the table faerie %08X (area %u, script %u), cell %d,%d (corners %d..%d), feet %d: "
                      "layer %u drawn in layer %u%s%s; %u table side(s) drawn again after it",
                      static_cast<unsigned>(Frame_Counter), Address(sprite), kTableArea, kTableFaerieScript, cell_x, cell_z,
                      table->lo, table->hi, feet, layer, drawn_in, drawn_in == layer ? "; stopped by " : "",
                      drawn_in == layer ? g_why : "", drawn_in == layer ? 0u : g_faerie_sides_n);
    }
    if (g_log && draw_order::Tagging())
        bof3::Log("DIV-0071    frame %u: sprite %08X at (%g, %g), cell %d,%d, feet %d, key %04X, pos %08X %08X: layer %u drawn in layer %u; stopped by %s",
                  static_cast<unsigned>(Frame_Counter), Address(sprite), sx, sy, cell_x, cell_z, feet, Get<std::uint16_t>(sprite + 0x32),
                  Get<std::uint32_t>(sprite + 0x34), Get<std::uint32_t>(sprite + 0x38), layer, drawn_in,
                  drawn_in == layer + static_cast<unsigned>(g_ahead) ? "nothing" : g_why);
    return drawn_in;
}

}  // namespace

void Arm() {
    g_mode = EnvInt("BOF3X_LAYERING", kFloor, 0, 2);   // on unless BOF3X_LAYERING=0 (the owner, 2026-10-03)
    if (g_mode == kOff) return;
    g_ahead = EnvInt("BOF3X_LAYERING_AHEAD", g_ahead, 1, 8);
    g_rise = EnvInt("BOF3X_LAYERING_RISE", g_rise, 0, 64);
    g_log = EnvInt("BOF3X_LAYERING_LOG", 0, 0, 1) != 0;
    g_run_boxes = EnvInt("BOF3X_LAYERING_RECORDS", 1, 0, 1) != 0;
    g_sprite_boxes = EnvInt("BOF3X_LAYERING_SPRITEBOX", 1, 0, 1) != 0;
    if (g_mode == kFloor)
        bof3::Log("DIV-0071    on: a sprite is drawn up to %d layers later, over floor only (a lowest corner up to %d "
                  "above the feet is floor)", g_ahead, g_rise);
    else
        bof3::Log("DIV-0071    comparison mode 2: every sprite's key one layer later");
}

bool Defer() {
    g_moved_n = 0;
    g_faerie = nullptr;
    g_faerie_sides_n = 0;
    // A new generation of record runs: the pass about to draw fills it.
    g_runs_before = g_runs_now;
    g_runs_now = 0;
    if (++g_pass == 0) g_pass = 1;   // past 2^32 passes: never stamp 0
    g_drawn_stamp[g_pass & 1] = g_pass;   // the sprites this pass draws (KeepDrawn)
    g_drawn_n[g_pass & 1] = 0;
    g_sprite_mark = nullptr;
    g_run = nullptr;
    g_in_run = false;
    if (Draw_OtSlot != 6 || !(Draw_PassFlags & 4) || Gfx_BufferIndex > 1) return false;
    std::memset(g_cell_of, 0, sizeof g_cell_of);
    std::memset(g_owner_of, 0, sizeof g_owner_of);
    for (unsigned v = 0; v < kViewCells; ++v) {
        const unsigned index = Get<std::uint16_t>(MapView_CellItems + v * 4u + 2) & 0xFFFu;
        if (index == 0 || index >= draw_pool::Count()) continue;
        g_cell_of[index] = static_cast<std::uint16_t>(v + 1);
        g_owner_of[index] = static_cast<std::uint16_t>(v + 1);
        // Its side triangles, the next cell's edge and the one below's.
        const unsigned char* const item = draw_pool::Items() + index * 0x90u;
        for (unsigned word : {0x7Eu, 0x8Eu}) {
            const unsigned side = Get<std::uint16_t>(item + word);
            if (side != 0 && side < 0x1000u && side < draw_pool::Count()) g_owner_of[side] = static_cast<std::uint16_t>(v + 1);
        }
    }
    // Every sprite is judged against the list as the original sorted it, and
    // the keys are changed only after the last one.
    unsigned layers[kSprites];
    const unsigned count = Sprite_DrawListCount < kSprites ? Sprite_DrawListCount : kSprites;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned layer = Get<std::uint16_t>(Sprite_DrawList[i] + 0x32) >> 8;
        layers[i] = layer < 0x37 ? LayerFor(i, layer) : layer;
    }
    // The table faerie's sides are drawn again right after it: anything else
    // drawn between its own layer's second list and it that the sides reach
    // would be covered by them where the original drew it over them - a
    // sprite drawn into those layers. Then the faerie stays where it was.
    for (unsigned i = 0; i < count && g_faerie != nullptr; ++i) {
        if (Sprite_DrawList[i] != g_faerie) continue;
        const unsigned from = Get<std::uint16_t>(Sprite_DrawList[i] + 0x32) >> 8;
        for (unsigned j = 0; j < count; ++j) {
            if (j == i || layers[j] <= from || layers[j] > layers[i]) continue;
            if (!Overlap(BoxRound(Sprite_DrawList[j], kFaerieHalfWidth, kBodyAbove, kBodyBelow), g_faerie_reach)) continue;
            if (g_log)
                bof3::Log("D300 fix    frame %u: the table faerie %08X kept in layer %u: sprite %08X, drawn in layer %u, "
                          "under its table's sides", static_cast<unsigned>(Frame_Counter), Address(g_faerie), from,
                          Address(Sprite_DrawList[j]), layers[j]);
            layers[i] = from;
            g_faerie = nullptr;
            g_faerie_sides_n = 0;
            break;
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const sprite = Sprite_DrawList[i];
        const std::uint16_t key = Get<std::uint16_t>(sprite + 0x32);
        if (layers[i] == static_cast<unsigned>(key >> 8)) continue;
        g_moved[g_moved_n++] = {sprite, key};
        const auto later = static_cast<std::uint16_t>(layers[i] << 8 | (key & 0xFFu));
        std::memcpy(sprite + 0x32, &later, 2);
    }
    return g_moved_n != 0;
}

void Restore() {
    for (unsigned i = 0; i < g_moved_n; ++i) std::memcpy(g_moved[i].sprite + 0x32, &g_moved[i].key, 2);
    g_moved_n = 0;
    g_faerie = nullptr;
    g_faerie_sides_n = 0;
}

void BeforeSprite() { g_sprite_mark = Gfx_PacketNext; }

void AfterSprite(const unsigned char* sprite) {
    // What it drew, for the next pass (DrawnBox): Sprite_Draw commits one
    // 0x20-byte cell sprite, or nothing (culled, no frame, the pool full).
    if (g_sprite_mark != nullptr && Gfx_PacketNext == g_sprite_mark + 0x20 && g_sprite_mark[7] == 0x84)
        KeepDrawn(sprite, g_sprite_mark);
    g_sprite_mark = nullptr;
    if (sprite != g_faerie || g_faerie == nullptr || g_faerie_sides_n == 0) return;
    // As DrawLayer_Close draws a second list: a draw mode (texture page
    // 0x95) into slot 6, then the primitives - here copies, each 0x48 bytes,
    // the draw-item record's half for this buffer. Not at all when the
    // packet pool has no room for every one (Gfx_CommitPrim's own limit,
    // tested once for the whole set).
    const std::uint32_t limit = (static_cast<std::uint32_t>(Gfx_BufferIndex) << 16) + ::bof3::Img(kPoolLimit);
    if (limit <= Address(Gfx_PacketNext) + 0xCu + g_faerie_sides_n * 0x48u) {
        if (g_log) bof3::Log("D300 fix    frame %u: no room in the packet pool for the table's sides", static_cast<unsigned>(Frame_Counter));
        g_faerie = nullptr;
        return;
    }
    Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, 0x95, 0);
    Gfx_CommitPrim(6, 0xC);
    for (unsigned k = 0; k < g_faerie_sides_n; ++k) {
        std::memcpy(Gfx_PacketNext, Pointer(g_faerie_sides[k]), 0x48);
        Gfx_CommitPrim(6, 0x48);
    }
    g_faerie = nullptr;   // once a pass
}

void AfterModel(const unsigned char* object, const unsigned long* records, unsigned count) {
    // The quads' screen points (Sprite_AddDrawRecords: four at +8, +0x18,
    // +0x28, +0x38 of each POLY_FT4), their union box.
    Box box = {1e9f, 1e9f, -1e9f, -1e9f};
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const prim = Pointer(static_cast<std::uint32_t>(records[i * 3]));
        for (unsigned k = 0; k < 4; ++k) {
            const float x = Get<float>(prim + 8 + 0x10 * k), y = Get<float>(prim + 0xC + 0x10 * k);
            if (x < box.x0) box.x0 = x;
            if (x > box.x1) box.x1 = x;
            if (y < box.y0) box.y0 = y;
            if (y > box.y1) box.y1 = y;
        }
    }
    if (count != 0) Keep(object, box);
}

bool g_in_run = false;

namespace {

// A POLY_FT4 or POLY_G4 (code 0x2C / 0x38, semi-transparency and shading bits
// aside: four float points at +8, +0x18, +0x28, +0x38, as the handlers'
// projection writes them) kept as a quad. Anything else is not kept (see Run).
void Keep(const unsigned char* prim) {
    const unsigned code = prim[7] & 0xFC;
    if (code != 0x2C && code != 0x38) return;
    float x[4], y[4];
    for (unsigned k = 0; k < 4; ++k) {
        x[k] = Get<float>(prim + kRound[k]);
        y[k] = Get<float>(prim + kRound[k] + 4);
        if (!(x[k] > -kFar && x[k] < kFar && y[k] > -kFar && y[k] < kFar)) return;   // NaN included
    }
    Run& run = *g_run;
    if (run.n < kQuads) {
        std::memcpy(run.x[run.n], x, sizeof x);
        std::memcpy(run.y[run.n], y, sizeof y);
        ++run.n;
        return;
    }
    run.overflow = true;
    for (unsigned k = 0; k < 4; ++k) {
        if (x[k] < run.box.x0) run.box.x0 = x[k];
        if (x[k] > run.box.x1) run.box.x1 = x[k];
        if (y[k] < run.box.y0) run.box.y0 = y[k];
        if (y[k] > run.box.y1) run.box.y1 = y[k];
    }
}

}  // namespace

void RunBegin(const unsigned char* run) {
    g_run = FindRun(Address(run), g_pass, true);
    g_in_run = g_run != nullptr;
    g_pending_n = 0;
}

// Noted at the commit and kept at RunEnd, when the run's handlers have written
// every point (g_pending). The packet pool is not reused within a pass, so the
// memory still holds the primitive then. A run that commits more than
// kPending keeps the oldest at once, as before.
void RunPrim(const unsigned char* prim, unsigned size) {
    if (g_run == nullptr || size < 0x44) return;   // a POLY_G4 is committed as 0x44 bytes, a POLY_FT4 as 0x48
    if (g_pending_n == kPending) {
        Keep(g_pending[0]);
        std::memmove(g_pending, g_pending + 1, (kPending - 1) * sizeof g_pending[0]);
        --g_pending_n;
    }
    g_pending[g_pending_n++] = prim;
}

void RunEnd() {
    if (g_run != nullptr)
        for (unsigned i = 0; i < g_pending_n; ++i) Keep(g_pending[i]);
    g_pending_n = 0;
    g_run = nullptr;
    g_in_run = false;
}

// The quicksave (SP-G, docs/quicksave.md, the graphics participant). Between two logic
// frames the last pass's runs are the table g_pass stamped, and the next
// pass's Defer reads them: they are what the next frame's layering is judged
// by, so a quicksave carries them (not the census's g_pass += 2, which would
// make that pass find no run at all and keep every sprite in its layer where a
// record run stands). Each run at its own index (the probe chain of FindRun
// stays as it was), restamped with this process's g_pass: the stamps are a
// count of this process's passes, and a stale entry of either table whose
// stamp matched a later pass would be taken for one of its runs - so both
// tables are emptied first.
void SnapshotSave(bof3::SnapshotWriter& w) {
    const Run* const table = g_runs[g_pass & 1];
    std::uint32_t n = 0;
    for (unsigned i = 0; i < kRuns; ++i) n += table[i].stamp == g_pass ? 1u : 0u;
    w.Put(static_cast<std::uint32_t>(g_runs_now));
    w.Put(n);
    for (unsigned i = 0; i < kRuns; ++i) {
        const Run& run = table[i];
        if (run.stamp != g_pass) continue;
        w.Put(static_cast<std::uint16_t>(i));
        w.Put(run.at);
        w.Put(static_cast<std::uint8_t>(run.n));
        w.Put(static_cast<std::uint8_t>(run.overflow ? 1 : 0));
        w.Put(run.box);
        for (unsigned q = 0; q < run.n; ++q) {
            w.Put(run.x[q]);
            w.Put(run.y[q]);
        }
    }
    // The sprites' boxes this pass drew (DrawnBox), read the same way.
    const unsigned g = g_pass & 1;
    const std::uint32_t drawn = g_drawn_stamp[g] == g_pass ? g_drawn_n[g] : 0u;
    w.Put(drawn);
    for (unsigned i = 0; i < drawn; ++i) {
        w.Put(g_drawn[g][i].at);
        w.Put(g_drawn[g][i].box);
    }
}

void SnapshotRestore(bof3::SnapshotReader& r) {
    std::memset(g_runs, 0, sizeof g_runs);
    g_runs_now = r.Get<std::uint32_t>();
    g_runs_before = 0;
    const std::uint32_t n = r.Get<std::uint32_t>();
    if (n > kRuns) bof3::Fatal("layering: a quicksave with %u record runs, more than the table's %u", n, kRuns);
    Run* const table = g_runs[g_pass & 1];
    for (std::uint32_t k = 0; k < n; ++k) {
        const unsigned i = r.Get<std::uint16_t>();
        if (i >= kRuns || table[i].stamp != 0) bof3::Fatal("layering: a quicksave's record run at index %u, out of the table or twice", i);
        Run& run = table[i];
        run.at = r.Get<std::uint32_t>();
        run.stamp = g_pass;
        run.n = r.Get<std::uint8_t>();
        run.overflow = r.Get<std::uint8_t>() != 0;
        run.box = r.Get<Box>();
        if (run.n > kQuads) bof3::Fatal("layering: a quicksave's record run with %u quads, more than %u", run.n, kQuads);
        for (unsigned q = 0; q < run.n; ++q) {
            r.Get(run.x[q], sizeof run.x[q]);
            r.Get(run.y[q], sizeof run.y[q]);
        }
    }
    g_run = nullptr;
    g_in_run = false;
    g_pending_n = 0;
    const std::uint32_t drawn = r.Get<std::uint32_t>();
    if (drawn > kDrawn) bof3::Fatal("layering: a quicksave with %u sprite boxes, more than the table's %u", drawn, kDrawn);
    g_drawn_stamp[0] = g_drawn_stamp[1] = 0;
    g_drawn_n[0] = g_drawn_n[1] = 0;
    const unsigned g = g_pass & 1;
    g_drawn_stamp[g] = g_pass;
    g_drawn_n[g] = drawn;
    for (std::uint32_t i = 0; i < drawn; ++i) {
        g_drawn[g][i].at = r.Get<std::uint32_t>();
        g_drawn[g][i].box = r.Get<Box>();
    }
    g_sprite_mark = nullptr;
}

}  // namespace layering
