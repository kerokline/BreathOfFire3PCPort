// Moved here by tools/move_funcs.py from area_backdrop.cpp, area_slope.cpp, battle_draw.cpp, draw_emit.cpp, draw_layers.cpp, draw_pool.cpp, field_e2.cpp, field_event.cpp, field_misc.cpp, kind2_object.cpp, map_cells.cpp, map_field_objects.cpp, map_layers.cpp, map_scroll.cpp, map_view.cpp, move_cmds.cpp, prim.cpp, scena_sx.cpp, scena_sx2.cpp, world_map.cpp (docs/source-layout-round.md).
#include "game/field/area_map.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/area_backdrop.h"
#include "game/area_backdrop_callees.h"
#include "game/area_slope.h"
#include "game/battle_draw.h"
#include "game/battle_draw_callees.h"
#include "game/crt_rest.h"
#include "game/d3d_lines.h"
#include "game/draw_emit.h"
#include "game/draw_layers.h"
#include "game/draw_pool.h"
#include "game/draw_pool_room.h"
#include "game/faerie_ring.h"
#include "game/field/area_map_callees.h"
#include "game/field_e2.h"
#include "game/field_e2_callees.h"
#include "game/field_event.h"
#include "game/field_event_callees.h"
#include "game/field_misc.h"
#include "game/field_misc_callees.h"
#include "game/kind2_object.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/layering.h"
#include "game/list_title.h"
#include "game/map_cells.h"
#include "game/map_field_objects.h"
#include "game/map_field_objects_callees.h"
#include "game/map_layers.h"
#include "game/map_layers_callees.h"
#include "game/map_rim.h"
#include "game/map_scroll.h"
#include "game/map_scroll_callees.h"
#include "game/map_view.h"
#include "game/move_cmds.h"
#include "game/move_cmds_callees.h"
#include "game/move_script_bytes.h"
#include "game/prim.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sx.h"
#include "game/scena_sx2.h"
#include "game/scena_sx2_callees.h"
#include "game/scena_sx_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/wide_rand.h"
#include "game/widescreen.h"
#include "game/world_map.h"
#include "game/world_map_callees.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/exe_layout.h"
#include "hook/host.h"
#include "hook/log.h"
#include "hook/snapshot.h"
#include "input/sigma.h"

namespace {

namespace at = area_map::at;

}  // namespace

namespace {

using namespace move_script;

using namespace battle_draw;

using namespace field_misc;

using namespace map_field_objects;

using namespace move_cmds;

using namespace world_map;

}  // namespace

