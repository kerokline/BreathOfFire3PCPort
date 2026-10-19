#include "game/wide_rand.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/map_cells.h"
#include "game/map_layers.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/snapshot.h"

// DIV-0041's random split (docs/ultrawide-rand.md): see the header.

namespace wide_rand {

namespace {

bool g_noted;                 // NoteInset has run since start-up (or the self-test's last restore)
unsigned g_original, g_written;
bool g_band;                  // DrawLayer_Open is handing over a band cell's records
bool g_control;               // the self-test's control: band cells back on Capcom's Rand
std::uint32_t g_seed = 1;     // DrawOnly's, as the C runtime's own seed starts

}  // namespace

void NoteInset(unsigned original, unsigned written) {
    g_noted = true;
    g_original = original;
    g_written = written;
}

// DrawLayer_Open's arithmetic (draw_emit.cpp) on the original's inset: the
// inset less 2 (not below 0) with the input flag, the first column
// MapView_Column + inset + (1 for an even row) less 0x1C once, then
// (14 - inset) * 2 columns, each advanced before it is read, 0x1B to 0.
Row::Row(unsigned read_inset, unsigned input_flags, int row, int view_column) {
    if (!g_noted || read_inset != g_written || g_original == g_written) return;
    split_ = true;
    int inset = static_cast<int>(g_original);
    if (input_flags & 1) inset = inset >= 2 ? inset - 2 : 0;
    int column = view_column + inset + (~row & 1);
    if (column >= 0x1C) column -= 0x1C;
    const int count = (14 - inset) * 2;
    for (int k = 0; k < count; ++k) {
        column = column == 0x1B ? 0 : column + 1;
        columns_[count_++] = column;
    }
}

bool Row::Band(int column) const {
    if (!split_) return false;
    for (int k = 0; k < count_; ++k)
        if (columns_[k] == column) return false;
    return true;
}

void SetBand(bool band) { g_band = band; }
bool Band() { return g_band && !g_control; }

int __cdecl DrawOnly() {
    g_seed = g_seed * 0x343FDu + 0x269EC3u;
    return static_cast<int>((g_seed >> 16) & 0x7FFFu);
}
std::uint32_t* DrawOnlySeed() { return &g_seed; }

// The quicksave (SP-G, docs/quicksave.md, the graphics participant): the draw-only generator
// steps in play, and the last inset NoteInset saw decides the next split -
// both the DLL's, carried as numbers.
void QuickSave(bof3::SnapshotWriter& w) {
    w.Put(static_cast<std::uint8_t>(g_noted ? 1 : 0));
    w.Put(static_cast<std::uint32_t>(g_original));
    w.Put(static_cast<std::uint32_t>(g_written));
    w.Put(g_seed);
}

void QuickRestore(bof3::SnapshotReader& r) {
    g_noted = r.Get<std::uint8_t>() != 0;
    g_original = r.Get<std::uint32_t>();
    g_written = r.Get<std::uint32_t>();
    g_seed = r.Get<std::uint32_t>();
    g_band = false;
}

namespace {

// --- BOF3X_SHADOW=wide_rand: the start-up self-test ----------------------------
// Ours alone: DrawLayer_Open (draw_emit.cpp) and map_cells::RisingRand - the
// one place MapCell_DrawRising takes a random number - with the handler table
// swapped for stand-ins and Capcom's Rand for a recorder (it cannot run before
// the C runtime does). Three parts:
//   1. the boundary: every original inset 0..14, input flag, row parity and
//      view column, at the lowering the rule gives 0, 53 and 120 columns -
//      each cell handed over must be a band cell exactly when the original's
//      walk of its row (computed here apart, as a set of columns) leaves it out;
//   2. the fuzz: random camera, input flag, row, column and cells, a whole
//      frame (layers 0..0x36) run narrow and then at each lowering - the
//      Capcom Rand calls (and which cell made each) must be the narrow
//      frame's, call for call;
//   3. the control: the same wide frames with band cells sent to Capcom's
//      Rand - the 2026-10-07..13 build - must differ from narrow in some round
//      at every lowering above 0, or the start-up is Fatal.

constexpr unsigned kCells = 28 * 0x38;
constexpr unsigned kAreaBytes = 0x2000 * 4;
constexpr unsigned kLayerBytes = 0x38 * 12 * 4;
constexpr unsigned kCellBase = 0x10;
constexpr unsigned kRising = 0x0F;   // MapCell_Handlers' MapCell_DrawRising slot
constexpr unsigned kMaxLog = 16384;   // above 8 x 1,568, a frame whose every cell rises

struct Saved {
    unsigned char area[kAreaBytes];
    unsigned char layers[kLayerBytes];
    unsigned short cells[kCells];
    unsigned long handlers[77];
    unsigned long cell_base;
    short row, column;
    unsigned char inset, input_flags, buffer_index;
};
Saved g_saved;

// The recorders: which cell the stand-in is drawing, each Capcom Rand call's
// cell, and the band decisions the stand-in saw.
unsigned g_cell;
unsigned g_capcom_log[kMaxLog];
unsigned g_capcom_n, g_draw_only_n, g_rising_cells, g_band_cells;
bool g_seen_band[kCells], g_seen[kCells];

int __cdecl RecordingRand() {
    if (g_capcom_n < kMaxLog) g_capcom_log[g_capcom_n] = g_cell;
    ++g_capcom_n;
    return static_cast<int>(g_capcom_n & 0x7FFF);
}
// MapCell_DrawRising's randomness, as it has it: eight calls a cell.
void __cdecl RisingStandIn(const unsigned char* record, unsigned, unsigned) {
    g_cell = static_cast<unsigned>(record - AreaMap_Header) / 4u - kCellBase - 1u;
    g_cell /= 2u;
    ++g_rising_cells;
    if (Band()) ++g_band_cells;
    if (g_cell < kCells) {
        g_seen[g_cell] = true;
        g_seen_band[g_cell] = Band();
    }
    const unsigned before = g_capcom_n;
    for (unsigned i = 0; i < 8; ++i) map_cells::RisingRand();
    g_draw_only_n += 8u - (g_capcom_n - before);
}
void __cdecl OtherStandIn(const unsigned char*, unsigned, unsigned) {}

std::uint32_t g_rng;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

// Cell i's run: its head at dword kCellBase + 2i + 1 - 1, one record after it.
void LayCells(unsigned empty_one_in, unsigned rising_one_in) {
    auto* area = reinterpret_cast<std::uint32_t*>(AreaMap_Header);
    for (unsigned i = 0; i < kCells; ++i) {
        const unsigned word = 2u * i + 1u;
        area[kCellBase + word - 1] = (2u << 16) | (i & 0xFFFFu);
        const unsigned kind = rising_one_in == 0 || Next() % rising_one_in == 0 ? kRising : Next() % 77;
        area[kCellBase + word] = (kind << 24) | (1u << 16) | (Next() & 0xFFFFu);
        const bool empty = empty_one_in != 0 && Next() % empty_one_in == 0;
        MapView_Cells[i] = static_cast<unsigned short>(empty ? 0 : word);
    }
}

void Frame() {
    for (int layer = 0; layer < 0x37; ++layer) DrawLayer_Open(layer);
}

// The original's walk of one row, apart from Row: a set of columns.
void OriginalSet(bool out[28], int inset, unsigned flag, int row, int view_column) {
    std::memset(out, 0, 28);
    if (flag & 1) inset = inset >= 2 ? inset - 2 : 0;
    const int count = (14 - inset) * 2;
    const int start = view_column + inset + (~row & 1);
    for (int k = 1; k <= count; ++k) out[(start + k) % 28] = true;
}

void Boundary(unsigned c, unsigned& bad_total) {
    const unsigned wide = widescreen_rule::InsetColumns(c);
    unsigned cases = 0, handed = 0, band = 0, bad = 0, edge_left = 0;
    for (unsigned original = 0; original <= 14; ++original)
        for (unsigned flag = 0; flag < 2; ++flag)
            for (int view_row = 0; view_row < 2; ++view_row)
                for (int view_column = 0; view_column < 28; ++view_column) {
                    ++cases;
                    const unsigned lowered = original >= wide ? original - wide : 0;
                    MapView_Inset = static_cast<unsigned char>(lowered);
                    NoteInset(original, MapView_Inset);
                    Field_InputFlags = static_cast<unsigned char>(flag);
                    MapView_Row = static_cast<short>(view_row);
                    MapView_Column = static_cast<short>(view_column);
                    std::memset(g_seen, 0, sizeof g_seen);
                    std::memset(g_seen_band, 0, sizeof g_seen_band);
                    g_capcom_n = g_draw_only_n = 0;
                    DrawLayer_Open(0);   // row view_row + 1
                    const int row = view_row + 1;
                    bool original_set[28];
                    OriginalSet(original_set, static_cast<int>(original), flag, row, view_column);
                    // The wide walk's own cells, as DrawLayer_Open takes them.
                    bool wide_set[28];
                    OriginalSet(wide_set, static_cast<int>(lowered), flag, row, view_column);
                    unsigned row_handed = 0, row_band = 0;
                    for (int column = 0; column < 28; ++column) {
                        const unsigned cell = static_cast<unsigned>(row * 28 + column);
                        if (g_seen[cell] != wide_set[column]) ++bad;
                        if (!g_seen[cell]) continue;
                        ++row_handed;
                        const bool expected = !original_set[column];
                        if (g_seen_band[cell] != expected) ++bad;
                        if (g_seen_band[cell]) ++row_band;
                    }
                    // Capcom's calls: eight for each cell the original walks.
                    unsigned original_cells = 0;
                    for (bool b : original_set) original_cells += b;
                    if (g_capcom_n != 8 * original_cells || g_draw_only_n != 8 * row_band) ++bad;
                    if (c == 0 && row_band != 0) ++bad;
                    // The edge, in walk order: the band cells are the first d and the
                    // last d the wide walk takes, d the difference of the two insets
                    // as DrawLayer_Open uses them.
                    const int o_eff = flag ? (original >= 2 ? static_cast<int>(original) - 2 : 0) : static_cast<int>(original);
                    const int l_eff = flag ? (lowered >= 2 ? static_cast<int>(lowered) - 2 : 0) : static_cast<int>(lowered);
                    const int d = o_eff - l_eff, count_w = (14 - l_eff) * 2;
                    const int start_w = view_column + l_eff + (~row & 1);
                    for (int k = 1; k <= count_w; ++k) {
                        const unsigned cell = static_cast<unsigned>(row * 28 + (start_w + k) % 28);
                        if (g_seen_band[cell] != (k <= d || k > count_w - d)) ++bad;
                    }
                    if (row_band) ++edge_left;
                    handed += row_handed;
                    band += row_band;
                }
    bof3::Log("shadow      wide_rand boundary at %u columns (inset lowered by %u): %u rows (original inset 0..14, "
              "input flag, row parity, view column), %u cells handed over, %u band cells in %u rows, %u MISMATCHES",
              c, wide, cases, handed, band, edge_left, bad);
    bad_total += bad;
}

struct FuzzCounts {
    unsigned rounds = 0, split_rounds = 0, band_calls = 0, mismatches = 0, control_differs = 0;
};

void SelfTest() {
    std::memcpy(g_saved.area, AreaMap_Header, kAreaBytes);
    std::memcpy(g_saved.layers, DrawLayers, kLayerBytes);
    std::memcpy(g_saved.cells, MapView_Cells, sizeof g_saved.cells);
    std::memcpy(g_saved.handlers, MapCell_Handlers, sizeof g_saved.handlers);
    g_saved.cell_base = AreaMap_CellBase;
    g_saved.row = MapView_Row;
    g_saved.column = MapView_Column;
    g_saved.inset = MapView_Inset;
    g_saved.input_flags = Field_InputFlags;
    g_saved.buffer_index = Gfx_BufferIndex;
    const std::uint32_t saved_seed = g_seed;
    const map_cells::RandFn saved_rand = map_cells::SwapRand(RecordingRand);

    for (unsigned i = 0; i < 77; ++i)
        MapCell_Handlers[i] = static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(&OtherStandIn));
    MapCell_Handlers[kRising] = static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(&RisingStandIn));
    std::memset(DrawLayers, 0, kLayerBytes);   // no first list: DrawLayer_Open links nothing
    AreaMap_CellBase = 0xABCD0000u | kCellBase;
    Gfx_BufferIndex = 0;

