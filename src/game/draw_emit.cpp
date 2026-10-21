#include "game/draw_emit.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/layering.h"
#include "game/map_rim.h"
#include "game/wide_rand.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"

namespace {

// The end of a 64 KB packet pool less 0x54 bytes of margin - the literal
// 0x7F1BAC in the original, for buffer 0.
constexpr std::uint32_t kPoolLimit = 0x7F1BAC;   // Gfx_PacketPools 0x7E1C00 + 0x10000 - 0x54

// A layer is 0x30 bytes: three lists, each a (first, last) pair per buffer.
// The second list, at +0x10, is the one closed here.
constexpr unsigned kLayerDwords = 12, kSecondList = 4;

// --- BOF3X_SHADOW=draw_emit: a differential fuzz, once at start-up ---------------
// Byte-copies of the two, their calls re-aimed at byte-copies of Gpu_LinkPrim,
// Gpu_SetDrawMode and each other. The packet pointer is kept inside the real
// pool - the room test compares addresses - often right at its limit; the
// ordering-table pointers are aimed at a scratch row of tails.

constexpr unsigned kLayers = 0x38;
constexpr unsigned kOwned = kLayers * kLayerDwords * 4 + 255 * 8;   // as DrawLayers_Reset's fuzz: the index is a whole byte
constexpr unsigned kWindow = 0x40;

struct State {
    unsigned char layers[kOwned];
    unsigned char* packet_next;
    unsigned long* ot[8];
    unsigned char buffer_index, slot;
    unsigned long tails[8];
    unsigned char window[kWindow];   // the pool, from packet_next as it was on the way in
};

unsigned long g_tails[8];

void Capture(State& s, const unsigned char* window_at) {
    std::memcpy(s.layers, DrawLayers, kOwned);
    s.packet_next = Gfx_PacketNext;
    std::memcpy(s.ot, Gfx_OtPointers, sizeof s.ot);
    s.buffer_index = Gfx_BufferIndex;
    s.slot = Draw_OtSlot;
    std::memcpy(s.tails, g_tails, sizeof s.tails);
    if (window_at) std::memcpy(s.window, window_at, kWindow);
}
void Apply(const State& s, unsigned char* window_at) {
    std::memcpy(DrawLayers, s.layers, kOwned);
    Gfx_PacketNext = s.packet_next;
    std::memcpy(Gfx_OtPointers, s.ot, sizeof s.ot);
    Gfx_BufferIndex = s.buffer_index;
    Draw_OtSlot = s.slot;
    std::memcpy(g_tails, s.tails, sizeof s.tails);
    if (window_at) std::memcpy(window_at, s.window, kWindow);
}

using Fn = void (__cdecl*)(unsigned, unsigned);

void SelfTest(Fn their_commit, Fn their_close) {
    constexpr unsigned kRounds = 12000;
    static State saved, input, their_out, our_out;
    Capture(saved, nullptr);
    static unsigned char saved_pool[2 * 0x10000];
    std::memcpy(saved_pool, Gfx_PacketPools, sizeof saved_pool);

    std::uint32_t rng = 0xC2B2AE35u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    unsigned bad = 0, full = 0, linked = 0;
    const Fn our_commit = reinterpret_cast<Fn>(reinterpret_cast<void*>(&Gfx_CommitPrim));
    const Fn our_close = reinterpret_cast<Fn>(reinterpret_cast<void*>(&DrawLayer_Close));
    for (unsigned round = 0; round < kRounds; ++round) {
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < sizeof input; ++i) bytes[i] = static_cast<unsigned char>(next());
        input.buffer_index = static_cast<unsigned char>(next() % 8 ? next() % 2 : next());
        input.slot = static_cast<unsigned char>(next() % 8);
        for (unsigned k = 0; k < 8; ++k) input.ot[k] = &g_tails[next() % 8];
        // The packet pointer: inside buffer 0's pool or buffer 1's, half the
        // time within 0x100 bytes of the room test's limit for buffer 0 or 1.
        const std::uint32_t pool = next() % 2 ? 0x10000u : 0u;
        std::uint32_t offset = next() % (0x10000u - kWindow - 0x200u);
        if (next() % 2 == 0) offset = 0x10000u - 0x54u - 0x100u + next() % 0x100u;
        input.packet_next = Gfx_PacketPools + pool + offset;
        // A layer's second list: empty a third of the time.
        const unsigned layer = next() % kLayers;
        const bool is_close = round % 2 == 1;
        unsigned long* const cell =
            reinterpret_cast<unsigned long*>(input.layers) + layer * kLayerDwords + kSecondList + input.buffer_index * 2u;
        if (reinterpret_cast<unsigned char*>(cell) + 8 <= input.layers + kOwned && next() % 3 == 0) cell[0] = 0;
        // Noise above the byte each reads; the slot itself one of the eight
        // there are, since the ninth is somebody else's memory.
        const unsigned a0 = is_close ? layer : ((next() & ~0xFFu) | (next() % 8)), a1 = next();

        unsigned char* const window_at = input.packet_next;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, window_at);
            if (is_close) (pass ? our_close : their_close)(a0, a1);
            else (pass ? our_commit : their_commit)(a0, a1);
            Capture(pass ? our_out : their_out, window_at);
        }
        if (their_out.packet_next == input.packet_next) ++full;
        if (is_close && std::memcmp(their_out.tails, input.tails, sizeof input.tails) != 0) ++linked;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 8)
            bof3::Log("shadow      draw_emit self-test MISMATCH: %s, round %u, buffer %u, packet pointer %p",
                      is_close ? "DrawLayer_Close" : "Gfx_CommitPrim", round, input.buffer_index,
                      static_cast<void*>(input.packet_next));
    }
    Apply(saved, nullptr);
    std::memcpy(Gfx_PacketPools, saved_pool, sizeof saved_pool);
    bof3::Log("shadow      draw_emit self-test: 2 functions, %u rounds (%u where the pool had no room, %u closes that "
              "linked something), %u MISMATCHES; the layers, the packet pointer and 0x%X bytes at it, the "
              "ordering-table pointers and their tails compared", kRounds, full, linked, bad, kWindow);
    if (bad) bof3::Fatal("draw_emit differs from the originals in %u of %u self-test rounds", bad, kRounds);
}