namespace {

using area_backdrop::g;

std::uint32_t Dword(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

unsigned WordAreaBackdrop(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void SetWordAreaBackdrop(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

void SetDword(unsigned char* p, std::uint32_t v) { std::memcpy(p, &v, sizeof v); }

void SetFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

unsigned char* AtAreaBackdrop(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

// A 5-bit colour channel of a packed entry colour, as the PSX's 15-bit
// (r, g, b) unpacked to 8 bits: the original's `and al, 0x1F; shl al, 3`.
unsigned char Channel(std::uint32_t colour, unsigned shift) {
    return static_cast<unsigned char>(((colour >> shift) & 0x1Fu) << 3);
}

// The party's three field objects: ObjTrio 0x802D40, 0x14C bytes each (the
// PSX's 0x140). Read here: the position dwords +0x34 / +0x38, the slot byte
// +0x29 (what Sprite_Draw commits the sprite's primitives to) and bit 0 of
// the flag byte +0x138 (what MapView_CheckHeightScale tests).
constexpr std::uint32_t kTrioStride = 0x14C, kTrioPosition = 0x34, kTrioSlot = 0x29, kTrioFlags = 0x138;

// A fourth object record at 0x905DA0, unnamed in symbols.toml: the one
// GameMode_Field's request 9 sets up and MapView_CheckHeightScale tests bit 0
// of its byte +0xB for, as it tests ObjTrio's +0x138. AreaMap_SlotZones
// reads its position +0x34 / +0x38 and sets or clears that bit; it has no
// slot byte in the zone's sense (the PSX twin does the same for its record
// at 0x8014629C / byte 0x80146273).
// kFourthObject: now FieldEvent_Object905DA0 (symbols.toml).
constexpr std::uint32_t kFourthFlags = 0xB;

// The rounded cell of a 16.16 position: the high word of (p + 0x8000), signed.
int Cell(const unsigned char* position) {
    return static_cast<short>((Dword(position) + 0x8000u) >> 16);
}

int S8(unsigned char v) { return static_cast<signed char>(v); }

std::int32_t Abs(std::int32_t v) { return v < 0 ? -v : v; }   // never INT_MIN here: sums of a few bytes

// A layer is 0x30 bytes: three lists, each a (first, last) pair per buffer.
// The second list, at +0x10, is the one closed here.
constexpr unsigned kLayerDwords = 12, kSecondList = 4;

using Handler = void (__cdecl*)(unsigned char* record, unsigned b1, unsigned b0);

// One map cell's run of records handed to MapCell_Handlers - DrawLayer_Open's
// body for a non-zero word. `track` is DIV-0071's recording (mode 1 only):
// off for the cells only DIV-0041's grown walk adds, which layering.cpp's
// copy of the walk does not know.
void RunCell(unsigned word, int layer, unsigned row, unsigned column, bool band, bool track) {
    const std::uint32_t at = (word + (AreaMap_CellBase & 0xFFFFu)) * 4u;
    unsigned char* record = AreaMap_Header + at;
    std::uint32_t head;
    std::memcpy(&head, record - 4, 4);
    const unsigned b1 = (head >> 8) & 0xFF, b0 = head & 0xFF;
    unsigned char* const end = record + (head >> 16) * 4u - 4u;
    wide_rand::SetBand(band);
    // DIV-0071 (layering.h): the run's drawing recorded, mode 1 only.
    if (track && layering::g_mode == layering::kFloor) layering::RunBegin(record);
    while (record != end) {
        std::uint32_t dword;
        std::memcpy(&dword, record, 4);
        if (draw_order::g_on)
            draw_order::Set(draw_order::kCell, static_cast<unsigned>(layer), row, column, dword >> 24, b1 << 8 | b0);
        reinterpret_cast<Handler>(static_cast<std::uintptr_t>(MapCell_Handlers[dword >> 24]))(record, b1, b0);
        record += record[2] * 4u;
    }
    if (layering::g_in_run) layering::RunEnd();
}

// The layer's first list appended to slot 6 - DrawLayer_Open's head. Layer
// -1 (DIV-0041's grown ring only) is map_rim's record.
void LinkFirst(int layer) {
    const std::uint32_t first = static_cast<std::uint32_t>(layer) * 6u;
    const auto at = [&]() -> unsigned long* {
        if (layer < 0) return map_rim::ExtraLayer() + Gfx_BufferIndex * 2u;
        return DrawLayers + (Gfx_BufferIndex + first) * 2u;
    };
    unsigned long* cell = at();
    if (cell[0] != 0) {
        if (draw_order::g_on) draw_order::TagList(cell[0], cell[1], draw_order::kList0, static_cast<unsigned>(layer), 6);
        Gpu_LinkPrim(Gfx_OtPointers[6], cell[0]);
        // The index is read again after the call, as the original reads it.
        cell = at();
        Gfx_OtPointers[6] = reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(cell[1]));
    }
}

// Capcom's walk of layer `layer`'s row (DrawLayer_Open's, below); false when
// it has no columns, where the original returns at once.
bool CapcomRow(int layer) {
    int row = MapView_Row + layer + 1;
    if (row >= 0x38) row -= 0x38;
    int inset = MapView_Inset;
    if (Field_InputFlags & 1) inset = inset >= 2 ? inset - 2 : 0;
    int column = MapView_Column + inset + (~row & 1);
    if (column >= 0x1C) column -= 0x1C;
    int count = (14 - inset) * 2;
    if (count <= 0) return false;
    // DIV-0041's random split (docs/ultrawide-rand.md): which of this row's cells the original's
    // walk - its inset before DIV-0041 lowered it - would not reach; their
    // handlers draw the rising squares from a generator of ours. Narrow, none.
    const wide_rand::Row split(MapView_Inset, Field_InputFlags, row, MapView_Column);
    const int row_base = row * 28;
    for (; count != 0; --count) {
        column = column == 0x1B ? 0 : column + 1;
        const unsigned word = MapView_Cells[row_base + column];
        if (word == 0) continue;
        RunCell(word, layer, static_cast<unsigned>(row), static_cast<unsigned>(column), split.Band(column), true);
    }
    return true;
}

// DIVERGENCE DIV-0041 (wide-ring, docs/wide-ring.md): the grown ring's walk of
// one layer, -1..0x37. The grown columns by the PSP's arithmetic (its
// 0x101BC): the grown inset g (map_rim::Walk, less 2 with Field_InputFlags
// bit 0), from grown column g + (1 for an even grown row), 33 - 2g columns,
// wrapping at 33. On Capcom's layers 0..0x36 Capcom's own walk runs in the
// middle exactly as narrow - its order, its split - and the grown columns it
// does not take run before it (those left of it) and after it (right): every
// one of them a band cell, outside the original's walk (which is inside
// today's). Layers -1 and 0x37 are rows the original never walks: every cell a
// band cell. Rim cells' words are map_rim's.
void OpenGrown(int layer) {
    LinkFirst(layer);
    const int grown_row = layer + 1;
    int walk = map_rim::Walk(MapView_Inset);
    if (Field_InputFlags & 1) walk = walk >= 2 ? walk - 2 : 0;
    bool grown[map_rim::kColumns] = {};
    const int count = map_rim::kColumns - 2 * walk;
    for (int k = 0, c = walk + ((grown_row & 1) == 0 ? 1 : 0); k < count; ++k)
        grown[(c + k) % map_rim::kColumns] = true;
    // Capcom's walk of this row, as grown columns: [lo, hi], contiguous.
    int lo = map_rim::kColumns, hi = -1;
    const bool capcom = layer >= 0 && layer < 0x37;
    if (capcom) {
        // CapcomRow's arithmetic, its columns turned into grown ones: view
        // column v of Capcom's row `layer` is grown v + 2 + (layer odd).
        int row = MapView_Row + layer + 1;
        if (row >= 0x38) row -= 0x38;
        int inset = MapView_Inset;
        if (Field_InputFlags & 1) inset = inset >= 2 ? inset - 2 : 0;
        int column = MapView_Column + inset + (~row & 1);
        if (column >= 0x1C) column -= 0x1C;
        for (int k = 0, n = (14 - inset) * 2; k < n; ++k) {
            column = column == 0x1B ? 0 : column + 1;
            const int view = ((column - MapView_Column - 1) % 0x1C + 0x1C) % 0x1C;
            const int grown_column = view + 2 + (layer & 1);
            lo = grown_column < lo ? grown_column : lo;
            hi = grown_column > hi ? grown_column : hi;
        }
    }
    const auto extra = [&](int grown_column) {
        const unsigned word = map_rim::RunWord(grown_row, grown_column);
        if (word == 0) return;
        RunCell(word, layer, 0x100u | static_cast<unsigned>(grown_row), static_cast<unsigned>(grown_column), true, false);
    };
    for (int c = 0; c < map_rim::kColumns && c < lo; ++c)
        if (grown[c]) extra(c);
    if (capcom) CapcomRow(layer);
    for (int c = hi + 1; c < map_rim::kColumns; ++c)
        if (grown[c] && c > lo) extra(c);
    wide_rand::SetBand(false);
    if (draw_order::g_on) draw_order::Clear();
}

constexpr unsigned kLayers = 0x38, kLayerBytes = 0x30;

// move_script::At: named in full below (source-layout B0, a clash on At)
// move_script::Long: named in full below (source-layout B0, a clash on Long)
using move_script::SetLong;

// move_script::SetWord: named in full below (source-layout B0, a clash on SetWord)
// move_script::Word: named in full below (source-layout B0, a clash on Word)
using U = std::uint32_t;

unsigned char& BFieldE2(U a) { return *move_script::At(a); }

U LFieldE2(U a) { return static_cast<U>(move_script::Long(move_script::At(a))); }

void SetL(U a, U v) { move_script::SetLong(move_script::At(a), static_cast<std::int32_t>(v)); }

std::uint16_t WFieldE2(U a) { return move_script::Word(move_script::At(a)); }

U Key(const void* p) { return Low32(p); }

// The record views (game/record_views.h): the leader helpers run on the leader,
// an ObjTrio record, and Field_State is one.
U LongAt(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

int S8FieldE2(unsigned v) { return static_cast<signed char>(v); }

// A vertex dword of the quads: the s8 at byte 3 and the s8 at byte 2, each
// doubled, about the cell's origin, and the low word; the fourth word 0
// (DIV-0023, as map_cells.cpp's QuadVertex).
void QuadVertex(short* v, U dword, U x0, U y0) {
    v[0] = static_cast<short>(x0 + 2u * static_cast<U>(S8FieldE2(dword >> 24)));
    v[1] = static_cast<short>(y0 + 2u * static_cast<U>(S8FieldE2(dword >> 16)));
    v[2] = static_cast<short>(dword);
    v[3] = 0;
}

// The four corners (v[0..3], eight words each) projected into the POLY_FT4 or
// POLY_G4 at prim as the originals order them - v1, v2, v3, then v0 to the
// first point - and their depths.
void Project4(short* v, unsigned char* prim) {
    long unused;
    SH_CALL(Gte_RotTransPers4)(v + 4, v + 8, v + 12, v, reinterpret_cast<float*>(prim + 0x18), reinterpret_cast<float*>(prim + 0x28),
                               reinterpret_cast<float*>(prim + 0x38), reinterpret_cast<float*>(prim + 8), &unused);
    SH_CALL(Gte_StoreDepthF4)(reinterpret_cast<float*>(prim + 0x20), reinterpret_cast<float*>(prim + 0x30),
                              reinterpret_cast<float*>(prim + 0x40), reinterpret_cast<float*>(prim + 0x10));
}

unsigned char* AtFieldMisc(field_misc::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

field_misc::U WordFieldMisc(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

std::int32_t Short(field_misc::U v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

field_misc::U LongFieldMisc(const unsigned char* p) {
    field_misc::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutWord(unsigned char* p, field_misc::U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

void PutLongFieldMisc(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

// The map-view ring (docs/map-scroll.md): 0x38 rows of 0x1C cells, their heads
// MapView_Row / MapView_Column; MapView_Origin the map cell of the view's corner.
field_misc::U OriginX() { return WordFieldMisc(AtFieldMisc(field_misc::at::MapView_OriginAt())); }

field_misc::U OriginZ() { return WordFieldMisc(AtFieldMisc(field_misc::at::MapView_OriginAt() + 2)); }

std::int32_t Row() { return Short(WordFieldMisc(AtFieldMisc(field_misc::at::MapView_RowAt()))); }

std::int32_t Column() { return Short(WordFieldMisc(AtFieldMisc(field_misc::at::MapView_ColumnAt()))); }

unsigned char BufferIndex() { return AtFieldMisc(field_misc::at::Gfx_BufferIndexAt())[0]; }

field_misc::U Header(field_misc::U byte_offset) { return field_misc::at::AreaMap_HeaderAt() + byte_offset; }

// A draw item's 0x48-byte half for this display buffer: DrawItems +
// (Gfx_BufferIndex + index * 2) * 0x48, all in 32 bits as the original's lea.
field_misc::U ItemHalf(field_misc::U index) { return field_misc::at::DrawItemsAt() + (BufferIndex() + index * 2u) * 0x48u; }

// The diagonal ring lookup MoveCmd_TestFB / FC, MapView_ItemAt (and the
// cell handler 0x570A00, Capcom's) share: map cell (x', z') relative to the
// view's origin lies on ring row (x' + z' + MapView_Row + 1) mod 0x38 and
// column ((x' - z') / 2 + MapView_Column + 1) mod 0x1C - each wrapped by ONE
// subtraction, so a head outside 0..0x37 / 0..0x1B leaves the grid - when
// 0 <= x' + z' < 0x38 and 0 <= x' - z' < 0x38. `spill` is what the original
// leaves in eax on the way out: the sum, or the difference, or the column.
struct Ring {
    bool inside;
    field_misc::U row, column;
    field_misc::U spill;
};

Ring RingCell(std::int32_t dx, std::int32_t dz) {
    const std::int32_t s = static_cast<std::int32_t>(static_cast<field_misc::U>(dx) + static_cast<field_misc::U>(dz));
    if (s >= 0x38 || s < 0) return {false, 0, 0, static_cast<field_misc::U>(dx)};
    std::int32_t row = static_cast<std::int32_t>(static_cast<field_misc::U>(s) + static_cast<field_misc::U>(Row()) + 1u);
    if (row >= 0x38) row -= 0x38;
    const std::int32_t d = static_cast<std::int32_t>(static_cast<field_misc::U>(dx) - static_cast<field_misc::U>(dz));
    if (d >= 0x38 || d < 0) return {false, 0, 0, static_cast<field_misc::U>(d)};
    std::int32_t column = static_cast<std::int32_t>(static_cast<field_misc::U>(d / 2) + static_cast<field_misc::U>(Column()) + 1u);
    if (column >= 0x1C) column -= 0x1C;
    return {true, static_cast<field_misc::U>(row), static_cast<field_misc::U>(column), static_cast<field_misc::U>(column)};
}

// `cdq` / `idiv`: the original's signed division, faults included - a zero
// divisor, or INT_MIN by -1, raises the same #DE at the same point of the
// same computation, which a C++ `/` does not promise (it is undefined there).
std::int32_t Idiv(std::int32_t dividend, std::int32_t divisor) {
    std::int32_t quotient;
    __asm__ volatile("cltd\n\tidivl %2" : "=a"(quotient) : "a"(dividend), "r"(divisor) : "edx", "cc");
    return quotient;
}

// `cdq` / `xor` / `sub`: an absolute value in which INT_MIN stays INT_MIN.
std::int32_t Abs32(std::uint32_t v) {
    const auto s = static_cast<std::int32_t>(v);
    return s < 0 ? static_cast<std::int32_t>(0u - v) : s;
}

template <class T> T Get(const unsigned char* p, unsigned offset) {
    // A pointer-typed field is a 32-bit word of the image on every width.
    if constexpr (std::is_pointer_v<T>) {
        std::uint32_t w;
        std::memcpy(&w, p + offset, sizeof w);
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(w));
    } else {
        T v;
        std::memcpy(&v, p + offset, sizeof v);
        return v;
    }
}

constexpr unsigned kPrimBytes = 0x48;   // a POLY_FT4 as the port lays it out

int S8MapCells(unsigned v) { return static_cast<signed char>(v); }

// A vertex dword of MapCell_DrawQuads: the s8 at byte 3 and the s8 at byte 2,
// each doubled, about the cell's origin; the low word is the third coordinate.
// The sums are the original's 32-bit ones, stored as words.
//
// DIV-0023: the fourth word is 0, as DIV-0021 zeros the matrix product's
// padding (the owner's call). The original builds these on its stack and
// never writes that word, so Gte_LoadVertex and Gte_LoadVertices3 carry stale
// stack into the top halves of Gte_Vertices[1], [3] and [5], which no
// instruction in the image reads.
void QuadVertexMapCells(short* v, std::uint32_t dword, std::uint32_t x0, std::uint32_t y0) {
    v[0] = static_cast<short>(x0 + 2u * static_cast<std::uint32_t>(S8MapCells(dword >> 24)));
    v[1] = static_cast<short>(y0 + 2u * static_cast<std::uint32_t>(S8MapCells(dword >> 16)));
    v[2] = static_cast<short>(dword);
    v[3] = 0;
}

}  // namespace

// original 0x570020: a map cell's textured quads. Unless the record's
// condition (its low word) says no, or its byte +2 is 1, walks five-dword
// subrecords from +4 until a count that starts at 1 and adds 5 each equals
// byte +2 again: four vertices, then a Prim_SetTexture word. The cell's origin
// is ((b1 - 0x80) * 128, (b0 - 0x80) * 128). The first vertex alone goes
// through Gte_Rtps and to the primitive at Gfx_PacketNext; the quad is drawn
// only if that point is inside -100 < x < 420, -150 < y < 300 - a POLY_FT4
// committed to slot 4, or 7 with bit 30 of the word, when bit 14 is set, else
// to Draw_OtSlot, or 6 with bit 30. 12,183 calls a whole attract cycle.
//
// Kept as the original has it: a culled quad still leaves its first screen
// point at +8 of the packet; a byte +2 that is not 1 + 5n never stops the
// walk (read, not run: the fuzz only builds lengths of 1 + 5n).
static void DrawQuads(const unsigned char* record, unsigned b1, unsigned b0) {
    if (Area_TestCondition(Get<std::uint16_t>(record, 0)) == 0) return;
    const std::uint32_t x0 = (b1 - 0x80u) << 7, y0 = (b0 - 0x80u) << 7;
    if (record[2] == 1) return;
    unsigned count = 1;
    for (const unsigned char* sub = record + 4;; sub += 0x14) {
        alignas(4) short first[4];
        QuadVertexMapCells(first, Get<std::uint32_t>(sub, 0), x0, y0);
        Gte_LoadVertex(reinterpret_cast<const unsigned long*>(first));
        Gte_Rtps();
        unsigned char* const prim = Gfx_PacketNext;
        Gte_StoreScreenXY(reinterpret_cast<unsigned long*>(prim + 8));
        const float sx = Get<float>(prim, 8), sy = Get<float>(prim, 0xC);
        const float cull = static_cast<float>(Widescreen_Fill());   // DIV-0041: the x cull out by the columns
        if (sx > -100.0f - cull && sx < 420.0f + cull && sy > -150.0f && sy < 300.0f) {
            Gpu_SetPolyFT4(prim);
            Gpu_SetShadeTex(prim, 0);
            alignas(4) short rest[12];
            for (unsigned k = 0; k < 3; ++k) QuadVertexMapCells(rest + 4 * k, Get<std::uint32_t>(sub, 4 + 4 * k), x0, y0);
            Gte_LoadVertices3(reinterpret_cast<const unsigned long*>(rest));
            const std::uint32_t texture = Get<std::uint32_t>(sub, 0x10);
            Prim_SetTexture(texture, prim, 1);
            Gte_Rtpt();
            unsigned slot;
            if (texture & 0x4000u) slot = texture & 0x40000000u ? 7u : 4u;
            else slot = texture & 0x40000000u ? 6u : Draw_OtSlot;
            if (draw_order::g_on) draw_order::g_ctx.texture = texture;   // BOF3X_DRAWORDER, diagnostic only
            Gfx_CommitPrim(slot, kPrimBytes);
            Gte_StoreScreenXY3(reinterpret_cast<unsigned long*>(prim + 0x18), reinterpret_cast<unsigned long*>(prim + 0x28),
                               reinterpret_cast<unsigned long*>(prim + 0x38));
            Gte_PrimDepths4_10(prim);
        }
        count += 5;
        if (count == record[2]) return;
    }
}

// original 0x570660: eight flat squares stacked on a map cell, redrawn each
// frame with one Rand each. The record is not read. For square i = 0..7, with
// f = Frame_Counter & 7:
//   half-side   0x20 + 0x20 * i + 4f + (Rand() & 3)
//   centre      ((b1 - 0x81) * 128 + s, (b0 - 0x80) * 128 - s), where the
//               sway s = |16 - ((Frame_Counter >> 8) & 0x1F)| * (f + 8i) / 10
//   third       -(AreaMap_Elevation(b1 - 1, b0) / 2) - 8 * (f + 8i)
//   texture     0xBA009124, shade ((0x3F - 8i - f) / 4) in bits 19..22
// The corners go through Prim_VertexScratch, whose fourth words are left as
// they were, into Gte_RotTransPers4; each square is committed to
// Draw_OtSlot. 5,098 calls a whole attract cycle.
//
// Kept as the original has it: the arithmetic is 32-bit on whatever b1 and b0
// hold and each coordinate is stored as its low word; the elevation is halved
// as an s16, so the header's top half that an off-grid answer carries is lost.
//
// DIV-0041's random split (docs/ultrawide-rand.md): the random number is map_cells::RisingRand's
// - Capcom's Rand for every cell the original walks, the draw-only generator
// for a cell only DIV-0041's lowered inset adds. Narrow, always Rand.
static void DrawRising(const unsigned char* record, unsigned b1, unsigned b0) {
    static_cast<void>(record);
    const long elevation = AreaMap_Elevation(static_cast<long>((b1 << 16) - 0x10000u), static_cast<long>(b0 << 16));
    const int base = -(static_cast<short>(elevation) / 2);
    const unsigned phase = Frame_Counter & 7u;
    const std::uint32_t x0 = (b1 - 0x81u) << 7, y0 = (b0 - 0x80u) << 7;
    short* const v = reinterpret_cast<short*>(Prim_VertexScratch);
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const prim = Gfx_PacketNext;
        Gpu_SetPolyFT4(prim);
        Gpu_SetShadeTex(prim, 0);
        const std::uint32_t half = (static_cast<std::uint32_t>(map_cells::RisingRand()) & 3u) + 4u * phase + 0x20u + 0x20u * i;
        int swing = 16 - static_cast<int>((Frame_Counter >> 8) & 0x1Fu);
        if (swing < 0) swing = -swing;
        const auto sway = static_cast<std::uint32_t>(swing * static_cast<int>(phase + 8u * i) / 10);
        const auto left = static_cast<short>(x0 + sway - half), right = static_cast<short>(x0 + sway + half);
        const auto top = static_cast<short>(y0 - sway - half), bottom = static_cast<short>(y0 - sway + half);
        const auto depth = static_cast<short>(base - 8 * static_cast<int>(phase + 8u * i));
        v[0] = left;   v[1] = top;     v[2] = depth;
        v[4] = right;  v[5] = top;     v[6] = depth;
        v[8] = left;   v[9] = bottom;  v[10] = depth;
        v[12] = right; v[13] = bottom; v[14] = depth;
        long unused;
        Gte_RotTransPers4(v + 4, v + 8, v + 12, v, reinterpret_cast<float*>(prim + 0x18), reinterpret_cast<float*>(prim + 0x28),
                          reinterpret_cast<float*>(prim + 0x38), reinterpret_cast<float*>(prim + 8), &unused);
        Gte_StoreDepthF4(reinterpret_cast<float*>(prim + 0x20), reinterpret_cast<float*>(prim + 0x30),
                         reinterpret_cast<float*>(prim + 0x40), reinterpret_cast<float*>(prim + 0x10));
        const unsigned shade = static_cast<unsigned>(static_cast<int>(0x3F - 8 * i - phase) / 4);
        Prim_SetTexture((shade << 19) | 0xBA009124u, prim, 1);
        Gfx_CommitPrim(Draw_OtSlot, kPrimBytes);
    }
}

namespace {

using HandlerFn = void (__cdecl*)(const unsigned char*, unsigned, unsigned);

constexpr unsigned kLiveGte = 0x380, kLiveWindow = 0x1000;

struct LiveState {
    unsigned char gte[kLiveGte];
    unsigned char scratch[0x20];
    unsigned char* packet_next;
    unsigned long* ot[8];
    unsigned long tails[8];
    std::uint32_t seed;
    std::uint32_t band_seed;   // DIV-0041's random split: the draw-only generator's (wide_rand.h)
    unsigned char window[kLiveWindow];
};

struct LiveCounts {
    unsigned calls, mismatches, padding, committed;
} g_live[2];

unsigned char* LiveGte() { return reinterpret_cast<unsigned char*>(&Gte_RampFar); }

unsigned LiveGteSize() {
    return static_cast<unsigned>(reinterpret_cast<unsigned char*>(&Gte_Depth) + 4 - LiveGte());
}

// The tails are read through `tails_at` - the input's pointers - on every
// capture, so that each side's links are seen where they were made.
void LiveCapture(LiveState& s, unsigned char* window_at, unsigned window, std::uint32_t* seed,
                 unsigned long* const* tails_at) {
    std::memset(&s, 0, sizeof s);
    std::memcpy(s.gte, LiveGte(), LiveGteSize());
    std::memcpy(s.scratch, Prim_VertexScratch, sizeof s.scratch);
    s.packet_next = Gfx_PacketNext;
    for (unsigned k = 0; k < 8; ++k) {
        s.ot[k] = Gfx_OtPointers[k];
        s.tails[k] = tails_at[k] ? *tails_at[k] : 0;
    }
    s.seed = *seed;
    s.band_seed = *wide_rand::DrawOnlySeed();
    std::memcpy(s.window, window_at, window);
}

void LiveApply(const LiveState& s, unsigned char* window_at, unsigned window, std::uint32_t* seed) {
    std::memcpy(LiveGte(), s.gte, LiveGteSize());
    std::memcpy(Prim_VertexScratch, s.scratch, sizeof s.scratch);
    Gfx_PacketNext = s.packet_next;
    for (unsigned k = 0; k < 8; ++k) Gfx_OtPointers[k] = s.ot[k];
    for (unsigned k = 0; k < 8; ++k)
        if (s.ot[k]) *s.ot[k] = s.tails[k];
    *seed = s.seed;
    *wide_rand::DrawOnlySeed() = s.band_seed;
    std::memcpy(window_at, s.window, window);
}

void Live(unsigned which, HandlerFn theirs, void (*ours)(const unsigned char*, unsigned, unsigned),
          const unsigned char* record, unsigned b1, unsigned b0) {
    static LiveState input, their_out, our_out;
    static const char* const kName[2] = {"MapCell_DrawQuads", "MapCell_DrawRising"};
    unsigned char* const window_at = Gfx_PacketNext;
    const auto room = static_cast<unsigned>(Gfx_PacketPools + 0x20000 - window_at);
    const unsigned window = room < kLiveWindow ? room : kLiveWindow;
    auto* seed = crt_rest::RandSeedCell();
    unsigned long* tails_at[8];
    for (unsigned k = 0; k < 8; ++k) tails_at[k] = Gfx_OtPointers[k];

    LiveCapture(input, window_at, window, seed, tails_at);
    theirs(record, b1, b0);
    LiveCapture(their_out, window_at, window, seed, tails_at);
    LiveApply(input, window_at, window, seed);
    ours(record, b1, b0);
    LiveCapture(our_out, window_at, window, seed, tails_at);

    LiveCounts& c = g_live[which];
    ++c.calls;
    c.committed += static_cast<unsigned>(our_out.packet_next - input.packet_next) / kPrimBytes;
    const unsigned pad_at = static_cast<unsigned>(reinterpret_cast<unsigned char*>(Gte_Vertices) - LiveGte());
    for (unsigned slot = 1; slot < 6; slot += 2) {
        unsigned char* our_pad = our_out.gte + pad_at + 4 * slot + 2;
        const unsigned char* their_pad = their_out.gte + pad_at + 4 * slot + 2;
        if (std::memcmp(our_pad, their_pad, 2) == 0 || our_pad[0] != 0 || our_pad[1] != 0) continue;
        ++c.padding;
        std::memcpy(our_pad, their_pad, 2);
    }
    if (std::memcmp(&their_out, &our_out, sizeof our_out) != 0 && ++c.mismatches <= 8) {
        const auto* a = reinterpret_cast<const unsigned char*>(&their_out);
        const auto* b = reinterpret_cast<const unsigned char*>(&our_out);
        unsigned first = 0;
        while (a[first] == b[first]) ++first;
        bof3::Log("shadow      %s live MISMATCH: call %u, b1 %X, b0 %X, first difference at state +0x%X (%02X / %02X)",
                  kName[which], c.calls, b1, b0, first, a[first], b[first]);
    }
    if (c.calls == 1 || c.calls % 1024 == 0)
        bof3::Log("shadow      %s live: %u calls, %u primitives committed, %u MISMATCHES; a vertex pad word "
                  "differed %u times (DIV-0023, not counted)",
                  kName[which], c.calls, c.committed, c.mismatches, c.padding);
}

}  // namespace

namespace map_field_objects {

namespace {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace map_field_objects

namespace {

unsigned char* AtMapFieldObjects(map_field_objects::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

map_field_objects::U Byte(map_field_objects::U address) { return AtMapFieldObjects(address)[0]; }

map_field_objects::U WordMapFieldObjects(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

map_field_objects::U DwordMapFieldObjects(const unsigned char* p) {
    map_field_objects::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void SetWordMapFieldObjects(unsigned char* p, map_field_objects::U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

void SetDwordMapFieldObjects(unsigned char* p, map_field_objects::U v) { std::memcpy(p, &v, sizeof v); }

float Float(map_field_objects::U address) {
    float f;
    std::memcpy(&f, AtMapFieldObjects(address), sizeof f);
    return f;
}

void SetFloatMapFieldObjects(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

map_field_objects::U S8MapFieldObjects(map_field_objects::U v) { return static_cast<map_field_objects::U>(static_cast<std::int32_t>(static_cast<signed char>(v))); }

map_field_objects::U S16(map_field_objects::U v) { return static_cast<map_field_objects::U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))); }

short* Vertex(map_field_objects::U address) { return reinterpret_cast<short*>(AtMapFieldObjects(address)); }

float* FloatAt(map_field_objects::U address) { return reinterpret_cast<float*>(AtMapFieldObjects(address)); }

unsigned char* PacketNext() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(DwordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::PacketNextAt())))); }

map_field_objects::U OtSlot() { return Byte(map_field_objects::at::OtSlotAt()); }

// The slot a cell handler commits to: 6 when the record's top nibble is set
// (kinds 0x10 and up), else Draw_OtSlot - zero-extended from its byte.
map_field_objects::U HandlerSlot(map_field_objects::U record) { return (record & 0xF0000000u) ? 6u : OtSlot(); }

void PatchThen(unsigned char* record, map_field_objects::U kind) {
    const map_field_objects::U r = (DwordMapFieldObjects(record) & 0x00FFFFFFu) | kind;
    SetDwordMapFieldObjects(record, r);
    const map_field_objects::U index = (r & 0xFFFFu) + (DwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(AreaMap_PatchBase_addr))) & 0xFFFFu);
    map_field_objects::g.apply_patch(AtMapFieldObjects(::bof3::Img(AreaMap_Header_addr) + index * 4u));
}

unsigned WordMapLayers(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void SetWordMapLayers(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

std::uint32_t Address(const void* p) { return Low32(p); }

int S8MapLayers(unsigned v) { return static_cast<signed char>(v); }

unsigned Width() { return AreaMap_Header[0]; }

// A corner byte as a vertex's third coordinate: -(s8) * 16, as a word.
short Height(unsigned char corner) { return static_cast<short>(-S8MapLayers(corner) * 16); }

unsigned char* Corners() { return reinterpret_cast<unsigned char*>(&AreaMap_Corners); }

unsigned char* Item(unsigned index) { return draw_pool::Items() + index * 0x90u; }

// A draw item's quad for buffer `buffer`: items hold one per display buffer.
unsigned char* Quad(unsigned index, unsigned buffer) { return draw_pool::Items() + (buffer + index * 2u) * 0x48u; }

// DrawLayers dword `slot * 2 + 1`: the last link of list (slot - buffer) / 2
// mod 3 of layer slot / 6 - the layers are six (first, last) pairs, three
// lists by two buffers.
unsigned long& Last(std::uint32_t slot) { return DrawLayers[slot * 2u + 1u]; }

// The same by layer and list-and-buffer `sub` (list * 2 + buffer): layer -1,
// the grown ring's row above Capcom's (DIV-0041, wide-ring), is map_rim's
// record of the same shape; narrow no layer is below 0.
unsigned long& LastOf(int layer, std::uint32_t sub) {
    if (layer < 0) return map_rim::ExtraLayer()[sub * 2u + 1u];
    return Last(static_cast<std::uint32_t>(layer) * 6u + sub);
}

// Draw-table cells added by the current build (the byte count wraps at 256):
// for the wide build's peak log only.
unsigned g_table_adds;

unsigned long AsLong(const void* p) { return static_cast<unsigned long>(Address(p)); }

// MapView_Build's inset: columns taken off each side of the view by the
// camera's angle and distance. The original stores each step's value in
// Scratch_Swap as it goes, with no call in between, so only the last store is
// ever seen: the inset itself, and its low byte in MapView_Inset.
//
// DIV-0041's random split (docs/ultrawide-rand.md): the inset before DIV-0041's lowering is
// noted for DrawLayer_Open, whose cells outside the original's walk draw from
// a generator of ours; narrow the two are the same and nothing is split.
std::uint32_t Inset(std::uint32_t* original = nullptr) {
    const map_layers::InsetPair pair =
        map_layers::ComputeInset(Cond_AngleFB, Camera_Distance, Field_InputFlags, Cond_ByteFE, Widescreen_InsetColumns());
    Scratch_Swap = static_cast<unsigned long>(pair.lowered);
    MapView_Inset = static_cast<unsigned char>(Scratch_Swap);
    wide_rand::NoteInset(pair.original, MapView_Inset);
    if (original) *original = pair.original;
    return pair.lowered;
}

int Columns(std::uint32_t inset) { return static_cast<std::int32_t>((0xEu - inset) << 1); }

// A side triangle's list: list 0 of layer `layer` when the cell word's `bit` is
// set, else list 1 - as LastOf's list-and-buffer.
std::uint32_t SideSub(const unsigned char* cell, unsigned shift) {
    return ((~WordMapLayers(cell + 2) >> shift) & 2u) + Gfx_BufferIndex;
}

// One cell of the view, drawn into layer `layer` (0..0x36; under DIV-0041's
// grown ring -1..0x37).
void BuildCell(unsigned char* cell, int layer, int threshold) {
    if (cell[0] == 0 || cell[1] == 0) {
        map_layers::g.release_cell(cell);
        MapView_BuildFlags |= 1;
        return;
    }
    short* const s = reinterpret_cast<short*>(Prim_VertexScratch);
    s[0] = static_cast<short>((static_cast<std::uint32_t>(cell[0]) << 7) - 0x4040u);
    s[1] = static_cast<short>((static_cast<std::uint32_t>(cell[1]) << 7) - 0x4040u);
    unsigned char* corner = Corners() + (cell[1] * Width() + cell[0]) * 4u;
    MapView_CornerPtr = corner;
    s[2] = Height(corner[0]);
    map_layers::g.load_vertex(reinterpret_cast<const unsigned long*>(s));
    map_layers::g.rtps();
    map_layers::g.store_xy(reinterpret_cast<unsigned long*>(MapView_ScreenXY));
    const float sx = MapView_ScreenXY[0], sy = MapView_ScreenXY[1];
    // x87 fcomp as the original has it: an unordered x or y is culled too.
    // The x bounds are -50 / 370 unless the view is wide (DIV-0041,
    // src/game/widescreen.cpp, which sets them after every start-up fuzz).
    if (!(sx >= Widescreen_TerrainLo) || !(sx <= Widescreen_TerrainHi) || !(sy >= -200.0f) || !(sy <= 290.0f)) {
        map_layers::g.release_cell(cell);
        return;
    }

    std::uint32_t index = WordMapLayers(cell + 2) & 0xFFFu;
    unsigned char* item;
    if (index != 0) {
        item = Item(index);
        const unsigned pending = WordMapLayers(item + 0x36);
        if (pending != 0) {
            map_layers::g.cell_textures(cell[0], cell[1], item, pending - 1);
            SetWordMapLayers(item + 0x36, 0);
        }
    } else {
        const unsigned short got = map_layers::g.alloc();
        SetWordMapLayers(cell + 2, got);
        index = got;
        if (got != 0 && (WordMapLayers(Item(got) + 0x7E) != 0 || WordMapLayers(Item(got) + 0x8E) != 0)) {
            // Diagnostic (2026-10-07): a fresh item still carrying side words.
            static unsigned logged = 0;
            if (logged < 40) {
                ++logged;
                bof3::Log("map_layers: frame %u: cell %u,%u given item %u with stale side words next %u below %u",
                          (unsigned)Frame_Counter, (unsigned)cell[0], (unsigned)cell[1], got, WordMapLayers(Item(got) + 0x7E),
                          WordMapLayers(Item(got) + 0x8E));
            }
        }
        if (index == 0) {
            // Diagnostic only, no behaviour: the pool had no item for this cell,
            // so it is not drawn this frame - the owner's coast glitch of
            // 2026-09-27, which DIV-0062's larger pool answers; kept so a
            // recurrence names itself.
            static unsigned logged = 0;
            if (logged < 200) {
                ++logged;
                bof3::Log("map_layers: no draw item for cell %u,%u layer %d at frame %u", (unsigned)cell[0], (unsigned)cell[1],
                          layer, (unsigned)Frame_Counter);
            }
            return;
        }
        corner = MapView_CornerPtr;
        item = Item(index);
        // A side item for each neighbour whose near corners stand lower -
        // the next cell's (+0x7E), the one below's (+0x8E).
        if (S8MapLayers(corner[6]) < S8MapLayers(corner[3]) || S8MapLayers(corner[4]) < S8MapLayers(corner[1])) {
            SetWordMapLayers(item + 0x7E, map_layers::g.alloc());
            corner = MapView_CornerPtr;
        }
        const unsigned char* const below = corner + Width() * 4u;
        if (S8MapLayers(below[1]) < S8MapLayers(corner[3]) || S8MapLayers(below[0]) < S8MapLayers(corner[2])) SetWordMapLayers(item + 0x8E, map_layers::g.alloc());
        const unsigned flags = map_layers::g.cell_textures(cell[0], cell[1], item, Gfx_BufferIndex);
        SetWordMapLayers(cell + 2, WordMapLayers(cell + 2) | flags);
        SetWordMapLayers(item + 0x36, Gfx_BufferIndex == 0 ? 2 : 1);
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
    map_layers::g.load_vertices3(reinterpret_cast<const unsigned long*>(s + 4));
    unsigned char* const quad = Quad(index, Gfx_BufferIndex);
    map_layers::g.rtpt();

    const unsigned word = WordMapLayers(cell + 2);
    if (word & 0x8000u) {
        if (word & 0x4000u) {   // list 0 of the layer before (layer 0 for the first)
            // DIV-0041's grown ring: the first is layer -1, as the PSP's.
            const int before = map_rim::Active() ? (layer >= 0 ? layer - 1 : -1) : (layer != 0 ? layer - 1 : 0);
            map_layers::g.link(reinterpret_cast<unsigned long*>(LastOf(before, Gfx_BufferIndex)),
                   AsLong(item + Gfx_BufferIndex * 0x48u));
            LastOf(before, Gfx_BufferIndex) = AsLong(item + Gfx_BufferIndex * 0x48u);
        } else {                // list 1 of this layer
            map_layers::g.link(reinterpret_cast<unsigned long*>(LastOf(layer, 2u + Gfx_BufferIndex)),
                   AsLong(item + Gfx_BufferIndex * 0x48u));
            LastOf(layer, 2u + Gfx_BufferIndex) = AsLong(item + Gfx_BufferIndex * 0x48u);
        }
    } else if ((word & 0x4000u) && (Draw_OtSlot == 6 || (Draw_OtSlot == 4 && layer > threshold))) {
        // The draw table, keyed by layer and the cell's lowest corner (each
        // byte's sign flipped, so the lowest s8 is the smallest byte). Each
        // new minimum passes through SharedScratch's byte, as the original
        // keeps it there.
        auto* const scratch = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(::bof3::Img(SharedScratch_addr)));
        const unsigned char* const c = MapView_CornerPtr;
        *scratch = static_cast<unsigned char>(c[0] ^ 0x80);
        for (unsigned k = 1; k < 4; ++k) {
            const auto biased = static_cast<unsigned char>(c[k] ^ 0x80);
            if (*scratch > biased) *scratch = biased;
        }
        if (layer < 0) {
            // DIV-0041's grown ring: DrawTable's key holds the layer in a byte
            // above layer 0's, so layer -1's cells are map_rim's table.
            map_rim::ExtraTablePush(*scratch, WordMapLayers(cell + 2) & 0xFFFu);
        } else {
            const std::uint32_t key = ((static_cast<std::uint32_t>(layer) << 8) | *scratch) << 16;
            // Diagnostic only, no behaviour: the byte count wraps at 256 and the
            // table's first entries are overwritten (the coast, 2026-09-27).
            if (DrawTable_Count == 0xFF) {
                static unsigned logged = 0;
                if (logged < 100) {
                    ++logged;
                    bof3::Log("map_layers: draw table full (256) at frame %u, cell %u,%u layer %d", (unsigned)Frame_Counter,
                              (unsigned)cell[0], (unsigned)cell[1], layer);
                }
            }
            DrawTable[DrawTable_Count] = key | (WordMapLayers(cell + 2) & 0xFFFu);
            DrawTable_Count = static_cast<unsigned char>(DrawTable_Count + 1);
            ++g_table_adds;
        }
    } else {                    // list 0 of this layer
        map_layers::g.link(reinterpret_cast<unsigned long*>(LastOf(layer, Gfx_BufferIndex)), AsLong(item + Gfx_BufferIndex * 0x48u));
        LastOf(layer, Gfx_BufferIndex) = AsLong(item + Gfx_BufferIndex * 0x48u);
    }
    {
        unsigned char* const mine = item + Gfx_BufferIndex * 0x48u;
        map_layers::g.store_xy3(reinterpret_cast<unsigned long*>(mine + 0x18), reinterpret_cast<unsigned long*>(mine + 0x28),
                    reinterpret_cast<unsigned long*>(mine + 0x38));
    }
    map_layers::g.depths4(item + Gfx_BufferIndex * 0x48u);

    // The side triangles: the next cell's edge (+0x7E) and the one below's
    // (+0x8E), each from two corners of the neighbour and the quad's own
    // vertex 2, left in the scratch.
    float unused;
    const unsigned next_item = WordMapLayers(item + 0x7E);
    if (draw_order::Tagging() && (next_item != 0 || WordMapLayers(item + 0x8E) != 0)) {
        const signed char* const c = reinterpret_cast<const signed char*>(MapView_CornerPtr.Native());
        bof3::Log("map_layers: cell %u,%u layer %d item %u: next side %u, below side %u; corners %d %d %d %d; screen (%g, %g)",
                  (unsigned)cell[0], (unsigned)cell[1], layer, index, next_item, WordMapLayers(item + 0x8E), c[0], c[1], c[2], c[3],
                  MapView_ScreenXY[0], MapView_ScreenXY[1]);
    }
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
        map_layers::g.load_vertices3(reinterpret_cast<const unsigned long*>(s));
        map_layers::g.link(reinterpret_cast<unsigned long*>(LastOf(layer, SideSub(cell, 12))), AsLong(side));
        map_layers::g.rtpt();
        LastOf(layer, SideSub(cell, 12)) = AsLong(side);
        map_layers::g.store_xy3(reinterpret_cast<unsigned long*>(side + 0x38), reinterpret_cast<unsigned long*>(side + 0x28),
                    reinterpret_cast<unsigned long*>(MapView_ScreenXY));
        map_layers::g.depth_f3(reinterpret_cast<float*>(side + 0x40), reinterpret_cast<float*>(side + 0x30), &unused);
    }
    const unsigned below_item = WordMapLayers(item + 0x8E);
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
        map_layers::g.load_vertices3(reinterpret_cast<const unsigned long*>(s));
        map_layers::g.link(reinterpret_cast<unsigned long*>(LastOf(layer, SideSub(cell, 11))), AsLong(side));
        map_layers::g.rtpt();
        LastOf(layer, SideSub(cell, 11)) = AsLong(side);
        map_layers::g.store_xy3(reinterpret_cast<unsigned long*>(side + 0x28), reinterpret_cast<unsigned long*>(side + 0x38),
                    reinterpret_cast<unsigned long*>(MapView_ScreenXY));
        map_layers::g.depth_f3(reinterpret_cast<float*>(side + 0x30), reinterpret_cast<float*>(side + 0x40), &unused);
    }
}

constexpr auto& gMapScroll = map_scroll::g;

unsigned WordMapScroll(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void SetWordMapScroll(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

// Ring cell (row, col), computed as the original does - in 32 bits, unchecked.
unsigned char* CellMapScroll(int row, int col) {
    return MapView_CellItems + static_cast<std::uint32_t>(col + row * 0x1C) * 4u;
}

// The next ring row / column after r: 0x37 / 0x1B wrap to 0, anything else
// (an out-of-range head included) plus one.
int NextRow(int r) { return r == 0x37 ? 0 : r + 1; }

int NextColumn(int c) { return c == 0x1B ? 0 : c + 1; }

void OriginStep(int dx, int dy) {
    MapView_Origin[0] = static_cast<short>(MapView_Origin[0] + dx);
    MapView_Origin[1] = static_cast<short>(MapView_Origin[1] + dy);
}

std::uint32_t AddressMoveCmds(const volatile void* p) { return Low32(p); }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

unsigned char* Kind2() { return rv::Bytes(&Sprite_Kind2); }

constexpr unsigned kTexPrimBytes = 0x48, kTexPrims = 6;

std::uint32_t UScenaSx2(std::int32_t v) { return static_cast<std::uint32_t>(v); }

unsigned char* AtWorldMap(world_map::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

world_map::U WordWorldMap(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

std::int32_t ShortWorldMap(const unsigned char* p) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(WordWorldMap(p))); }

world_map::U LongWorldMap(const unsigned char* p) {
    world_map::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

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
        map_layers::g.frame_bd();
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
        map_layers::g.column_next();
        MapView_ScrollX = static_cast<unsigned short>(MapView_ScrollX - 0x200);
        MapView_Redraw = 2;
        shifted = true;
    }
    if (static_cast<short>(MapView_ScrollX) <= -0x200) {
        map_layers::g.column_prev();
        MapView_ScrollX = static_cast<unsigned short>(MapView_ScrollX + 0x200);
        MapView_Redraw = 2;
        shifted = true;
    }
    if (static_cast<short>(MapView_ElevationOffset) >= 0x200) {
        map_layers::g.rows_next();
        MapView_ElevationOffset = static_cast<unsigned short>(MapView_ElevationOffset - 0x200);
        MapView_Redraw = 2;
        shifted = true;
    }
    if (static_cast<short>(MapView_ElevationOffset) <= -0x200) {
        map_layers::g.rows_prev();
        MapView_ElevationOffset = static_cast<unsigned short>(MapView_ElevationOffset + 0x200);
        MapView_Redraw = 2;
        map_layers::g.place_runs();
    } else if (shifted) {
        map_layers::g.place_runs();
    }

    // The camera: redraw everything when an angle moved.
    auto* const angles = reinterpret_cast<unsigned char*>(Camera_Angles);
    auto* const drawn = reinterpret_cast<unsigned char*>(Camera_AnglesDrawn);
    const std::uint32_t angles_xy = Dword(angles), angles_z = Dword(angles + 4);
    if (WordMapLayers(angles) != WordMapLayers(drawn) || WordMapLayers(angles + 2) != WordMapLayers(drawn + 2) || WordMapLayers(angles + 4) != WordMapLayers(drawn + 4)) {
        MapView_Redraw = 3;
        SetDword(drawn, angles_xy);
        SetDword(drawn + 4, angles_z);
    }
    map_layers::g.rot_matrix(Camera_Angles, Camera_Matrix);
    short focus[3];
    focus[0] = static_cast<short>(static_cast<std::uint32_t>(MapView_FocusX) >> 1);
    focus[1] = static_cast<short>(static_cast<std::uint32_t>(MapView_FocusZ) >> 1);
    focus[2] = static_cast<short>(static_cast<std::uint32_t>(MapView_Elevation) >> 1);
    long moved[3];
    map_layers::g.apply_matrix(Camera_Matrix, focus, moved);
    auto* const translation = reinterpret_cast<unsigned char*>(Camera_Matrix) + 0x14;
    SetDword(translation, static_cast<std::uint32_t>(moved[0]) + static_cast<std::uint32_t>(static_cast<int>(Camera_ShiftX)));
    SetDword(translation + 4,
             static_cast<std::uint32_t>(moved[1]) + static_cast<std::uint32_t>(static_cast<int>(Camera_ShiftY)));
    SetDword(translation + 8, static_cast<std::uint32_t>(moved[2]) +
                                  static_cast<std::uint32_t>(static_cast<int>(Camera_Distance)) + 0x1194u);
    map_layers::g.set_rot(reinterpret_cast<const unsigned long*>(Camera_Matrix));
    map_layers::g.set_trans(reinterpret_cast<const unsigned long*>(Camera_Matrix));
    if (MapView_Redraw != 0) {
        map_layers::g.build();
        --MapView_Redraw;
    }
}

// original 0x56E9A0: the view one map cell back along x (PSX FUN_80153758).
// The ring slot at the head column is refilled with view column -1, then the
// head steps back onto it.
//
// As the original has it: the head row is read once and the head column
// again for every cell; the rows run from the one after the head, 0x37
// wrapping to 0; CellToMap is given the cell's position in the walk (0..0x37)
// as its row, not the ring row.
extern "C" void __cdecl MapView_ShiftColumnPrev(void) {
    int row = MapView_Row;
    for (int i = 0; i < 0x38; ++i) {
        row = NextRow(row);
        unsigned char* const cell = CellMapScroll(row, MapView_Column);
        gMapScroll.release_cell(cell);
        gMapScroll.cell_to_map(i, -1, cell);
    }
    if (MapView_Column == 0) {
        OriginStep(-1, 1);
        MapView_Column = 0x1B;
    } else {
        MapView_Column = static_cast<short>(MapView_Column - 1);
        OriginStep(-1, 1);
    }
    // DIV-0041 (wide-ring): the rim follows, on the new origin (map_rim.h).
    if (map_rim::Active()) map_rim::RimShift(map_rim::Shift::kColumnPrev);
}

// original 0x56EA30: the view one map cell on along x (PSX FUN_80153858).
// The head column advances, and its slot is refilled with view column 0x1C -
// the one past the last.
//
// As the original has it: both heads are read once, before the walk, and the
// new column is stored only after it.
extern "C" void __cdecl MapView_ShiftColumnNext(void) {
    const int column = NextColumn(MapView_Column);
    int row = MapView_Row;
    for (int i = 0; i < 0x38; ++i) {
        row = NextRow(row);
        unsigned char* const cell = CellMapScroll(row, column);
        gMapScroll.release_cell(cell);
        gMapScroll.cell_to_map(i, 0x1C, cell);
    }
    OriginStep(1, -1);
    MapView_Column = static_cast<short>(column);
    if (map_rim::Active()) map_rim::RimShift(map_rim::Shift::kColumnNext);   // DIV-0041 (wide-ring)
}

// original 0x56EAB0 (PSX 0x80153950): the view one map cell back along x and y - two ring rows
// (PSX FUN_80153950). Twice: the slot at the head row is refilled with view
// row -2, then -3, and the head steps back onto it.
//
// As the original has it: the head column is read once a pass, the head row
// again for every cell.
extern "C" void __cdecl MapView_ShiftRowsPrev(void) {
    for (int pass = 0; pass < 2; ++pass) {
        int column = MapView_Column;
        const int view_row = pass == 0 ? -2 : -3;
        for (int j = 0; j < 0x1C; ++j) {
            column = NextColumn(column);
            unsigned char* const cell = CellMapScroll(MapView_Row, column);
            gMapScroll.release_cell(cell);
            gMapScroll.cell_to_map(view_row, j, cell);
        }
        if (MapView_Row == 0) MapView_Row = 0x37;
        else MapView_Row = static_cast<short>(MapView_Row - 1);
    }
    OriginStep(-1, -1);
    if (map_rim::Active()) map_rim::RimShift(map_rim::Shift::kRowsPrev);   // DIV-0041 (wide-ring)
}

// original 0x56EB50 (PSX 0x80153A6C): the view one map cell on along x and y - two ring rows
// (PSX FUN_80153A6C). Twice: the head row advances and its slot is refilled
// with view row 0x38, then 0x39.
//
// As the original has it: the head row is read from memory for the first
// pass only - the second advances the value the first stored; the head column
// is read once a pass.
extern "C" void __cdecl MapView_ShiftRowsNext(void) {
    unsigned short head = static_cast<unsigned short>(MapView_Row);
    for (int pass = 0; pass < 2; ++pass) {
        const int row = head == 0x37 ? 0 : static_cast<short>(head) + 1;
        int column = MapView_Column;
        for (int j = 0; j < 0x1C; ++j) {
            column = NextColumn(column);
            unsigned char* const cell = CellMapScroll(row, column);
            gMapScroll.release_cell(cell);
            gMapScroll.cell_to_map(0x38 + pass, j, cell);
        }
        head = static_cast<unsigned short>(row);
        MapView_Row = static_cast<short>(head);
    }
    OriginStep(1, 1);
    if (map_rim::Active()) map_rim::RimShift(map_rim::Shift::kRowsNext);   // DIV-0041 (wide-ring)
}

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
//   - the draw-table key's minimum passes through SharedScratch's byte, and
//     a cell's corner pointer through MapView_CornerPtr, both globals;
//   - the side items are allocated only for a new item, and an item's +0x7E
//     and +0x8E are otherwise used as they stand;
//   - an existing item's textures are refreshed for buffer (+0x36) - 1 and
//     +0x36 cleared; a new item's +0x36 is 2 on buffer 0, 1 on any other,
//     so the next frame refreshes buffer 1 either way;
//   - a failed allocation leaves the cell word 0 and draws nothing, the cell
//     released by no one.
extern "C" void __cdecl MapView_Build(void) {
    map_rim::NoteOwnView(false);   // the ring's view again (issue 185)
    map_layers::g.layers_reset();
    const int origin_y = MapView_Origin[1], origin_x = MapView_Origin[0];
    const int kind2_z = static_cast<short>(static_cast<std::uint32_t>(Field_Kind2Z) >> 16);
    const int kind2_x = static_cast<short>(static_cast<std::uint32_t>(Field_Kind2X) >> 16);
    MapView_BuildFlags &= 0xFE;
    DrawTable_Count = 0;
    const int threshold = kind2_z - origin_y - origin_x + kind2_x + 8;
    std::uint32_t original = 0;
    std::uint32_t inset = Inset(&original);
    if (map_rim::Active()) {
        // DIVERGENCE DIV-0041 (wide-ring, docs/wide-ring.md): the PSP's grown
        // ring, rows -1..55 (its 0..56) and grown columns g .. 32 - g, g =
        // max(0, i + 2 - L) - today's walk plus one column, into the rim
        // where today's would be clipped at 0 (owner's answer 3,
        // 2026-10-21). Capcom's window cells are walked in Capcom's slots, in
        // its order within a row (ascending column); Scratch_Swap is not
        // re-read between cells, the walk being ours.
        g_table_adds = 0;
        const int lowered_by = static_cast<int>(Widescreen_InsetColumns());
        int walk = static_cast<int>(original) + 2 - lowered_by;
        walk = walk < 0 ? 0 : walk > 16 ? 16 : walk;
        map_rim::NoteWalk(static_cast<unsigned>(walk), MapView_Inset);
        for (int grown_row = 0; grown_row < map_rim::kRows - 1; ++grown_row)
            for (int grown_column = walk; grown_column < map_rim::kColumns - walk; ++grown_column)
                BuildCell(map_rim::Cell(grown_row, grown_column), grown_row - 1, threshold);
        map_rim::ExtraTableSort();
        map_layers::g.sort();
        map_rim::NoteBuildPeaks(g_table_adds);
        return;
    }
    int row = MapView_Row;
    for (unsigned layer = 0; layer < 0x37; ++layer) {
        row = row == 0x37 ? 0 : row + 1;
        auto column = static_cast<int>(static_cast<std::uint32_t>(MapView_Column) + inset);
        if (column >= 0x1C) column -= 0x1C;
        for (int k = 0; k < Columns(inset);) {
            column = column == 0x1B ? 0 : column + 1;
            BuildCell(MapView_CellItems + static_cast<std::uint32_t>(column + row * 28) * 4u, static_cast<int>(layer),
                      threshold);
            inset = static_cast<std::uint32_t>(Scratch_Swap);
            ++k;
        }
    }
    map_layers::g.sort();
}

// original 0x56F5B0 (PSX 0x80154780). Empties two of each layer's three draw lists, for the
// display buffer being built: first = null, last = the head itself, so that
// the first append lands in `first` through `last` like any other.
//
// As the original has it: 0x38 layers where 0x593060 walks 0x37, the third
// list of each layer left alone, and the buffer index taken as a whole byte.
extern "C" void __cdecl DrawLayers_Reset(void) {
    auto* layer = reinterpret_cast<unsigned char*>(DrawLayers) + Gfx_BufferIndex * 8u;
    for (unsigned i = 0; i < kLayers; ++i, layer += kLayerBytes) {
        for (unsigned char* head = layer; head != layer + 0x20; head += 0x10) {
            const std::uint32_t none = 0, self = Low32(head);
            std::memcpy(head, &none, 4);
            std::memcpy(head + 4, &self, 4);
        }
    }
    // DIV-0041 (wide-ring): the grown ring's layer -1 record and its table
    // too, for every caller (AreaMapBD_BuildView's area among them).
    if (map_rim::Active()) map_rim::ExtraReset(Gfx_BufferIndex);
}

// original 0x56F5F0 (PSX 0x801547F0). Orders the draw table by layer and then key - the high 16
// bits of each entry - for 0x593060 to merge with the sprite list.
//
// As the original has it: an exchange sort, each entry against every later
// one, so entries with equal keys do not keep their order; and every exchange
// leaves the value it moved up in Scratch_Swap, the game's all-purpose swap
// temporary, where a memory comparison would miss it if we did not.
extern "C" void __cdecl DrawTable_Sort(void) {
    const unsigned count = DrawTable_Count;
    for (unsigned i = 0; i < count; ++i) {
        for (unsigned j = i + 1; j < count; ++j) {
            const unsigned long a = DrawTable[i], b = DrawTable[j];
            if ((a & 0xFFFF0000u) <= (b & 0xFFFF0000u)) continue;
            DrawTable[i] = b;
            Scratch_Swap = a;
            DrawTable[j] = a;
        }
    }
}

// original 0x56F670: the view set up for an area (PSX func_0x801548A4) -
// called by 0x594E60 at area set-up, and by 43 other call sites (Kind2_Script
// among them).
//
// As the original has it:
//   - the draw items' quads are initialised two by two up to MoveScript_Object
//     - all 0x800 of them - each word +0x36 and +0x46 cleared after its two
//     calls;
//   - Camera_AnglesDrawn takes both dwords of the angles as just written, the
//     word after the three angles too;
//   - the elevation is the s16 of AreaMap_Elevation's result, and 0 without a
//     call when Field_InputFlags bit 3 is set; the build flags are the input
//     flags read after that call, shifted left 7 - the whole byte;
//   - each ring cell's word is cleared before MapView_CellToMap fills its
//     position; the ring's draw items are not released (the pool is reset
//     after);
//   - the Kind2 position is read once, before the elevation call.
extern "C" void __cdecl Field_ViewReset(void) {
    gMapScroll.geom_screen(0x3E8);
    gMapScroll.geom_offset(0xA0, 0x78);
    gMapScroll.back_color(0x78, 0x78, 0x78);
    // Every item's two halves - 0x800 of them in the original; DIV-0062's
    // pool has twice as many, and a half never primed here draws as garbage.
    for (unsigned q = 0; q < draw_pool::Count() * 2; ++q) {
        unsigned char* const quad = draw_pool::Items() + q * 0x48u;
        gMapScroll.set_poly_ft4(quad);
        gMapScroll.set_shade_tex(quad, 0);
        SetWordMapScroll(quad + 0x36, 0);
        SetWordMapScroll(quad + 0x46, 0);
    }
    Camera_Angles[0] = static_cast<short>(0xFD56);
    Camera_Angles[1] = 0;
    auto* const angles = reinterpret_cast<unsigned char*>(Camera_Angles);
    auto* const drawn = reinterpret_cast<unsigned char*>(Camera_AnglesDrawn);
    SetDword(drawn, Dword(angles));
    const auto kind2_z = static_cast<std::int32_t>(Field_Kind2Z);
    const auto kind2_x = static_cast<std::int32_t>(Field_Kind2X);
    SetWordMapScroll(angles + 4, 0x200);   // Cond_AngleFB's low word, the yaw
    SetDword(drawn + 4, Dword(angles + 4));
    // Draw item 0 - the "none" index - has its eight screen points cleared.
    for (const unsigned at : {0x08u, 0x0Cu, 0x18u, 0x1Cu, 0x28u, 0x2Cu, 0x38u, 0x3Cu, 0x50u, 0x54u, 0x60u, 0x64u, 0x70u,
                              0x74u, 0x80u, 0x84u})
        SetDword(draw_pool::Items() + at, 0);
    MapView_FocusX = static_cast<long>(0x7FFF - (kind2_x >> 8));
    MapView_FocusZ = static_cast<long>(0x8000 - (kind2_z >> 8));
    MapView_Origin[1] = static_cast<short>((kind2_z >> 16) + 3);
    Camera_Distance = 0;
    Camera_ShiftY = 0;
    Camera_ShiftX = 0;
    MapView_Origin[0] = static_cast<short>((kind2_x >> 16) - 0x18);
    unsigned char flags = Field_InputFlags;
    std::int32_t elevation = 0;
    if (flags & 8) {
        MapView_Elevation = 0;
    } else {
        const long got = gMapScroll.elevation(kind2_x, kind2_z);
        flags = Field_InputFlags;
        elevation = static_cast<short>(got);
        MapView_Elevation = elevation;
    }
    MapView_BuildFlags = static_cast<unsigned char>(flags << 7);
    const unsigned short fixed = AreaMap_Word1E;
    DrawTable_Count = 0;
    if (fixed != 0) MapView_ElevationOffset = fixed;
    else MapView_ElevationOffset = static_cast<unsigned short>(static_cast<std::uint32_t>(-elevation) << 1);
    const std::uint32_t bytes = AreaMap_BytesBase;
    MapView_ScrollX = 0;
    MoveScript_F3Divisor = 0;
    MoveScript_FAWord = 0;
    AreaMap_Bytes = AreaMap_Header + bytes * 4u;
    MapView_HeightScale = 0;
    Cond_ByteFE = 0;
    MapView_Redraw = 3;
    for (int row = 0; row < 0x38; ++row) {
        for (int column = 0; column < 0x1C; ++column) {
            unsigned char* const cell = CellMapScroll(row, column);
            SetWordMapScroll(cell + 2, 0);
            gMapScroll.cell_to_map(row, column, cell);
        }
    }
    // DIV-0041 (wide-ring): the rim filled, no items (the pool is reset next).
    if (map_rim::Active()) map_rim::RimFill(false);
    for (unsigned i = 0; i < draw_pool::Count(); ++i) draw_pool::Free()[i] = static_cast<unsigned short>(i);   // DIV-0062: the pool's size
    DrawItemPool_Top = 1;
    MapView_Column = 0x1B;
    MapView_Row = 0x37;
    gMapScroll.bake_patches();
    gMapScroll.setup_entries();
    gMapScroll.place_runs();
}

// original 0x56F910 (PSX 0x80154C7C). Which map cell the view's cell (row, col) shows, as two
// bytes to out - or 0, 0 for none. The view is isometric: going down a row
// moves half a cell in x and in y, going along a column one cell in x and
// back one in y.
//
// As the original has it: the outermost ring of the map never shows (0 < x <
// width - 1), a cell whose word is 0 does not show, and "none" and the map
// cell (0, 0) are the same answer - which costs nothing, (0, 0) being in the
// ring. Only the low bytes of x and y are kept; with a width and height of one
// byte each, nothing is lost.
extern "C" void __cdecl MapView_CellToMap(int row, int col, unsigned char* out) {
    const int x = MapView_Origin[0] + col + (row + 1) / 2;
    const int y = MapView_Origin[1] + row / 2 - col;
    const int width = AreaMap_Header[0], height = AreaMap_Header[1];
    const unsigned offset = static_cast<unsigned>(AreaMap_Header[2] | (AreaMap_Header[3] << 8));
    if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
        const auto* cells = reinterpret_cast<const unsigned short*>(AreaMap_Header);
        if (cells[static_cast<unsigned>(x + width * y) + offset * 2] != 0) {
            out[0] = static_cast<unsigned char>(x);
            out[1] = static_cast<unsigned char>(y);
            return;
        }
    }
    out[0] = 0;
    out[1] = 0;
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
    const unsigned offset = WordMapLayers(AreaMap_Header + 2);
    const unsigned tile = WordMapLayers(AreaMap_Header + (x + width * y + offset * 2u) * 2u);
    if (tile == 0) return 0;
    const auto height = static_cast<std::int32_t>((header >> 8) & 0xFFu);
    const std::int32_t half = (height * static_cast<std::int32_t>(width) + 1) / 2;
    const unsigned char* texture =
        AreaMap_Header + (static_cast<std::uint32_t>(half) + offset + tile) * 4u;
    // DIV-0204: the desert entrance's cell takes the drawn ring's word for its
    // top (faerie_ring.h); its sides, and every other cell, the map's own.
    std::uint32_t top = Dword(texture);
    FaerieRing_CellWord(x, y, &top);
    unsigned flags = ((static_cast<std::int32_t>(top) >> 15) & 0x8000) | (top & 0x4000u);
    map_layers::g.set_texture(top, item + buffer * 0x48u, 1);
    texture += 4;
    const unsigned below = WordMapLayers(item + 0x8E);
    if (below != 0) {
        if (Dword(texture) == 0 && map_layers::g_side_zero_release) {
            // BOF3X_SIDE_ZERO=1, an experiment (2026-10-07, off by default): a
            // below side the map gives word 0 is released like the +0x7E one;
            // the original draws it with that word. Not the owner's streaks
            // (known-defects.md D239) - kept for the next reading of them.
            if (draw_order::Tagging())
                bof3::Log("map_layers: cell %u,%u: the below side's texture word is 0, released", x, y);
            map_layers::g.release(static_cast<unsigned short>(below));
            SetWordMapLayers(item + 0x8E, 0);
            texture += 4;
        } else {
            if (draw_order::Tagging())
                bof3::Log("map_layers: cell %u,%u: below side item %u texture word %08X (cell's %08X)", x, y, below,
                          Dword(texture), Dword(texture - 4));
            map_layers::g.set_texture(Dword(texture), Quad(below, buffer), 1);
            flags |= (static_cast<std::int32_t>(Dword(texture)) >> 18) & 0x1000;
            texture += 4;
        }
    }
    const unsigned next = WordMapLayers(item + 0x7E);
    if (map_layers::g_side_dup && below != 0 && next != 0 && x < width && y < static_cast<std::uint32_t>(height)) {
        // DIV-0085 (D239): the tile carries a word for the one side the file's
        // heights give this cell and no third; the second side exists only
        // because the heights moved since (AREA060's sky effect), and the
        // dword the original would give it is the next tile's own. Both
        // sides take the one side word.
        const unsigned file = map_layers::g_file_sides[y * width + x];
        if (file == 1 || file == 2) {
            texture -= 4;
            static unsigned logged = 0;
            if (logged < 20) {
                ++logged;
                bof3::Log("DIV-0085    map cell %u,%u (file sides: %s only) created with both side faces: both drawn "
                          "with %08X, not %08X", x, y, file == 1 ? "south" : "east", Dword(texture),
                          Dword(texture + 4));
            }
        }
    }
    if (next != 0) {
        if (draw_order::Tagging())
            bof3::Log("map_layers: cell %u,%u: next side item %u texture word %08X", x, y, next, Dword(texture));
        if (Dword(texture) == 0) {
            map_layers::g.release(static_cast<unsigned short>(next));
            SetWordMapLayers(item + 0x7E, 0);
            return flags;
        }
        map_layers::g.set_texture(Dword(texture), Quad(next, buffer), 1);
        flags |= (static_cast<std::int32_t>(Dword(texture)) >> 17) & 0x2000;
    }
    return flags;
}

// original 0x56FAD0: the area's always-on patches baked into its texture run
// (PSX FUN_80154EF4). The patch list from AreaMap_PatchBase: entries of a
// header dword - an Area_TestCondition code in the low word, a length n in
// the high word - and n dwords of 3-dword records. An entry whose code is
// 0x8001 (kind 0x80, bit 0: always true) becomes 0x8000 (always false), and
// if its first record's top byte is below 2 each record's second dword is
// written into the texture run at the record's cell's tile plus (r >> 16) &
// 0xF, the run MapView_CellTextures reads.
//
// As the original has it: every value is read afresh for each record - the
// width, height and offset, the tile, the record, and the entry's length
// after the write - so a record that patches its own list is seen; a length
// not a multiple of 3 writes the whole last record; the third dword of a
// record is not read; the next entry is n + 1 dwords on, the length read
// after the entry is done (a length of 0 steps one dword); nothing is bounded
// (read, not run - the fuzz keeps every write inside its block).
extern "C" void __cdecl AreaMap_BakePatches(void) {
    unsigned char* entry = AreaMap_Header + static_cast<std::uint32_t>(AreaMap_PatchBase) * 4u;
    std::uint32_t d = Dword(entry);
    while (d != 0) {
        if ((d & 0xFFFFu) == 0x8001u) {
            const std::uint32_t first = Dword(entry + 4);
            const std::uint32_t cleared = Dword(entry) & ~1u;
            SetDword(entry, cleared);
            if ((first >> 24) < 2 && (cleared >> 16) != 0) {
                const unsigned char* record = entry + 4;
                std::int32_t k = 0;
                do {
                    const std::uint32_t r = Dword(record);
                    const std::uint32_t width = AreaMap_Header[0];
                    const std::uint32_t offset = WordMapScroll(AreaMap_Header + 2);
                    const std::uint32_t cell = (r & 0xFFu) * width + ((r >> 8) & 0xFFu) + offset * 2u;
                    record += 0xC;
                    const std::uint32_t tile = WordMapScroll(AreaMap_Header + cell * 2u);
                    const auto area = static_cast<std::int32_t>(AreaMap_Header[1] * width + 1u);
                    const std::uint32_t slot =
                        ((r >> 16) & 0xFu) + static_cast<std::uint32_t>(area / 2) + offset + tile;
                    SetDword(AreaMap_Header + slot * 4u, Dword(record - 8));
                    k += 3;
                } while (k < static_cast<std::int32_t>(Dword(entry) >> 16));
            }
        }
        const std::uint32_t step = Dword(entry) >> 16;
        d = Dword(entry + step * 4u + 4u);
        entry += step * 4u + 4u;
    }
}

// original 0x56FBD0 (PSX 0x80155028). The next free draw-item index, or 0 for none: index 0 is
// never in play, the pool starts with top = 1.
//
// As the original has it: top wraps from 0x3FF to 0, and 0 reads as "none
// left" from then until something is released.
extern "C" unsigned short __cdecl DrawItemPool_Alloc(void) {
    const unsigned short top = DrawItemPool_Top;
    if (top == 0) return 0;
    const unsigned short index = draw_pool::Free()[top];
    DrawItemPool_Top = static_cast<unsigned short>((top + 1) & draw_pool::Mask());
    return index;
}

// original 0x56FC00 (PSX 0x80155070). Gives back everything a cell of the view holds: its own
// draw item, and the two further items that one may own (the words at +0x7E
// and +0x8E of it). The hottest function of the attract run - 863,659 calls -
// because the view's 1,568 cells are all put through it whenever the view is
// rebuilt, nearly all of them holding nothing.
//
// As the original has it: the index is taken as 12 bits of the cell's word
// though the pool has 10, and the word is then zeroed whole.
extern "C" void __cdecl DrawItemPool_ReleaseCell(unsigned char* cell) {
    std::uint16_t word;
    std::memcpy(&word, cell + 2, sizeof word);
    const unsigned index = word & 0xFFFu;
    if (index == 0) return;
    DrawItemPool_Release(static_cast<unsigned short>(index));
    std::memset(cell + 2, 0, 2);
    unsigned char* const item = draw_pool::Items() + index * 0x90;
    for (const unsigned at : {0x7Eu, 0x8Eu}) {
        std::uint16_t owned;
        std::memcpy(&owned, item + at, sizeof owned);
        if (owned == 0) continue;
        DrawItemPool_Release(owned);
        std::memset(item + at, 0, 2);
    }
}

// original 0x56FC70 (PSX 0x80155120). Gives an index back.
//
// As the original has it: no check that the pool is not already full - with
// top == 1 this stores at entry 0, with top == 0 at entry 0x3FF - and the new
// top is what is left in eax. No caller read so far uses that; it is returned
// because it is free to.
extern "C" unsigned __cdecl DrawItemPool_Release(unsigned short index) {
    const unsigned top = (DrawItemPool_Top - 1u) & draw_pool::Mask();
    DrawItemPool_Top = static_cast<unsigned short>(top);
    draw_pool::Free()[top] = index;
    return top;
}

// original 0x56FCA0: the view's cell items refilled (PSX 0x80155154,
// pairs_propagated "call"). From MapView_Row + 1 and, each of the 56 rows,
// MapView_Column + 1 (read again each row), both wrapping (row 0x37 -> 0,
// column 0x1B -> 0; a value outside the table is not wrapped and runs past
// it): MapView_CellToMap(row index, column index, &MapView_CellItems[row *
// 28 + column]) for each of 28 columns; then MapView_PlaceRuns.
extern "C" void __cdecl MapView_FillCells(void) {
    std::int32_t row = MapView_Row;
    for (int i = 0; i < 0x38; ++i) {
        row = row == 0x37 ? 0 : row + 1;
        std::int32_t col = MapView_Column;
        for (int j = 0; j < 0x1C; ++j) {
            col = col == 0x1B ? 0 : col + 1;
            const std::uint32_t cell = Low32(MapView_CellItems) + static_cast<std::uint32_t>(row * 28 + col) * 4;
            SH_CALL(MapView_CellToMap)(i, j, move_script::At(cell));
        }
    }
    // DIV-0041 (wide-ring): the rim refilled in place, its items kept, as Capcom's.
    if (map_rim::Active()) map_rim::RimFill(true);
    SH_CALL(MapView_PlaceRuns)();
}

// original 0x56FD20: before a layer's sprites, in the draw-order pass - the
// layer's first list, if it has one, appended to ordering-table slot 6, then
// one row of the field view's map cells handed to their handlers.
//
// The row is (MapView_Row + layer + 1) mod 0x38 of MapView_Cells, 28 words
// wide. From (MapView_Column + inset + (1 if the row is even)) mod 0x1C it
// takes (14 - inset) * 2 columns, each advanced BEFORE it is read and wrapping
// 0x1B to 0; the inset is MapView_Inset, less 2 (not below 0) when bit 0 of
// Field_InputFlags is set. A non-zero word w is a run of records at dword
// w + (AreaMap_CellBase & 0xFFFF) of the area block, headed by the dword
// before it: the run's length in dwords plus one in its high half, and two
// bytes each handler is given. Each record goes to MapCell_Handlers[its top
// byte] and the walk moves on by its byte +2 in dwords - read after the call.
//
// As the original has it: each "mod" is one subtraction, so a row or column
// word outside its range is not brought back into it; a record's top byte is
// not checked against the 77 handlers; a step of 0, or a run whose steps pass
// its end, never stops.
extern "C" void __cdecl DrawLayer_Open(int layer) {
    // DIV-0041 (wide-ring): the grown ring's walk when the view is wide.
    if (map_rim::Drawn()) {
        OpenGrown(layer);
        return;
    }
    LinkFirst(layer);
    if (!CapcomRow(layer)) return;
    wide_rand::SetBand(false);
    if (draw_order::g_on) draw_order::Clear();
}

// original 0x56FE80: after a layer's sprites, in the draw-order pass - a
// draw-mode primitive (texture page 0x95, nothing else set) committed to slot
// 6, then the layer's second list, if it has one, appended to the slot
// Draw_OtSlot names. 403,315 calls.
//
// DIV-0041 (wide-ring): layer -1, the grown ring's row above Capcom's, is
// map_rim's record of the same shape; Capcom's layers never go below 0.
extern "C" void __cdecl DrawLayer_Close(int layer) {
    Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, 0x95, 0);
    Gfx_CommitPrim(6, 0xC);
    const std::uint32_t first = static_cast<std::uint32_t>(layer) * 6u;
    const auto at = [&]() -> unsigned long* {
        if (layer < 0) return map_rim::ExtraLayer() + kSecondList + Gfx_BufferIndex * 2u;
        return DrawLayers + kSecondList + (Gfx_BufferIndex + first) * 2u;
    };
    unsigned long* cell = at();
    if (cell[0] == 0) return;
    if (draw_order::g_on) draw_order::TagList(cell[0], cell[1], draw_order::kList1, static_cast<unsigned>(layer), Draw_OtSlot);
    Gpu_LinkPrim(Gfx_OtPointers[Draw_OtSlot], cell[0]);
    // The index is read again after the call, as the original reads it.
    cell = at();
    Gfx_OtPointers[Draw_OtSlot] = reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(cell[1]));
}

// original 0x56FF00 (PSX 0x80156938): a record's condition. The high byte of the u16 `code`
// is the kind, the low byte its operand:
//   0x00..0x1F   bit (operand) of Cond_Flags + 8 * kind
//   0x20..0x3F   the same bit of row kind & 0x1F, complemented
//   0xFF         (code & 1) XOR Cond_ByteFF - the whole byte, not 0 or 1
//   0xFE / 0xFD  operand == / != Cond_ByteFE / Cond_ByteFD
//   0xFC         (Game_Mode == 7 or Field_Request == 4) XOR (code & 1)
//   0xFB         ((Cond_AngleFB - 0x200) & 0xFFF) <= 0x800, XOR (code & 1)
//   0xFA         operand == Cond_ByteFA sign-extended, so never at 0x80 and up
//   anything else, 0x40 included, code & 1.
// Only the low 16 bits are read: the callers push ax with stale bits above.
// The value is al; two callers widen it with movsx, so all eight bits count.
extern "C" unsigned char __cdecl Area_TestCondition(unsigned long code) {
    const unsigned kind = code & 0xFF00u, operand = code & 0xFFu, low = code & 1u;
    switch (kind) {
        case 0x4000: return static_cast<unsigned char>(low);
        case 0xFF00: return static_cast<unsigned char>(low ^ Cond_ByteFF);
        case 0xFE00: return operand == Cond_ByteFE;
        case 0xFD00: return operand != Cond_ByteFD;
        case 0xFC00: return static_cast<unsigned char>(((Game_Mode == 7) | (Field_Request == 4)) ^ low);
        case 0xFB00: return static_cast<unsigned char>((((Cond_AngleFB - 0x200u) & 0xFFFu) <= 0x800u) ^ low);
        case 0xFA00: return static_cast<int>(operand) == static_cast<int>(Cond_ByteFA);
        default: break;
    }
    if (kind < 0x2000) return Flags_Test(Cond_Flags + 8u * (kind >> 8), code);
    if (kind < 0x4000) return Flags_Test(Cond_Flags + 8u * ((code >> 8) & 0x1Fu), code) == 0;
    return static_cast<unsigned char>(low);
}

extern "C" void __cdecl MapCell_DrawQuads(const unsigned char* record, unsigned b1, unsigned b0) {
    if (map_cells::g_quads_live) Live(0, map_cells::g_quads_live, DrawQuads, record, b1, b0);
    else DrawQuads(record, b1, b0);
}

// original 0x570210: kinds 1..3, 5..0xF and 0x31..0x33, 0x35..0x3E - upright
// quads standing on the cell. With e = (kind & 0xF) - 1 (32-bit, as the
// original's dec):
//   - the cell's origin ((b1 - 0x81) << 7, (b0 - 0x80) << 7) goes to the
//     scratch dwords kScratchX / kScratchY, and h = -(s16
//     AreaMap_Elevation((b1 << 16) - 0x10000, b0 << 16) / 2) (a C divide, toward
//     zero) to kScratchH; the scratch dwords are read again after the call;
//   - three points through Gte_RotTransPers3 into MapView_ScreenXY: the origin
//     at h - MapCell_UprightHeights[e] (Prim_VertexScratch +0x10), the origin at
//     h (+0x18), and +0 at that top height with whatever x and y it holds
//     (stale - its screen point is never read); then Gte_StoreDepthF3;
//   - culled unless x0 >= -80 and x0 <= 400 and y1 >= -20 and y0 <= 260 (x87
//     compares: a NaN fails the first three, passes the fourth);
//   - MapCell_UprightCounts[e] times (the byte read again after each): two
//     POLY_FT4s at Gfx_PacketNext (read once for the pair), each
//     Gpu_SetPolyFT4, Gpu_SetShadeTex(0), vertices 2 and 3 the top and the base
//     (their screen dwords and depths copied), u / v / CLUT / tpage from the
//     tables, colour 0x80, Gfx_CommitPrim(slot, 0x48); then the four outer
//     vertices - entry 2 (i + 3e) and the next of MapCell_UprightOffsetsX / Y,
//     about the origin, top and base - through Gte_RotTransPers4 into the pair's
//     vertices 0 and 1, and Gte_StoreDepthF4 into their depths.
// As the original has it: every table is indexed by e unchecked, so kinds with
// a low nibble of 0xA..0xF read past the nine the tables hold (counts 0, 0, 0,
// then bytes of the next table - 9, 0xFF, 0xBE); the counts' loop counter and
// both depths live in the argument slots, which the callers do not read.
extern "C" void __cdecl MapCell_DrawUprights(const unsigned char* record, unsigned b1, unsigned b0) {
    const map_field_objects::U rec = DwordMapFieldObjects(record);
    const map_field_objects::U slot = HandlerSlot(rec);
    const map_field_objects::U e = ((rec >> 24) & 0xFu) - 1u;
    kScratchX.d_00 = (b1 - 0x81u) << 7;
    kScratchY = (b0 - 0x80u) << 7;
    const map_field_objects::U elevation = static_cast<map_field_objects::U>(map_field_objects::g.elevation(static_cast<long>((b1 << 16) - 0x10000u), static_cast<long>(b0 << 16)));
    const map_field_objects::U h = static_cast<map_field_objects::U>(-(static_cast<std::int32_t>(S16(elevation)) / 2));
    const map_field_objects::U x = static_cast<std::uint32_t>(kScratchX.d_00);
    Prim_VertexScratch[3].x = x;
    Prim_VertexScratch[2].x = x;
    const map_field_objects::U y = static_cast<std::uint32_t>(kScratchY);
    Prim_VertexScratch[3].y = y;
    Prim_VertexScratch[2].y = y;
    kScratchH = h;
    const map_field_objects::U top = h - WordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::UprightHeightsAt() + e * 2u));
    Prim_VertexScratch[3].z = h;
    Prim_VertexScratch[1].z = h;
    Prim_VertexScratch[2].z = top;
    Prim_VertexScratch[0].z = top;
    long p = 0;
    map_field_objects::g.rtp3(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18), Vertex(::bof3::Img(Prim_VertexScratch_addr)), FloatAt(::bof3::Img(MapView_ScreenXY_addr)), FloatAt(::bof3::Img(Effect6c_OffsetX_addr)),
           FloatAt(::bof3::Img(kScreen2)), &p);
    float depth0 = 0, depth1 = 0, depth2 = 0;
    map_field_objects::g.depth_f3(&depth0, &depth1, &depth2);
    // DIV-0041: the x cull moved outward by the wide picture's columns (0
    // until every self-test has run, widescreen.h): the margin an upright
    // had beyond the 320 view, kept beyond the 426.
    const float cull = static_cast<float>(Widescreen_Fill());
    if (!(MapView_ScreenXY[0] >= -80.0f - cull)) return;   // fcomp [0x5C4264]; test ah, 1
    if (MapView_ScreenXY[0] > 400.0f + cull) return;       // fcomp [0x5C4260]; test ah, 0x41
    if (!(Float(::bof3::Img(kScreen1) + 4) >= -20.0f)) return;   // fcomp [0x5C422C]; test ah, 1
    if (MapView_ScreenXY[1] > 260.0f) return;       // fcomp [0x5C425C]; test ah, 0x41
    if (Byte(map_field_objects::at::UprightCountsAt() + e) == 0) return;
    map_field_objects::U d0, d1;
    std::memcpy(&d0, &depth0, 4);
    std::memcpy(&d1, &depth1, 4);
    const map_field_objects::U e2 = e * 2u;
    for (map_field_objects::U i = 0;;) {
        unsigned char* const packet = PacketNext();
        for (map_field_objects::U j = 0; j < 2; ++j) {
            unsigned char* const prim = packet + j * 0x48u;
            map_field_objects::g.set_poly_ft4(prim);
            map_field_objects::g.set_shade_tex(prim, 0);
            SetDwordMapFieldObjects(prim + 0x28, DwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(MapView_ScreenXY_addr))));
            SetDwordMapFieldObjects(prim + 0x2C, DwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(MapView_ScreenXY_addr) + 4)));
            SetDwordMapFieldObjects(prim + 0x30, d0);
            SetDwordMapFieldObjects(prim + 0x38, DwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Effect6c_OffsetX_addr))));
            SetDwordMapFieldObjects(prim + 0x3C, DwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Effect6c_OffsetX_addr) + 4)));
            SetDwordMapFieldObjects(prim + 0x40, d1);
            const auto u02 = static_cast<unsigned char>(Byte(map_field_objects::at::UprightU02At() + e));
            prim[0x34] = u02;
            prim[0x14] = u02;
            const auto u13 = static_cast<unsigned char>(Byte(map_field_objects::at::UprightU13At() + e));
            prim[0x44] = u13;
            prim[0x24] = u13;
            const auto v01 = static_cast<unsigned char>(Byte(map_field_objects::at::UprightV01At() + e2 + j));
            prim[0x25] = v01;
            prim[0x15] = v01;
            const auto v23 = static_cast<unsigned char>(Byte(map_field_objects::at::UprightV23At() + e2 + j));
            prim[0x45] = v23;
            prim[0x35] = v23;
            prim[6] = prim[5] = prim[4] = 0x80;
            rv::As<records::GpuPolyFT4>(prim)->clut = (Byte(map_field_objects::at::UprightClutAt() + e) >> 4) | 0x78C0u;
            rv::As<records::GpuPolyFT4>(prim)->tpage = 0x1B;
            map_field_objects::g.commit(slot, 0x48);
        }
        const map_field_objects::U base = (i + e * 3u) * 2u;
        const map_field_objects::U vy = static_cast<std::uint32_t>(kScratchY);
        for (map_field_objects::U v = 0; v < 4; ++v) {
            const map_field_objects::U k = ((v >> 1) + base) * 2u;
            SetWordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Prim_VertexScratch_addr) + v * 8u), WordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::UprightOffsetsXAt() + k)) + static_cast<std::uint32_t>(kScratchX.d_00));
            SetWordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Prim_VertexScratch_addr) + v * 8u + 2), WordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::UprightOffsetsYAt() + k)) + vy);
        }
        map_field_objects::g.rtp4(Vertex(::bof3::Img(Prim_VertexScratch_addr)), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18),
               reinterpret_cast<float*>(packet + 8), reinterpret_cast<float*>(packet + 0x18),
               reinterpret_cast<float*>(packet + 0x50), reinterpret_cast<float*>(packet + 0x60), &p);
        map_field_objects::g.depth_f4(reinterpret_cast<float*>(packet + 0x10), reinterpret_cast<float*>(packet + 0x20),
                   reinterpret_cast<float*>(packet + 0x58), reinterpret_cast<float*>(packet + 0x68));
        ++i;
        if (static_cast<std::int32_t>(i) >= static_cast<std::int32_t>(Byte(map_field_objects::at::UprightCountsAt() + e))) return;
    }
}

