#include "game/map_rim.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/snapshot.h"

// The field's view ring grown for the wide picture: see the header, and
// docs/wide-ring-read.md section 6 for the plan this builds to.

namespace map_rim {

namespace {

bool g_armed;    // Arm(): BOF3X_WIDE_RING on and the cell inset lowered
bool g_forced;   // a self-test's
bool g_control;  // the model test's control: Capcom's CellToMap at the grown row

// The rim, by grown position; Capcom's window positions are never read here.
unsigned char g_cells[kRows][kColumns][4];
unsigned short g_runs[kRows][kColumns];

// Layer -1's DrawLayers-shaped record and its draw-table cells (key, item).
unsigned long g_extra[12];
constexpr unsigned kTableMax = 64;   // a layer -1 walk is at most 33 cells
unsigned g_table_key[kTableMax], g_table_item[kTableMax];
unsigned g_table_n;

bool g_walk_noted;
unsigned g_walk, g_walk_written;

unsigned g_peak_pool, g_peak_table, g_peak_logs;

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

int Mod(int v, int n) {
    v %= n;
    return v < 0 ? v + n : v;
}

bool Valid(int gr, int gc) { return gr >= 0 && gr < kRows && gc >= 0 && gc < kColumns; }

}  // namespace

bool Active() { return g_armed || g_forced; }

bool InWindow(int gr, int gc) {
    const int r = gr - 1;
    if (r < 0 || r >= 0x38) return false;
    const int c = gc - 2 - (r & 1);
    return c >= 0 && c < 0x1C;
}

int RingRow(int gr) { return Mod(MapView_Row + gr, 0x38); }

int RingColumn(int gr, int gc) {
    const int r = gr - 1;
    return Mod(MapView_Column + 1 + gc - 2 - (r & 1), 0x1C);
}

unsigned char* Cell(int gr, int gc) {
    if (InWindow(gr, gc))
        return MapView_CellItems + static_cast<std::uint32_t>(RingColumn(gr, gc) + RingRow(gr) * 0x1C) * 4u;
    return g_cells[gr][gc];
}

unsigned short RunWord(int gr, int gc) {
    if (InWindow(gr, gc)) return MapView_Cells[RingColumn(gr, gc) + RingRow(gr) * 0x1C];
    return g_runs[gr][gc];
}

void GrownCellToMap(int gr, int gc, unsigned char* out) {
    if (g_control) {
        // The control (the model test only): Capcom's own function, given the
        // grown position in Capcom's terms - row -1 and 56 included.
        const int r = gr - 1;
        MapView_CellToMap(r, gc - 2 - (r & 1), out);
        return;
    }
    const int x = MapView_Origin[0] - 3 + gc + (gr + 1) / 2;
    const int y = MapView_Origin[1] + 2 + gr / 2 - gc;
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

// One function, four directions: the rim's content moves with the map, as
// Capcom's ring's does by its heads. new(p) = old(p + step). A rim cell whose
// map cell leaves the grown ring, or enters Capcom's window (where Capcom's
// shift has just filled the slot afresh), gives its draw items back; a rim
// position whose map cell comes from Capcom's window (released there by
// Capcom's shift) or from past the grown ring's edge is filled fresh. Run
// words move with their cells; MapView_PlaceRuns follows every shift in
// AreaMap_Frame and rewrites them.
void RimShift(Shift shift) {
    int sr = 0, sc = 0;
    switch (shift) {
    case Shift::kColumnNext: sc = 1; break;
    case Shift::kColumnPrev: sc = -1; break;
    case Shift::kRowsNext: sr = 2; break;
    case Shift::kRowsPrev: sr = -2; break;
    }
    for (int gr = 0; gr < kRows; ++gr)
        for (int gc = 0; gc < kColumns; ++gc) {
            if (InWindow(gr, gc)) continue;
            const int pr = gr - sr, pc = gc - sc;
            if (!Valid(pr, pc) || InWindow(pr, pc)) DrawItemPool_ReleaseCell(g_cells[gr][gc]);
        }
    static unsigned char cells[kRows][kColumns][4];
    static unsigned short runs[kRows][kColumns];
    for (int gr = 0; gr < kRows; ++gr)
        for (int gc = 0; gc < kColumns; ++gc) {
            if (InWindow(gr, gc)) continue;
            const int qr = gr + sr, qc = gc + sc;
            if (Valid(qr, qc) && !InWindow(qr, qc)) {
                std::memcpy(cells[gr][gc], g_cells[qr][qc], 4);
                runs[gr][gc] = g_runs[qr][qc];
            } else {
                std::memset(cells[gr][gc], 0, 4);
                GrownCellToMap(gr, gc, cells[gr][gc]);
                runs[gr][gc] = 0;
            }
        }
    std::memcpy(g_cells, cells, sizeof g_cells);
    std::memcpy(g_runs, runs, sizeof g_runs);
}

void RimFill(bool keep_items) {
    for (int gr = 0; gr < kRows; ++gr)
        for (int gc = 0; gc < kColumns; ++gc) {
            if (InWindow(gr, gc)) continue;
            if (!keep_items) std::memset(g_cells[gr][gc] + 2, 0, 2);
            GrownCellToMap(gr, gc, g_cells[gr][gc]);
        }
    if (!keep_items) g_table_n = 0;
    RimRuns();
}

// MapView_PlaceRuns' arithmetic in the grown frame: a run whose map cell lies
// at s' = s + 1 in 0..0x39 and d' = d + 5 in 0..0x41 and outside Capcom's
// window (0 <= s, d < 0x38, which Capcom's half placed) gets its dword offset
// plus 1 at grown (s', d' / 2) - the PSP's bounds (its MoveCmd_TestFB twin
// 0xEDC0 tests the same window).
void RimRuns() {
    std::memset(g_runs, 0, sizeof g_runs);
    const std::uint32_t base = static_cast<std::uint32_t>(AreaMap_CellBase) & 0xFFFFu;
    const unsigned char* const list = AreaMap_Header + base * 4u;
    const unsigned char* run = list;
    for (std::uint32_t d = Dword(run); d != 0; d = Dword(run)) {
        const int x = static_cast<int>((d >> 8) & 0xFFu) - MapView_Origin[0];
        const int y = static_cast<int>(d & 0xFFu) - MapView_Origin[1];
        const int s = y + x, diff = x - y;
        const bool capcom = s >= 0 && s < 0x38 && diff >= 0 && diff < 0x38;
        const int gs = s + 1, gd = diff + 5;
        if (!capcom && gs >= 0 && gs < kRows && gd >= 0 && gd < 2 * kColumns) {
            const auto offset = static_cast<std::uint32_t>(static_cast<std::int32_t>(run - list) >> 2) + 1u;
            g_runs[gs][gd / 2] = static_cast<unsigned short>(offset);
        }
        const std::uint32_t step = d >> 16;
        // Capcom's half never ends on a step of 0 either, and always runs first.
        if (step == 0) bof3::Fatal("map_rim: the area's run list has a step of 0 at dword %u", (unsigned)((run - list) >> 2));
        run += step * 4u;
    }
}

void NoteWalk(unsigned grown_inset, unsigned written_inset) {
    g_walk_noted = true;
    g_walk = grown_inset;
    g_walk_written = written_inset;
}

int Walk(unsigned read_inset) {
    if (g_walk_noted && read_inset == g_walk_written) return static_cast<int>(g_walk);
    const unsigned g = read_inset + 2u;
    return static_cast<int>(g > 16u ? 16u : g);
}

unsigned long* ExtraLayer() { return g_extra; }

void ExtraReset(unsigned buffer_index) {
    auto* const layer = reinterpret_cast<unsigned char*>(g_extra) + (buffer_index & 0xFFu) * 8u;
    if (layer + 0x18 > reinterpret_cast<unsigned char*>(g_extra) + sizeof g_extra)
        bof3::Fatal("map_rim: buffer index %u outside the extra layer's two", buffer_index);
    for (unsigned char* head = layer; head != layer + 0x20; head += 0x10) {
        const std::uint32_t none = 0, self = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(head));
        std::memcpy(head, &none, 4);
        std::memcpy(head + 4, &self, 4);
    }
    g_table_n = 0;
}

void ExtraTablePush(unsigned key, unsigned item) {
    if (g_table_n == kTableMax) bof3::Fatal("map_rim: more than %u draw-table cells in layer -1", kTableMax);
    g_table_key[g_table_n] = key;
    g_table_item[g_table_n] = item;
    ++g_table_n;
}

// DrawTable_Sort's exchange sort, on the key alone.
void ExtraTableSort() {
    for (unsigned i = 0; i < g_table_n; ++i)
        for (unsigned j = i + 1; j < g_table_n; ++j) {
            if (g_table_key[i] <= g_table_key[j]) continue;
            const unsigned k = g_table_key[i], it = g_table_item[i];
            g_table_key[i] = g_table_key[j];
            g_table_item[i] = g_table_item[j];
            g_table_key[j] = k;
            g_table_item[j] = it;
        }
}

unsigned ExtraTableCount() { return g_table_n; }
unsigned ExtraTableItem(unsigned i) { return g_table_item[i]; }

unsigned RimItemAt(std::int32_t dx, std::int32_t dz) {
    const std::int32_t s = static_cast<std::int32_t>(static_cast<std::uint32_t>(dx) + static_cast<std::uint32_t>(dz));
    const std::int32_t d = static_cast<std::int32_t>(static_cast<std::uint32_t>(dx) - static_cast<std::uint32_t>(dz));
    if (s >= 0 && s < 0x38 && d >= 0 && d < 0x38) return 0;   // Capcom's window: the caller's
    if (s < -1 || s >= kRows - 1 || d < -5 || d >= 2 * kColumns - 5) return 0;
    return Word(g_cells[s + 1][(d + 5) / 2] + 2) & 0xFFFu;
}

void NoteBuildPeaks(unsigned table_entries) {
    // Items in use: the free queue's top less one, round the ring (top 0 is
    // the pool exhausted, Count() - 1 handed out).
    const unsigned used = (DrawItemPool_Top - 1u) & draw_pool::Mask();
    if (used <= g_peak_pool && table_entries <= g_peak_table) return;
    if (used > g_peak_pool) g_peak_pool = used;
    if (table_entries > g_peak_table) g_peak_table = table_entries;
    if (g_peak_logs < 400) {
        ++g_peak_logs;
        bof3::Log("wide-ring   peak at frame %u: draw items %u of %u in use, draw table %u of 256 (area %u)",
                  (unsigned)Frame_Counter, g_peak_pool, draw_pool::Count(), g_peak_table, (unsigned)Game_AreaNumber);
    }
}

// --- the quicksave -----------------------------------------------------------
// The record: a version, the inset columns at the take (0 narrow), then when
// wide the rim's cells and run words, the extra layer and its table. Its
// pointers name bof3x.dll, which a load requires at the same base.

namespace {
constexpr std::uint32_t kRecordVersion = 1;
}  // namespace

void QuickSave(bof3::SnapshotWriter& w) {
    w.Put(kRecordVersion);
    w.Put(static_cast<std::uint32_t>(Active() ? Widescreen_InsetColumns() : 0));
    if (!Active()) return;
    w.Put(g_cells, sizeof g_cells);
    w.Put(g_runs, sizeof g_runs);
    w.Put(g_extra, sizeof g_extra);
    w.Put(static_cast<std::uint32_t>(g_table_n));
    w.Put(g_table_key, sizeof g_table_key);
    w.Put(g_table_item, sizeof g_table_item);
    w.Put(static_cast<std::uint8_t>(g_walk_noted ? 1 : 0));
    w.Put(static_cast<std::uint32_t>(g_walk));
    w.Put(static_cast<std::uint32_t>(g_walk_written));
}

// Across widths, rebuilt (owner's answer 4, 2026-10-21): Capcom's ring is
// whole at every width, so a wide take loaded narrow gives the rim's items
// back to the pool (written back by the core before the participants), and a
// narrow take loaded wide fills the rim afresh with no items. Either way a
// redraw of both display buffers (MapView_Redraw at least 2) rebuilds the
// layer lists, which named the other width's cells.
void QuickRestore(bof3::SnapshotReader& r) {
    if (r.Get<std::uint32_t>() != kRecordVersion) bof3::Fatal("map_rim: the quicksave's rim record is another version");
    const std::uint32_t taken = r.Get<std::uint32_t>();
    if (taken != 0) {
        static unsigned char cells[kRows][kColumns][4];
        static unsigned short runs[kRows][kColumns];
        static unsigned long extra[12];
        static unsigned keys[kTableMax], items[kTableMax];
        r.Get(cells, sizeof cells);
        r.Get(runs, sizeof runs);
        r.Get(extra, sizeof extra);
        const std::uint32_t n = r.Get<std::uint32_t>();
        if (n > kTableMax) bof3::Fatal("map_rim: a quicksave's layer -1 table holds %u cells, more than %u", n, kTableMax);
        r.Get(keys, sizeof keys);
        r.Get(items, sizeof items);
        const bool noted = r.Get<std::uint8_t>() != 0;
        const std::uint32_t walk = r.Get<std::uint32_t>(), written = r.Get<std::uint32_t>();
        if (Active()) {
            std::memcpy(g_cells, cells, sizeof g_cells);
            std::memcpy(g_runs, runs, sizeof g_runs);
            std::memcpy(g_extra, extra, sizeof g_extra);
            std::memcpy(g_table_key, keys, sizeof keys);
            std::memcpy(g_table_item, items, sizeof items);
            g_table_n = n;
            g_walk_noted = noted;
            g_walk = walk;
            g_walk_written = written;
            if (taken != Widescreen_InsetColumns() && MapView_Redraw < 2) MapView_Redraw = 2;
            bof3::Log("quicksave   wide-ring: the rim as at the take (taken at inset columns %u, this run %u)", taken,
                      Widescreen_InsetColumns());
            return;
        }
        unsigned released = 0;
        for (int gr = 0; gr < kRows; ++gr)
            for (int gc = 0; gc < kColumns; ++gc) {
                if (InWindow(gr, gc) || (Word(cells[gr][gc] + 2) & 0xFFFu) == 0) continue;
                DrawItemPool_ReleaseCell(cells[gr][gc]);
                ++released;
            }
        if (MapView_Redraw < 2) MapView_Redraw = 2;
        bof3::Log("quicksave   wide-ring: taken wide, loaded narrow - the rim's %u draw items given back, a redraw "
                  "asked", released);
        return;
    }
    if (!Active()) return;
    std::memset(g_cells, 0, sizeof g_cells);
    RimFill(false);
    ExtraReset(0);
    ExtraReset(1);
    g_walk_noted = false;
    if (MapView_Redraw < 2) MapView_Redraw = 2;
    bof3::Log("quicksave   wide-ring: taken narrow, loaded wide - the rim filled afresh, a redraw asked");
}

void ForceForTest(bool on) { g_forced = on; }
void TestSetRun(int gr, int gc, unsigned short word) {
    if (!Valid(gr, gc) || InWindow(gr, gc)) bof3::Fatal("map_rim: TestSetRun(%d, %d) is not a rim position", gr, gc);
    g_runs[gr][gc] = word;
}
void TestClearRuns() { std::memset(g_runs, 0, sizeof g_runs); }

// --- BOF3X_SHADOW=map_rim: the model test --------------------------------------
// A 58 x 33 ring of the test's own, kept by the PSP's arithmetic - twins of the
// four shifts at 0x21 / 0x3A, the reset with heads 0x20 / 0x39, the origin
// Capcom's plus (-3, +2), its run placement at s' < 0x3A, d' < 0x42 - against
// Capcom's code (our MapView_Shift*, MapView_FillCells, MapView_PlaceRuns,
// their real callees) plus the rim, over random sequences of shifts, resets,
// refills, origin jumps of 0x14 / 0x1E / 0x20 cells and builds that hand cells
// draw items. After every step: each grown position's map cell and run word
// through Cell / RunWord against the model's; the pool's items in use against
// the items the cells hold, every index held once (no leak, no double
// release). The control - the rim filled by Capcom's MapView_CellToMap at the
// grown row - must be refused.

namespace {

constexpr unsigned kWidth = 100, kHeight = 100, kOffset = 0x40;   // tiles at dword 0x80 of the block
constexpr unsigned kTileBytes = kOffset * 4u + kWidth * kHeight * 2u;
constexpr unsigned kRunBase = (kTileBytes + 3u) / 4u + 4u;   // dwords
constexpr unsigned kAreaBytes = kRunBase * 4u + 0xA000u;   // 3,000 runs of at most 3 dwords and the end: 9,001 of 10,240

struct Model {
    unsigned char cells[kRows][kColumns][2];
    unsigned short runs[kRows][kColumns];
    int head_row, head_column, ox, oz;
};
Model g_m;

std::uint32_t g_rng;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

void ModelToMap(int row, int col, unsigned char* out) {
    const int x = g_m.ox + col + (row + 1) / 2;
    const int y = g_m.oz + row / 2 - col;
    const int width = AreaMap_Header[0], height = AreaMap_Header[1];
    const unsigned offset = static_cast<unsigned>(AreaMap_Header[2] | (AreaMap_Header[3] << 8));
    out[0] = out[1] = 0;
    if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
        const auto* cells = reinterpret_cast<const unsigned short*>(AreaMap_Header);
        if (cells[static_cast<unsigned>(x + width * y) + offset * 2] != 0) {
            out[0] = static_cast<unsigned char>(x);
            out[1] = static_cast<unsigned char>(y);
        }
    }
}
unsigned char* ModelSlot(int gr, int gc) {
    return g_m.cells[Mod(g_m.head_row + 1 + gr, kRows)][Mod(g_m.head_column + 1 + gc, kColumns)];
}
unsigned short& ModelRun(int gr, int gc) {
    return g_m.runs[Mod(g_m.head_row + 1 + gr, kRows)][Mod(g_m.head_column + 1 + gc, kColumns)];
}

// The PSP's twins, by its bounds: Field_ViewReset 0xE560's fill and heads,
// the shifts 0xCD98 / 0xCE94 / 0xCF8C / 0xD0A8, the placement 0x11AE8.
void ModelReset() {
    g_m.ox = MapView_Origin[0] - 3;
    g_m.oz = MapView_Origin[1] + 2;
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < kColumns; ++c) ModelToMap(r, c, g_m.cells[r][c]);
    g_m.head_row = 0x39;
    g_m.head_column = 0x20;
}
void ModelColumnPrev() {
    for (int i = 0, row = g_m.head_row; i < kRows; ++i) {
        row = row == kRows - 1 ? 0 : row + 1;
        ModelToMap(i, -1, g_m.cells[row][g_m.head_column]);
    }
    g_m.head_column = g_m.head_column == 0 ? kColumns - 1 : g_m.head_column - 1;
    g_m.ox -= 1;
    g_m.oz += 1;
}
void ModelColumnNext() {
    const int column = g_m.head_column == kColumns - 1 ? 0 : g_m.head_column + 1;
    for (int i = 0, row = g_m.head_row; i < kRows; ++i) {
        row = row == kRows - 1 ? 0 : row + 1;
        ModelToMap(i, kColumns, g_m.cells[row][column]);
    }
    g_m.head_column = column;
    g_m.ox += 1;
    g_m.oz -= 1;
}
void ModelRowsPrev() {
    for (int pass = 0; pass < 2; ++pass) {
        for (int j = 0, column = g_m.head_column; j < kColumns; ++j) {
            column = column == kColumns - 1 ? 0 : column + 1;
            ModelToMap(pass == 0 ? -2 : -3, j, g_m.cells[g_m.head_row][column]);
        }
        g_m.head_row = g_m.head_row == 0 ? kRows - 1 : g_m.head_row - 1;
    }
    g_m.ox -= 1;
    g_m.oz -= 1;
}
void ModelRowsNext() {
    for (int pass = 0; pass < 2; ++pass) {
        g_m.head_row = g_m.head_row == kRows - 1 ? 0 : g_m.head_row + 1;
        for (int j = 0, column = g_m.head_column; j < kColumns; ++j) {
            column = column == kColumns - 1 ? 0 : column + 1;
            ModelToMap(kRows + pass, j, g_m.cells[g_m.head_row][column]);
        }
    }
    g_m.ox += 1;
    g_m.oz += 1;
}
void ModelFill() {
    for (int gr = 0; gr < kRows; ++gr)
        for (int gc = 0; gc < kColumns; ++gc) ModelToMap(gr, gc, ModelSlot(gr, gc));
}
void ModelPlaceRuns() {
    std::memset(g_m.runs, 0, sizeof g_m.runs);
    const unsigned char* const list = AreaMap_Header + kRunBase * 4u;
    for (const unsigned char* run = list; Dword(run) != 0; run += (Dword(run) >> 16) * 4u) {
        const int x = static_cast<int>((Dword(run) >> 8) & 0xFFu) - g_m.ox;
        const int y = static_cast<int>(Dword(run) & 0xFFu) - g_m.oz;
        const int s = x + y, d = x - y;
        if (s >= 0 && s < kRows && d >= 0 && d < 2 * kColumns)
            ModelRun(s, d / 2) = static_cast<unsigned short>((run - list) / 4 + 1);
    }
}

