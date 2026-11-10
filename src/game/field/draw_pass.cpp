#include "game/draw_pass.h"

#include "game/draw_pool.h"
#include "game/layering.h"
#include "game/map_rim.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"

namespace {

std::uint16_t Key(const unsigned char* object) {
    std::uint16_t key;
    std::memcpy(&key, object + 0x32, sizeof key);
    return key;
}

// The three callees that are not ours, through pointers so that the start-up
// fuzz can stand recording functions in for them - for the original's copy
// and for ours alike.
using OpenFn = void (__cdecl*)(int);
using AddRecordsFn = unsigned (__cdecl*)(unsigned long*);
using DrawFn = void (__cdecl*)();
}  // namespace
// draw_pass::g_open, g_add_records and g_draw are defined in field/sprite_2.cpp,
// beside their reader Sprite_DrawPass (the source layout's cleanup round, I60).
namespace {

// --- BOF3X_SHADOW=draw_pass: a differential fuzz, once at start-up ----------------
// A byte-copy of the original with its eight calls to functions of ours
// re-aimed at byte-copies of those, and its three calls to DrawLayer_Open,
// Sprite_AddDrawRecords and Sprite_Draw at the recorders below. One round: a
// random draw list of fake objects, a random - but ordered - draw table,
// flags, slot and buffer; theirs, then from the same state ours; the list,
// the records, the ordering-table tails, the packet pool's head, the layers
// and the recorders' log compared.
//
// What the fuzz must NOT generate, because the original never returns from
// it: a slot byte other than 4 or 6 with a sprite in a layer, or a draw table
// out of order. Both leave the merge loop with nothing to consume.

constexpr unsigned kObjects = 40, kObjectBytes = 0xA4, kRecords = 100, kLog = 512;
// The layers and no further: the buffer index is 0 or 1 here, and what lies
// 2 KB on is Sprite_DrawListCount, which this fuzz also owns.
constexpr unsigned kLayerBytes = 0x38 * 0x30;
constexpr unsigned kWindow = 0x300;   // a draw mode per layer: 0x38 * 0xC bytes
constexpr unsigned kPrims = 64;

unsigned char g_objects[kObjects][kObjectBytes];
unsigned long g_tails[8];
unsigned long g_prims[kPrims];   // stand-ins for primitives: a link is a store through one of these
struct LogEntry { unsigned long what, arg, current; };
LogEntry g_log[kLog];
unsigned g_log_n, g_stub_seed;

void Record(unsigned long what, unsigned long arg) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, arg, Low32(Sprite_Current.Native())};
    ++g_log_n;
}
void __cdecl StubOpen(int layer) { Record(1, static_cast<unsigned long>(layer)); }
void __cdecl StubDraw() { Record(3, 0); }
// Adds none, one or two records, by a hash of the round and the call; each
// either a sprite to draw (bit 31) or a primitive to link, whose third dword
// must then be an object - the pass reads its byte +0x29. Only al of the
// result counts, so the rest of eax is noise.
unsigned __cdecl StubAddRecords(unsigned long* record) {
    const unsigned index = static_cast<unsigned>(record - Sprite_DrawRecords) / 3u;
    Record(2, index);
    std::uint32_t h = (g_stub_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    const unsigned n = h % 3u;
    for (unsigned i = 0; i < n; ++i) {
        h = h * 0x85EBCA6Bu + 1u;
        record[3 * i] = Low32(&g_prims[h >> 20 & (kPrims - 1)]);
        record[3 * i + 1] = (h >> 8 & 0x7FFF0000u) | (h >> 3 & 0xFFFFu) | (h & 1u ? 0x80000000u : 0u);
        record[3 * i + 2] = Low32(g_objects[h >> 4 & 31u]);
    }
    return n | 0xABCDEF00u;
}

struct State {
    unsigned char objects[kObjects][kObjectBytes];
    unsigned char* list[kObjects];
    unsigned char list_count, table_count, flags, slot, buffer_index;
    unsigned long table[256];
    unsigned long records[kRecords * 3];
    unsigned char layers[kLayerBytes];
    unsigned char* packet_next;
    unsigned long* ot[8];
    unsigned long tails[8], prims[kPrims];
    unsigned char items[DrawItems_count];
    unsigned char* current;
    unsigned char window[kWindow];
    LogEntry log[kLog];
    unsigned log_n;
};

void Capture(State& s, const unsigned char* window_at) {
    std::memcpy(s.objects, g_objects, sizeof s.objects);
    std::memcpy(s.list, Sprite_DrawList, sizeof s.list);
    s.list_count = Sprite_DrawListCount; s.table_count = DrawTable_Count;
    s.flags = Draw_PassFlags; s.slot = Draw_OtSlot; s.buffer_index = Gfx_BufferIndex;
    std::memcpy(s.table, DrawTable, sizeof s.table);
    std::memcpy(s.records, Sprite_DrawRecords, sizeof s.records);
    std::memcpy(s.layers, DrawLayers, kLayerBytes);
    s.packet_next = Gfx_PacketNext;
    std::memcpy(s.ot, Gfx_OtPointers, sizeof s.ot);
    std::memcpy(s.tails, g_tails, sizeof s.tails);
    std::memcpy(s.prims, g_prims, sizeof s.prims);
    std::memcpy(s.items, DrawItems, sizeof s.items);
    s.current = Sprite_Current;
    if (window_at) std::memcpy(s.window, window_at, kWindow);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s, unsigned char* window_at) {
    std::memcpy(g_objects, s.objects, sizeof s.objects);
    std::memcpy(Sprite_DrawList, s.list, sizeof s.list);
    Sprite_DrawListCount = s.list_count; DrawTable_Count = s.table_count;
    Draw_PassFlags = s.flags; Draw_OtSlot = s.slot; Gfx_BufferIndex = s.buffer_index;
    std::memcpy(DrawTable, s.table, sizeof s.table);
    std::memcpy(Sprite_DrawRecords, s.records, sizeof s.records);
    std::memcpy(DrawLayers, s.layers, kLayerBytes);
    Gfx_PacketNext = s.packet_next;
    std::memcpy(Gfx_OtPointers, s.ot, sizeof s.ot);
    std::memcpy(g_tails, s.tails, sizeof s.tails);
    std::memcpy(g_prims, s.prims, sizeof s.prims);
    std::memcpy(DrawItems, s.items, sizeof s.items);
    Sprite_Current = s.current;
    if (window_at) std::memcpy(window_at, s.window, kWindow);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

void SelfTest(DrawFn theirs) {
    constexpr unsigned kRounds = 3000;
    static State saved, input, their_out, our_out;
    Capture(saved, nullptr);
    draw_pass::g_open = &StubOpen; draw_pass::g_add_records = &StubAddRecords; draw_pass::g_draw = &StubDraw;

    std::uint32_t rng = 0x27D4EB2Fu;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    unsigned bad = 0, sprites = 0, table_items = 0, ties = 0, stub_calls = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, items); ++i) bytes[i] = static_cast<unsigned char>(next());
        for (auto& b : input.window) b = static_cast<unsigned char>(next());
        // Objects: keys from a few layers and a few orders, so that ties at
        // every level of the sort are common; +0x29 one of the eight slots.
        const unsigned layers_used = 1 + next() % 6, first_layer = next() % 0x3A;
        for (auto& o : input.objects) {
            const std::uint16_t key = static_cast<std::uint16_t>((first_layer + next() % layers_used) << 8 | next() % 4);
            std::memcpy(o + 0x32, &key, 2);
            for (unsigned at : {0x34u, 0x38u, 0x3Cu}) {
                const std::int32_t v = next() % 4 ? static_cast<std::int32_t>(next() % 5) - 2 : static_cast<std::int32_t>(next());
                std::memcpy(o + at, &v, 4);
            }
            o[0x29] = static_cast<unsigned char>(next() % 8);
        }
        input.list_count = static_cast<unsigned char>(next() % 8 ? next() % 12 : next() % (kObjects + 1));
        for (unsigned i = 0; i < kObjects; ++i) input.list[i] = g_objects[next() % kObjects];
        // The table: ascending in its top 16 bits, as DrawTable_Sort leaves
        // it, from the same few layers; the entry at the count too, since the
        // pass reads it.
        input.table_count = static_cast<unsigned char>(next() % 10);
        std::uint32_t key16 = first_layer << 8;
        for (unsigned i = 0; i < 256; ++i) {
            if (i && next() % 3) key16 += next() % 3 ? next() % 3 : 0x100u;
            if (key16 > 0xFFFFu) key16 = 0xFFFFu;
            // The item index below 1,024: two per index, 2,048 in DrawItems; bits 12-15 are noise.
            input.table[i] = key16 << 16 | (next() & 0xF3FFu);
        }
        input.flags = static_cast<unsigned char>(next() % 4 ? next() | 0x1E : next());
        input.slot = next() % 2 ? 6 : 4;
        input.buffer_index = static_cast<unsigned char>(next() % 2);
        for (unsigned k = 0; k < 8; ++k) input.ot[k] = &g_tails[k];
        // Every list's "last" is somewhere a link may be stored; a "first" is
        // only ever stored, and is zero - no list - a third of the time.
        auto* const pairs = reinterpret_cast<unsigned long*>(input.layers);
        for (unsigned d = 0; d + 1 < 0x38 * 12; d += 2) {
            if (next() % 3 == 0) pairs[d] = 0;
            pairs[d + 1] = Low32(&g_prims[next() % kPrims]);
        }
        std::memcpy(input.items, saved.items, sizeof input.items);   // the items as found: linking stores into them
        input.packet_next = Gfx_PacketPools + (next() % 2 ? 0x10000u : 0u) + next() % 0x8000u;
        input.current = g_objects[0];
        g_stub_seed = next();

        unsigned char* const window_at = input.packet_next;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, window_at);
            (pass ? &Sprite_DrawPass : theirs)();
            Capture(pass ? our_out : their_out, window_at);
        }
        sprites += input.list_count;
        table_items += input.table_count;
        stub_calls += their_out.log_n;
        for (unsigned i = 1; i < input.list_count; ++i)
            if (Key(their_out.list[i]) == Key(their_out.list[i - 1])) ++ties;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 8)
            bof3::Log("shadow      draw_pass self-test MISMATCH: round %u, %u sprites, %u table items, flags %02X, "
                      "slot %u, log %u / %u", round, input.list_count, input.table_count, input.flags, input.slot,
                      their_out.log_n, our_out.log_n);
    }
    draw_pass::g_open = DrawLayer_Open; draw_pass::g_add_records = Sprite_AddDrawRecords; draw_pass::g_draw = Sprite_Draw;
    Apply(saved, nullptr);
    std::memset(g_log, 0, sizeof g_log);
    bof3::Log("shadow      draw_pass self-test: %u rounds, %u sprites (%u neighbours with equal keys after the sort), "
              "%u table items, %u calls to the three stand-ins, %u MISMATCHES; the list, the records, the tails, "
              "the packet pool's head, the layers and the stand-ins' log compared", kRounds, sprites, ties,
              table_items, stub_calls, bad);
    if (bad) bof3::Fatal("Sprite_DrawPass differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void DrawPass_Inject() {
    // Called before the modules that own its callees: a function can only be
    // cloned before its entry is patched. Every jump stays inside 0x593060..
    // 0x59352B; its eleven calls, by capstone 2026-09-20, are tabled below.
    if (bof3::WantsShadow("draw_pass")) {
        const void* list_swap = bof3::CloneOriginal("Sprite_DrawListSwap", bof3::addr::Sprite_DrawListSwap, 0x36);
        const void* record_swap = bof3::CloneOriginal("Sprite_DrawRecordSwap", bof3::addr::Sprite_DrawRecordSwap, 0x35);
        const void* link = bof3::CloneOriginal("Gpu_LinkPrim", bof3::addr::Gpu_LinkPrim, 0xB);
        const void* mode = bof3::CloneOriginal("Gpu_SetDrawMode", bof3::addr::Gpu_SetDrawMode, 0x41);
        const bof3::CloneCall commit_calls[] = {{0x44, link}};
        const void* commit = bof3::CloneOriginal("Gfx_CommitPrim", bof3::addr::Gfx_CommitPrim, 0x5D, commit_calls, 1);
        const bof3::CloneCall close_calls[] = {{0x12, mode}, {0x1B, commit}, {0x52, link}};
        const void* close = bof3::CloneOriginal("DrawLayer_Close", bof3::addr::DrawLayer_Close, 0x7C, close_calls, 3);
        const bof3::CloneCall calls[] = {
            {0x89, list_swap}, {0xE0, mode}, {0xE9, commit}, {0x115, reinterpret_cast<const void*>(&StubOpen)}, {0x14B, link}, {0x26D, link},
            {0x35B, reinterpret_cast<const void*>(&StubAddRecords)}, {0x3EE, record_swap}, {0x440, reinterpret_cast<const void*>(&StubDraw)}, {0x463, link}, {0x498, close},
        };
        SelfTest(reinterpret_cast<DrawFn>(
            bof3::CloneOriginal("Sprite_DrawPass", bof3::addr::Sprite_DrawPass, 0x4CB, calls, 11)));
    }
}