// original 0x570530: kinds 4 and 0x34 - a wall 0x80 high along the cell's
// diagonal, from its corner (0, 1) to its corner (1, 0), at the heights of
// the cell's corner bytes 2 and 1 (unsigned, times -16): one POLY_FT4 at
// Gfx_PacketNext through Gpu_SetPolyFT4, Gpu_SetShadeTex(0),
// Gte_RotTransPers4, Gte_PrimDepths4_10, Prim_SetTexture(0xB1800110, 1) and
// Gfx_CommitPrim(slot, 0x48). The corner dword is AreaMap_Corners + (width *
// b0 + b1) * 4, 32-bit and unchecked. The vertices' fourth words are never
// written - Gte_RotTransPers4 reads three.
extern "C" void __cdecl MapCell_DrawDiagonalWall(const unsigned char* record, unsigned b1, unsigned b0) {
    const map_field_objects::U slot = HandlerSlot(DwordMapFieldObjects(record));
    const map_field_objects::U width = DwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(AreaMap_Header_addr))) & 0xFFu;
    const unsigned char* const corner = AtMapFieldObjects(::bof3::Img(AreaMap_Corners_addr) + (width * b0 + b1) * 4u);
    const map_field_objects::U y = (b0 << 7) - 0x4040u, x = (b1 << 7) - 0x4040u;
    const map_field_objects::U z2 = (0u - corner[2]) << 4, z1 = (0u - corner[1]) << 4;
    short v[4][4] = {};
    v[0][0] = v[2][0] = static_cast<short>(x);
    v[0][1] = v[2][1] = static_cast<short>(y + 0x80u);
    v[1][0] = v[3][0] = static_cast<short>(x + 0x80u);
    v[1][1] = v[3][1] = static_cast<short>(y);
    v[0][2] = static_cast<short>(z2 - 0x80u);
    v[2][2] = static_cast<short>(z2);
    v[1][2] = static_cast<short>(z1 - 0x80u);
    v[3][2] = static_cast<short>(z1);
    unsigned char* const prim = PacketNext();
    map_field_objects::g.set_poly_ft4(prim);
    map_field_objects::g.set_shade_tex(prim, 0);
    long p = 0;
    map_field_objects::g.rtp4(v[0], v[1], v[2], v[3], reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
           reinterpret_cast<float*>(prim + 0x28), reinterpret_cast<float*>(prim + 0x38), &p);
    map_field_objects::g.depths4(prim);
    map_field_objects::g.set_texture(0xB1800110u, prim, 1);
    map_field_objects::g.commit(slot, 0x48);
}