// The test's area: a 100 x 100 map, one tile in seven empty, and 3,000 runs
// at random cells, steps 1..3 dwords.
void LayArea() {
    std::memset(AreaMap_Header, 0, kAreaBytes);
    AreaMap_Header[0] = static_cast<unsigned char>(kWidth);
    AreaMap_Header[1] = static_cast<unsigned char>(kHeight);
    AreaMap_Header[2] = static_cast<unsigned char>(kOffset);
    AreaMap_Header[3] = 0;
    auto* const tiles = reinterpret_cast<unsigned short*>(AreaMap_Header) + kOffset * 2u;
    for (unsigned i = 0; i < kWidth * kHeight; ++i) tiles[i] = Next() % 7 == 0 ? 0 : static_cast<unsigned short>(1 + Next() % 500);
    AreaMap_CellBase = 0x5EED0000u | kRunBase;
    auto* run = reinterpret_cast<std::uint32_t*>(AreaMap_Header) + kRunBase;
    for (unsigned k = 0; k < 3000; ++k) {
        const std::uint32_t step = 1 + Next() % 3;
        run[0] = (step << 16) | ((Next() % kWidth) << 8) | (Next() % kHeight);
        for (std::uint32_t j = 1; j < step; ++j) run[j] = Next();
        run += step;
    }
    run[0] = 0;
}

