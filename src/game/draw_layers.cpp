#include "game/draw_layers.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr unsigned kLayers = 0x38, kLayerBytes = 0x30;
static_assert(kLayers * kLayerBytes == DrawLayers_count * sizeof(unsigned long));

// --- BOF3X_SHADOW=draw_layers: differential fuzzes, once at start-up -----------
// Neither function calls anything and every jump is internal, so byte-copies
// run in place against the same globals, which the fuzz owns and puts back.

std::uint32_t g_rng = 0xA54FF53Au;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}

using VoidFn = void (__cdecl*)();

// Reset uses Gfx_BufferIndex as a whole byte, so it can reach 255 * 8 bytes
// past the layers: the fuzz owns that much more. The index is 0 or 1 three
// rounds in four.
void SelfTestReset(VoidFn theirs) {
    constexpr unsigned kRounds = 600;
    constexpr unsigned kOwned = kLayers * kLayerBytes + 255 * 8;
    static unsigned char saved[kOwned], input[kOwned], their_out[kOwned];
    auto* const base = reinterpret_cast<unsigned char*>(DrawLayers);
    std::memcpy(saved, base, kOwned);
    const unsigned char saved_index = Gfx_BufferIndex;
    unsigned bad = 0, beyond = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (auto& b : input) b = static_cast<unsigned char>(Rng());
        Gfx_BufferIndex = static_cast<unsigned char>(Rng() % 4 ? Rng() % 2 : Rng());
        if (Gfx_BufferIndex > 1) ++beyond;
        std::memcpy(base, input, kOwned);
        theirs();
        std::memcpy(their_out, base, kOwned);
        std::memcpy(base, input, kOwned);
        DrawLayers_Reset();
        if (std::memcmp(their_out, base, kOwned) != 0 && ++bad <= 8)
            bof3::Log("shadow      DrawLayers_Reset self-test MISMATCH round %u: buffer index %u", round, Gfx_BufferIndex);
    }
    std::memcpy(base, saved, kOwned);
    Gfx_BufferIndex = saved_index;
    bof3::Log("shadow      DrawLayers_Reset self-test: %u rounds (%u with a buffer index above 1), %u MISMATCHES; "
              "0x%X bytes compared", kRounds, beyond, bad, kOwned);
    if (bad) bof3::Fatal("DrawLayers_Reset differs from the original in %u of %u self-test rounds", bad, kRounds);
}

// Sort: counts 0 to 255, small ones more often; keys from a handful of values,
// so that equal keys - where an unstable sort shows its order - are common,
// with the low 16 bits random, so that the order shows.
void SelfTestSort(VoidFn theirs) {
    constexpr unsigned kRounds = 3000;
    static unsigned long saved[DrawTable_count], input[DrawTable_count], their_out[DrawTable_count];
    std::memcpy(saved, DrawTable, sizeof saved);
    const unsigned char saved_count = DrawTable_Count;
    const unsigned long saved_scratch = Scratch_Swap;
    unsigned bad = 0, empty = 0, with_ties = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned spread = 1 + Rng() % (Rng() % 2 ? 6 : 0x10000);
        for (auto& w : input) w = ((Rng() % spread) << 16 ^ (Rng() % 4 == 0 ? 0x80000000u : 0u)) | (Rng() & 0xFFFFu);
        DrawTable_Count = static_cast<unsigned char>(Rng() % 3 ? Rng() % 24 : Rng());
        if (DrawTable_Count == 0) ++empty;
        if (spread <= 6 && DrawTable_Count > 6) ++with_ties;
        const unsigned long scratch_in = Rng();

        std::memcpy(DrawTable, input, sizeof input);
        Scratch_Swap = scratch_in;
        theirs();
        std::memcpy(their_out, DrawTable, sizeof their_out);
        const unsigned long their_scratch = Scratch_Swap;
        std::memcpy(DrawTable, input, sizeof input);
        Scratch_Swap = scratch_in;
        DrawTable_Sort();
        if ((std::memcmp(their_out, DrawTable, sizeof their_out) != 0 || their_scratch != Scratch_Swap) && ++bad <= 8)
            bof3::Log("shadow      DrawTable_Sort self-test MISMATCH round %u: count %u, scratch 0x%08lX vs ours 0x%08lX",
                      round, DrawTable_Count, their_scratch, Scratch_Swap);
    }
    std::memcpy(DrawTable, saved, sizeof saved);
    DrawTable_Count = saved_count;
    Scratch_Swap = saved_scratch;
    bof3::Log("shadow      DrawTable_Sort self-test: %u rounds (%u empty, %u certain to hold equal keys), %u MISMATCHES; "
              "256 entries and the scratch word compared", kRounds, empty, with_ties, bad);
    if (bad) bof3::Fatal("DrawTable_Sort differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

// original 0x56F5B0. Empties two of each layer's three draw lists, for the
// display buffer being built: first = null, last = the head itself, so that
// the first append lands in `first` through `last` like any other.
//
// As the original has it: 0x38 layers where 0x593060 walks 0x37, the third
// list of each layer left alone, and the buffer index taken as a whole byte.
extern "C" void __cdecl DrawLayers_Reset(void) {
    auto* layer = reinterpret_cast<unsigned char*>(DrawLayers) + Gfx_BufferIndex * 8u;
    for (unsigned i = 0; i < kLayers; ++i, layer += kLayerBytes) {
        for (unsigned char* head = layer; head != layer + 0x20; head += 0x10) {
            const std::uint32_t none = 0, self = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(head));
            std::memcpy(head, &none, 4);
            std::memcpy(head + 4, &self, 4);
        }
    }
}

// original 0x56F5F0. Orders the draw table by layer and then key - the high 16
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

void DrawLayers_Inject() {
    // 0x56F5B0..0x56F5E1 and 0x56F5F0..0x56F665: no calls, every jump
    // internal (disasm 2026-09-20).
    if (bof3::WantsShadow("draw_layers")) {
        SelfTestReset(reinterpret_cast<VoidFn>(
            bof3::CloneOriginal("DrawLayers_Reset", bof3::addr::DrawLayers_Reset, 0x32)));
        SelfTestSort(reinterpret_cast<VoidFn>(
            bof3::CloneOriginal("DrawTable_Sort", bof3::addr::DrawTable_Sort, 0x76)));
    }
    BOF3_INJECT(DrawLayers_Reset);
    BOF3_INJECT(DrawTable_Sort);
}