extern "C" void __cdecl MapCell_DrawRising(const unsigned char* record, unsigned b1, unsigned b0) {
    if (map_cells::g_rising_live) Live(1, wide_rand::Band() ? map_cells::g_rising_band_live : map_cells::g_rising_live, DrawRising, record, b1, b0);
    else DrawRising(record, b1, b0);
}

// original 0x570870 (MapCell_Handlers[16], in Area_TestCondition's extent):
// a cell's animated quads. Unless the record's condition (its low word) says
// no, or its byte +2 is 1, walks subrecords from dword 1 until the dword
// index equals byte +2 again: four vertex dwords (QuadVertex about ((b1 -
// 0x80) * 128, (b0 - 0x80) * 128)) into a POLY_FT4 at Gfx_PacketNext
// (Gte_RotTransPers4, Gte_StoreDepthF4, shade off); then a frame dword F:
// Frame_Counter % (F & 0xFF) (unsigned) against the threshold bytes from F's
// byte 2 on - the first above it (signed compare) picks the frame -, n =
// ((F >> 8 & 0xFF) + 5) / 4 dwords of thresholds, the texture word n + frame
// dwords after F (Prim_SetTexture), committed to Draw_OtSlot (0x48); the next
// subrecord n + (F >> 8 & 0xFF) dwords after F. Kept as the original: a byte
// +2 no subrecord reaches never ends the walk, and the threshold scan has no
// bound. A period of 0 divides by 0: ours aborts. No PSX twin paired.
extern "C" void __cdecl MapCell_DrawFrames(const unsigned char* record, unsigned b1, unsigned b0) {
    if (SH_CALL(Area_TestCondition)(WFieldE2(Key(record))) == 0) return;
    const U x0 = (b1 - 0x80u) << 7, y0 = (b0 - 0x80u) << 7;
    if ((LongAt(record) & 0xFF0000u) == 0x10000u) return;
    U at = 1;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        alignas(4) short v[16];
        for (U k = 0; k < 4; ++k) QuadVertex(v + 4 * k, LongAt(record + (at + k) * 4u), x0, y0);
        at += 4;
        Project4(v, prim);
        const U frames = LongAt(record + at * 4u);
        const U period = frames & 0xFF;
        if (period == 0) bof3::Fatal("MapCell_DrawFrames: a frame period of 0 at record 0x%X + %u (the original divides by it)",
                                     (unsigned)Key(record), (unsigned)(at * 4u));
        const auto phase = static_cast<std::int32_t>(Frame_Counter % period);
        U frame = 0;
        for (const unsigned char* p = record + at * 4u + 2; phase >= static_cast<std::int32_t>(*p); ++p) ++frame;
        const U skip = (((frames >> 8) & 0xFF) + 5u) >> 2;
        SH_CALL(Prim_SetTexture)(LongAt(record + (frame + skip + at) * 4u), prim, 1);
        const unsigned char slot = Draw_OtSlot;
        at += record[at * 4u + 1] + skip;
        SH_CALL(Gfx_CommitPrim)(slot, field_e2::at::kPrimBytes);
    } while (at != record[2]);
}