// Capcom's reset of the ring, as Field_ViewReset has it (its cell loop, heads
// and pool), then the rim's - the one line Field_ViewReset adds.
void CapcomReset(int ox, int oz) {
    MapView_Origin[0] = static_cast<short>(ox);
    MapView_Origin[1] = static_cast<short>(oz);
    for (int row = 0; row < 0x38; ++row)
        for (int column = 0; column < 0x1C; ++column) {
            unsigned char* const cell = MapView_CellItems + (row * 0x1C + column) * 4;
            std::memset(cell + 2, 0, 2);
            MapView_CellToMap(row, column, cell);
        }
    for (unsigned i = 0; i < draw_pool::Count(); ++i) draw_pool::Free()[i] = static_cast<unsigned short>(i);
    DrawItemPool_Top = 1;
    MapView_Column = 0x1B;
    MapView_Row = 0x37;
    RimFill(false);
}

// A build's allocations, without the GTE: each walked cell with a map cell and
// no item takes one, its side words cleared.
void Build(unsigned one_in) {
    for (int gr = 0; gr < kRows; ++gr)
        for (int gc = 0; gc < kColumns; ++gc) {
            if (Next() % one_in != 0) continue;
            unsigned char* const cell = Cell(gr, gc);
            if (cell[0] == 0 || cell[1] == 0 || (Word(cell + 2) & 0xFFFu) != 0) continue;
            const unsigned short item = DrawItemPool_Alloc();
            if (item == 0) continue;
            std::memset(draw_pool::Items() + item * 0x90u + 0x7E, 0, 2);
            std::memset(draw_pool::Items() + item * 0x90u + 0x8E, 0, 2);
            const auto w = static_cast<std::uint16_t>((Word(cell + 2) & 0xF000u) | item | ((Next() & 1u) << 12));
            std::memcpy(cell + 2, &w, 2);
        }
}

