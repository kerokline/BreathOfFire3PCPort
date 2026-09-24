#include "game/map_layers.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/map_layers_callees.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

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
Callees g = kOriginals;

}  // namespace map_layers

namespace {

using map_layers::g;

std::uint32_t Dword(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
unsigned Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void SetWord(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void SetDword(unsigned char* p, std::uint32_t v) { std::memcpy(p, &v, sizeof v); }
std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
int S8(unsigned v) { return static_cast<signed char>(v); }
unsigned Width() { return AreaMap_Header[0]; }

// A corner byte as a vertex's third coordinate: -(s8) * 16, as a word.
short Height(unsigned char corner) { return static_cast<short>(-S8(corner) * 16); }

unsigned char* Corners() { return reinterpret_cast<unsigned char*>(&AreaMap_Corners); }
unsigned char* Item(unsigned index) { return DrawItems + index * 0x90u; }
// A draw item's quad for buffer `buffer`: items hold one per display buffer.
unsigned char* Quad(unsigned index, unsigned buffer) { return DrawItems + (buffer + index * 2u) * 0x48u; }

// DrawLayers dword `slot * 2 + 1`: the last link of list (slot - buffer) / 2
// mod 3 of layer slot / 6 - the layers are six (first, last) pairs, three
// lists by two buffers.
unsigned long& Last(std::uint32_t slot) { return DrawLayers[slot * 2u + 1u]; }

unsigned long AsLong(const void* p) { return static_cast<unsigned long>(Address(p)); }

}  // namespace

// original 0x56E6C0: the view's frame (PSX FUN_801532A0). Called by the field
// frame entries 0x517200 / 0x517240.
//
// As the original has it: the steps and the scroll words are 16-bit where the
// original stores words and 32-bit where it keeps registers; the scroll
// words, the redraw byte, the angles, the focus, the elevation and the
// translation's three shifts are read after the calls that precede them in
// the original, so a callee that moved them is seen; Camera_AnglesDrawn takes
// both dwords of the angles, the word after them too. A step is MoveScript_
// F3Divisor by the sign of the difference: a difference smaller than the step
// overshoots, and the hold stays on until a frame lands on it exactly.
extern "C" void __cdecl AreaMap_Frame(void) {
    if (Game_AreaNumber == 0xBD) {
        g.frame_bd();
        return;
    }
    std::uint32_t focus_x = static_cast<std::uint32_t>(MapView_FocusX);
    const std::uint32_t kind2_z = static_cast<std::uint32_t>(Field_Kind2Z) & 0xFFFF8000u;
    const std::uint32_t kind2_x = static_cast<std::uint32_t>(Field_Kind2X) & 0xFFFF8000u;
    const std::uint32_t focus_z = static_cast<std::uint32_t>(MapView_FocusZ);
    const short step = MoveScript_F3Divisor;
    const std::uint32_t dx = ((0x7FFFu - focus_x) << 8) - kind2_x;
    const std::uint32_t dz = ((0x8000u - focus_z) << 8) - kind2_z;
    if (dx != 0) {
        Field_Kind2Hold = 1;
        const int sign = static_cast<std::int32_t>(dx) > 0 ? 1 : -1;
        focus_x += static_cast<std::uint32_t>(step * sign);
        const auto moved = static_cast<unsigned short>(step * sign);
        MapView_ScrollX = static_cast<unsigned short>(MapView_ScrollX - moved);
        MapView_ElevationOffset = static_cast<unsigned short>(MapView_ElevationOffset - moved);
        MapView_FocusX = static_cast<long>(focus_x);
    }
    if (dz != 0 || Field_Kind2Hold != 0) {
        if (dz != 0) {
            Field_Kind2Hold = 1;
            const int sign = static_cast<std::int32_t>(dz) > 0 ? 1 : -1;
            const std::uint32_t z = static_cast<std::uint32_t>(MapView_FocusZ) + static_cast<std::uint32_t>(step * sign);
            const auto moved = static_cast<unsigned short>(step * sign);
            MapView_ScrollX = static_cast<unsigned short>(MapView_ScrollX + moved);
            MapView_ElevationOffset = static_cast<unsigned short>(MapView_ElevationOffset - moved);
            MapView_FocusZ = static_cast<long>(z);
        }
        const unsigned short rise = MoveScript_FAWord;
        MapView_Elevation = static_cast<long>(static_cast<std::uint32_t>(MapView_Elevation) +
                                              static_cast<std::uint32_t>(static_cast<short>(rise)));
        if (AreaMap_Word1E == 0)
            MapView_ElevationOffset = static_cast<unsigned short>(MapView_ElevationOffset + rise * 0xFFFEu);
        const std::uint32_t still_z = ((0x8000u - static_cast<std::uint32_t>(MapView_FocusZ)) << 8) - kind2_z;
        const std::uint32_t still_x = ((0x7FFFu - focus_x) << 8) - kind2_x;
        MapView_Redraw = 2;
        if ((still_z | still_x) == 0) Field_Kind2Hold = 0;
    }

    // The fine scroll: a whole column or two rows at 0x200 either way.
    bool shifted = false;
    if (static_cast<short>(MapView_ScrollX) >= 0x200) {
        g.column_next();
        MapView_ScrollX = static_cast<unsigned short>(MapView_ScrollX - 0x200);
        MapView_Redraw = 2;
        shifted = true;
    }
    if (static_cast<short>(MapView_ScrollX) <= -0x200) {
        g.column_prev();
        MapView_ScrollX = static_cast<unsigned short>(MapView_ScrollX + 0x200);
        MapView_Redraw = 2;
        shifted = true;
    }
    if (static_cast<short>(MapView_ElevationOffset) >= 0x200) {
        g.rows_next();
        MapView_ElevationOffset = static_cast<unsigned short>(MapView_ElevationOffset - 0x200);
        MapView_Redraw = 2;
        shifted = true;
    }
    if (static_cast<short>(MapView_ElevationOffset) <= -0x200) {
        g.rows_prev();
        MapView_ElevationOffset = static_cast<unsigned short>(MapView_ElevationOffset + 0x200);
        MapView_Redraw = 2;
        g.place_runs();
    } else if (shifted) {
        g.place_runs();
    }

    // The camera: redraw everything when an angle moved.
    auto* const angles = reinterpret_cast<unsigned char*>(Camera_Angles);
    auto* const drawn = reinterpret_cast<unsigned char*>(Camera_AnglesDrawn);
    const std::uint32_t angles_xy = Dword(angles), angles_z = Dword(angles + 4);
    if (Word(angles) != Word(drawn) || Word(angles + 2) != Word(drawn + 2) || Word(angles + 4) != Word(drawn + 4)) {
        MapView_Redraw = 3;
        SetDword(drawn, angles_xy);
        SetDword(drawn + 4, angles_z);
    }
    g.rot_matrix(Camera_Angles, Camera_Matrix);
    short focus[3];
    focus[0] = static_cast<short>(static_cast<std::uint32_t>(MapView_FocusX) >> 1);
    focus[1] = static_cast<short>(static_cast<std::uint32_t>(MapView_FocusZ) >> 1);
    focus[2] = static_cast<short>(static_cast<std::uint32_t>(MapView_Elevation) >> 1);
    long moved[3];
    g.apply_matrix(Camera_Matrix, focus, moved);
    auto* const translation = reinterpret_cast<unsigned char*>(Camera_Matrix) + 0x14;
    SetDword(translation, static_cast<std::uint32_t>(moved[0]) + static_cast<std::uint32_t>(static_cast<int>(Camera_ShiftX)));
    SetDword(translation + 4,
             static_cast<std::uint32_t>(moved[1]) + static_cast<std::uint32_t>(static_cast<int>(Camera_ShiftY)));
    SetDword(translation + 8, static_cast<std::uint32_t>(moved[2]) +
                                  static_cast<std::uint32_t>(static_cast<int>(Camera_Distance)) + 0x1194u);
    g.set_rot(reinterpret_cast<const unsigned long*>(Camera_Matrix));
    g.set_trans(reinterpret_cast<const unsigned long*>(Camera_Matrix));
    if (MapView_Redraw != 0) {
        g.build();
        --MapView_Redraw;
    }
}

namespace {

// MapView_Build's inset: columns taken off each side of the view by the
// camera's angle and distance.
std::uint32_t Inset() {
    int off_axis = static_cast<short>(Cond_AngleFB) - 0x200;
    if (off_axis < 0) off_axis = -off_axis;
    int inset = (0x160 - off_axis) / 50;
    inset = inset >= 1 ? inset - 1 : 0;
    const int far = static_cast<int>(Camera_Distance) / 650;
    inset = inset >= far ? inset - far : 0;
    Scratch_Swap = static_cast<unsigned long>(inset);
    if (Field_InputFlags & 1) {
        inset = inset >= 2 ? inset - 2 : 0;
        Scratch_Swap = static_cast<unsigned long>(inset);
    }
    if (Cond_ByteFE == 0x23) {
        inset = 0;
        Scratch_Swap = 0;
    }
    MapView_Inset = static_cast<unsigned char>(Scratch_Swap);
    return static_cast<std::uint32_t>(inset);
}

int Columns(std::uint32_t inset) { return static_cast<std::int32_t>((0xEu - inset) << 1); }

// A side triangle's list: list 0 of layer `layer` when the cell word's `bit` is
// set, else list 1.
std::uint32_t SideSlot(unsigned layer, const unsigned char* cell, unsigned shift) {
    return layer * 6u + ((~Word(cell + 2) >> shift) & 2u) + Gfx_BufferIndex;
}

// One cell of the view, drawn into layer `layer` (0..0x36).
void BuildCell(unsigned char* cell, unsigned layer, int threshold) {
    if (cell[0] == 0 || cell[1] == 0) {
        g.release_cell(cell);
        MapView_BuildFlags |= 1;
        return;
    }
    short* const s = Prim_VertexScratch;
    s[0] = static_cast<short>((static_cast<std::uint32_t>(cell[0]) << 7) - 0x4040u);
    s[1] = static_cast<short>((static_cast<std::uint32_t>(cell[1]) << 7) - 0x4040u);
    unsigned char* corner = Corners() + (cell[1] * Width() + cell[0]) * 4u;
    MapView_CornerPtr = corner;
    s[2] = Height(corner[0]);
    g.load_vertex(reinterpret_cast<const unsigned long*>(s));
    g.rtps();
    g.store_xy(reinterpret_cast<unsigned long*>(MapView_ScreenXY));
    const float sx = MapView_ScreenXY[0], sy = MapView_ScreenXY[1];
    // x87 fcomp as the original has it: an unordered x or y is culled too.
    // The x bounds are -50 / 370 unless the view is wide (DIV-0041,
    // src/game/widescreen.cpp, which sets them after every start-up fuzz).
    if (!(sx >= Widescreen_TerrainLo) || !(sx <= Widescreen_TerrainHi) || !(sy >= -200.0f) || !(sy <= 290.0f)) {
        g.release_cell(cell);
        return;
    }

    std::uint32_t index = Word(cell + 2) & 0xFFFu;
    unsigned char* item;
    if (index != 0) {
        item = Item(index);
        const unsigned pending = Word(item + 0x36);
        if (pending != 0) {
            g.cell_textures(cell[0], cell[1], item, pending - 1);
            SetWord(item + 0x36, 0);
        }
    } else {
        const unsigned short got = g.alloc();
        SetWord(cell + 2, got);
        index = got;
        if (index == 0) return;
        corner = MapView_CornerPtr;
        item = Item(index);
        // A side item for each neighbour whose near corners stand lower -
        // the next cell's (+0x7E), the one below's (+0x8E).
        if (S8(corner[6]) < S8(corner[3]) || S8(corner[4]) < S8(corner[1])) {
            SetWord(item + 0x7E, g.alloc());
            corner = MapView_CornerPtr;
        }
        const unsigned char* const below = corner + Width() * 4u;
        if (S8(below[1]) < S8(corner[3]) || S8(below[0]) < S8(corner[2])) SetWord(item + 0x8E, g.alloc());
        const unsigned flags = g.cell_textures(cell[0], cell[1], item, Gfx_BufferIndex);
        SetWord(cell + 2, Word(cell + 2) | flags);
        SetWord(item + 0x36, Gfx_BufferIndex == 0 ? 2 : 1);
    }

    // The cell's quad: its first screen point from above, the other three
    // corners through the GTE.
    std::memcpy(item + Gfx_BufferIndex * 0x48u + 8, &MapView_ScreenXY[0], 4);
    std::memcpy(item + Gfx_BufferIndex * 0x48u + 0xC, &MapView_ScreenXY[1], 4);
    corner = MapView_CornerPtr;
    const short x = s[0], y = s[1];
    s[4] = static_cast<short>(x + 0x80);
    s[5] = y;
    s[6] = Height(corner[1]);
    s[8] = s[0];
    s[9] = static_cast<short>(y + 0x80);
    s[12] = static_cast<short>(x + 0x80);
    s[10] = Height(corner[2]);
    s[13] = static_cast<short>(y + 0x80);
    s[14] = Height(corner[3]);
    g.load_vertices3(reinterpret_cast<const unsigned long*>(s + 4));
    unsigned char* const quad = Quad(index, Gfx_BufferIndex);
    g.rtpt();

    const unsigned word = Word(cell + 2);
    if (word & 0x8000u) {
        if (word & 0x4000u) {   // list 0 of the layer before (layer 0 for the first)
            const unsigned before = layer != 0 ? layer - 1 : 0;
            g.link(reinterpret_cast<unsigned long*>(Last(before * 6u + Gfx_BufferIndex)),
                   AsLong(item + Gfx_BufferIndex * 0x48u));
            Last(before * 6u + Gfx_BufferIndex) =AsLong(item + Gfx_BufferIndex * 0x48u);
        } else {                // list 1 of this layer
            g.link(reinterpret_cast<unsigned long*>(Last(layer * 6u + 2u + Gfx_BufferIndex)),
                   AsLong(item + Gfx_BufferIndex * 0x48u));
            Last(layer * 6u + 2u + Gfx_BufferIndex) = AsLong(item + Gfx_BufferIndex * 0x48u);
        }
    } else if ((word & 0x4000u) &&
               (Draw_OtSlot == 6 || (Draw_OtSlot == 4 && static_cast<int>(layer) > threshold))) {
        // The draw table, keyed by layer and the cell's lowest corner (each
        // byte's sign flipped, so the lowest s8 is the smallest byte). Each
        // new minimum passes through DamageScratch's byte, as the original
        // keeps it there.
        auto* const scratch = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(bof3::addr::DamageScratch));
        const unsigned char* const c = MapView_CornerPtr;
        *scratch = static_cast<unsigned char>(c[0] ^ 0x80);
        for (unsigned k = 1; k < 4; ++k) {
            const auto biased = static_cast<unsigned char>(c[k] ^ 0x80);
            if (*scratch > biased) *scratch = biased;
        }
        const std::uint32_t key = ((layer << 8) | *scratch) << 16;
        DrawTable[DrawTable_Count] = key | (Word(cell + 2) & 0xFFFu);
        DrawTable_Count = static_cast<unsigned char>(DrawTable_Count + 1);
    } else {                    // list 0 of this layer
        g.link(reinterpret_cast<unsigned long*>(Last(layer * 6u + Gfx_BufferIndex)), AsLong(item + Gfx_BufferIndex * 0x48u));
        Last(layer * 6u + Gfx_BufferIndex) = AsLong(item + Gfx_BufferIndex * 0x48u);
    }
    {
        unsigned char* const mine = item + Gfx_BufferIndex * 0x48u;
        g.store_xy3(reinterpret_cast<unsigned long*>(mine + 0x18), reinterpret_cast<unsigned long*>(mine + 0x28),
                    reinterpret_cast<unsigned long*>(mine + 0x38));
    }
    g.depths4(item + Gfx_BufferIndex * 0x48u);

    // The side triangles: the next cell's edge (+0x7E) and the one below's
    // (+0x8E), each from two corners of the neighbour and the quad's own
    // vertex 2, left in the scratch.
    float unused;
    const unsigned next_item = Word(item + 0x7E);
    if (next_item != 0) {
        unsigned char* const side = Quad(next_item, Gfx_BufferIndex);
        std::memcpy(side + 8, quad + 0x38, 12);
        std::memcpy(side + 0x18, quad + 0x18, 12);
        const auto sx0 = static_cast<short>((static_cast<std::uint32_t>(cell[0]) << 7) - 0x3FC0u);
        const auto sy0 = static_cast<short>((static_cast<std::uint32_t>(cell[1]) << 7) - 0x4040u);
        s[0] = sx0;
        s[1] = sy0;
        unsigned char* const next = Corners() + 4u + (cell[1] * Width() + cell[0]) * 4u;
        MapView_CornerPtr = next;
        s[2] = Height(next[0]);
        s[4] = sx0;
        s[5] = static_cast<short>(sy0 + 0x80);
        s[6] = Height(next[2]);
        g.load_vertices3(reinterpret_cast<const unsigned long*>(s));
        g.link(reinterpret_cast<unsigned long*>(Last(SideSlot(layer, cell, 12))), AsLong(side));
        g.rtpt();
        Last(SideSlot(layer, cell, 12)) = AsLong(side);
        g.store_xy3(reinterpret_cast<unsigned long*>(side + 0x38), reinterpret_cast<unsigned long*>(side + 0x28),
                    reinterpret_cast<unsigned long*>(MapView_ScreenXY));
        g.depth_f3(reinterpret_cast<float*>(side + 0x40), reinterpret_cast<float*>(side + 0x30), &unused);
    }
    const unsigned below_item = Word(item + 0x8E);
    if (below_item != 0) {
        unsigned char* const side = Quad(below_item, Gfx_BufferIndex);
        std::memcpy(side + 8, quad + 0x28, 12);
        std::memcpy(side + 0x18, quad + 0x38, 12);
        const auto sx0 = static_cast<short>((static_cast<std::uint32_t>(cell[0]) << 7) - 0x4040u);
        const auto sy0 = static_cast<short>((static_cast<std::uint32_t>(cell[1]) << 7) - 0x3FC0u);
        s[0] = sx0;
        s[1] = sy0;
        unsigned char* const under = Corners() + ((cell[1] + 1u) * Width() + cell[0]) * 4u;
        MapView_CornerPtr = under;
        s[2] = Height(under[0]);
        s[5] = sy0;
        s[4] = static_cast<short>(s[0] + 0x80);
        s[6] = Height(under[1]);
        g.load_vertices3(reinterpret_cast<const unsigned long*>(s));
        g.link(reinterpret_cast<unsigned long*>(Last(SideSlot(layer, cell, 11))), AsLong(side));
        g.rtpt();
        Last(SideSlot(layer, cell, 11)) = AsLong(side);
        g.store_xy3(reinterpret_cast<unsigned long*>(side + 0x28), reinterpret_cast<unsigned long*>(side + 0x38),
                    reinterpret_cast<unsigned long*>(MapView_ScreenXY));
        g.depth_f3(reinterpret_cast<float*>(side + 0x30), reinterpret_cast<float*>(side + 0x40), &unused);
    }
}

}  // namespace

// original 0x56EC00: the view's cell draw items rebuilt (PSX FUN_80153B8C).
// Layer j (0..0x36) is view row (MapView_Row + 1 + j) mod 0x38; its columns
// run from MapView_Column + inset, each advanced before it is read, wrapping
// 0x1B to 0.
//
// As the original has it:
//   - the inset is re-read from Scratch_Swap after every cell, for the column
//     count and for the next row's first column - Scratch_Swap being the
//     game's all-purpose temporary, anything a cell's callees leave there
//     changes the view;
//   - the draw-table key's minimum passes through DamageScratch's byte, and
//     a cell's corner pointer through MapView_CornerPtr, both globals;
//   - the side items are allocated only for a new item, and an item's +0x7E
//     and +0x8E are otherwise used as they stand;
//   - an existing item's textures are refreshed for buffer (+0x36) - 1 and
//     +0x36 cleared; a new item's +0x36 is 2 on buffer 0, 1 on any other,
//     so the next frame refreshes buffer 1 either way;
//   - a failed allocation leaves the cell word 0 and draws nothing, the cell
//     released by no one.
extern "C" void __cdecl MapView_Build(void) {
    g.layers_reset();
    const int origin_y = MapView_Origin[1], origin_x = MapView_Origin[0];
    const int kind2_z = static_cast<short>(static_cast<std::uint32_t>(Field_Kind2Z) >> 16);
    const int kind2_x = static_cast<short>(static_cast<std::uint32_t>(Field_Kind2X) >> 16);
    MapView_BuildFlags &= 0xFE;
    DrawTable_Count = 0;
    const int threshold = kind2_z - origin_y - origin_x + kind2_x + 8;
    std::uint32_t inset = Inset();
    int row = MapView_Row;
    for (unsigned layer = 0; layer < 0x37; ++layer) {
        row = row == 0x37 ? 0 : row + 1;
        auto column = static_cast<int>(static_cast<std::uint32_t>(MapView_Column) + inset);
        if (column >= 0x1C) column -= 0x1C;
        for (int k = 0; k < Columns(inset);) {
            column = column == 0x1B ? 0 : column + 1;
            BuildCell(MapView_CellItems + static_cast<std::uint32_t>(column + row * 28) * 4u, layer, threshold);
            inset = static_cast<std::uint32_t>(Scratch_Swap);
            ++k;
        }
    }
    g.sort();
}

// original 0x56F9B0: a map cell's textures (PSX FUN_80154D50) - for its draw
// item's quad in buffer `buffer`, then the +0x8E side item's, then the +0x7E
// one's; the flags MapView_Build ORs into the cell word come back.
//
// As the original has it: every argument is used at 32 bits; the texture run
// starts at dword (height * width + 1) / 2 + offset + tile of the block; the
// +0x7E item takes the dword after the +0x8E one's only when there is one; a
// zero dword releases the +0x7E item, but not the +0x8E one, which is given a
// zero texture word.
extern "C" unsigned __cdecl MapView_CellTextures(unsigned x, unsigned y, unsigned char* item, unsigned buffer) {
    const std::uint32_t header = Dword(AreaMap_Header);
    const std::uint32_t width = header & 0xFFu;
    const unsigned offset = Word(AreaMap_Header + 2);
    const unsigned tile = Word(AreaMap_Header + (x + width * y + offset * 2u) * 2u);
    if (tile == 0) return 0;
    const auto height = static_cast<std::int32_t>((header >> 8) & 0xFFu);
    const std::int32_t half = (height * static_cast<std::int32_t>(width) + 1) / 2;
    const unsigned char* texture =
        AreaMap_Header + (static_cast<std::uint32_t>(half) + offset + tile) * 4u;
    unsigned flags = ((static_cast<std::int32_t>(Dword(texture)) >> 15) & 0x8000) | (Dword(texture) & 0x4000u);
    g.set_texture(Dword(texture), item + buffer * 0x48u, 1);
    texture += 4;
    const unsigned below = Word(item + 0x8E);
    if (below != 0) {
        g.set_texture(Dword(texture), Quad(below, buffer), 1);
        flags |= (static_cast<std::int32_t>(Dword(texture)) >> 18) & 0x1000;
        texture += 4;
    }
    const unsigned next = Word(item + 0x7E);
    if (next != 0) {
        if (Dword(texture) == 0) {
            g.release(static_cast<unsigned short>(next));
            SetWord(item + 0x7E, 0);
            return flags;
        }
        g.set_texture(Dword(texture), Quad(next, buffer), 1);
        flags |= (static_cast<std::int32_t>(Dword(texture)) >> 17) & 0x2000;
    }
    return flags;
}

// original 0x571500: MapCell_Handlers 0x3F..0x4C (PSX FUN_801587A0) - a
// cell's walls, one or two POLY_FT4s 128 high hanging below an edge of the
// cell. A texture index t picks the edge by its parity - even the top edge
// (toward the row before), odd the left - and the heights are the edge's two
// corners, the cell's own or the neighbour's across the edge, whichever pair
// has a corner standing higher (signed).
//
// As the original has it: kinds 0x4B and 0x4C take texture words 4 and 5 of
// a four-entry table - the first two dwords of the code-pointer table after
// it (docs/map-layers.md, defects); b1 and b0 are used at 32 bits; the kind
// is not checked (read, not run: only 0x3F..0x4C reach it).
extern "C" void __cdecl MapCell_DrawWalls(const unsigned char* record, unsigned b1, unsigned b0) {
    std::uint32_t t = ((Dword(record) >> 24) - 0x3Fu) >> 1;
    int count = 1;
    if (static_cast<std::int32_t>(t) >= 4) {
        count = 2;
        t = t * 2 - 8;
    }
    const unsigned char* const corners = Corners() + (Width() * b0 + b1) * 4u;
    const std::uint32_t x0 = (b1 << 7) - 0x4040u, y0 = (b0 << 7) - 0x4040u;
    // Four vertices as the original lays them on its stack: 0 and 2 at the
    // corner the edge starts from, 1 and 3 where it ends; 0 and 1 the wall's
    // top. The fourth words are never read (Gte_RotTransPers4 reads three).
    short v[4][4] = {};
    v[0][0] = v[2][0] = static_cast<short>(x0);
    v[0][1] = v[2][1] = static_cast<short>(y0);
    const unsigned long* texture = MapCell_WallTextures + t;
    for (; count > 0; --count, ++t, ++texture) {
        unsigned char* const prim = Gfx_PacketNext;
        g.set_poly_ft4(prim);
        g.set_shade_tex(prim, 0);
        const std::uint32_t odd = t & 1u;
        v[1][0] = v[3][0] = static_cast<short>(x0 + ((~t & 1u) << 7));
        v[1][1] = v[3][1] = static_cast<short>(y0 + (odd << 7));
        short h1, h2;
        if (odd) {
            const unsigned char* const left = corners - 4;
            if (S8(left[1]) > S8(corners[0]) || S8(left[3]) > S8(corners[2])) {
                h1 = Height(left[1]);
                h2 = Height(left[3]);
            } else {
                h1 = Height(corners[0]);
                h2 = Height(corners[2]);
            }
        } else {
            const unsigned char* const up = corners - Width() * 4u;
            if (S8(up[2]) > S8(corners[0]) || S8(up[3]) > S8(corners[1])) {
                h1 = Height(up[2]);
                h2 = Height(up[3]);
            } else {
                h1 = Height(corners[0]);
                h2 = Height(corners[1]);
            }
        }
        v[2][2] = h1;
        v[3][2] = h2;
        v[0][2] = static_cast<short>(h1 - 0x80);
        v[1][2] = static_cast<short>(h2 - 0x80);
        long depth;
        g.rot_trans_pers4(v[0], v[1], v[2], v[3], reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
                          reinterpret_cast<float*>(prim + 0x28), reinterpret_cast<float*>(prim + 0x38), &depth);
        g.depths4(prim);
        g.set_texture(*texture, prim, 1);
        g.commit(Draw_OtSlot, 0x48);
    }
}

// original 0x571B40: AreaMap_EntryHandlers 0 (PSX FUN_8015900C) - a palette
// cycle. The entry's low word is a period in frames; the next word a
// condition; then dwords whose top byte is a frame 1..period, in order: on a
// frame named, and the condition true, one 16-colour row of Gfx_ClutStrip is
// copied over another.
//
// As the original has it, read and not run (the fuzz keeps inside all
// three): a period of 0 divides by zero; the scan stops only at a top byte
// equal to or above the frame, so a list without one runs on; the rows are 12
// bits and unchecked against the strip's 512.
extern "C" void __cdecl AreaMap_ClutCycle(const unsigned char* entry) {
    const unsigned frame = Frame_Counter % (Dword(entry) & 0xFFFFu) + 1;
    const unsigned char* p = entry + 4;
    if (g.test(Word(p)) == 0) return;
    for (;;) {
        p += 4;
        const auto at = static_cast<int>(Dword(p) >> 24);
        if (static_cast<int>(frame) == at) break;
        if (static_cast<int>(frame) < at) return;
    }
    const std::uint32_t d = Dword(p);
    const std::uint32_t from = (((d >> 16) & 0xFFu) << 4) + ((d >> 12) & 0xFu);
    const std::uint32_t to = (((d >> 4) & 0xFFu) << 4) + (d & 0xFu);
    auto* const strip = reinterpret_cast<unsigned char*>(Gfx_ClutStrip);
    for (unsigned k = 0; k < 32; k += 4) std::memcpy(strip + to * 32 + k, strip + from * 32 + k, 4);
    Gfx_ClutStripDirty = 1;
}

// original 0x571AF0: the area header's pass (PSX FUN_80158F60), first thing
// in 0x592F00.
//
// As the original has it: the kind is masked to 7 bits and not checked
// against the table's four - all 128 entries are run by the fuzz; the step is
// read after the handler; a step of 0 never ends (read, not run).
extern "C" void __cdecl AreaMap_HeaderPass(void) {
    using Handler = void(__cdecl*)(const unsigned char*);
    if (!(Draw_PassFlags & 4)) return;
    const unsigned char* p = AreaMap_Header + AreaMap_EntryBase * 4u;
    for (std::uint32_t e = Dword(p); e != 0; e = Dword(p)) {
        reinterpret_cast<Handler>(static_cast<std::uintptr_t>(AreaMap_EntryHandlers[(e >> 24) & 0x7Fu]))(p);
        p += p[2] * 4u;
    }
}

void MapLayers_Inject() {
    if (bof3::WantsShadow("map_layers")) map_layers::SelfTest();
    BOF3_INJECT(AreaMap_Frame);
    BOF3_INJECT(MapView_Build);
    BOF3_INJECT(MapView_CellTextures);
    BOF3_INJECT(MapCell_DrawWalls);
    BOF3_INJECT(AreaMap_ClutCycle);
    BOF3_INJECT(AreaMap_HeaderPass);
}