// original 0x570A00: kinds 0x11..0x17 and 0x19..0x1F (0x18 is a bare ret) -
// the cell's flat faces. The cell's draw item by the view ring's diagonal
// lookup (MapView_ItemAt's, inline here, docs/field-misc.md): (x', z') = (b1,
// b0) - MapView_Origin, both 32-bit; row (x' + z' + MapView_Row + 1) and column
// ((x' - z') / 2 + MapView_Column + 1), each wrapped by one subtraction, when 0
// <= x' + z' < 0x38 and 0 <= x' - z' < 0x38; the item the low 12 bits of the
// ring word +2. A non-zero one goes to MapCell_FlatOverlay(kind - 0x10, item).
//
// DIV-0041 (wide-ring): a cell of the grown ring's rim takes the rim's item.
extern "C" void __cdecl MapCell_DrawFlatFaces(const unsigned char* record, unsigned b1, unsigned b0) {
    const map_field_objects::U ox = S16(WordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::OriginAt()))), oz = S16(WordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::OriginAt() + 2)));
    const map_field_objects::U dx = b1 - ox, dz = b0 - oz;
    const auto sum = static_cast<std::int32_t>(dx + dz);
    const auto diff = static_cast<std::int32_t>(dx - dz);
    if (map_rim::Active() && !(sum >= 0 && sum < 0x38 && diff >= 0 && diff < 0x38)) {
        const map_field_objects::U item = map_rim::RimItemAt(static_cast<std::int32_t>(dx), static_cast<std::int32_t>(dz));
        if (item != 0) map_field_objects::g.flat_overlay(static_cast<int>((DwordMapFieldObjects(record) >> 24) - 0x10u), item);
        return;
    }
    if (sum >= 0x38 || sum < 0) return;
    auto row = static_cast<std::int32_t>(static_cast<map_field_objects::U>(sum) + S16(WordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::RowAt()))) + 1u);
    if (row >= 0x38) row -= 0x38;
    if (diff >= 0x38 || diff < 0) return;
    auto column = static_cast<std::int32_t>(static_cast<map_field_objects::U>(diff / 2) + S16(WordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::ColumnAt()))) + 1u);
    if (column >= 0x1C) column -= 0x1C;
    const map_field_objects::U index = static_cast<map_field_objects::U>(column) + static_cast<map_field_objects::U>(row) * 28u;
    const map_field_objects::U item = WordMapFieldObjects(AtMapFieldObjects(::bof3::Img(MapView_CellItems_addr) + 0x2 + index * 4u)) & 0xFFFu;
    if (item == 0) return;
    map_field_objects::g.flat_overlay(static_cast<int>((DwordMapFieldObjects(record) >> 24) - 0x10u), item);
}

// original 0x570AB0 (no PSX twin paired): the cell handler 0x570A00's draw, for
// map-cell records of kinds 0x10 and up with `faces` = kind - 0x10 and `item`
// the cell's draw item (MapView_ItemAt's lookup, inline there). For each of
// faces' bits 1, 2 and 4 that is set: a POLY_F4 at Gfx_PacketNext (read once
// for the face) made by Gpu_SetPolyF4, its four corners' x and y copied from
// the face's POLY_FT4 half for this buffer - bit 1 the item itself, bit 2 the
// item at its +0x8E, bit 4 the item at its +0x7E - its colour the 15-bit word
// of the area header at +0x1A + (faces >> 3) * 2 (arithmetic shift), each
// channel's five bits << 3, and Gfx_CommitPrim(Draw_OtSlot, 0x38). The
// original's eax on the way out is whatever the last call or its caller left;
// its one caller returns it to DrawLayer_Open, which does not read it.
extern "C" void __cdecl MapCell_FlatOverlay(int faces, unsigned item) {
    for (field_misc::U bit = 1; bit < 8; bit <<= 1) {
        if (!(static_cast<field_misc::U>(faces) & bit)) continue;
        unsigned char* const packet = AtFieldMisc(LongFieldMisc(AtFieldMisc(field_misc::at::Gfx_PacketNextAt())));
        field_misc::g.set_poly_f4(packet);
        field_misc::U index;
        if (bit == 1) index = item;
        else if (bit == 2) index = WordFieldMisc(AtFieldMisc(field_misc::at::ItemFaceA() + item * 0x90u));
        else index = WordFieldMisc(AtFieldMisc(field_misc::at::ItemFaceB() + item * 0x90u));
        const unsigned char* const src = AtFieldMisc(ItemHalf(index));
        for (field_misc::U corner = 0; corner < 4; ++corner) {
            PutLongFieldMisc(packet + 8 + corner * 0xC, LongFieldMisc(src + 8 + corner * 0x10));
            PutLongFieldMisc(packet + 0xC + corner * 0xC, LongFieldMisc(src + 0xC + corner * 0x10));
        }
        const unsigned char* const colour = AtFieldMisc(::bof3::Img(kOverlayColours) + static_cast<field_misc::U>(faces >> 3) * 2u);
        packet[4] = static_cast<unsigned char>((colour[0] & 0x1F) << 3);
        packet[5] = static_cast<unsigned char>(((WordFieldMisc(colour) >> 5) & 0x1F) << 3);
        packet[6] = static_cast<unsigned char>(((WordFieldMisc(colour) >> 10) & 0x1F) << 3);
        field_misc::g.commit_prim(AtFieldMisc(field_misc::at::Draw_OtSlotAt())[0], 0x38);
    }
}

// original 0x570BC0 (MapCell_Handlers[33], in MapCell_FlatOverlay's extent): a
// cell's shaded quads. Unless the record's condition says no: unless byte +2
// is 1, subrecords of eight dwords from dword 1 until the dword index equals
// byte +2 - a draw mode (tpage 0x95 | (bits 24..25 of the first colour) << 5,
// dtd 1) committed to Draw_OtSlot (0xC), a POLY_G4 (Gpu_SetPolyG4) at
// Gfx_PacketNext, four vertex dwords (QuadVertex) through Gte_RotTransPers4
// and Gte_StoreDepthF4, semi-transparent by the first colour's bit 31, four
// colours (bytes 2, 1, 0 of each dword to r, g, b), committed to Draw_OtSlot
// (0x44). Then, and when byte +2 is 1, a draw mode (tpage 0x95) committed to
// Draw_OtSlot (0xC). A byte +2 no subrecord reaches never ends the walk, as
// the original. No PSX twin paired.
extern "C" void __cdecl MapCell_DrawShaded(const unsigned char* record, unsigned b1, unsigned b0) {
    if (SH_CALL(Area_TestCondition)(WFieldE2(Key(record))) == 0) return;
    const U x0 = (b1 - 0x80u) << 7, y0 = (b0 - 0x80u) << 7;
    if ((LongAt(record) & 0xFF0000u) != 0x10000u) {
        U at = 1;
        const unsigned char* sub = record + 4;
        do {
            const U tpage = (((LongAt(sub + 0x10) >> 24) & 3u) << 5) | 0x95u;
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
            SH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0xC);
            unsigned char* const prim = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(prim);
            at += 4;
            alignas(4) short v[16];
            for (U k = 0; k < 4; ++k) QuadVertex(v + 4 * k, LongAt(sub + 4u * k), x0, y0);
            sub += 0x10;
            Project4(v, prim);
            SH_CALL(Gpu_SetSemiTrans)(prim, LongAt(sub) >> 31);
            static const unsigned kColours[4] = {4, 0x14, 0x24, 0x34};
            for (U k = 0; k < 4; ++k) {
                const U c = LongAt(sub + 4u * k);
                prim[kColours[k]] = static_cast<unsigned char>(c >> 16);
                prim[kColours[k] + 1] = static_cast<unsigned char>(c >> 8);
                prim[kColours[k] + 2] = static_cast<unsigned char>(c);
            }
            sub += 0x10;
            at += 4;
            SH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0x44);
        } while (at != record[2]);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0xC);
}

// original 0x570DE0 (MapCell_Handlers[34], in MapCell_FlatOverlay's extent): a
// cell's spinning quads. Unless the record's condition says no: the centre
// (words of ((b1 - 0x80) * 64 + s8 byte 3 of dword 1) * 2, ((b0 - 0x80) * 64 +
// s8 byte 2) * 2 and dword 1's low word) and a spin S = the three 10-bit
// fields of dword 2 (bits 20, 10, 0) times (Frame_Counter & 0xFFF) * 4, as
// words. Unless byte +2 is 3, subrecords of six dwords from dword 3 until
// the dword index equals byte +2: under a pushed matrix the angles (S + the
// subrecord's first dword's three fields * 4) & 0xFFF through Gte_RotMatrix,
// the centre through Gte_RotTrans into its translation, Gte_MulMatrix0 with
// Camera_Matrix, set as rotation and translation; a POLY_FT4, four vertex
// dwords of three 10-bit fields each (bit 9 the sign; the top field keeps
// bits 10 and 11 when its bit 9 is clear, as the original's shift leaves
// them) through Gte_RotTransPers4 and Gte_StoreDepthF4, the texture word
// (dword 5) through Prim_SetTexture, committed (0x48) to slot 6 when that
// word, read again, has bit 30, else Draw_OtSlot; the matrix popped. The
// SVECTORs' fourth words are 0 (DIV-0023). A byte +2 no subrecord reaches
// never ends the walk, as the original. No PSX twin paired.
extern "C" void __cdecl MapCell_DrawSpinning(const unsigned char* record, unsigned b1, unsigned b0) {
    if (SH_CALL(Area_TestCondition)(WFieldE2(Key(record))) == 0) return;
    const U d1 = LongAt(record + 4), d2 = LongAt(record + 8);
    const U counter = Frame_Counter & 0xFFFu;
    alignas(4) short centre[4];
    centre[0] = static_cast<short>((static_cast<U>(static_cast<std::uint16_t>(S8FieldE2(d1 >> 24))) + ((b1 - 0x80u) << 6)) << 1);
    centre[1] = static_cast<short>((static_cast<U>(static_cast<std::uint16_t>(S8FieldE2(d1 >> 16))) + ((b0 - 0x80u) << 6)) << 1);
    centre[2] = static_cast<short>(d1);
    centre[3] = 0;
    const U spin[3] = {((d2 >> 20) & 0x3FFu) * counter * 4u, ((d2 >> 10) & 0x3FFu) * counter * 4u, (d2 & 0x3FFu) * counter * 4u};
    if ((LongAt(record) & 0xFF0000u) == 0x30000u) return;
    U at = 3;
    const unsigned char* sub = record + 0xC;
    do {
        SH_CALL(Gte_PushMatrix)();
        const U d = LongAt(sub);
        alignas(4) short angles[4];
        angles[0] = static_cast<short>((spin[0] + ((d >> 20) & 0x3FFu) * 4u) & 0xFFFu);
        angles[1] = static_cast<short>((spin[1] + ((d >> 10) & 0x3FFu) * 4u) & 0xFFFu);
        angles[2] = static_cast<short>((spin[2] + (d & 0x3FFu) * 4u) & 0xFFFu);
        angles[3] = 0;
        alignas(4) unsigned char matrix[0x20];
        SH_CALL(Gte_RotTrans)(centre, reinterpret_cast<long*>(matrix + 0x14));
        SH_CALL(Gte_RotMatrix)(angles, reinterpret_cast<short*>(matrix));
        SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<short*>(matrix), reinterpret_cast<short*>(matrix));
        SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<unsigned long*>(matrix));
        SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<unsigned long*>(matrix));
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        alignas(4) short v[16];
        for (U k = 0; k < 4; ++k) {
            const U e = LongAt(sub + 4u + 4u * k);
            U fx = e >> 20, fy = (e >> 10) & 0x3FFu, fz = e & 0x3FFu;
            if (fx & 0x200u) fx |= 0xFFFFFC00u;
            if (fy & 0x200u) fy |= 0xFFFFFC00u;
            if (fz & 0x200u) fz |= 0xFFFFFC00u;
            v[4 * k] = static_cast<short>(fx);
            v[4 * k + 1] = static_cast<short>(fy);
            v[4 * k + 2] = static_cast<short>(fz);
            v[4 * k + 3] = 0;
        }
        Project4(v, prim);
        SH_CALL(Prim_SetTexture)(LongAt(sub + 0x14), prim, 1);
        SH_CALL(Gfx_CommitPrim)((LongAt(sub + 0x14) & 0x40000000u) ? 6u : Draw_OtSlot, field_e2::at::kPrimBytes);
        SH_CALL(Gte_PopMatrix)();
        at += 6;
        sub += 0x18;
    } while (at != record[2]);
}

extern "C" void __cdecl MapCell_PatchThenStep(unsigned char* record, unsigned, unsigned) { PatchThen(record, 0x25000000u); }

extern "C" void __cdecl MapCell_PatchThenStop(unsigned char* record, unsigned, unsigned) { PatchThen(record, 0x23000000u); }

// original 0x571110 (PSX 0x801581B8): one entry of the area's patch list
// (AreaMap_SetupEntries' second walk, and the cell handlers 0x571090 /
// 0x5710D0, and two callers in 0x413xxx / 0x41Cxxx). The entry's low word is an
// Area_TestCondition code; its result, al sign-extended, is `c` (0 or 1 from
// that function). The high word, read after the call, is the entry's length n
// in dwords. Records follow, each headed by a dword: kind (byte 3), bits 20-23
// a face, bits 16-19 a texture offset, x (byte 1), y (byte 0). While the
// dwords consumed are fewer than n (signed):
//   kind 0 (3 dwords): dword (c ? +8 : +4) into the area's texture run at
//       ((height * width + 1) / 2 + offset + tile + bits 16-19), `tile` the
//       word of map (x, y) - AreaMap_BakePatches' address (docs/map-scroll.md)
//       - then kind 1's work;
//   kind 1 (2 dwords - its +8 is the next record's head): the face's draw item
//       of MapView_ItemAt(x, y), when there is one - face 0 the item, 1 the
//       word at its +0x8E, 2 and up the word at +0x7E - its half for this
//       buffer given the dword (c ? +8 : +4) through Prim_SetTexture(tex,
//       half, 1), the head and dword re-read after the lookup;
//   kind 2 (3 dwords): dword (c ? +8 : +4) to AreaMap_Corners[width * y + x],
//       MapView_Redraw 2;
//   any other (2 dwords): the low byte of dword +4 >> ((c << 4) & 31) - its
//       low half for c 0, its high half for c 1 - to AreaMap_Bytes[width * y + x].
// The width and height are the header's bytes 0 and 1, its word +2 the offset,
// all re-read per record. The original also stores c over its own argument
// slot; every caller pops it unread (field-misc.md section 2), so ours does not.
extern "C" void __cdecl AreaMap_ApplyPatch(const unsigned char* entry) {
    const std::int32_t c = static_cast<signed char>(field_misc::g.test_condition(WordFieldMisc(entry)));
    const auto n = static_cast<std::int32_t>(LongFieldMisc(entry) >> 16);
    const unsigned char* cursor = entry + 4;
    std::int32_t used = 0;
    if (n <= 0) return;
    do {
        const field_misc::U head = LongFieldMisc(cursor);
        const unsigned char* const record = cursor;
        const field_misc::U kind = head >> 24, x = (head >> 8) & 0xFF, y = head & 0xFF;
        switch (kind) {
        case 0: {
            const field_misc::U header = LongFieldMisc(AtFieldMisc(Header(0)));
            const field_misc::U offset = WordFieldMisc(AtFieldMisc(::bof3::Img(AreaMap_Header_addr) + 0x2));
            const field_misc::U width = header & 0xFF;
            const field_misc::U tile = WordFieldMisc(AtFieldMisc(Header(0) + (x + width * y + offset * 2u) * 2u));
            const auto area = static_cast<std::int32_t>(((header >> 8) & 0xFF) * width + 1u);
            const field_misc::U run = ((head >> 16) & 0xF) + static_cast<field_misc::U>(area / 2) + offset + tile;
            used += 1;
            const field_misc::U value = LongFieldMisc(record + static_cast<field_misc::U>(c) * 4u + 4u);
            cursor += 4;
            PutLongFieldMisc(AtFieldMisc(Header(0) + run * 4u), value);
        }
            [[fallthrough]];
        case 1: {
            const field_misc::U face = (LongFieldMisc(record) >> 20) & 0xF;
            const field_misc::U item = static_cast<field_misc::U>(field_misc::g.item_at(static_cast<long>(x), static_cast<long>(y)));
            if (item != 0) {
                field_misc::U index;
                if (face == 0) index = item;
                else if (face == 1) index = WordFieldMisc(AtFieldMisc(field_misc::at::ItemFaceA() + item * 0x90u));
                else index = WordFieldMisc(AtFieldMisc(field_misc::at::ItemFaceB() + item * 0x90u));
                unsigned char* const half = AtFieldMisc(ItemHalf(index));
                field_misc::g.set_texture(LongFieldMisc(record + static_cast<field_misc::U>(c) * 4u + 4u), half, 1);
            }
            used += 2;
            cursor += 8;
            break;
        }
        case 2: {
            const field_misc::U width = LongFieldMisc(AtFieldMisc(Header(0))) & 0xFF;
            used += 3;
            const field_misc::U value = LongFieldMisc(cursor + static_cast<field_misc::U>(c) * 4u + 4u);
            cursor += 12;
            PutLongFieldMisc(AtFieldMisc(field_misc::at::AreaMap_CornersAt() + (width * y + x) * 4u), value);
            AtFieldMisc(field_misc::at::MapView_RedrawAt())[0] = 2;
            break;
        }
        default: {
            const field_misc::U value = LongFieldMisc(cursor + 4) >> ((static_cast<field_misc::U>(c) << 4) & 31);
            const field_misc::U width = LongFieldMisc(AtFieldMisc(Header(0))) & 0xFF;
            const field_misc::U bytes = LongFieldMisc(AtFieldMisc(field_misc::at::AreaMap_BytesAt()));
            AtFieldMisc(width * y + bytes + x)[0] = static_cast<unsigned char>(value);
            used += 2;
            cursor += 8;
            break;
        }
        }
    } while (used < n);
}

// original 0x5712E0 (PSX 0x80158464, gap4 pair): kind 0x27 - an animated
// sprite standing on the cell. Unless Area_TestCondition(word +0) says no: one
// point, (s8 byte 7 + (b1 - 0x80) * 64) * 2, (s8 byte 6 + (b0 - 0x80) * 64) * 2,
// word +4 (Prim_VertexScratch +0), through Gte_RotTransPers into
// MapView_ScreenXY; culled unless -60 <= x <= 380 and -150 <= y <= 300 (a NaN
// fails the lower bounds) and the depth p it returns is non-zero. The frame:
// with period byte +8 and n = (byte +9 + 5) >> 2, t = Frame_Counter mod
// period (unsigned), k the first index with t < byte +0xA + k (signed); dword
// +(n + 2k + 2) * 4 holds four s8 corner offsets and the next dword the
// texture. The POLY_FT4 at Gfx_PacketNext: Gpu_SetPolyFT4, Gpu_SetShadeTex(0),
// Gte_PrimDepthFlat4_10; its corners x - o0 * 1125 / p, then + o1 * 1125 / p
// (vertices 0 / 2 and 1 / 3), y - o2 * 1125 / p, then + o3 * 1125 / p (0 / 1
// and 2 / 3) - x87 at the game's double precision, the running value kept in
// the register and each corner stored as a float; Prim_SetTexture(texture
// without bit 15, 1); then, by bit 30 of the texture dword read again,
// Gfx_CommitPrim(6 or Draw_OtSlot, 0x48).
// As the original has it: a period of 0 divides by zero and the threshold scan
// has no bound (both read, not run); the depth is stored over the argument
// slot and then the corner offsets over it again.
extern "C" void __cdecl MapCell_DrawAnimated(const unsigned char* record, unsigned b1, unsigned b0) {
    if (map_field_objects::g.test(WordMapFieldObjects(record)) == 0) return;
    Prim_VertexScratch[0].x = (S8MapFieldObjects(DwordMapFieldObjects(record + 4) >> 24) + ((b1 - 0x80u) << 6)) << 1;
    Prim_VertexScratch[0].y = (S8MapFieldObjects(DwordMapFieldObjects(record + 4) >> 16) + ((b0 - 0x80u) << 6)) << 1;
    Prim_VertexScratch[0].z = WordMapFieldObjects(record + 4);
    long depth = 0;
    const auto p = static_cast<std::int32_t>(
        map_field_objects::g.rtp(Vertex(::bof3::Img(Prim_VertexScratch_addr)), reinterpret_cast<unsigned long*>(AtMapFieldObjects(::bof3::Img(MapView_ScreenXY_addr))), &depth));
    const float cull = static_cast<float>(Widescreen_Fill());   // DIV-0041, as MapCell_DrawUprights
    if (!(MapView_ScreenXY[0] >= -60.0f - cull)) return;  // fcomp [0x5C4210]; test ah, 1
    if (MapView_ScreenXY[0] > 380.0f + cull) return;      // fcomp [0x5C420C]; test ah, 0x41
    if (!(MapView_ScreenXY[1] >= -150.0f)) return; // fcomp [0x5C4200]; test ah, 1
    if (Float(::bof3::Img(kScreen0) + 4) > 300.0f) return;      // fcomp [0x5C41FC]; test ah, 0x41
    if (p == 0) return;
    const map_field_objects::U timing = DwordMapFieldObjects(record + 8);
    const map_field_objects::U t = DwordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::FrameCounterAt())) % (timing & 0xFFu);
    map_field_objects::U k = 0;
    while (static_cast<std::int32_t>(t) >= static_cast<std::int32_t>(record[0xA + k])) ++k;
    unsigned char* const prim = PacketNext();
    const map_field_objects::U n = (((timing >> 8) & 0xFFu) + 5u) >> 2;
    map_field_objects::g.set_poly_ft4(prim);
    map_field_objects::g.set_shade_tex(prim, 0);
    map_field_objects::g.depth_flat4(prim);
    const unsigned char* const offsets = record + (n + k * 2u + 2u) * 4u;
    const unsigned char* const texture = record + (n + k * 2u) * 4u + 0xC;
    const double dp = static_cast<double>(p);
    const double x = static_cast<double>(MapView_ScreenXY[0]);
    const double a = x - static_cast<double>(static_cast<std::int32_t>(S8MapFieldObjects(DwordMapFieldObjects(offsets)))) * 1125.0 / dp;
    SetFloatMapFieldObjects(prim + 0x28, static_cast<float>(a));
    SetFloatMapFieldObjects(prim + 8, static_cast<float>(a));
    const double b = static_cast<double>(static_cast<std::int32_t>(S8MapFieldObjects(DwordMapFieldObjects(offsets) >> 8))) * 1125.0 / dp + a;
    SetFloatMapFieldObjects(prim + 0x38, static_cast<float>(b));
    SetFloatMapFieldObjects(prim + 0x18, static_cast<float>(b));
    const double y = static_cast<double>(MapView_ScreenXY[1]);
    const double c = y - static_cast<double>(static_cast<std::int32_t>(S8MapFieldObjects(DwordMapFieldObjects(offsets) >> 16))) * 1125.0 / dp;
    SetFloatMapFieldObjects(prim + 0x1C, static_cast<float>(c));
    SetFloatMapFieldObjects(prim + 0xC, static_cast<float>(c));
    const double d = static_cast<double>(static_cast<std::int32_t>(S8MapFieldObjects(DwordMapFieldObjects(offsets) >> 24))) * 1125.0 / dp + c;
    SetFloatMapFieldObjects(prim + 0x3C, static_cast<float>(d));
    SetFloatMapFieldObjects(prim + 0x2C, static_cast<float>(d));
    map_field_objects::g.set_texture(DwordMapFieldObjects(texture) & 0xFFFF7FFFu, prim, 1);
    if (DwordMapFieldObjects(texture) & 0x40000000u) map_field_objects::g.commit(6, 0x48);
    else map_field_objects::g.commit(OtSlot(), 0x48);
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
        map_layers::g.set_poly_ft4(prim);
        map_layers::g.set_shade_tex(prim, 0);
        const std::uint32_t odd = t & 1u;
        v[1][0] = v[3][0] = static_cast<short>(x0 + ((~t & 1u) << 7));
        v[1][1] = v[3][1] = static_cast<short>(y0 + (odd << 7));
        short h1, h2;
        if (odd) {
            const unsigned char* const left = corners - 4;
            if (S8MapLayers(left[1]) > S8MapLayers(corners[0]) || S8MapLayers(left[3]) > S8MapLayers(corners[2])) {
                h1 = Height(left[1]);
                h2 = Height(left[3]);
            } else {
                h1 = Height(corners[0]);
                h2 = Height(corners[2]);
            }
        } else {
            const unsigned char* const up = corners - Width() * 4u;
            if (S8MapLayers(up[2]) > S8MapLayers(corners[0]) || S8MapLayers(up[3]) > S8MapLayers(corners[1])) {
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
        map_layers::g.rot_trans_pers4(v[0], v[1], v[2], v[3], reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
                          reinterpret_cast<float*>(prim + 0x28), reinterpret_cast<float*>(prim + 0x38), &depth);
        map_layers::g.depths4(prim);
        map_layers::g.set_texture(*texture, prim, 1);
        map_layers::g.commit(Draw_OtSlot, 0x48);
    }
}