struct Counts {
    unsigned steps = 0, cell_bad = 0, run_bad = 0, pool_bad = 0, rim_runs = 0, rim_items = 0;
};

void Compare(Counts& n, unsigned seq, unsigned step, const char* what, bool log) {
    ++n.steps;
    unsigned cell_bad = 0, run_bad = 0, held = 0;
    static unsigned char seen[4096];
    std::memset(seen, 0, sizeof seen);
    bool twice = false;
    for (int gr = 0; gr < kRows; ++gr)
        for (int gc = 0; gc < kColumns; ++gc) {
            const unsigned char* const ours = Cell(gr, gc);
            const unsigned char* const model = ModelSlot(gr, gc);
            if (ours[0] != model[0] || ours[1] != model[1]) ++cell_bad;
            if (RunWord(gr, gc) != ModelRun(gr, gc)) ++run_bad;
            if (!InWindow(gr, gc) && RunWord(gr, gc) != 0) ++n.rim_runs;
            const unsigned item = Word(ours + 2) & 0xFFFu;
            if (item != 0) {
                ++held;
                if (!InWindow(gr, gc)) ++n.rim_items;
                if (seen[item]) twice = true;
                seen[item] = 1;
            }
        }
    const unsigned used = (DrawItemPool_Top - 1u) & draw_pool::Mask();
    const bool pool_bad = twice || used != held;
    if (cell_bad) ++n.cell_bad;
    if (run_bad) ++n.run_bad;
    if (pool_bad) ++n.pool_bad;
    if (log && (cell_bad || run_bad || pool_bad) && n.cell_bad + n.run_bad + n.pool_bad <= 8)
        bof3::Log("shadow      map_rim MISMATCH: sequence %u step %u after %s: %u cells, %u run words differ from the "
                  "model; items held %u, pool in use %u%s", seq, step, what, cell_bad, run_bad, held, used,
                  twice ? ", an index held twice" : "");
}