    // 1. The boundary.
    g_rng = 0x56FD2A01u;
    LayCells(0, 0);
    unsigned bad = 0;
    Boundary(0, bad);
    Boundary(53, bad);
    Boundary(120, bad);

    // 2 and 3. The fuzz and its control, at the rule's lowering for 0, 53 and
    // 120 columns and at this run's own (BOF3X_WIDE_INSET may move it).
    const unsigned live = Widescreen_InsetColumns();
    const unsigned lowerings[4] = {widescreen_rule::InsetColumns(0), widescreen_rule::InsetColumns(53),
                                   widescreen_rule::InsetColumns(120), live};
    FuzzCounts counts[4];
    static unsigned narrow_log[kMaxLog];
    constexpr unsigned kRounds = 3000;
    unsigned truncated = 0;
    g_rng = 0x56FD2A02u;
    for (unsigned round = 0; round < kRounds; ++round) {
        // The camera and the view, biased to the insets the field sees.
        unsigned long angle = Next();
        if (Next() % 2) angle = (angle & ~0xFFFFul) | (0x200u + Next() % 0x81 - 0x40);
        static const short kDistance[] = {0, 256, 1124, 1543, 2176, 2560, 3900, -256, 7296};
        const short distance = Next() % 3 == 0 ? static_cast<short>(Next()) : kDistance[Next() % 9];
        const unsigned char flags = static_cast<unsigned char>(Next());
        const unsigned char fe = Next() % 8 == 0 ? 0x23 : static_cast<unsigned char>(Next());
        const short view_row = static_cast<short>(Next() % 0x38), view_column = static_cast<short>(Next() % 0x1C);
        LayCells(1 + Next() % 4, 1 + Next() % 3);
        Field_InputFlags = flags;
        MapView_Row = view_row;
        MapView_Column = view_column;

        // Narrow: the original's inset, written and noted as Inset() does.
        const map_layers::InsetPair narrow = map_layers::ComputeInset(angle, distance, flags, fe, 0);
        if (narrow.original != narrow.lowered) ++bad;
        MapView_Inset = static_cast<unsigned char>(narrow.lowered);
        NoteInset(narrow.original, MapView_Inset);
        g_capcom_n = g_draw_only_n = 0;
        Frame();
        const unsigned narrow_n = g_capcom_n;
        if (narrow_n > kMaxLog) ++truncated;
        std::memcpy(narrow_log, g_capcom_log, sizeof narrow_log);

        for (unsigned w = 0; w < 4; ++w) {
            FuzzCounts& f = counts[w];
            ++f.rounds;
            const map_layers::InsetPair pair = map_layers::ComputeInset(angle, distance, flags, fe, lowerings[w]);
            if (pair.original != narrow.original) ++bad;
            if (pair.lowered != pair.original) ++f.split_rounds;
            for (int control = 0; control < 2; ++control) {
                g_control = control != 0;
                MapView_Inset = static_cast<unsigned char>(pair.lowered);
                NoteInset(pair.original, MapView_Inset);
                g_capcom_n = g_draw_only_n = 0;
                Frame();
                const bool same = g_capcom_n == narrow_n &&
                                  std::memcmp(g_capcom_log, narrow_log,
                                              (narrow_n < kMaxLog ? narrow_n : kMaxLog) * sizeof narrow_log[0]) == 0;
                if (control) {
                    if (!same) ++f.control_differs;
                } else {
                    f.band_calls += g_draw_only_n;
                    if (!same && ++f.mismatches <= 4)
                        bof3::Log("shadow      wide_rand MISMATCH: round %u, lowered by %u: inset %u -> %u, Capcom's "
                                  "Rand %u calls against narrow's %u", round, lowerings[w], pair.original, pair.lowered,
                                  g_capcom_n, narrow_n);
                }
            }
            g_control = false;
        }
    }
    unsigned fuzz_bad = 0, control_bad = 0;
    for (unsigned w = 0; w < 4; ++w) {
        const FuzzCounts& f = counts[w];
        fuzz_bad += f.mismatches;
        // The control must be refused wherever the lowering splits some row.
        const bool refused = f.control_differs != 0;
        if (lowerings[w] != 0 && !refused) ++control_bad;
        if (lowerings[w] == 0 && f.control_differs != 0) ++control_bad;   // nothing to split: control = narrow
        bof3::Log("shadow      wide_rand fuzz, inset lowered by %u%s: %u frames (%u with a lowered inset), Capcom's Rand "
                  "as narrow's call for call in all but %u; %u draw-only calls by band cells; the control (band cells "
                  "on Capcom's Rand) %s (%u frames differ from narrow)",
                  lowerings[w], w == 3 ? " (this run's)" : w == 0 ? " (0 columns)" : w == 1 ? " (53)" : " (120)",
                  f.rounds, f.split_rounds, f.mismatches, f.band_calls,
                  lowerings[w] == 0 ? "not split, equal as it must be" : refused ? "refused" : "NOT REFUSED",
                  f.control_differs);
    }