// original 0x571720: an area's set-up entries (PSX FUN_80158A54). Cond_ByteFF
// from bit 0 of Field_StatusBits; the set-up list from AreaMap_SetupBase,
// each entry to AreaMap_SetupHandlers by its top byte; the palette cycles'
// first frame; then each entry of the patch list to AreaMap_ApplyPatch.
//
// As the original has it: the handler index is masked to 6 bits and not
// checked against the table's three - past them it reaches AreaMap_Entry-
// Handlers, Kind2_RunTable and data (the fuzz runs all 64 slots, as stand-
// ins); each step is read after the call, the set-up list's from byte +2, the
// patch list's from the high word, plus one; AreaMap_PatchBase is read after
// AreaMap_ClutCycleStart; a step of 0 on the set-up list never ends (read,
// not run).
extern "C" void __cdecl AreaMap_SetupEntries(void) {
    using Handler = void(__cdecl*)(const unsigned char*);
    const std::uint32_t base = AreaMap_SetupBase;
    Cond_ByteFF = static_cast<unsigned char>(Field_StatusBits & 1);
    const unsigned char* entry = AreaMap_Header + base * 4u;
    for (std::uint32_t e = Dword(entry); e != 0; e = Dword(entry)) {
        reinterpret_cast<Handler>(static_cast<std::uintptr_t>(AreaMap_SetupHandlers[(e >> 24) & 0x3Fu]))(entry);
        entry += entry[2] * 4u;
    }
    gMapScroll.clut_start();
    const unsigned char* patch = AreaMap_Header + static_cast<std::uint32_t>(AreaMap_PatchBase) * 4u;
    while (Dword(patch) != 0) {
        gMapScroll.apply_patch(patch);
        patch += (Dword(patch) >> 16) * 4u + 4u;
    }
}

// original 0x5717B0 (PSX 0x80158B60): every palette cycle of the area header put at its frame
// at once (PSX FUN_80158B60). The entries from AreaMap_EntryBase, as
// AreaMap_HeaderPass walks them; one whose top byte is exactly 0x80 - kind 0,
// AreaMap_ClutCycle's, with bit 7 - takes f = Frame_Counter mod its low byte,
// plus 1, and the first dword after it whose top byte is f or more: that
// dword's 16-colour row (d >> 12) & 0xFFF of Gfx_ClutStrip is copied over
// row d & 0xFFF.
//
// As the original has it, where it differs from AreaMap_ClutCycle: the
// period is the low byte, not the word; there is no condition; the scan
// takes the first top byte at or above f, not an equal one. Read and not run
// (the fuzz keeps inside all three): a period byte of 0 divides by zero; a
// list with no top byte at or above f runs on; the rows are unchecked
// against the strip's 512.
extern "C" void __cdecl AreaMap_ClutCycleStart(void) {
    const unsigned char* entry = AreaMap_Header + static_cast<std::uint32_t>(AreaMap_EntryBase) * 4u;
    for (std::uint32_t e = Dword(entry); e != 0; e = Dword(entry)) {
        if ((e & 0xFF000000u) == 0x80000000u) {
            const auto frame = static_cast<std::int32_t>(Frame_Counter % (e & 0xFFu) + 1u);
            const unsigned char* q = entry + 4;
            while (frame > static_cast<std::int32_t>(Dword(q) >> 24)) q += 4;
            const std::uint32_t d = Dword(q);
            const std::uint32_t from = (((d >> 16) & 0xFFu) << 4) + ((d >> 12) & 0xFu);
            const std::uint32_t to = (((d >> 4) & 0xFFu) << 4) + (d & 0xFu);
            auto* const strip = reinterpret_cast<unsigned char*>(Gfx_ClutStrip);
            for (unsigned k = 0; k < 32; k += 4) std::memcpy(strip + to * 32u + k, strip + from * 32u + k, 4);
            Gfx_ClutStripDirty = 1;
        }
        entry += entry[2] * 4u;
    }
}

// original 0x571880 (PSX 0x80158C70, gap4): entry 0, the area's palette tint.
// Gfx_ClutAdjust(word +4, word +6, then - when Area_TestCondition(word +0),
// its al signed, is non-zero - the s8 bytes +8, +9, +10 as red, green and
// blue, else 0, 0, 0). The words are read after the condition.
extern "C" void __cdecl AreaMap_SetupTint(const unsigned char* entry) {
    const auto yes = static_cast<signed char>(map_field_objects::g.test(WordMapFieldObjects(entry)));
    const map_field_objects::U d = DwordMapFieldObjects(entry + 4);
    const int columns = static_cast<int>(d & 0xFFFFu), rows = static_cast<int>(d >> 16);
    if (yes == 0) {
        map_field_objects::g.clut_adjust(columns, rows, 0, 0, 0);
        return;
    }
    const map_field_objects::U rgb = DwordMapFieldObjects(entry + 8);
    map_field_objects::g.clut_adjust(columns, rows, static_cast<int>(S8MapFieldObjects(rgb)), static_cast<int>(S8MapFieldObjects(rgb >> 8)),
                  static_cast<int>(S8MapFieldObjects(rgb >> 16)));
}

// original 0x5718F0 (PSX 0x80158CE8): the area palette's tint. For CLUT rows
// 3..14 whose bit of `rows` is set (bit 0 row 3; `rows` halved per row, signed),
// each 16-colour CLUT of the row - on row 3 only those whose bit of `columns`
// is set (`columns` halved per CLUT of every row processed, but tested on row 3
// alone, which comes first) - copied from Gfx_ClutStripSource to Gfx_ClutStrip
// with red += `red`, green += `green`, blue += `blue`, each only when non-zero
// and then clamped to 1..0x1F; bit 15 kept. Then Gfx_ClutStripDirty = 1.
// Returns the original's eax: `rows` after its twelve halvings. The halvings
// write the caller's argument slots; the five callers pop them unread.
extern "C" long __cdecl Gfx_ClutAdjust(int columns, int rows, int red, int green, int blue) {
    const auto channel = [](field_misc::U v, int add) -> U {
        if (v == 0) return 0;
        const auto s = static_cast<std::int32_t>(v + static_cast<field_misc::U>(add));
        if (s <= 0) return 1;
        if (s > 0x1F) return 0x1F;
        return static_cast<field_misc::U>(s);
    };
    for (field_misc::U row = 3; row < 15; ++row) {
        if (rows % 2 != 0) {
            for (field_misc::U clut = 0; clut < 16; ++clut) {
                if (row != 3 || columns % 2 != 0) {
                    const field_misc::U from = field_misc::at::Gfx_ClutStripSourceAt() + (row * 16 + clut) * 32;
                    for (field_misc::U k = 0; k < 16; ++k) {
                        const field_misc::U w = WordFieldMisc(AtFieldMisc(from + k * 2));
                        const field_misc::U r = channel(w & 0x1F, red), gr = channel((w >> 5) & 0x1F, green),
                                b = channel((w >> 10) & 0x1F, blue);
                        PutWord(AtFieldMisc(from + 0x4000 + k * 2), (((b << 5) | gr) << 5) | (w & 0x8000) | r);
                    }
                }
                columns /= 2;
            }
        }
        rows /= 2;
    }
    AtFieldMisc(field_misc::at::Gfx_ClutStripDirtyAt())[0] = 1;
    return rows;
}

// original 0x571A30 (PSX 0x80158E78, gap4): entry 1, MapCell_FlatOverlay's two
// colours (area block +0x1A and +0x1C): the high words of dwords +4 and +8
// when Cond_ByteFF (the story bit AreaMap_SetupEntries copies in) is set,
// else their low words.
extern "C" void __cdecl AreaMap_SetupFlatColours(const unsigned char* entry) {
    if (Byte(map_field_objects::at::CondByteFFAt()) != 0) {
        SetWordMapFieldObjects(AtMapFieldObjects(::bof3::Img(kFlatColour0)), DwordMapFieldObjects(entry + 4) >> 16);
        SetWordMapFieldObjects(AtMapFieldObjects(::bof3::Img(kFlatColour1)), DwordMapFieldObjects(entry + 8) >> 16);
    } else {
        SetWordMapFieldObjects(AtMapFieldObjects(::bof3::Img(kFlatColour0)), WordMapFieldObjects(entry + 4));
        SetWordMapFieldObjects(AtMapFieldObjects(::bof3::Img(kFlatColour1)), WordMapFieldObjects(entry + 8));
    }
}

// original 0x571A70 (PSX 0x80158EBC, not the pairing's 0x80158EE4 inside it):
// entry 2, a texture of the area's
// run replaced - dword +4 when Cond_ByteFF is set, else +8 - at (height *
// width + 1) / 2 + offset + tile of the block, tile the word of map (x = byte
// 1, y = byte 0) as MapView_CellTextures reads it (word y * width + x + offset
// * 2). All unchecked.
extern "C" void __cdecl AreaMap_SetupTexture(const unsigned char* entry) {
    const map_field_objects::U flag = Byte(map_field_objects::at::CondByteFFAt());
    const map_field_objects::U width = DwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(AreaMap_Header_addr))) & 0xFFu;
    const map_field_objects::U d = DwordMapFieldObjects(entry);
    const map_field_objects::U offset = WordMapFieldObjects(AtMapFieldObjects(::bof3::Img(AreaMap_Header_addr) + 0x2));
    const map_field_objects::U texture = DwordMapFieldObjects(entry + 4 + (flag == 0 ? 4 : 0));
    const map_field_objects::U tile = WordMapFieldObjects(AtMapFieldObjects(::bof3::Img(AreaMap_Header_addr) + ((d & 0xFFu) * width + ((d >> 8) & 0xFFu) + offset * 2u) * 2u));
    const auto half = static_cast<std::int32_t>(static_cast<std::uint32_t>(AreaMap_Header[1]) * width + 1u) / 2;
    SetDwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(AreaMap_Header_addr) + (static_cast<map_field_objects::U>(half) + offset + tile) * 4u), texture);
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
    if (map_layers::g.test(WordMapLayers(p)) == 0) return;
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

// original 0x571BE0: AreaMap_EntryHandlers 1 (PSX FUN_80159108) - the area's
// backdrop, one Gouraud quad over the whole view, drawn while the view shows
// cells off the map (MapView_BuildFlags non-zero) and the focus lies within
// the entry's bounds: dword +4 holds four bytes x0, z0, x1, z1 against the
// focus in cells, (0x7FFF - MapView_FocusX) >> 8 and (0x8000 - MapView_FocusZ)
// >> 8. A draw mode of tpage 0x95 with dithering (slot 7, 0xC bytes), then a
// POLY_G4 (slot 7, 0x44) whose top colour is the low word and bottom colour
// the high word of the entry's dword +8 - or +12 when Cond_ByteFF is set -
// each a 15-bit r, g, b unpacked to 8.
//
// DIV-0041: under a wide picture the quad spans the wide view, x from -53 to
// 373 in the game's 320-wide units, so the sky reaches both edges of the
// 426 x 240 picture; otherwise the original's 0..320. The rows are the same.
//
// As the original has it: the four bound tests are signed 32-bit compares of
// a byte against the 24-bit shifted focus; the packet cursor is read again
// after the first commit for the quad; the PC dropped the PSX's video-mode
// test that picks tpage 0x225 over 0x95, and passes 0x95 always.
extern "C" void __cdecl AreaMap_DrawBackdrop(const unsigned char* entry) {
    if (MapView_BuildFlags == 0) return;
    const auto fz = static_cast<std::int32_t>((0x8000u - static_cast<std::uint32_t>(MapView_FocusZ)) >> 8);
    const auto fx = static_cast<std::int32_t>((0x7FFFu - static_cast<std::uint32_t>(MapView_FocusX)) >> 8);
    const std::uint32_t bounds = Dword(entry + 4);
    if (static_cast<std::int32_t>(bounds & 0xFFu) > fx) return;
    if (static_cast<std::int32_t>((bounds >> 8) & 0xFFu) > fz) return;
    if (static_cast<std::int32_t>((bounds >> 16) & 0xFFu) < fx) return;
    if (static_cast<std::int32_t>(bounds >> 24) < fz) return;

    area_backdrop::g.draw_mode(Gfx_PacketNext, 0, 1, 0x95, 0);
    area_backdrop::g.commit(7, 0xC);
    unsigned char* const quad = Gfx_PacketNext;   // read again: the commit moved it
    area_backdrop::g.set_poly_g4(quad);
    area_backdrop::g.set_semi(quad, 0);
    // DIV-0041: (-53, 0)..(373, 240) under a wide picture, (0, 0)..(320, 240) otherwise.
    const float wide = static_cast<float>(Widescreen_Live());
    const float left = 0.0f - wide;   // not -wide, which is -0.0f when wide is 0
    const float right = 320.0f + wide;
    SetFloat(quad + 0x8, left);
    SetDword(quad + 0xC, 0);
    SetFloat(quad + 0x18, right);
    SetDword(quad + 0x1C, 0);
    SetFloat(quad + 0x28, left);
    SetFloat(quad + 0x2C, 240.0f);
    SetFloat(quad + 0x38, right);
    SetFloat(quad + 0x3C, 240.0f);
    const std::uint32_t colour = Dword(entry + (Cond_ByteFF != 0 ? 12 : 8));
    for (unsigned shift = 0; shift < 3; ++shift) {
        const unsigned char top = Channel(colour, shift * 5), bottom = Channel(colour, 16 + shift * 5);
        quad[0x4 + shift] = top;
        quad[0x14 + shift] = top;
        quad[0x24 + shift] = bottom;
        quad[0x34 + shift] = bottom;
    }
    area_backdrop::g.commit(7, 0x44);
}

// original 0x571D30: AreaMap_EntryHandlers 2 (PSX FUN_801592EC) - a texture
// cycle: a VRAM rectangle copied over another on a frame schedule. Byte 0 of
// the entry plus one is the period; word +4 a condition; then pairs of
// dwords (A, B) from +8 whose A's top byte is a frame threshold, in order.
// With f = Frame_Counter mod period, the first pair whose threshold is at or
// above f is taken: the source rect is x = A bits 16..19 << 7 | B bits 9..15,
// + 0x140, y = B byte 0 + 0x100, w = B's top byte, h = B byte 2 + 1; the
// destination x = A bits 20..23 << 7 | A bits 9..15, + 0x140, y = A byte 0 +
// 0x100. Gpu_SetDrawMove, then Gfx_CommitPrim(6, 0x18).
//
// As the original has it: the condition is pushed with stale bits above the
// word (Area_TestCondition reads 16); the scan stops only at a threshold at
// or above f, so a list without one runs on (read, not run: the fuzz ends
// every list with 0xFF).
extern "C" void __cdecl AreaMap_TextureCycle(const unsigned char* entry) {
    if (area_backdrop::g.test(WordAreaBackdrop(entry + 4)) == 0) return;
    const unsigned frame = Frame_Counter % (entry[0] + 1u);
    const unsigned char* pair = entry + 8;
    while (static_cast<int>(frame) > static_cast<int>(pair[3])) pair += 8;
    const std::uint32_t a = Dword(pair), b = Dword(pair + 4);
    unsigned char rect[8];
    SetWordAreaBackdrop(rect, (((a >> 16) & 0xFu) << 7) + ((b >> 9) & 0x7Fu) + 0x140u);
    SetWordAreaBackdrop(rect + 2, (b & 0xFFu) + 0x100u);
    SetWordAreaBackdrop(rect + 4, pair[7]);
    SetWordAreaBackdrop(rect + 6, pair[6] + 1u);
    const std::uint32_t x = (((a >> 20) & 0xFu) << 7) + ((a >> 9) & 0x7Fu) + 0x140u;
    const std::uint32_t y = (a & 0xFFu) + 0x100u;
    area_backdrop::g.set_draw_move(Gfx_PacketNext, rect, x, y);
    area_backdrop::g.commit(6, 0x18);
}

// original 0x571E20: AreaMap_EntryHandlers 3 (PSX FUN_801593FC) - draw-slot
// zones. Nothing while Draw_OtSlot is 4. Else the entry's byte +2 less one
// is a count of zone dwords from +4: x0 = the top byte, z0 = byte 2, a width
// of bits 9..15 and a depth of bits 2..8, and bit 0 the sense. For each of
// the party's three objects whose rounded cell lies in [x0, x0 + width) x
// [z0, z0 + depth): bit 0 set - flag bit 0 set and the slot byte 4; clear -
// flag bit 0 cleared and the slot byte Draw_OtSlot. The fourth object at
// 0x905DA0 gets only its flag bit, by the same test.
//
// As the original has it: the cell is the signed high word of the position
// plus 0x8000, compared as 16 bits against the byte bounds; a count byte of
// 0 or 1 does nothing; Draw_OtSlot is read again for every object.
extern "C" void __cdecl AreaMap_SlotZones(const unsigned char* entry) {
    if (Draw_OtSlot == 4) return;
    int count = static_cast<int>(entry[2]) - 1;
    if (count <= 0) return;
    for (const unsigned char* zone = entry + 4; count > 0; --count, zone += 4) {
        const std::uint32_t z = Dword(zone);
        const int x0 = static_cast<int>(z >> 24), width = static_cast<int>((z >> 9) & 0x7Fu);
        const int z0 = static_cast<int>((z >> 16) & 0xFFu), depth = static_cast<int>((z >> 2) & 0x7Fu);
        const auto inside = [&](const unsigned char* position) {
            const int cx = Cell(position), cz = Cell(position + 4);
            return cx >= x0 && cx < x0 + width && cz >= z0 && cz < z0 + depth;
        };
        for (unsigned k = 0; k < 3; ++k) {
            unsigned char* const object = rv::Bytes(&ObjTrio[k]);
            if (!inside(object + kTrioPosition)) continue;
            if (zone[0] & 1) {
                object[kTrioFlags] |= 1;
                object[kTrioSlot] = 4;
            } else {
                object[kTrioFlags] &= 0xFE;
                object[kTrioSlot] = Draw_OtSlot;
            }
        }
        unsigned char* const fourth = AtAreaBackdrop(::bof3::Img(FieldEvent_Object905DA0_addr));
        if (!inside(fourth + kTrioPosition)) continue;
        if (zone[0] & 1) fourth[kFourthFlags] |= 1;
        else fourth[kFourthFlags] &= 0xFE;
    }
}

// original 0x571FF0: the area's cell runs placed in the view (PSX
// FUN_801595E0). MapView_Cells is cleared; each run of the list from
// AreaMap_CellBase whose map cell lies in the view gets its dword offset from
// the list's start, plus 1, at that cell's ring position - MapView_CellToMap
// turned round: s = x' + y' is the view row, d = x' - y' twice the view
// column (odd rows add one).
//
// As the original has it: the step is the run's high word in dwords, re-read
// after the store (a step of 0 never ends - read, not run); the list ends at
// a zero dword only; the ring position wraps once, so a head out of range
// writes outside MapView_Cells (read, not run); the stored word is the dword
// offset's low 16 bits.
extern "C" void __cdecl MapView_PlaceRuns(void) {
    const std::uint32_t base = static_cast<std::uint32_t>(AreaMap_CellBase) & 0xFFFFu;
    std::memset(MapView_Cells, 0, 0x310u * 4u);
    const unsigned char* const list = AreaMap_Header + base * 4u;
    const unsigned char* run = list;
    for (std::uint32_t d = Dword(run); d != 0; d = Dword(run)) {
        const int x = static_cast<int>((d >> 8) & 0xFFu) - MapView_Origin[0];
        const int y = static_cast<int>(d & 0xFFu) - MapView_Origin[1];
        const int s = y + x, diff = x - y;
        if (s >= 0 && s < 0x38 && diff >= 0 && diff < 0x38) {
            int row = s + MapView_Row + 1;
            if (row >= 0x38) row -= 0x38;
            int column = diff / 2 + MapView_Column + 1;
            if (column >= 0x1C) column -= 0x1C;
            const auto offset = static_cast<std::uint32_t>(static_cast<std::int32_t>(run - list) >> 2) + 1u;
            MapView_Cells[static_cast<std::uint32_t>(column + row * 0x1C)] = static_cast<unsigned short>(offset);
        }
        run += (Dword(run) >> 16) * 4u;
    }
    // DIV-0041 (wide-ring): the runs that fall in the grown ring's rim.
    if (map_rim::Active()) map_rim::RimRuns();
}