Counts RunModel(std::uint32_t seed, unsigned sequences, unsigned steps, bool log) {
    Counts n;
    g_rng = seed;
    for (unsigned seq = 0; seq < sequences; ++seq) {
        LayArea();
        CapcomReset(static_cast<int>(Next() % 90) - 20, static_cast<int>(Next() % 90) - 10);
        ModelReset();
        MapView_PlaceRuns();
        ModelPlaceRuns();
        Compare(n, seq, 0, "the reset", log);
        int direction = 0;
        for (unsigned step = 1; step <= steps; ++step) {
            const unsigned pick = Next() % 100;
            const char* what;
            if (pick < 2) {
                CapcomReset(static_cast<int>(Next() % 90) - 20, static_cast<int>(Next() % 90) - 10);
                ModelReset();
                what = "a reset";
            } else if (pick < 6) {
                static const int kJump[] = {0x14, 0x1E, 0x20, -0x14, -0x1E, -0x20};
                const int jump = kJump[Next() % 6];
                if (Next() % 2) MapView_Origin[0] = static_cast<short>(MapView_Origin[0] + jump);
                else MapView_Origin[1] = static_cast<short>(MapView_Origin[1] + jump);
                g_m.ox = MapView_Origin[0] - 3;
                g_m.oz = MapView_Origin[1] + 2;
                MapView_FillCells();
                ModelFill();
                what = "an origin jump and MapView_FillCells";
            } else if (pick < 30) {
                Build(1 + Next() % 3);
                what = "a build";
            } else {
                if (Next() % 4 == 0) direction = static_cast<int>(Next() % 4);
                switch (direction) {
                case 0: MapView_ShiftColumnPrev(); ModelColumnPrev(); what = "MapView_ShiftColumnPrev"; break;
                case 1: MapView_ShiftColumnNext(); ModelColumnNext(); what = "MapView_ShiftColumnNext"; break;
                case 2: MapView_ShiftRowsPrev(); ModelRowsPrev(); what = "MapView_ShiftRowsPrev"; break;
                default: MapView_ShiftRowsNext(); ModelRowsNext(); what = "MapView_ShiftRowsNext"; break;
                }
            }
            MapView_PlaceRuns();
            ModelPlaceRuns();
            Compare(n, seq, step, what, log);
        }
    }
    return n;
}

