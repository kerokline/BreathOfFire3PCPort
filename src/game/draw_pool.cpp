#include "game/draw_pool.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

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

using ReleaseCellFn = void (__cdecl*)(unsigned char*);

// The same for DrawItemPool_ReleaseCell, whose copy calls the copy of Release.
// The fuzz owns the pool, its top and all of DrawItems. The cell's index is
// zero one round in five; the four bits above the index mask are noise; the
// item's two owned words are zero or not, independently. Pool, top, cell and
// the whole item compared.
void SelfTestReleaseCell(ReleaseCellFn theirs) {
    constexpr unsigned kRounds = 6000;
    static unsigned short saved_pool[DrawItemPool_Free_count], input_pool[DrawItemPool_Free_count],
        their_pool[DrawItemPool_Free_count];
    static unsigned char saved_items[DrawItems_count];
    std::memcpy(saved_pool, DrawItemPool_Free, sizeof saved_pool);
    std::memcpy(saved_items, DrawItems, sizeof saved_items);
    const unsigned short saved_top = DrawItemPool_Top;

    unsigned bad = 0, nothing = 0, released[4] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        for (auto& w : input_pool) w = static_cast<unsigned short>(Rng());
        const unsigned short top = static_cast<unsigned short>(Rng() % 8 == 0 ? Rng() % 4 : Rng() & kMask);
        const unsigned index = Rng() % 5 == 0 ? 0 : 1 + Rng() % kMask;
        unsigned char cell_in[4], item_in[0x90];
        for (auto& b : cell_in) b = static_cast<unsigned char>(Rng());
        for (auto& b : item_in) b = static_cast<unsigned char>(Rng());
        const std::uint16_t word = static_cast<std::uint16_t>((Rng() & 0xF000u) | index);
        std::memcpy(cell_in + 2, &word, 2);
        unsigned owned = 0;
        for (const unsigned at : {0x7Eu, 0x8Eu}) {
            if (Rng() % 2) std::memset(item_in + at, 0, 2);
            else if (item_in[at] | item_in[at + 1]) ++owned;
        }

        unsigned char cell[2][4], item[2][0x90];
        unsigned short top_after[2];
        for (int pass = 0; pass < 2; ++pass) {
            std::memcpy(DrawItemPool_Free, input_pool, sizeof input_pool);
            DrawItemPool_Top = top;
            std::memcpy(DrawItems + index * 0x90, item_in, sizeof item_in);
            std::memcpy(cell[pass], cell_in, sizeof cell_in);
            if (pass) DrawItemPool_ReleaseCell(cell[pass]); else theirs(cell[pass]);
            top_after[pass] = DrawItemPool_Top;
            std::memcpy(item[pass], DrawItems + index * 0x90, sizeof item_in);
            if (pass == 0) std::memcpy(their_pool, DrawItemPool_Free, sizeof their_pool);
        }
        if (index == 0) ++nothing; else ++released[1 + owned];
        if ((top_after[0] != top_after[1] || std::memcmp(cell[0], cell[1], 4) != 0 ||
             std::memcmp(item[0], item[1], 0x90) != 0 ||
             std::memcmp(their_pool, DrawItemPool_Free, sizeof their_pool) != 0) && ++bad <= 8)
            bof3::Log("shadow      DrawItemPool_ReleaseCell self-test MISMATCH round %u: index 0x%X, top 0x%X: "
                      "top after 0x%X vs ours 0x%X", round, index, top, top_after[0], top_after[1]);
    }
    std::memcpy(DrawItemPool_Free, saved_pool, sizeof saved_pool);
    std::memcpy(DrawItems, saved_items, sizeof saved_items);
    DrawItemPool_Top = saved_top;
    bof3::Log("shadow      DrawItemPool_ReleaseCell self-test: %u rounds (%u with nothing to release, %u / %u / %u "
              "releasing one, two, three indices), %u MISMATCHES; pool, top, cell and item compared",
              kRounds, nothing, released[1], released[2], released[3], bad);
    if (bad) bof3::Fatal("DrawItemPool_ReleaseCell differs from the original in %u of %u self-test rounds", bad, kRounds);
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

// original 0x56FC00. Gives back everything a cell of the view holds: its own
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
    unsigned char* const item = DrawItems + index * 0x90;
    for (const unsigned at : {0x7Eu, 0x8Eu}) {
        std::uint16_t owned;
        std::memcpy(&owned, item + at, sizeof owned);
        if (owned == 0) continue;
        DrawItemPool_Release(owned);
        std::memset(item + at, 0, 2);
    }
}

void DrawPool_Inject() {
    // 0x56FBD0..0x56FBFC: one jump, internal. 0x56FC70..0x56FC95: none. Neither
    // calls anything. 0x56FC00..0x56FC6F: every jump internal, and three
    // calls, all to 0x56FC70, which the copy makes to the copy (disasm
    // 2026-09-20).
    if (bof3::WantsShadow("draw_pool")) {
        const auto their_alloc = reinterpret_cast<AllocFn>(
            bof3::CloneOriginal("DrawItemPool_Alloc", bof3::addr::DrawItemPool_Alloc, 0x2D));
        const auto their_release = reinterpret_cast<ReleaseFn>(
            bof3::CloneOriginal("DrawItemPool_Release", bof3::addr::DrawItemPool_Release, 0x26));
        const void* const release_copy = reinterpret_cast<const void*>(their_release);
        const bof3::CloneCall calls[] = {{0x13, release_copy}, {0x39, release_copy}, {0x5C, release_copy}};
        const auto their_release_cell = reinterpret_cast<ReleaseCellFn>(bof3::CloneOriginal(
            "DrawItemPool_ReleaseCell", bof3::addr::DrawItemPool_ReleaseCell, 0x70, calls, 3));
        SelfTest(their_alloc, their_release);
        SelfTestReleaseCell(their_release_cell);
    }
    BOF3_INJECT(DrawItemPool_Alloc);
    BOF3_INJECT(DrawItemPool_Release);
    BOF3_INJECT(DrawItemPool_ReleaseCell);
}
