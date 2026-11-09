#include "game/map_layers.h"

#include <windows.h>

#include "game/draw_pool.h"
#include "game/faerie_ring.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/map_layers_callees.h"
#include "game/map_rim.h"
#include "game/wide_rand.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"
#include "hook/snapshot.h"

// The view's frame, its rebuild and the area header's pass - see
// docs/map-layers.md for each function, its PSX twin and the fuzz.

namespace map_layers {

const Callees kOriginals = {
    AreaMap_FrameAreaBD, MapView_ShiftColumnNext, MapView_ShiftColumnPrev, MapView_ShiftRowsNext,
    MapView_ShiftRowsPrev, MapView_PlaceRuns, Gte_RotMatrix, Gte_ApplyMatrix, Gte_SetRotMatrix, Gte_SetTransMatrix,
    MapView_Build,
    DrawLayers_Reset, Gte_LoadVertex, Gte_Rtps, Gte_StoreScreenXY, MapView_CellTextures, DrawItemPool_Alloc,
    Gte_LoadVertices3, Gte_Rtpt, Gpu_LinkPrim, Gte_StoreScreenXY3, Gte_PrimDepths4_10, Gte_StoreDepthF3,
    DrawItemPool_ReleaseCell, DrawTable_Sort,
    Prim_SetTexture, DrawItemPool_Release,
    Gpu_SetPolyFT4, Gpu_SetShadeTex, Gte_RotTransPers4, Gfx_CommitPrim,
    Area_TestCondition,
};
Callees map_layers::g = kOriginals;

}  // namespace map_layers

namespace {

// map_layers::g: named in full below (source-layout B0, a clash on g)
}  // namespace
namespace map_layers { bool g_side_zero_release; }   // BOF3X_SIDE_ZERO=1 (MapLayers_Inject)