// --- BOF3X_SHADOW=draw_emit, second part: DrawLayer_Open -------------------------
// Its clone keeps its call to Gpu_LinkPrim aimed at Capcom's (psx_gpu injects
// later), and its handler call reads MapCell_Handlers as ours does - so for
// the fuzz the table's 77 entries are four recording stand-ins, put back
// after. One round: the layers, the buffer byte, the view's row, column and
// inset, the input flag, the cell row and a set of record runs laid out in the
// area block, all random; theirs, then ours from the same state; the stand-ins'
// log, the layers, the area window and slot 6's pointer and tails compared.
//
// Half the rounds build every run of one-dword records, and a stand-in then
// lengthens the record it was handed to two dwords where the run has room:
// the walk must read the step after the call to follow it.

constexpr unsigned kLayerBytes = 0x38 * kLayerDwords * 4;
constexpr unsigned kAreaDwords = 0x2000;   // the area block window the runs are laid out in
constexpr unsigned kMaxLog = 2048;
constexpr unsigned kMaxRuns = 24;

struct OpenLog {
    unsigned n;
    std::uint32_t entry[kMaxLog][4];   // stand-in, record offset in the area block, b1, b0
};
OpenLog g_open_log;
bool g_open_lengthen;
std::uint32_t g_open_rng;
struct Run { unsigned char* start; unsigned char* end; };
Run g_runs[kMaxRuns];
unsigned g_run_count;