// original 0x5720C0: the ground's elevation at (x, y), both 16.16. The cell
// is the high words' in a grid AreaMap_Header bytes 0 by 1; outside it the
// answer is 0 in the low half and the header's own top half above - kept, for
// a caller that reads eax. Inside, with h(dx, dy) a neighbour's height byte
// (at AreaMap_Header + AreaMap_HeightBase * 4, one per cell) times
// MapView_HeightScale and c[k] byte k of the cell's AreaMap_Corners dword
// onward, by quadrant - bit 15 of x, bit 15 of y:
//   neither   (c[1] + c[2] + 2 * h) << 4, in full precision
//   x only    max(c[1] + c[3] + 2h, c[4] + c[6] + 2h(1, 0)) << 4
//   y only    max(c[2] + c[3] + 2h, c[4w] + c[4w + 1] + 2h(0, 1)) << 4
//   both      the largest of c[3] + h, c[6] + h(1, 0), c[4w + 4] + h(1, 1),
//             c[4w + 1] + h(0, 1), << 5
// Kept as the original has it: past the first quadrant each height is its
// multiplication's low byte, and each corner-plus-height a byte sum, before
// they are widened as signed.
extern "C" long __cdecl AreaMap_Elevation(long x, long y) {
    std::uint32_t header;
    std::memcpy(&header, AreaMap_Header, 4);
    const auto cx = static_cast<short>(static_cast<std::uint32_t>(x) >> 16);
    const auto cy = static_cast<short>(static_cast<std::uint32_t>(y) >> 16);
    const unsigned width = header & 0xFFu;
    if (cy >= static_cast<short>((header >> 8) & 0xFFu) || cx >= static_cast<short>(width) || cy < 0 || cx < 0)
        return static_cast<long>(header & 0xFFFF0000u);
    const int cell = cx + cy * static_cast<int>(width);
    const unsigned char* h = AreaMap_Header + static_cast<unsigned>(AreaMap_HeightBase) * 4u + cell;
    const unsigned char* c = reinterpret_cast<const unsigned char*>(&AreaMap_Corners) + cell * 4;
    const unsigned scale = MapView_HeightScale;
    const bool half_x = (x & 0x8000) != 0, half_y = (y & 0x8000) != 0;
    if (!half_x && !half_y) return (S8MapCells(c[2]) + S8MapCells(h[0]) * static_cast<int>(scale) * 2 + S8MapCells(c[1])) * 16;
    // imul r/m8: only the product's low byte is kept.
    auto height = [&](unsigned at) { return static_cast<unsigned char>(h[at] * scale); };
    if (half_x != half_y) {
        const unsigned char here = height(0);
        unsigned char a, b, d, e;
        if (half_x) {
            a = static_cast<unsigned char>(c[1] + here);
            b = static_cast<unsigned char>(c[3] + here);
            const unsigned char next = height(1);
            d = static_cast<unsigned char>(c[4] + next);
            e = static_cast<unsigned char>(c[6] + next);
        } else {
            a = static_cast<unsigned char>(c[2] + here);
            b = static_cast<unsigned char>(c[3] + here);
            const unsigned char next = height(width);
            d = static_cast<unsigned char>(c[4 * width] + next);
            e = static_cast<unsigned char>(c[4 * width + 1] + next);
        }
        const int second = S8MapCells(e) + S8MapCells(d), first = S8MapCells(b) + S8MapCells(a);
        return (first > second ? first : second) * 16;
    }
    const int p0 = S8MapCells(static_cast<unsigned char>(c[3] + height(0)));
    const int p1 = S8MapCells(static_cast<unsigned char>(c[6] + height(1)));
    const int p2 = S8MapCells(static_cast<unsigned char>(c[4 * width + 4] + height(width + 1)));
    const int p3 = S8MapCells(static_cast<unsigned char>(c[4 * width + 1] + height(width)));
    if (p0 >= p3 && p0 >= p2 && p0 >= p1) return p0 * 32;
    if (p1 >= p3 && p1 >= p2) return p1 * 32;
    return (p2 < p3 ? p3 : p2) * 32;
}

// original 0x5722D0: how steep the ground is at (x, y), 16.16, and whether it
// is sloped at all - scratch byte 0 (SharedScratch, the PSX scratchpad's first
// byte) is 1 for sloped, 0 for flat or outside the grid. Its three callers
// that were read (0x46B611, 0x518784, MoveCmd_Move 0x578CC3) test that byte
// straight after the call. The same grid and cell lookup as AreaMap_Elevation;
// per half-cell quadrant:
//   whole cell (bits 15 of x and y clear): flat, and the elevation formula;
//   one half: the two edge sums a + b and d + e of AreaMap_Elevation - flat
//     when b == e and a == d (then (b + a) << 4), else |(a + b) - (d + e)| << 4;
//   both halves: the four corners p0..p3 - flat when all four are equal (then
//     p0 << 5), else by the third argument's low byte n, i = n >> 1:
//     |p[i] - p[(i - 2) & 3]| << 5, or for odd n the larger of p[i] and
//     p[(i + 1) & 3] in place of p[i].
//
// As the original has it:
//  - It uses its caller's argument slots as scratch: x and y become
//    x & 0xFFFF8000 and y & 0xFFFF8000 inside the grid, and y's four bytes
//    then hold b, d, e (from byte 1) or p0..p3. Ours keeps those in locals:
//    no caller reads its pushed slots back (AreaMap_TooSteep, MoveCmd_Move,
//    EffectKind17_BlockedAt, Field_LeaderStepTarget, MapView_SlopeAt's 102
//    callers - every one read discards them), and on x86-64 the arguments
//    are registers with no slot to scribble (the 64-bit build round, B2).
//  - p[i] is not masked: an n of 8 or more reads past the four corners - the
//    third argument's own bytes for 8..15 (kept: `slots` holds them), the
//    caller's stack from 16, which no reimplementation reproduces: a Fatal by
//    name. No caller passes 8 or more (AreaMap_TooSteep masks & 7, the others
//    pass a sprite's facing byte 0..7; event_ops.cpp's note).
//  - Outside the grid the return is AreaMap_Header's top half with ax 0, as
//    AreaMap_Elevation's.
//  - Heights are the low byte of height x MapView_HeightScale (imul r/m8), and
//    corner sums wrap at 8 bits before they are compared or widened.
extern "C" long __cdecl AreaMap_Slope(long x, long y, unsigned long direction) {
    const auto ax = static_cast<std::uint32_t>(x), ay = static_cast<std::uint32_t>(y);
    std::uint32_t header;
    std::memcpy(&header, AreaMap_Header, 4);
    const auto cx = static_cast<short>(ax >> 16), cy = static_cast<short>(ay >> 16);
    const unsigned width = header & 0xFFu;
    if (cy >= static_cast<short>((header >> 8) & 0xFFu) || cx >= static_cast<short>(width) || cy < 0 || cx < 0) {
        Scratch()[0] = 0;
        return static_cast<long>(header & 0xFFFF0000u);
    }
    const std::uint32_t xm = ax & 0xFFFF8000u, ym = ay & 0xFFFF8000u;
    const int cell = cx + cy * static_cast<int>(width);
    const unsigned char* const h = AreaMap_Header + static_cast<unsigned>(AreaMap_HeightBase) * 4u + cell;
    const unsigned char* const c = reinterpret_cast<const unsigned char*>(&AreaMap_Corners) + cell * 4;
    const unsigned scale = MapView_HeightScale;
    if (((xm | ym) & 0xFFFFu) == 0) {   // the whole cell: flat
        Scratch()[0] = 0;
        return static_cast<long>((static_cast<std::uint32_t>(S8(c[2]) + S8(h[0]) * static_cast<int>(scale) * 2 + S8(c[1]))) << 4);
    }
    const auto height = [&](unsigned at) { return static_cast<unsigned char>(h[at] * scale); };
    if (((xm ^ ym) & 0xFFFFu) != 0) {   // one half: the two edges
        const unsigned char here = height(0);
        unsigned char a, b, d, e;
        if (xm & 0x8000u) {
            a = static_cast<unsigned char>(c[1] + here);
            b = static_cast<unsigned char>(c[3] + here);
            const unsigned char next = height(1);
            d = static_cast<unsigned char>(c[4] + next);
            e = static_cast<unsigned char>(c[6] + next);
        } else {
            a = static_cast<unsigned char>(c[2] + here);
            b = static_cast<unsigned char>(c[3] + here);
            const unsigned char next = height(width);
            d = static_cast<unsigned char>(c[4 * width] + next);
            e = static_cast<unsigned char>(c[4 * width + 1] + next);
        }
        if (b == e && a == d) {
            Scratch()[0] = 0;
            return static_cast<long>(static_cast<std::uint32_t>(S8(b) + S8(a)) << 4);
        }
        Scratch()[0] = 1;
        return static_cast<long>(static_cast<std::uint32_t>(Abs(S8(b) - S8(e) - S8(d) + S8(a))) << 4);
    }
    // Both halves: the four corners (the original's y slot), then the
    // direction dword's own bytes (its third slot) - what an unmasked corner
    // index of 4..7 reads in the original.
    unsigned char slots[8];
    slots[0] = static_cast<unsigned char>(c[3] + height(0));
    slots[1] = static_cast<unsigned char>(c[6] + height(1));
    slots[2] = static_cast<unsigned char>(c[4 * width + 4] + height(width + 1));
    slots[3] = static_cast<unsigned char>(c[4 * width + 1] + height(width));
    const auto d32 = static_cast<std::uint32_t>(direction);
    for (unsigned k = 0; k < 4; ++k) slots[4 + k] = static_cast<unsigned char>(d32 >> (8 * k));
    const unsigned char p0 = slots[0];
    if (p0 == slots[3] && p0 == slots[2] && p0 == slots[1]) {
        Scratch()[0] = 0;
        return static_cast<long>(static_cast<std::uint32_t>(S8(p0)) << 5);
    }
    Scratch()[0] = 1;
    const unsigned n = d32 & 0xFFu;
    const unsigned i = n >> 1;
    if (i >= 8)
        bof3::Fatal("AreaMap_Slope: direction %u would read its caller's stack past the corners in the original (no caller passes 8 or more)", n);
    int high = S8(slots[i]);   // unmasked, as the original's: 4..7 are the direction's bytes
    if (n & 1) {
        const int next = S8(slots[(i + 1) & 3]);
        if (high < next) high = next;
    }
    return static_cast<long>(static_cast<std::uint32_t>(Abs(high - S8(slots[(i - 2) & 3]))) << 5);
}

// original 0x572570 (PSX 0x801559E4): the ground at (x, z), with the height
// scale set for the call and cleared after it.
extern "C" long __cdecl MapView_GroundAt(long x, long z) {
    field_event::g.height_check();
    const long ground = field_event::g.elevation(x, z);
    MapView_HeightScale = 0;
    return ground;
}

// original 0x572590 (PSX 0x8015598C): MapView_HeightScale = 1 for the object
// at 0x905DA0 with its +0xB bit 0, or for any other Sprite_Current when
// Field_State +0x138 bit 0 is set; otherwise left as it is.
extern "C" void __cdecl MapView_CheckHeightScale(void) {
    unsigned char* const c = Sprite_Current;
    if (c == At(::bof3::Img(FieldEvent_Object905DA0_addr))) {
        if (c[0xB] & 1) MapView_HeightScale = 1;
        return;
    }
    if (Field_State[0x138] & 1) MapView_HeightScale = 1;
}

// original 0x5725C0 (PSX 0x80155A34): the slope at (x, y) along `direction`
// with the height scale the leader's state asks for: MapView_CheckHeightScale,
// AreaMap_Slope(x, y, direction) - the three dwords passed on whole - then
// MapView_HeightScale = 0; returns AreaMap_Slope's eax untouched (the PSX
// sign-extends its low half; the PC's 102 callers get the whole register).
// AreaMap_Slope's scratch flag 0x903850 is left as it set it.
extern "C" long __cdecl MapView_SlopeAt(long x, long y, unsigned long direction) {
    field_misc::g.check_height_scale();
    const long slope = field_misc::g.slope(x, y, direction);
    AtFieldMisc(field_misc::at::MapView_HeightScaleAt())[0] = 0;
    return slope;
}

// original 0x5725F0 (PSX 0x80155A94). Sets the view's elevation and asks for a redraw; unless
// the area's header word +0x1E is set, the offset moves by two for every unit
// the elevation changed.
//
// The original takes the difference from all 32 bits of the argument and the
// stored elevation from its low 16; since the offset is itself 16 bits the
// first cannot show, and a build that differed there passed the fuzz.
extern "C" void __cdecl MapView_SetElevation(int value) {
    std::uint16_t fixed;
    std::memcpy(&fixed, AreaMap_Header + 0x1E, sizeof fixed);
    if (fixed == 0) {
        const std::uint32_t moved = (static_cast<std::uint32_t>(MapView_Elevation) - static_cast<std::uint32_t>(value)) << 1;
        MapView_ElevationOffset = static_cast<unsigned short>(MapView_ElevationOffset + moved);
    }
    MapView_Elevation = static_cast<short>(value);
    MapView_Redraw = 2;
}

// original 0x572620 (PSX 0x80155AF4): the height layer's byte of cell (x, z) (s16s) set:
// AreaMap_Header[(AreaMap_Header's dword & 0xFF) * z + AreaMap_HeightBase * 4
// + x] = the value's low byte - AreaMap_Elevation's layer (the width byte
// times z, the base a u16), 32-bit sums, unbounded. eax is left the row
// offset; no caller reads it. Area 52's block puzzle, areas 108 and 135, and
// EffectKind30_ClaimCells / _FreeCells (0x46BF80 / 0x46C100; values 0x10, 0). PSX 0x80155AF4
// (pairs_propagated "call").
extern "C" void __cdecl AreaMap_SetHeight(unsigned x, unsigned z, unsigned value) {
    const std::uint32_t width = AreaMap_Header[0];
    const std::uint32_t row = width * UScenaSx2(static_cast<short>(z)) + static_cast<std::uint32_t>(AreaMap_HeightBase) * 4;
    AreaMap_Header[row + UScenaSx2(static_cast<short>(x))] = static_cast<unsigned char>(value);
}

extern "C" __attribute__((naked)) unsigned char __cdecl MoveCmd_TestFB(short, short) {
    asm("jmp " BOF3_SYM(FieldMisc_TestFB));
}

extern "C" __attribute__((naked)) unsigned char __cdecl MoveCmd_TestFC(short, short) {
    asm("jmp " BOF3_SYM(FieldMisc_TestFC));
}

// original 0x5728D0 (PSX 0x80155E74): a map cell's byte cleared and its drawn records
// retyped. AreaMap_Bytes[x + z * width] = 0 (s16 x and z, the header's width
// byte). Then the cell in the view: c = x + 1 - MapView_Origin's x, r = z -
// its z (s16s); with c + r in 0..0x37 and c - r in 0..0x37, the row (c + r +
// MapView_Row + 1, wrapped once at 0x38) and column ((c - r) / 2 +
// MapView_Column + 1, wrapped once at 0x1C) name a MapView_Cells word; not
// 0, it plus AreaMap_CellBase's word is a run of records in the area block
// (their count the high word of the dword before), walked by each record's
// byte +2 dwords, and every record of kind 3, 9 or 0x27 (the top byte)
// becomes kind 0x30. Ours aborts where the original reads past MapView_Cells
// (a row or column past its wrap) or where the walk would never end (a step
// of 0, or past the run's end). Called by FC2's 0x46D180, Field_CellPickup
// and 19 Capcom functions; FieldHidden_ClearCell's cells. PSX twin
// 0x80155E74 (gap4, a hypothesis).
extern "C" void __cdecl AreaMap_ClearCell(unsigned x, unsigned z) {
    const int zs = static_cast<short>(z), xs = static_cast<short>(x);
    const U width = LongAt(AreaMap_Header) & 0xFF;
    const_cast<unsigned char*>(AreaMap_Bytes.Native())[static_cast<std::int32_t>(width * static_cast<U>(zs)) + xs] = 0;
    const int c = static_cast<short>(x + 1u - static_cast<std::uint16_t>(MapView_Origin[0]));
    const int r = static_cast<short>(z - static_cast<std::uint16_t>(MapView_Origin[1]));
    int row = c + r;
    if (row >= 0x38 || row < 0) return;
    row += MapView_Row + 1;
    if (row >= 0x38) row -= 0x38;
    int column = c - r;
    if (column >= 0x38 || column < 0) return;
    column = column / 2 + MapView_Column + 1;
    if (column >= 0x1C) column -= 0x1C;
    if (row < 0 || row >= 0x38 || column < 0 || column >= 0x1C)
        bof3::Fatal("AreaMap_ClearCell: view cell (%d, %d) past MapView_Cells' 0x38 x 0x1C (the original reads there)", row, column);
    const U item = WFieldE2(::bof3::Img(field_e2::at::kMapCells) + 2u * static_cast<U>(column + row * 0x1C));
    if (item == 0) return;
    const U n = item + (AreaMap_CellBase & 0xFFFF);
    U p = ::bof3::Img(AreaMap_Header_addr) + n * 4u;
    const U end = p + (LFieldE2(::bof3::Img(field_e2::at::kCellRecords) - 4u + n * 4u) >> 16) * 4u - 4u;
    while (p != end) {
        if (p > end) bof3::Fatal("AreaMap_ClearCell: the record walk passed its end 0x%X at 0x%X (the original runs on)", (unsigned)end, (unsigned)p);
        const U v = LFieldE2(p);
        const U kind = v & 0xFF000000u;
        if (kind == 0x03000000u || kind == 0x09000000u || kind == 0x27000000u) SetL(p, (v & 0xFFFFFFu) | 0x30000000u);
        const U step = BFieldE2(p + 2);
        if (step == 0) bof3::Fatal("AreaMap_ClearCell: a record of 0 dwords at 0x%X (the original never ends)", (unsigned)p);
        p += step * 4u;
    }
}

// original 0x572A00. Texture a run of `count` POLY_FT4s of 0x48 bytes from one
// packed word - the texture of a cell of the field view's map, and of much
// else (91 call sites). The word, bit by bit:
//
//   0..11   where the four (u, v) come from - below
//   12, 13  the semi-transparency mode, into the tpage
//   15      semi-transparency on (Gpu_SetSemiTrans)
//   16      mirror left to right;  17  top to bottom;  18  a quarter turn
//   19..23  the shade: r = g = b = (word >> 16) & 0xF8
//   24..27  the palette: a 4-bit texture's CLUT column, an 8-bit one's row
//   28, 29  the texture page, x = 320 + 128n, y = 256
//   31      set: a 4-bit texture; clear: 8-bit
//
// The corners, in the POLY_FT4's order (top left, top right, bottom left,
// bottom right):
//   - bits 8..11 clear: a 16-texel cell of a 16 x 16 grid, bits 0..3 its
//     column and 4..7 its row, the corners 15 apart;
//   - bit 11 set: two dwords of the area block (AreaMap_Header) at dword
//     u16[+6 + 4n] plus twice bits 0..10 - the four corners a byte each;
//   - otherwise: one dword at dword u16[+4 + 4n] plus bits 0..7 - the top
//     left's u and v, then a width and a height.
// The original takes each dword's top byte signed. Only a coordinate's low
// byte reaches the primitive, and that is the same either way, width added
// or not; the fuzz cannot see it (a control that drops the sign passes), so
// ours keeps the sign only to read like the original.
//
// The original goes through x87: each coordinate an integer to a float, the
// rectangle's far edges one float add, each result back through _ftol. Every
// such value is an integer of magnitude under 400, so every step is exact at
// any precision and in any rounding mode, and _ftol truncates: integers are
// the same arithmetic. The fuzz runs theirs under four control words to hold
// that claim to account. Its locals also hold four floats it never writes,
// swapped along with the corners by the turn and the mirrors: stale stack,
// and none of it reaches the primitive.
//
// Kept as the original has it: a count of 0 or less draws nothing; the turn
// comes first, then the mirrors.
extern "C" void __cdecl Prim_SetTexture(unsigned long texture, unsigned char* prim, int count) {
    const auto word = static_cast<std::uint32_t>(texture);
    const unsigned page = (word >> 28) & 3;
    const bool eight_bit = (word & 0x80000000u) == 0;
    const unsigned char* area = AreaMap_Header;
    auto u16_at = [area](unsigned at) { std::uint16_t x; std::memcpy(&x, area + at, 2); return unsigned{x}; };
    auto u32_at = [area](unsigned at) { std::uint32_t x; std::memcpy(&x, area + at, 4); return x; };
    int u[4], v[4];
    if ((word & 0xF00) == 0) {
        u[0] = u[2] = static_cast<int>((word & 0xF) << 4);
        v[0] = v[1] = static_cast<int>(word & 0xF0);
        u[1] = u[3] = u[0] + 15;
        v[2] = v[3] = v[0] + 15;
    } else if (word & 0x800) {
        const unsigned at = (u16_at(6 + 4 * page) + (word & 0x7FF) * 2) * 4;
        const std::uint32_t first = u32_at(at), second = u32_at(at + 4);
        u[0] = static_cast<std::int32_t>(first) >> 24;
        v[0] = static_cast<int>((first >> 16) & 0xFF);
        u[1] = static_cast<int>((first >> 8) & 0xFF);
        v[1] = static_cast<int>(first & 0xFF);
        u[2] = static_cast<std::int32_t>(second) >> 24;
        v[2] = static_cast<int>((second >> 16) & 0xFF);
        u[3] = static_cast<int>((second >> 8) & 0xFF);
        v[3] = static_cast<int>(second & 0xFF);
    } else {
        const std::uint32_t cell = u32_at((u16_at(4 + 4 * page) + (word & 0xFF)) * 4);
        u[0] = u[2] = static_cast<std::int32_t>(cell) >> 24;
        v[0] = v[1] = static_cast<int>((cell >> 16) & 0xFF);
        u[1] = u[3] = u[0] + static_cast<int>((cell >> 8) & 0xFF);
        v[2] = v[3] = v[0] + static_cast<int>(cell & 0xFF);
    }
    if (word & 0x40000) {   // corners 0, 1, 2, 3 take old 2, 0, 3, 1
        const int ou[4] = {u[0], u[1], u[2], u[3]}, ov[4] = {v[0], v[1], v[2], v[3]};
        static const int kFrom[4] = {2, 0, 3, 1};
        for (int i = 0; i < 4; ++i) u[i] = ou[kFrom[i]], v[i] = ov[kFrom[i]];
    }
    auto swap = [&u, &v](int a, int b) { std::swap(u[a], u[b]); std::swap(v[a], v[b]); };
    if (word & 0x10000) swap(0, 1), swap(2, 3);
    if (word & 0x20000) swap(0, 2), swap(1, 3);

    const auto shade = static_cast<unsigned char>((word >> 16) & 0xF8);
    const unsigned palette = (word >> 24) & 0xF;
    // getClut(x, y) is y << 6 | x >> 4: an 8-bit texture's palette is row
    // 0x1E3 + n at x 0, a 4-bit one's is x 16n of row 0x1E3.
    const auto clut = static_cast<std::uint16_t>(eight_bit ? 0x78C0 + (palette << 6) : 0x78C0 | palette);
    const auto tpage = static_cast<std::uint16_t>(
        ((((word & 0x3000) | 0x800) >> 1 | (((page << 7) + 0x140) & 0x3C0)) >> 6) | (eight_bit ? 0x80u : 0u));
    const unsigned semi_trans = (word >> 15) & 1;
    for (; count > 0; --count, prim += kTexPrimBytes) {
        Gpu_SetSemiTrans(prim, semi_trans);
        prim[4] = prim[5] = prim[6] = shade;
        std::memcpy(prim + 0x16, &clut, 2);
        std::memcpy(prim + 0x26, &tpage, 2);
        for (int i = 0; i < 4; ++i) {
            prim[0x14 + 0x10 * i] = static_cast<unsigned char>(u[i]);
            prim[0x15 + 0x10 * i] = static_cast<unsigned char>(v[i]);
        }
    }
}