// DIV-0085: 0 until ArmSideDup (and with BOF3X_SIDE_DUP=0), so every self-test
// compares Capcom's walk. g_file_sides: per map cell (z * width + x), 1 when the
// heights as the area file loaded them give it a south side only, 2 an east
// side only - and its tile carries no third word - else 0 (SnapshotSides).
namespace map_layers { bool g_side_dup; }  namespace {
}  namespace map_layers { unsigned char g_file_sides[256 * 256]; }
namespace {
unsigned WordMapLayers(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
int S8MapLayers(unsigned v) { return static_cast<signed char>(v); }
unsigned Width() { return AreaMap_Header[0]; }

unsigned char* Corners() { return reinterpret_cast<unsigned char*>(&AreaMap_Corners); }

}  // namespace

// The inset's arithmetic, the globals it reads passed in (map_layers.h).
map_layers::InsetPair map_layers::ComputeInset(unsigned long angle, short distance, unsigned char input_flags,
                                               unsigned char byte_fe, unsigned wide_columns) {
    int off_axis = static_cast<short>(angle) - 0x200;
    if (off_axis < 0) off_axis = -off_axis;
    int inset = (0x160 - off_axis) / 50;
    inset = inset >= 1 ? inset - 1 : 0;
    const int by_distance = static_cast<int>(distance) / 650;   // `far` is a windows.h macro
    inset = inset >= by_distance ? inset - by_distance : 0;
    if (input_flags & 1) inset = inset >= 2 ? inset - 2 : 0;
    if (byte_fe == 0x23) inset = 0;
    InsetPair pair;
    pair.original = static_cast<std::uint32_t>(inset);
    // DIV-0041: the wide picture reaches past the columns the inset keeps
    // (widescreen.h); 0 unless the view is wide, so the fuzz and every
    // narrow run see the original's inset.
    if (const int wide = static_cast<int>(wide_columns); wide != 0) inset = inset >= wide ? inset - wide : 0;
    pair.lowered = static_cast<std::uint32_t>(inset);
    return pair;
}

void MapLayers_Inject() {
    if (bof3::WantsShadow("map_layers")) map_layers::SelfTest();
    {
        char text[8];
        const DWORD n = GetEnvironmentVariableA("BOF3X_SIDE_ZERO", text, sizeof text);
        // An experiment's switch (2026-10-07, the owner's "waterfall"), off
        // unless BOF3X_SIDE_ZERO=1; DIV-0085 "Reversible?" names it.
        if (n >= sizeof text || (n != 0 && !((text[0] == '0' || text[0] == '1') && text[1] == 0)))
            bof3::Fatal("BOF3X_SIDE_ZERO must be 0 or 1");
        map_layers::g_side_zero_release = n == 1 && text[0] == '1';
        if (map_layers::g_side_zero_release)
            bof3::Log("map_layers: a side face with texture word 0 is not drawn (BOF3X_SIDE_ZERO=1, an experiment)");
    }
}

namespace map_layers {

// MapView_Build's two side tests, cell by cell, on the corners as loaded - the
// row's last cell reading the next row's first and the last row reading past
// the corner plane into the block, as the build does. A cell is marked only
// when the file gives it one side and its tile's run has no third dword of
// its own: the run ends at the next tile index any cell of the map uses (the
// last one at the texture run's end, the header's word +4); a tile with a
// third word carries one for the other side, and the original's read of it
// stands.
void SnapshotSides() {
    if (!g_side_dup) return;  // not armed: no self-test's map is read
    const unsigned width = Width(), depth = AreaMap_Header[1];
    const unsigned offset = WordMapLayers(AreaMap_Header + 2);
    const unsigned half = (depth * width + 1) / 2;
    const unsigned char* const tiles = AreaMap_Header + offset * 4u;
    static std::uint8_t used[0x10000];
    static std::uint32_t next_used[0x10000];
    std::memset(used, 0, sizeof used);
    for (unsigned i = 0; i < width * depth; ++i) used[WordMapLayers(tiles + i * 2u)] = 1;
    const unsigned end = WordMapLayers(AreaMap_Header + 4);
    std::uint32_t after = end > offset + half ? end - offset - half : 0x10000u;
    for (unsigned t = 0x10000; t-- > 0;) {
        next_used[t] = after;
        if (used[t]) after = t;
    }
    const unsigned char* const corners = Corners();
    for (unsigned z = 0; z < depth; ++z) {
        for (unsigned x = 0; x < width; ++x) {
            const unsigned char* const c = corners + (z * width + x) * 4u;
            const unsigned char* const below = c + width * 4u;
            const bool east = S8MapLayers(c[6]) < S8MapLayers(c[3]) || S8MapLayers(c[4]) < S8MapLayers(c[1]);
            const bool south = S8MapLayers(below[1]) < S8MapLayers(c[3]) || S8MapLayers(below[0]) < S8MapLayers(c[2]);
            const unsigned tile = WordMapLayers(tiles + (z * width + x) * 2u);
            const bool short_run = tile != 0 && next_used[tile] > tile && next_used[tile] - tile <= 2;
            g_file_sides[z * width + x] =
                static_cast<unsigned char>(south != east && short_run ? (south ? 1 : 2) : 0);
        }
    }
}

void ArmSideDup() {
    char text[8];
    const DWORD n = GetEnvironmentVariableA("BOF3X_SIDE_DUP", text, sizeof text);
    if (n > 1 || (n == 1 && text[0] != '0' && text[0] != '1')) bof3::Fatal("BOF3X_SIDE_DUP must be 0 or 1");
    if (n == 1 && text[0] == '0') {
        bof3::Log("DIV-0085 off (BOF3X_SIDE_DUP=0): a cell given a second side face at run time reads the next "
                  "tile's word for it, as the original does");
        return;
    }
    g_side_dup = true;
    bof3::Log("DIV-0085    a cell given a second side face at run time draws it with its one side word "
              "(BOF3X_SIDE_DUP=0 for the original's read)");
}

// The quicksave (SP-G, docs/quicksave.md, the graphics participant): g_file_sides is the area
// file's heights as loaded, and the arena they came from has been patched
// since (the sea bridge's sky), so it is carried, not rebuilt: up to its last
// non-zero byte, the rest zero.
void QuickSaveSides(bof3::SnapshotWriter& w) {
    std::uint32_t n = sizeof g_file_sides;
    while (n > 0 && g_file_sides[n - 1] == 0) --n;
    w.Put(n);
    if (n) w.Put(g_file_sides, n);
}

void QuickRestoreSides(bof3::SnapshotReader& r) {
    const std::uint32_t n = r.Get<std::uint32_t>();
    if (n > sizeof g_file_sides) bof3::Fatal("map_layers: a quicksave's side faces run to %u bytes, more than %u", n,
                                             static_cast<unsigned>(sizeof g_file_sides));
    std::memset(g_file_sides, 0, sizeof g_file_sides);
    if (n) r.Get(g_file_sides, n);
}

}  // namespace map_layers