void OpenRecord(unsigned id, unsigned char* record, unsigned b1, unsigned b0) {
    if (g_open_log.n < kMaxLog) {
        std::uint32_t* e = g_open_log.entry[g_open_log.n];
        e[0] = id;
        e[1] = static_cast<std::uint32_t>(record - AreaMap_Header);
        e[2] = b1;
        e[3] = b0;
    }
    ++g_open_log.n;
    if (!g_open_lengthen) return;
    g_open_rng ^= g_open_rng << 13; g_open_rng ^= g_open_rng >> 17; g_open_rng ^= g_open_rng << 5;
    if (g_open_rng % 3 != 0) return;
    for (unsigned r = 0; r < g_run_count; ++r)
        if (record >= g_runs[r].start && record + 8 <= g_runs[r].end) { record[2] = 2; return; }
}
void __cdecl OpenStandIn0(unsigned char* r, unsigned b1, unsigned b0) { OpenRecord(0, r, b1, b0); }
void __cdecl OpenStandIn1(unsigned char* r, unsigned b1, unsigned b0) { OpenRecord(1, r, b1, b0); }
void __cdecl OpenStandIn2(unsigned char* r, unsigned b1, unsigned b0) { OpenRecord(2, r, b1, b0); }
void __cdecl OpenStandIn3(unsigned char* r, unsigned b1, unsigned b0) { OpenRecord(3, r, b1, b0); }

struct OpenState {
    unsigned char layers[kLayerBytes];
    unsigned char area[kAreaDwords * 4];
    std::uint32_t cell_base;
    unsigned short cells[1568];
    short row, column;
    unsigned char inset, input_flags, buffer_index;
    unsigned long* slot6;
    unsigned long tails[8];
    OpenLog log;
};

void OpenCapture(OpenState& s) {
    std::memcpy(s.layers, DrawLayers, kLayerBytes);
    std::memcpy(s.area, AreaMap_Header, sizeof s.area);
    s.cell_base = AreaMap_CellBase;
    std::memcpy(s.cells, MapView_Cells, sizeof s.cells);
    s.row = MapView_Row;
    s.column = MapView_Column;
    s.inset = MapView_Inset;
    s.input_flags = Field_InputFlags;
    s.buffer_index = Gfx_BufferIndex;
    s.slot6 = Gfx_OtPointers[6];
    std::memcpy(s.tails, g_tails, sizeof s.tails);
    std::memcpy(&s.log, &g_open_log, sizeof s.log);
}
void OpenApply(const OpenState& s) {
    std::memcpy(DrawLayers, s.layers, kLayerBytes);
    std::memcpy(AreaMap_Header, s.area, sizeof s.area);
    AreaMap_CellBase = s.cell_base;
    std::memcpy(MapView_Cells, s.cells, sizeof s.cells);
    MapView_Row = s.row;
    MapView_Column = s.column;
    MapView_Inset = s.inset;
    Field_InputFlags = s.input_flags;
    Gfx_BufferIndex = s.buffer_index;
    Gfx_OtPointers[6] = s.slot6;
    std::memcpy(g_tails, s.tails, sizeof s.tails);
    std::memcpy(&g_open_log, &s.log, sizeof s.log);
}

using OpenFn = void (__cdecl*)(int);

