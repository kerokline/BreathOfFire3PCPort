#include "game/layering.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "hook/draw_order.h"
#include "hook/log.h"

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

// A draw item's quad or a POLY_FT4: four float points at +8, +0x18, +0x28 and
// +0x38, in strip order. Anything else has no shape here, and counts as in
// the way (`known` false, the result true).
bool Reaches(std::uint32_t prim, const Box& target, bool* known = nullptr) {
    const unsigned char* const p = Pointer(prim);
    if (known) *known = InPool(prim) || p[7] == 0x2C;
    if (!InPool(prim) && p[7] != 0x2C) return true;
    float x[4], y[4];
    static const unsigned kRound[4] = {0x08u, 0x18u, 0x38u, 0x28u};   // the perimeter: 0, 1, 3, 2
    Box box = {1e9f, 1e9f, -1e9f, -1e9f};
    for (unsigned k = 0; k < 4; ++k) {
        x[k] = Get<float>(p + kRound[k]);
        y[k] = Get<float>(p + kRound[k] + 4);
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

// What stops the sprite at the layer before: a primitive that reaches the
// feet and is not floor allowed there, or one that is not floor and reaches
// the sprite anywhere (`whole_box`, the feet and the body) - a raised cell
// of a later layer stands in front of the sprite even where it covers only
// the body (the owner's Nina behind a crate, 2026-10-06: the crate's top
// reached her head, not her feet, and she was drawn through it).
bool Stops(std::uint32_t prim, const Box& feet_box, const Box& whole_box, int feet, Need need) {
    if (Reaches(prim, feet_box) && !(need == kFloorOnly && IsFloor(prim, feet))) return true;
    return Reaches(prim, whole_box) && !IsFloor(prim, feet);
}

// One of a layer's three lists (0, 1, 2: the first, the second, the frame
// nodes): false when a primitive of it stops the sprite (Stops).
bool ListClear(unsigned layer, unsigned which, const Box& feet_box, const Box& whole_box, int feet, Need need) {
    const unsigned long* const list = DrawLayers + which * 4u + (Gfx_BufferIndex + layer * 6u) * 2u;   // first, last
    std::uint32_t at = list[0];
    for (unsigned steps = 0; at != 0; ++steps) {
        if (steps == 256) return false;
        if (Stops(at, feet_box, whole_box, feet, need)) {
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

Box BoxRound(const unsigned char* sprite, float half_width, float above, float below) {
    const float x = Get<float>(sprite + 0x74), y = Get<float>(sprite + 0x78);
    return {x - half_width, y - above, x + half_width, y + below};
}

// The layer `sprite` (number `index` of the sorted draw list, in `layer`) is
// drawn in.
unsigned LayerFor(unsigned index, unsigned layer) {
    const unsigned char* const sprite = Sprite_DrawList[index];
    const float sx = Get<float>(sprite + 0x74), sy = Get<float>(sprite + 0x78);
    if (!(sx > -64.0f && sx < 1024.0f && sy > -64.0f && sy < 512.0f)) return layer;   // NaN included
    const Box feet_box = BoxRound(sprite, kFeetHalfWidth, kFeetAbove, kFeetBelow);
    const Box whole_box = BoxRound(sprite, kBodyHalfWidth, kBodyAbove, kFeetBelow);   // the body and the feet
    // The feet in corner units, as the sprite's own key has them.
    const int feet = static_cast<signed char>(Get<std::uint16_t>(sprite + 0x3E) >> 5);
    const int cell_x = Get<std::uint16_t>(sprite + 0x36), cell_z = Get<std::uint16_t>(sprite + 0x3A);

    unsigned drawn_in = layer;
    for (unsigned next = layer + 1; next < 0x37 && next <= layer + static_cast<unsigned>(g_ahead); ++next) {
        // What the sprite would newly be drawn over: the second list and the
        // later sprites and table items of the layer before, then this
        // layer's first list, cell records and frame nodes.
        if (!ListClear(next - 1, 1, feet_box, whole_box, feet, kNothing)) { g_why = "the layer before's second list"; break; }
        bool clear = true;
        for (unsigned other = index + 1; other < Sprite_DrawListCount && clear; ++other) {
            const unsigned char* const o = Sprite_DrawList[other];
            if (static_cast<unsigned>(Get<std::uint16_t>(o + 0x32) >> 8) > next - 1) break;
            if (Overlap(BoxRound(o, kBodyHalfWidth, kBodyAbove, kBodyBelow), whole_box)) clear = false;
        }
        for (unsigned item = 0; item < DrawTable_Count && clear; ++item) {
            const unsigned long entry = DrawTable[item];
            if ((entry >> 24) != next - 1) continue;
            const std::uint32_t prim =
                Address(draw_pool::Items() + (Gfx_BufferIndex + (entry & 0xFFFu) * 2u) * 0x48u);
            if (Stops(prim, feet_box, whole_box, feet, kFloorOnly)) clear = false;
        }
        if (!clear) { g_why = "a sprite or a table item of the layer before"; break; }
        if (!ListClear(next, 0, feet_box, whole_box, feet, kFloorOnly)) { g_why = "the first list: not floor"; break; }
        if (!RecordsClear(next, cell_x, cell_z)) { g_why = "a cell record nearby"; break; }
        if (!ListClear(next, 2, feet_box, whole_box, feet, kNothing)) { g_why = "a frame node"; break; }
        drawn_in = next;
    }
    if (g_log && draw_order::Tagging())
        bof3::Log("DIV-0071    frame %u: sprite %08X at (%g, %g), cell %d,%d, feet %d: layer %u drawn in layer %u; stopped by %s",
                  static_cast<unsigned>(Frame_Counter), Address(sprite), sx, sy, cell_x, cell_z, feet, layer, drawn_in,
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
    if (g_mode == kFloor)
        bof3::Log("DIV-0071    on: a sprite is drawn up to %d layers later, over floor only (a lowest corner up to %d "
                  "above the feet is floor)", g_ahead, g_rise);
    else
        bof3::Log("DIV-0071    comparison mode 2: every sprite's key one layer later");
}

bool Defer() {
    g_moved_n = 0;
    if (Draw_OtSlot != 6 || !(Draw_PassFlags & 4) || Gfx_BufferIndex > 1) return false;
    std::memset(g_cell_of, 0, sizeof g_cell_of);
    for (unsigned v = 0; v < kViewCells; ++v) {
        const unsigned index = Get<std::uint16_t>(MapView_CellItems + v * 4u + 2) & 0xFFFu;
        if (index != 0) g_cell_of[index] = static_cast<std::uint16_t>(v + 1);
    }
    // Every sprite is judged against the list as the original sorted it, and
    // the keys are changed only after the last one.
    unsigned layers[kSprites];
    const unsigned count = Sprite_DrawListCount < kSprites ? Sprite_DrawListCount : kSprites;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned layer = Get<std::uint16_t>(Sprite_DrawList[i] + 0x32) >> 8;
        layers[i] = layer < 0x37 ? LayerFor(i, layer) : layer;
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
}

}  // namespace layering