    std::memcpy(AreaMap_Header, g_saved.area, kAreaBytes);
    std::memcpy(DrawLayers, g_saved.layers, kLayerBytes);
    std::memcpy(MapView_Cells, g_saved.cells, sizeof g_saved.cells);
    std::memcpy(MapCell_Handlers, g_saved.handlers, sizeof g_saved.handlers);
    AreaMap_CellBase = g_saved.cell_base;
    MapView_Row = g_saved.row;
    MapView_Column = g_saved.column;
    MapView_Inset = g_saved.inset;
    Field_InputFlags = g_saved.input_flags;
    Gfx_BufferIndex = g_saved.buffer_index;
    map_cells::SwapRand(saved_rand);
    g_seed = saved_seed;
    g_noted = false;
    g_band = false;
    g_control = false;
    bof3::Log("shadow      wide_rand self-test: the boundary at 0 / 53 / 120 columns and %u fuzz frames at four "
              "lowerings, %u MISMATCHES (%u in the fuzz), the control %s; frames with more than %u Capcom calls: %u",
              kRounds, bad + fuzz_bad, fuzz_bad, control_bad ? "NOT REFUSED where it must be" : "refused at every split",
              kMaxLog, truncated);
    if (bad + fuzz_bad) bof3::Fatal("wide_rand: the split differs from the original's walk in %u checks", bad + fuzz_bad);
    if (control_bad) bof3::Fatal("wide_rand: the control was not refused at %u lowerings", control_bad);
}

}  // namespace

void Inject() {
    if (bof3::WantsShadow("wide_rand")) SelfTest();
    const unsigned wide = Widescreen_InsetColumns();
    if (wide == 0)
        bof3::Log("DIV-0041    the map walk's random split: idle (the cell inset is not lowered), every cell on Capcom's "
                  "Rand");
    else
        bof3::Log("DIV-0041    the map walk's random split: cells only the inset lowered by %u adds draw "
                  "MapCell_DrawRising's squares from a generator of ours; the original's cells keep Capcom's Rand",
                  wide);
}

}  // namespace wide_rand