void SelfTest() {
    static unsigned char saved_area[kAreaBytes];
    static unsigned char saved_items[sizeof MapView_CellItems];
    static unsigned short saved_cells[0x38 * 0x1C];
    std::memcpy(saved_area, AreaMap_Header, kAreaBytes);
    std::memcpy(saved_items, MapView_CellItems, sizeof saved_items);
    std::memcpy(saved_cells, MapView_Cells, sizeof saved_cells);
    const unsigned long saved_base = AreaMap_CellBase;
    const short saved_row = MapView_Row, saved_column = MapView_Column;
    const short saved_ox = MapView_Origin[0], saved_oz = MapView_Origin[1];
    const unsigned short saved_top = DrawItemPool_Top;
    const unsigned count = draw_pool::Count();
    auto* const saved_free = new unsigned short[count];
    auto* const saved_pool = new unsigned char[count * 0x90u];
    std::memcpy(saved_free, draw_pool::Free(), count * sizeof(unsigned short));
    std::memcpy(saved_pool, draw_pool::Items(), count * 0x90u);

    g_forced = true;
    constexpr unsigned kSequences = 40, kSteps = 250;
    const Counts n = RunModel(0x3A21C0DEu, kSequences, kSteps, true);
    g_control = true;
    const Counts c = RunModel(0x3A21C0DEu, 4, 60, false);
    g_control = false;
    g_forced = false;

    std::memcpy(AreaMap_Header, saved_area, kAreaBytes);
    std::memcpy(MapView_CellItems, saved_items, sizeof saved_items);
    std::memcpy(MapView_Cells, saved_cells, sizeof saved_cells);
    AreaMap_CellBase = saved_base;
    MapView_Row = saved_row;
    MapView_Column = saved_column;
    MapView_Origin[0] = saved_ox;
    MapView_Origin[1] = saved_oz;
    DrawItemPool_Top = saved_top;
    std::memcpy(draw_pool::Free(), saved_free, count * sizeof(unsigned short));
    std::memcpy(draw_pool::Items(), saved_pool, count * 0x90u);
    delete[] saved_free;
    delete[] saved_pool;
    std::memset(g_cells, 0, sizeof g_cells);
    std::memset(g_runs, 0, sizeof g_runs);
    g_table_n = 0;

    const unsigned bad = n.cell_bad + n.run_bad + n.pool_bad;
    const bool refused = c.cell_bad != 0;
    bof3::Log("shadow      map_rim self-test: %u sequences, %u steps against the PSP-shaped 58 x 33 model ring: %u "
              "steps with cells differing, %u with run words, %u with the pool's count or an index held twice - %u "
              "MISMATCHES; rim run words seen %u, rim items held %u; the control (Capcom's CellToMap at the grown row) "
              "%s (%u of %u steps differ)",
              kSequences, n.steps, n.cell_bad, n.run_bad, n.pool_bad, bad, n.rim_runs, n.rim_items,
              refused ? "refused" : "NOT REFUSED", c.cell_bad, c.steps);
    if (bad) bof3::Fatal("map_rim: the grown ring differs from the model in %u steps", bad);
    if (!refused) bof3::Fatal("map_rim: the self-test's control was not refused");
}

}  // namespace

void Arm() {
    if (bof3::WantsShadow("map_rim")) SelfTest();
    char text[8];
    const DWORD n = GetEnvironmentVariableA("BOF3X_WIDE_RING", text, sizeof text);
    if (n > 1 || (n == 1 && text[0] != '0' && text[0] != '1')) bof3::Fatal("BOF3X_WIDE_RING must be 0 or 1");
    const bool off = n == 1 && text[0] == '0';
    ExtraReset(0);
    ExtraReset(1);
    if (Widescreen_InsetColumns() == 0) {
        bof3::Log("DIV-0041    the grown view ring: idle (the view is not wide), Capcom's 28 x 56");
        return;
    }
    if (off) {
        bof3::Log("DIV-0041    the grown view ring: off (BOF3X_WIDE_RING=0), Capcom's 28 x 56 at the wide picture");
        return;
    }
    g_armed = true;
    bof3::Log("DIV-0041    the grown view ring: the draw walks the PSP's 33 x 58 (a rim of %d cells beside Capcom's "
              "28 x 56, which the game's logic keeps reading); BOF3X_WIDE_RING=0 for Capcom's ring alone", kRimCells);
}

}  // namespace map_rim