// original 0x572ED0 (PSX 0x801563AC): the draw item of map cell (x, y) - the
// low 12 bits of the word at +2 of its MapView_CellItems slot - or 0 when the
// cell is outside the view's ring. The arguments are whole dwords, less
// MapView_Origin's words sign-extended, in 32 bits (MoveCmd_TestFB's subtract
// in 16). Returns the whole eax: the masked word, or 0.
//
// DIV-0041 (wide-ring): outside Capcom's window but inside the grown ring, the
// rim's item - every caller textures, draws or allocates draw items with it
// (docs/wide-ring-read.md 1.3).
extern "C" unsigned long __cdecl MapView_ItemAt(long x, long y) {
    const auto dx = static_cast<std::int32_t>(static_cast<field_misc::U>(x) - static_cast<field_misc::U>(Short(OriginX())));
    const auto dz = static_cast<std::int32_t>(static_cast<field_misc::U>(y) - static_cast<field_misc::U>(Short(OriginZ())));
    const Ring ring = RingCell(dx, dz);
    if (!ring.inside) return map_rim::Active() ? map_rim::RimItemAt(dx, dz) : 0;
    return WordFieldMisc(AtFieldMisc(field_misc::at::MapView_CellItemsAt() + 2u + (ring.column + ring.row * 28u) * 4u)) & 0xFFF;
}

// original 0x572F70 (PSX 0x80156468): the 0x48-byte half for this display
// buffer of map cell (x, y)'s draw item - DrawItems + (Gfx_BufferIndex + item
// * 2) * 0x48, the buffer index read after MapView_ItemAt - or 0 when the
// cell has none. The whole of MapView_ItemAt's eax is the item.
extern "C" __attribute__((disable_tail_calls)) unsigned char* __cdecl MapView_ItemHalfAt(long x, long y) {
    const world_map::U item = static_cast<world_map::U>(world_map::g.item_at(x, y));
    if (item == 0) return nullptr;
    return AtWorldMap(world_map::at::DrawItemsAt() + (AtWorldMap(world_map::at::Gfx_BufferIndexAt())[0] + item * 2u) * 0x48u);
}

// original 0x572FA0 (PSX 0x801564C4): links the primitive at Gfx_PacketNext
// into the map-view row of map (x, z) - the arguments' high words less
// MapView_Origin's, plus one each for a non-zero low word, plus `dy` (its low
// byte, signed) plus 2 - when that row is 0..0x37 and the pool has room:
// (Gfx_BufferIndex << 16) + 0x7F1BAC above Gfx_PacketNext + (size & 0xFF),
// unsigned. Then Gpu_LinkPrim(tail, prim) with the row's tail for this
// buffer - the dword at 0x8022C4 + (Gfx_BufferIndex + row * 6) * 8 - and,
// with the buffer index and Gfx_PacketNext READ AGAIN after that call, the
// tail slot = Gfx_PacketNext and Gfx_PacketNext += size. No result; the
// original's eax is the tail slot's new value or, on the exits, the row or
// the bound - its 371 callers (E8 scan) are draw code that reloads eax.
extern "C" __attribute__((disable_tail_calls)) void __cdecl MapView_LinkPrimAt(unsigned long x, unsigned long z,
                                                                           int dy, unsigned size) {
    std::int32_t row = static_cast<std::int16_t>(z >> 16);
    row -= (z & 0xFFFF) == 0 ? 1 : 0;
    row -= (x & 0xFFFF) == 0 ? 1 : 0;
    row -= ShortWorldMap(AtWorldMap(world_map::at::MapView_OriginAt() + 2));
    row += static_cast<std::int16_t>(x >> 16);
    row -= ShortWorldMap(AtWorldMap(world_map::at::MapView_OriginAt()));
    row += static_cast<signed char>(dy) + 2;
    if (row < 0 || row >= 0x38) return;
    const world_map::U bytes = size & 0xFF;
    const world_map::U prim = LongWorldMap(AtWorldMap(world_map::at::Gfx_PacketNextAt()));
    // Tooling only (BOF3X_DRAWORDER): the walk's log names a frame node by its
    // row alone; this says who linked it.
    if (draw_order::Tagging())
        bof3::Log("draworder   linked %08X (%u bytes) into row %d from %08X", prim, bytes, row,
                  Low32(__builtin_return_address(0)));
    const world_map::U bound = (static_cast<world_map::U>(AtWorldMap(world_map::at::Gfx_BufferIndexAt())[0]) << 16) + ::bof3::Img(kPoolBound);
    if (bound <= prim + bytes) {
        if (draw_order::Tagging())
            bof3::Log("draworder   packet pool full: a %u-byte primitive for row %d skipped (MapView_LinkPrimAt) from %08X", bytes, row,
                      Low32(__builtin_return_address(0)));
        return;
    }
    const world_map::U slot_index = static_cast<world_map::U>(row) * 6u;
    unsigned char* const slot = AtWorldMap(::bof3::Img(DrawLayers_addr) + 0x24 + (slot_index + AtWorldMap(world_map::at::Gfx_BufferIndexAt())[0]) * 8u);
    world_map::g.link_prim(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(LongWorldMap(slot))), prim);
    const world_map::U next = LongWorldMap(AtWorldMap(world_map::at::Gfx_PacketNextAt()));
    PutLong(AtWorldMap(::bof3::Img(DrawLayers_addr) + 0x24 + (slot_index + AtWorldMap(world_map::at::Gfx_BufferIndexAt())[0]) * 8u), next);
    PutLong(AtWorldMap(world_map::at::Gfx_PacketNextAt()), next + bytes);
}

// original 0x573050 (PSX 0x801565B4, the same two calls): unless bit 0 of
// Field_StatusBits 0x8034E1, every area CLUT (rows 3..14, every column) tinted
// by level on all three channels - Gfx_ClutAdjust(0xFFFF, 0xFFF, level, level,
// level) - then AreaMap_ClutCycleStart, a tail jump. Three callers (rel32
// scan), each a state step reached through a pointer table that counts a
// byte down or up by one and passes it sign-extended (0x4B51E0, 0x4FB010,
// 0x4FB070) and returns what this leaves in eax. That is the caller's own eax
// on the early exit and AreaMap_ClutCycleStart's (ours, void) on the other,
// so nothing can rely on it: ours is void.
extern "C" void __cdecl AreaMap_TintClut(int level) {
    if (Field_StatusBits & 1) return;
    battle_draw::g.clut_adjust(0xFFFF, 0xFFF, level, level, level);
    battle_draw::g.clut_cycle();
}

// original 0x573080: the kind-2 object's frame (PSX FUN_801C67EC) - a jump
// through Kind2_RunTable by the run byte k +1.
//
// As the original has it: the index is the whole byte and nothing checks it.
// The run table is two entries (0x437CC0, a bare ret, and Kind2_Dispatch) and
// Kind2_StateTable follows it directly, so a run byte of 2..6 would run the
// states 0..4 without the dispatch, 7 would jump to address 0 (the dword at
// 0x6632C8) and anything larger to what follows. The exe stores only 0 and 1
// there directly (docs/kind2-object.md section 3); op E5 copies k +3 into it
// when the kind-2 object's own script runs it.
extern "C" void __cdecl Kind2_Run(void) {
    kind2_object::gKind2Object.run_table[Sprite_Kind2.state]();
}

// original 0x573090: the state k +2 through Kind2_StateTable (the PSX
// 0x801C6830). As the original has it: unchecked, a whole byte - a state of 5
// jumps to address 0.
extern "C" void __cdecl Kind2_Dispatch(void) {
    kind2_object::gKind2Object.state_table[Sprite_Kind2.step]();
}

// original 0x5730A0, state 0: one step of the kind-2 object's movement script
// (PSX FUN_801C686C).
//
// As the original has it:
//  - Sprite_Current is left pointing at Sprite_Kind2 (the step needs it; it is
//    not put back).
//  - The glide (result 0xFF with context bit 4) takes its distance as the
//    larger of |dx| and |dz| in wrapping 32-bit arithmetic, where |INT_MIN| is
//    INT_MIN; and its frame count as the HIGH word of twice that distance,
//    signed - 16.16 whole units, so a distance of 0x40000000 or more counts
//    negative frames.
//  - The two divisions fault as the original's do (Idiv): 0x80 by
//    MoveScript_F3Divisor, which is 0 for speed index 0 (and past the table's
//    six bytes, 6 and 7); then by quotient x frames, which is 0 for a
//    distance under 0x8000 - half a unit - and for a speed above 16 (index 8
//    reads 0x40). docs/kind2-object.md section 5 has these as defects.
//  - MoveScript_F3Divisor, the elevation long and MapView_Elevation are read
//    back after AreaMap_Elevation, not kept from before it.
extern "C" void __cdecl Kind2_Script(void) {
    unsigned char* const k = rv::Bytes(&Sprite_Kind2);
    if (k[0x81] != 0) {   // a wait: count it down
        --k[0x81];
        return;
    }
    const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
    const auto scripts = static_cast<std::uint32_t>(Long(descriptor + 0x1C));   // a table of 32-bit places
    Sprite_Current = k;
    const unsigned char result = kind2_object::gKind2Object.step(k + 0x80, At(static_cast<std::uint32_t>(Long(At(scripts + 4u * k[0x83])))));
    const unsigned char flags = k[0x80];
    k[8] = result;
    if (flags & 8) {   // the script ended: idle, at speed 3
        k[0x80] = static_cast<unsigned char>(flags & 0xF7);
        k[1] = 0;
        k[2] = 0;
        k[3] = 0;
        k[0x84] = 3;
        return;
    }
    if (result == 0xFF) {
        if (flags & 4) {   // a glide toward Field_Kind2X / Z
            const long x = Field_Kind2X, z = Field_Kind2Z;
            const std::int32_t dx = Abs32(static_cast<std::uint32_t>(x) - static_cast<std::uint32_t>(rv::As<records::SpriteObject>(k)->x));
            const std::int32_t dz = Abs32(static_cast<std::uint32_t>(z) - static_cast<std::uint32_t>(rv::As<records::SpriteObject>(k)->z));
            const std::int32_t distance = dx >= dz ? dx : dz;
            if (distance == 0) {   // already there
                k[0x80] = static_cast<unsigned char>(flags & 0xFB);
                k[2] = 0;
                return;
            }
            const std::uint32_t twice = static_cast<std::uint32_t>(distance) * 2u;
            const unsigned char speed = Field_MoveSpeeds[k[0x84]];   // an unchecked byte index, as the original's
            MoveScript_F3Divisor = static_cast<short>(static_cast<std::uint16_t>(speed << 3));
            const long ground = kind2_object::gKind2Object.elevation(x, z);
            const auto rest = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteObject>(k)->height) -
                                                        (static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16));
            const std::int32_t per_speed = Idiv(0x80, MoveScript_F3Divisor);
            MoveScript_FAWord = 0;
            k[2] = 3;
            const auto frames = static_cast<std::int32_t>(static_cast<std::int16_t>(twice >> 16));
            const std::int32_t slope = Idiv(rest, static_cast<std::int32_t>(static_cast<std::uint32_t>(per_speed) * static_cast<std::uint32_t>(frames)));
            Field_Kind2X = rv::As<records::SpriteObject>(k)->x;
            Field_Kind2Z = rv::As<records::SpriteObject>(k)->z;
            rv::As<records::SpriteObject>(k)->step_y = slope;
            rv::Store<unsigned>(rv::As<records::SpriteHead>(k)->height_whole, static_cast<std::uint16_t>(MapView_Elevation));
            return;
        }
        // No glide: the view is reset on the object where it stands.
        Field_Kind2X = rv::As<records::SpriteObject>(k)->x;
        Field_Kind2Z = rv::As<records::SpriteObject>(k)->z;
        kind2_object::gKind2Object.view_reset();
        k[8] = 0;
        k[2] = 0;
        return;
    }
    if (flags & 0x20) {
        k[2] = 2;
        return;
    }
    if (flags & 0x80) {
        k[2] = 4;
        return;
    }
    if (k[0x87] == 0) {
        k[2] = 0;
        return;
    }
    // A timed move began: its first frame of elevation, then state 1.
    rv::As<records::SpriteObject>(k)->height = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteObject>(k)->height) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(k)->step_y));
    kind2_object::gKind2Object.set_elevation(static_cast<std::uint16_t>(rv::As<records::SpriteHead>(k)->height_whole));
    k[2] = 1;
}

// original 0x5732B0, state 1: the timed move (PSX 0x801C6BB0). While
// Field_Kind2Hold, one more frame of elevation; when it clears, the view is
// grounded where Field_Kind2X / Z stand (unless context bit 0x40), and the
// script steps again the same frame.
//
// As the original has it: MapView_SetElevation is given the high word of the
// elevation long (the original pushes a dword whose high half is that word
// again; the callee uses the low 16 bits only). The context flags are read
// after the two calls. The script step is a tail jump (the PSX calls it).
extern "C" void __cdecl Kind2_Move(void) {
    unsigned char* const k = rv::Bytes(&Sprite_Kind2);
    if (Field_Kind2Hold != 0) {
        rv::As<records::SpriteObject>(k)->height = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteObject>(k)->height) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(k)->step_y));
        kind2_object::gKind2Object.set_elevation(static_cast<std::uint16_t>(rv::As<records::SpriteHead>(k)->height_whole));
        return;
    }
    if (!(k[0x80] & 0x40)) kind2_object::gKind2Object.set_elevation(static_cast<int>(kind2_object::gKind2Object.elevation(Field_Kind2X, Field_Kind2Z)));
    const unsigned char flags = k[0x80];
    k[0x87] = 0;
    k[2] = 0;
    k[0x80] = static_cast<unsigned char>(flags & 0xBF);
    kind2_object::gKind2Object.script();
}

// original 0x573320, state 2: wait while Field_Request is 2 (PSX 0x801C6C74).
extern "C" void __cdecl Kind2_WaitRequest(void) {
    if (Field_Request == 2) return;
    unsigned char* const k = rv::Bytes(&Sprite_Kind2);
    const unsigned char flags = k[0x80];
    k[2] = 0;
    k[0x80] = static_cast<unsigned char>(flags & 0xDF);
}

// original 0x573340, state 3: the glide Kind2_Script set up (PSX 0x801C6CAC).
// While Field_Kind2Hold, the slope k +0x14 is added to the elevation - if it
// is not 0; when it clears, the view is grounded where Field_Kind2X / Z stand
// and the glide ends. As the original has it: the context flags are read after
// the two calls.
extern "C" void __cdecl Kind2_Travel(void) {
    unsigned char* const k = rv::Bytes(&Sprite_Kind2);
    if (Field_Kind2Hold != 0) {
        const std::int32_t slope = rv::As<records::SpriteObject>(k)->step_y;
        if (slope == 0) return;
        rv::As<records::SpriteObject>(k)->height = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteObject>(k)->height) + static_cast<std::uint32_t>(slope));
        kind2_object::gKind2Object.set_elevation(static_cast<std::uint16_t>(rv::As<records::SpriteHead>(k)->height_whole));
        return;
    }
    kind2_object::gKind2Object.set_elevation(static_cast<int>(kind2_object::gKind2Object.elevation(Field_Kind2X, Field_Kind2Z)));
    const unsigned char flags = k[0x80];
    rv::As<records::SpriteObject>(k)->step_y = 0;
    k[0x80] = static_cast<unsigned char>(flags & 0xFB);
    k[8] = 0;
    k[2] = 0;
}

// original 0x5733B0, state 4: k +9 frames of rising (PSX FUN_801C6D60).
//
// As the original has it: the rise is 16-bit - the elevation's high word plus
// the LOW word of k +0x14, wrapping - where state 1 and 3 add whole longs; and
// the frame count is read again after MapView_SetElevation.
extern "C" void __cdecl Kind2_Lift(void) {
    unsigned char* const k = rv::Bytes(&Sprite_Kind2);
    if (k[9] == 0) {
        const unsigned char flags = k[0x80];
        k[2] = 0;
        k[0x80] = static_cast<unsigned char>(flags & 0x7F);
        return;
    }
    const auto risen = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(k)->height_whole + Word(k + 0x14));
    rv::Store<unsigned>(rv::As<records::SpriteHead>(k)->height_whole, risen);
    kind2_object::gKind2Object.set_elevation(risen);
    --k[9];
}

// original 0x573400: the kind-2 object's move (PSX FUN_801C6DF4), the twin of
// MoveCmd_Move: the divisor speed * (4 for directions 2 and 6, else 8) to
// MoveScript_F3Divisor; the position moved by MoveCmd_F9Steps[direction] * steps *
// (0x80 / divisor) * speed, kept in Field_Kind2X / Z as well; then the rise to
// the ground there spread over (0x80 / divisor) * steps frames into +0x14, and
// MoveScript_FAWord cleared.
//
// As the original has it: every product wraps in 32 bits; the divisor is 16
// bits; the rise is (ground - height) as 16-bit values, shifted up 16 and
// wrapping; the steps byte and the divisor are read again after the ground;
// a speed of 0, a divisor above 0x80 or 0 steps divide by zero - as the
// original faults (Field_MoveSpeeds is 0, 1, 2, 4, 8, 16, so only entry 0 or
// a speed index past the table can); any direction byte indexes the steps.
extern "C" void __cdecl MoveCmd_MoveKind2(unsigned char direction) {
    unsigned char* const k2 = Kind2();
    const unsigned multiplier = (direction == 2 || direction == 6) ? 4 : 8;
    const unsigned char speed = At(AddressMoveCmds(Field_MoveSpeeds))[k2[kK2Speed]];
    const auto divisor = static_cast<short>(static_cast<std::uint16_t>(speed * multiplier));
    MoveScript_F3Divisor = divisor;
    const std::int32_t per_frame = 0x80 / static_cast<std::int32_t>(divisor);
    const unsigned char* const step = reinterpret_cast<const unsigned char*>(MoveCmd_F9Steps + direction * 2u);
    const std::int32_t steps = k2[kK2Steps];
    const std::int32_t dx = Mul(Mul(Mul(Long(step), steps), per_frame), speed);
    const std::int32_t dz = Mul(Mul(Mul(Long(step + 4), steps), per_frame), speed);
    const std::int32_t x = Add(Long(k2 + kK2X), dx);
    const std::int32_t z = Add(Long(k2 + kK2Z), dz);
    SetLong(k2 + kK2X, x);
    SetLong(k2 + kK2Z, z);
    Field_Kind2X = x;
    Field_Kind2Z = z;
    const long ground = move_cmds::g.elevation(x, z);
    const std::int32_t frames = Mul(0x80 / static_cast<std::int32_t>(MoveScript_F3Divisor), k2[kK2Steps]);
    MoveScript_FAWord = 0;
    const std::int32_t rise = static_cast<short>(static_cast<std::uint16_t>(ground)) - static_cast<short>(Word(k2 + kK2Height));
    SetLong(k2 + kK2Rise, static_cast<std::int32_t>(static_cast<std::uint32_t>(rise) << 16) / frames);
}

// original 0x5734F0: the kind-2 object placed (PSX FUN_801C7014): its script
// context reset, +0x85 bit 7, script `script` at +0x83, speed index 3, at
// (Field_Kind2X, Field_Kind2Z) on the ground, and +1 = 1, +2 = +3 = 0.
// As the original has it: +0x85 is read after the reset (which clears it);
// MapView_SetElevation gets the ground's whole dword where the PSX passes it
// as a short - AreaMap_Elevation's high half is not always 0.
extern "C" void __cdecl Kind2_Place(unsigned char script) {
    unsigned char* const k2 = Kind2();
    move_cmds::g.context_reset(k2 + kK2Context);
    k2[kK2Flags] |= 0x80;
    const long z = Field_Kind2Z;
    k2[kK2Script] = script;
    const long x = Field_Kind2X;
    k2[kK2Speed] = 3;
    SetLong(k2 + kK2X, x);
    SetLong(k2 + kK2Z, z);
    const long ground = move_cmds::g.elevation(x, z);
    SetWord(k2 + kK2Height, static_cast<std::uint32_t>(ground));
    move_cmds::g.set_elevation(ground);
    k2[1] = 1;
    k2[2] = 0;
    k2[3] = 0;
}

void AreaMap_Inject() {
    BOF3_INJECT(AreaMap_Frame);
    BOF3_INJECT(MapView_ShiftColumnPrev);
    BOF3_INJECT(MapView_ShiftColumnNext);
    BOF3_INJECT(MapView_ShiftRowsPrev);
    BOF3_INJECT(MapView_ShiftRowsNext);
    BOF3_INJECT(MapView_Build);
    BOF3_INJECT(DrawLayers_Reset);
    BOF3_INJECT(DrawTable_Sort);
    BOF3_INJECT(Field_ViewReset);
    BOF3_INJECT(MapView_CellToMap);
    BOF3_INJECT(MapView_CellTextures);
    BOF3_INJECT(AreaMap_BakePatches);
    BOF3_INJECT(DrawItemPool_Alloc);
    BOF3_INJECT(DrawItemPool_ReleaseCell);
    BOF3_INJECT(DrawItemPool_Release);
    BOF3_INJECT(MapView_FillCells);
    BOF3_INJECT(DrawLayer_Open);
    BOF3_INJECT(DrawLayer_Close);
    BOF3_INJECT(Area_TestCondition);
    BOF3_INJECT(MapCell_DrawQuads);
    BOF3_INJECT(MapCell_DrawUprights);
    BOF3_INJECT(MapCell_DrawDiagonalWall);
    BOF3_INJECT(MapCell_DrawRising);
    BOF3_INJECT(MapCell_DrawFrames);
    BOF3_INJECT(MapCell_DrawFlatFaces);
    BOF3_INJECT(MapCell_FlatOverlay);
    BOF3_INJECT(MapCell_DrawShaded);
    BOF3_INJECT(MapCell_DrawSpinning);
    BOF3_INJECT(MapCell_PatchThenStep);
    BOF3_INJECT(MapCell_PatchThenStop);
    BOF3_INJECT(AreaMap_ApplyPatch);
    BOF3_INJECT(MapCell_DrawAnimated);
    BOF3_INJECT(MapCell_DrawWalls);
    BOF3_INJECT(AreaMap_SetupEntries);
    BOF3_INJECT(AreaMap_ClutCycleStart);
    BOF3_INJECT(AreaMap_SetupTint);
    BOF3_INJECT(Gfx_ClutAdjust);
    BOF3_INJECT(AreaMap_SetupFlatColours);
    BOF3_INJECT(AreaMap_SetupTexture);
    BOF3_INJECT(AreaMap_HeaderPass);
    BOF3_INJECT(AreaMap_ClutCycle);
    BOF3_INJECT(AreaMap_DrawBackdrop);
    BOF3_INJECT(AreaMap_TextureCycle);
    BOF3_INJECT(AreaMap_SlotZones);
    BOF3_INJECT(MapView_PlaceRuns);
    BOF3_INJECT(AreaMap_Elevation);
    BOF3_INJECT(AreaMap_Slope);
    BOF3_INJECT(MapView_GroundAt);
    BOF3_INJECT(MapView_CheckHeightScale);
    BOF3_INJECT(MapView_SlopeAt);
    BOF3_INJECT(MapView_SetElevation);
    BOF3_INJECT(AreaMap_SetHeight);
    BOF3_INJECT(MoveCmd_TestFB);
    BOF3_INJECT(MoveCmd_TestFC);
    BOF3_INJECT(AreaMap_ClearCell);
    BOF3_INJECT(Prim_SetTexture);
    BOF3_INJECT(MapView_ItemAt);
    BOF3_INJECT(MapView_ItemHalfAt);
    BOF3_INJECT(MapView_LinkPrimAt);
    BOF3_INJECT(AreaMap_TintClut);
    BOF3_INJECT(Kind2_Run);
    BOF3_INJECT(Kind2_Dispatch);
    BOF3_INJECT(Kind2_Script);
    BOF3_INJECT(Kind2_Move);
    BOF3_INJECT(Kind2_WaitRequest);
    BOF3_INJECT(Kind2_Travel);
    BOF3_INJECT(Kind2_Lift);
    BOF3_INJECT(MoveCmd_MoveKind2);
    BOF3_INJECT(Kind2_Place);
}
