#include "game/draw_pool.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr unsigned kMask = DrawItemPool_Free_count - 1;
static_assert(kMask == 0x3FF);

// --- BOF3X_SHADOW=draw_pool: a differential fuzz, once at start-up -------------
// Neither function calls anything and every jump stays inside, so byte-copies
// of them run in place against the same globals. Random pool, random top -
// one round in four at an edge (0, 1, 0x3FF), where the wrap and the "none
// left" reading live - then theirs, the same state again, ours; array, top
// and return value compared. The state is put back as found.

std::uint32_t g_rng = 0x6C8E9CF5u;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}

using AllocFn = unsigned short (__cdecl*)();
using ReleaseFn = unsigned (__cdecl*)(unsigned short);

void SelfTest(AllocFn their_alloc, ReleaseFn their_release) {
    constexpr unsigned kRounds = 6000;
    static unsigned short saved[DrawItemPool_Free_count], input[DrawItemPool_Free_count],
        their_out[DrawItemPool_Free_count];
    std::memcpy(saved, DrawItemPool_Free, sizeof saved);
    const unsigned short saved_top = DrawItemPool_Top;

    unsigned bad = 0, allocs = 0, releases = 0, empty = 0, edges = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (auto& w : input) w = static_cast<unsigned short>(Rng());
        static const unsigned short kEdge[] = {0, 1, 0x3FF, 0x3FE};
        const bool edge = Rng() % 4 == 0;
        const unsigned short top = edge ? kEdge[Rng() % 4] : static_cast<unsigned short>(Rng() & kMask);
        edges += edge;
        const bool alloc = Rng() % 2;
        const unsigned short index = static_cast<unsigned short>(Rng());

        unsigned result[2];
        unsigned short top_after[2];
        for (int pass = 0; pass < 2; ++pass) {
            std::memcpy(DrawItemPool_Free, input, sizeof input);
            DrawItemPool_Top = top;
            if (alloc) result[pass] = pass ? DrawItemPool_Alloc() : their_alloc();
            else result[pass] = pass ? DrawItemPool_Release(index) : their_release(index);
            top_after[pass] = DrawItemPool_Top;
            if (pass == 0) std::memcpy(their_out, DrawItemPool_Free, sizeof their_out);
        }
        ++(alloc ? allocs : releases);
        if (alloc && top == 0) ++empty;
        if ((result[0] != result[1] || top_after[0] != top_after[1] ||
             std::memcmp(their_out, DrawItemPool_Free, sizeof their_out) != 0) && ++bad <= 8)
            bof3::Log("shadow      draw_pool self-test MISMATCH round %u: %s, top 0x%X: result 0x%X vs ours 0x%X, "
                      "top after 0x%X vs ours 0x%X", round, alloc ? "alloc" : "release", top,
                      result[0], result[1], top_after[0], top_after[1]);
    }
    std::memcpy(DrawItemPool_Free, saved, sizeof saved);
    DrawItemPool_Top = saved_top;
    bof3::Log("shadow      draw_pool self-test: %u rounds (%u allocs of which %u from an empty pool, %u releases, "
              "%u at an edge), %u MISMATCHES; array, top and result compared",
              kRounds, allocs, empty, releases, edges, bad);
    if (bad) bof3::Fatal("the draw-item pool differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

// original 0x56FBD0. The next free draw-item index, or 0 for none: index 0 is
// never in play, the pool starts with top = 1.
//
// As the original has it: top wraps from 0x3FF to 0, and 0 reads as "none
// left" from then until something is released.
extern "C" unsigned short __cdecl DrawItemPool_Alloc(void) {
    const unsigned short top = DrawItemPool_Top;
    if (top == 0) return 0;
    const unsigned short index = DrawItemPool_Free[top];
    DrawItemPool_Top = static_cast<unsigned short>((top + 1) & kMask);
    return index;
}

// original 0x56FC70. Gives an index back.
//
// As the original has it: no check that the pool is not already full - with
// top == 1 this stores at entry 0, with top == 0 at entry 0x3FF - and the new
// top is what is left in eax. No caller read so far uses that; it is returned
// because it is free to.
extern "C" unsigned __cdecl DrawItemPool_Release(unsigned short index) {
    const unsigned top = (DrawItemPool_Top - 1u) & kMask;
    DrawItemPool_Top = static_cast<unsigned short>(top);
    DrawItemPool_Free[top] = index;
    return top;
}

void DrawPool_Inject() {
    // 0x56FBD0..0x56FBFC: one jump, internal. 0x56FC70..0x56FC95: none. Neither
    // calls anything (disasm 2026-09-20).
    if (bof3::WantsShadow("draw_pool"))
        SelfTest(reinterpret_cast<AllocFn>(
                     bof3::CloneOriginal("DrawItemPool_Alloc", bof3::addr::DrawItemPool_Alloc, 0x2D)),
                 reinterpret_cast<ReleaseFn>(
                     bof3::CloneOriginal("DrawItemPool_Release", bof3::addr::DrawItemPool_Release, 0x26)));
    BOF3_INJECT(DrawItemPool_Alloc);
    BOF3_INJECT(DrawItemPool_Release);
}
