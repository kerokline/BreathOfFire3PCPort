#include "game/map_scroll.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/map_scroll_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

// The view's cell ring - its scrolling and its set-up for an area - see
// docs/map-scroll.md for each function, its PSX twin and the fuzz.
//
// MapView_CellItems is a ring of 0x38 rows by 0x1C columns, one 4-byte cell
// each (map x, map y, a draw-item word), headed by MapView_Row and
// MapView_Column: the slot at the head is the one just before view row /
// column 0. A scroll never moves a cell. It releases and refills the ring
// slots that come into view through MapView_CellToMap and moves the head and
// MapView_Origin by one map cell.

namespace map_scroll {

const Callees kOriginals = {
    DrawItemPool_ReleaseCell, MapView_CellToMap,
    Gte_SetGeomScreen, Gte_SetGeomOffset, Gte_SetBackColor, Gpu_SetPolyFT4, Gpu_SetShadeTex, AreaMap_Elevation,
    AreaMap_BakePatches, AreaMap_SetupEntries, MapView_PlaceRuns,
    AreaMap_ClutCycleStart, AreaMap_ApplyPatch,
};
Callees g = kOriginals;

}  // namespace map_scroll

namespace {

using map_scroll::g;

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

// Ring cell (row, col), computed as the original does - in 32 bits, unchecked.
unsigned char* Cell(int row, int col) {
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

}  // namespace

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
        unsigned char* const cell = Cell(row, MapView_Column);
        g.release_cell(cell);
        g.cell_to_map(i, -1, cell);
    }
    if (MapView_Column == 0) {
        OriginStep(-1, 1);
        MapView_Column = 0x1B;
    } else {
        MapView_Column = static_cast<short>(MapView_Column - 1);
        OriginStep(-1, 1);
    }
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
        unsigned char* const cell = Cell(row, column);
        g.release_cell(cell);
        g.cell_to_map(i, 0x1C, cell);
    }
    OriginStep(1, -1);
    MapView_Column = static_cast<short>(column);
}

// original 0x56EAB0: the view one map cell back along x and y - two ring rows
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
            unsigned char* const cell = Cell(MapView_Row, column);
            g.release_cell(cell);
            g.cell_to_map(view_row, j, cell);
        }
        if (MapView_Row == 0) MapView_Row = 0x37;
        else MapView_Row = static_cast<short>(MapView_Row - 1);
    }
    OriginStep(-1, -1);
}

// original 0x56EB50: the view one map cell on along x and y - two ring rows
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
            unsigned char* const cell = Cell(row, column);
            g.release_cell(cell);
            g.cell_to_map(0x38 + pass, j, cell);
        }
        head = static_cast<unsigned short>(row);
        MapView_Row = static_cast<short>(head);
    }
    OriginStep(1, 1);
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
    g.geom_screen(0x3E8);
    g.geom_offset(0xA0, 0x78);
    g.back_color(0x78, 0x78, 0x78);
    for (unsigned q = 0; q < 0x800; ++q) {
        unsigned char* const quad = DrawItems + q * 0x48u;
        g.set_poly_ft4(quad);
        g.set_shade_tex(quad, 0);
        SetWord(quad + 0x36, 0);
        SetWord(quad + 0x46, 0);
    }
    Camera_Angles[0] = static_cast<short>(0xFD56);
    Camera_Angles[1] = 0;
    auto* const angles = reinterpret_cast<unsigned char*>(Camera_Angles);
    auto* const drawn = reinterpret_cast<unsigned char*>(Camera_AnglesDrawn);
    SetDword(drawn, Dword(angles));
    const auto kind2_z = static_cast<std::int32_t>(Field_Kind2Z);
    const auto kind2_x = static_cast<std::int32_t>(Field_Kind2X);
    SetWord(angles + 4, 0x200);   // Cond_AngleFB's low word, the yaw
    SetDword(drawn + 4, Dword(angles + 4));
    // Draw item 0 - the "none" index - has its eight screen points cleared.
    for (const unsigned at : {0x08u, 0x0Cu, 0x18u, 0x1Cu, 0x28u, 0x2Cu, 0x38u, 0x3Cu, 0x50u, 0x54u, 0x60u, 0x64u, 0x70u,
                              0x74u, 0x80u, 0x84u})
        SetDword(DrawItems + at, 0);
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
        const long got = g.elevation(kind2_x, kind2_z);
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
            unsigned char* const cell = Cell(row, column);
            SetWord(cell + 2, 0);
            g.cell_to_map(row, column, cell);
        }
    }
    for (unsigned i = 0; i < 0x400; ++i) DrawItemPool_Free[i] = static_cast<unsigned short>(i);
    DrawItemPool_Top = 1;
    MapView_Column = 0x1B;
    MapView_Row = 0x37;
    g.bake_patches();
    g.setup_entries();
    g.place_runs();
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
                    const std::uint32_t offset = Word(AreaMap_Header + 2);
                    const std::uint32_t cell = (r & 0xFFu) * width + ((r >> 8) & 0xFFu) + offset * 2u;
                    record += 0xC;
                    const std::uint32_t tile = Word(AreaMap_Header + cell * 2u);
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

// original 0x571720: an area's set-up entries (PSX FUN_80158A54). Cond_ByteFF
// from bit 0 of Cond_ByteFFSource; the set-up list from AreaMap_SetupBase,
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
    Cond_ByteFF = static_cast<unsigned char>(Cond_ByteFFSource & 1);
    const unsigned char* entry = AreaMap_Header + base * 4u;
    for (std::uint32_t e = Dword(entry); e != 0; e = Dword(entry)) {
        reinterpret_cast<Handler>(static_cast<std::uintptr_t>(AreaMap_SetupHandlers[(e >> 24) & 0x3Fu]))(entry);
        entry += entry[2] * 4u;
    }
    g.clut_start();
    const unsigned char* patch = AreaMap_Header + static_cast<std::uint32_t>(AreaMap_PatchBase) * 4u;
    while (Dword(patch) != 0) {
        g.apply_patch(patch);
        patch += (Dword(patch) >> 16) * 4u + 4u;
    }
}

// original 0x5717B0: every palette cycle of the area header put at its frame
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

void MapScroll_Inject() {
    if (bof3::WantsShadow("map_scroll")) map_scroll::SelfTest();
    BOF3_INJECT(MapView_ShiftColumnPrev);
    BOF3_INJECT(MapView_ShiftColumnNext);
    BOF3_INJECT(MapView_ShiftRowsPrev);
    BOF3_INJECT(MapView_ShiftRowsNext);
    BOF3_INJECT(MapView_PlaceRuns);
    BOF3_INJECT(Field_ViewReset);
    BOF3_INJECT(AreaMap_BakePatches);
    BOF3_INJECT(AreaMap_SetupEntries);
    BOF3_INJECT(AreaMap_ClutCycleStart);
}