void SelfTestOpen(OpenFn theirs) {
    constexpr unsigned kRounds = 12000;
    static OpenState saved, input, their_out, our_out;
    OpenCapture(saved);
    unsigned long saved_handlers[77];
    std::memcpy(saved_handlers, MapCell_Handlers, sizeof saved_handlers);
    const unsigned long stand_ins[4] = {
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(&OpenStandIn0)),
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(&OpenStandIn1)),
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(&OpenStandIn2)),
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(&OpenStandIn3))};
    for (unsigned i = 0; i < 77; ++i) MapCell_Handlers[i] = stand_ins[i % 4];

    std::uint32_t rng = 0x56FD2011u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    unsigned bad = 0, linked = 0, walked = 0, calls = 0, lengthened_rounds = 0, idle = 0, wrapped = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < sizeof input; ++i) bytes[i] = static_cast<unsigned char>(next());
        input.log.n = 0;
        const int layer = static_cast<int>(next() % 0x37);
        input.buffer_index = static_cast<unsigned char>(next() % 6);
        if (next() % 3 == 0) {   // the first list empty a third of the time
            std::uint32_t* pair = reinterpret_cast<std::uint32_t*>(input.layers) + (input.buffer_index + layer * 6u) * 2u;
            pair[0] = 0;
        }
        input.slot6 = &g_tails[next() % 8];
        input.row = static_cast<short>(next() % 0x38);
        input.column = static_cast<short>(next() % 0x1C);
        input.inset = static_cast<unsigned char>(next() % 4 == 0 ? next() % 18 : next() % 4);
        const unsigned base = 0x10 + next() % 0x100;
        input.cell_base = (next() & 0xFFFF0000u) | base;

        // The runs: laid out one after another from dword base + 1, each
        // headed by the dword before it.
        const bool lengthen = next() % 2 == 0;
        std::uint32_t* area = reinterpret_cast<std::uint32_t*>(input.area);
        unsigned at = base + 1;
        unsigned run_words[kMaxRuns];
        g_run_count = 1 + next() % kMaxRuns;
        for (unsigned r = 0; r < g_run_count; ++r) {
            const unsigned length = next() % 5 == 0 ? 0 : 1 + next() % 12;   // dwords of records
            area[at - 1] = (next() & 0xFFFFu) | ((length + 1) << 16);
            unsigned k = 0;
            while (k < length) {
                const unsigned step = lengthen ? 1u : 1u + next() % (length - k < 4 ? length - k : 4);
                auto* record = reinterpret_cast<unsigned char*>(&area[at + k]);
                record[2] = static_cast<unsigned char>(step);
                record[3] = static_cast<unsigned char>(next() % 77);
                k += step;
            }
            g_runs[r].start = AreaMap_Header + at * 4;
            g_runs[r].end = AreaMap_Header + (at + length) * 4;
            run_words[r] = at - base;
            at += length + 1;
        }
        // The cells: mostly empty, the rest naming runs.
        for (auto& c : input.cells) c = next() % 4 == 0 ? static_cast<unsigned short>(run_words[next() % g_run_count]) : 0;

        const bool field_flag = input.input_flags & 1;
        int inset = input.inset;
        if (field_flag) inset = inset >= 2 ? inset - 2 : 0;
        if (inset >= 14) ++idle;
        if (input.row + layer + 1 >= 0x38) ++wrapped;
        if (lengthen) ++lengthened_rounds;

        for (int pass = 0; pass < 2; ++pass) {
            OpenApply(input);
            g_open_lengthen = lengthen;
            g_open_rng = round * 2654435761u + 1;
            (pass ? OpenFn(&DrawLayer_Open) : theirs)(layer);
            OpenCapture(pass ? our_out : their_out);
        }
        if (std::memcmp(their_out.tails, input.tails, sizeof input.tails) != 0) ++linked;
        if (their_out.log.n) ++walked;
        calls += their_out.log.n;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 8)
            bof3::Log("shadow      DrawLayer_Open self-test MISMATCH: round %u, layer %d, row %d, column %d, inset %u, "
                      "calls %u / %u", round, layer, input.row, input.column, input.inset, their_out.log.n,
                      our_out.log.n);
    }
    std::memcpy(MapCell_Handlers, saved_handlers, sizeof saved_handlers);
    OpenApply(saved);
    bof3::Log("shadow      DrawLayer_Open self-test: %u rounds (%u linked the first list, %u walked cells - %u "
              "handler calls, %u rounds with steps lengthened by the handler - %u with no columns, %u rows wrapped), %u "
              "MISMATCHES; the layers, the area window, the cells, slot 6 and its tails and the handlers' log compared",
              kRounds, linked, walked, calls, lengthened_rounds, idle, wrapped, bad);
    if (bad) bof3::Fatal("DrawLayer_Open differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

// original 0x461E50: the primitive just built at Gfx_PacketNext joins ordering
// -table slot `slot`, and the packet pointer moves on by `size` - if the pool
// has room for it, and silently not at all if it has not. Both arguments are
// bytes. 640,316 calls a whole attract cycle.
//
// As the original has it: the slot is not checked against the eight there are.
extern "C" void __cdecl Gfx_CommitPrim(unsigned slot, unsigned size) {
    size &= 0xFF;
    const std::uint32_t limit = (static_cast<std::uint32_t>(Gfx_BufferIndex) << 16) + kPoolLimit;
    const std::uint32_t next = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Gfx_PacketNext));
    if (limit <= next + size) {
        // Diagnostic only (BOF3X_DRAWORDER): the original's silent skip.
        if (draw_order::Tagging())
            bof3::Log("draworder   packet pool full: a %u-byte primitive for slot %u skipped (next %08X, limit %08X) from %08X",
                      size, slot & 0xFF, next, limit, static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
        return;
    }
    slot &= 0xFF;
    if (draw_order::g_on)   // BOF3X_DRAWORDER, diagnostic only
        draw_order::TagCommit(next, slot, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
    // DIV-0071 (layering.h): what a map cell's record run draws, for the next
    // pass's verdicts; only while DrawLayer_Open runs a record under mode 1.
    if (layering::g_in_run) layering::RunPrim(Gfx_PacketNext, size);
    Gpu_LinkPrim(Gfx_OtPointers[slot], next);
    Gfx_OtPointers[slot] = reinterpret_cast<unsigned long*>(Gfx_PacketNext);
    Gfx_PacketNext = Gfx_PacketNext + size;
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

namespace {

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

}  // namespace

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
    if (map_rim::Active()) {
        OpenGrown(layer);
        return;
    }
    LinkFirst(layer);
    if (!CapcomRow(layer)) return;
    wide_rand::SetBand(false);
    if (draw_order::g_on) draw_order::Clear();
}

void DrawEmit_Inject() {
    // Called before psx_gpu's Inject: a function can only be cloned before its
    // entry is patched. Calls, capstone 2026-09-20: Gfx_CommitPrim +0x44 ->
    // Gpu_LinkPrim; DrawLayer_Close +0x12 -> Gpu_SetDrawMode, +0x1B ->
    // Gfx_CommitPrim, +0x52 -> Gpu_LinkPrim. No jump leaves either.
    if (bof3::WantsShadow("draw_emit")) {
        const void* link = bof3::CloneOriginal("Gpu_LinkPrim", bof3::addr::Gpu_LinkPrim, 0xB);
        const void* mode = bof3::CloneOriginal("Gpu_SetDrawMode", bof3::addr::Gpu_SetDrawMode, 0x41);
        const bof3::CloneCall commit_calls[] = {{0x44, link}};
        const void* commit =
            bof3::CloneOriginal("Gfx_CommitPrim", bof3::addr::Gfx_CommitPrim, 0x5D, commit_calls, 1);
        const bof3::CloneCall close_calls[] = {{0x12, mode}, {0x1B, commit}, {0x52, link}};
        const void* close = bof3::CloneOriginal("DrawLayer_Close", bof3::addr::DrawLayer_Close, 0x7C, close_calls, 3);
        SelfTest(reinterpret_cast<Fn>(const_cast<void*>(commit)), reinterpret_cast<Fn>(const_cast<void*>(close)));
        // DrawLayer_Open: 0x159 bytes, one direct call, +0x2A -> Gpu_LinkPrim
        // (left on Capcom's), and the handler call through MapCell_Handlers,
        // an absolute operand the copy keeps (capstone 2026-09-21).
        const bof3::CloneCall open_calls[] = {{0x2A, nullptr}};
        SelfTestOpen(reinterpret_cast<OpenFn>(
            bof3::CloneOriginal("DrawLayer_Open", bof3::addr::DrawLayer_Open, 0x159, open_calls, 1)));
    }
    BOF3_INJECT(Gfx_CommitPrim);
    BOF3_INJECT(DrawLayer_Close);
    BOF3_INJECT(DrawLayer_Open);
}
