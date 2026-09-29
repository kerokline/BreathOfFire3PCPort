#include "game/draw_pool.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/draw_pool_room.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace draw_pool {
unsigned char* g_items = DrawItems;
unsigned short* g_free = DrawItemPool_Free;
unsigned g_count = DrawItemPool_Free_count;
}  // namespace draw_pool

namespace {

static_assert(DrawItemPool_Free_count == 1024);
constexpr unsigned kMask = DrawItemPool_Free_count - 1;   // the original's ring, for the self-test's edges

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
    const unsigned short index = draw_pool::Free()[top];
    DrawItemPool_Top = static_cast<unsigned short>((top + 1) & draw_pool::Mask());
    return index;
}

// original 0x56FC70. Gives an index back.
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
    unsigned char* const item = draw_pool::Items() + index * 0x90;
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

// DIVERGENCE DIV-0062: the pool doubled. The original's 1,024 items (index
// 0 never handed out) were enough for its [-50, 370] terrain cull; the wide
// view's cull (DIV-0041) keeps half as many cells again, and a cutscene pan
// over the Yraall coast ran the pool dry at frame 523 of the owner's route
// (200 cells a second refused an item and drawn as nothing - their walls
// showing through as blue parallelograms), some for good. Measured
// 2026-09-27: the narrow view peaks at 855 of 1,023 in the same scene, the
// wide one wraps the counter. Now 2,048 items and a 2,048-word free queue in
// the dll; the index is 12 bits in the cell word, so nothing else changes
// shape. The thirteen sites in Capcom's code that name the item array as an
// immediate (a raw scan of .text, each confirmed by disassembly) are re-aimed,
// and the one bound AreaMap_FrameAreaBD compares its bump index with
// (`cmp word [Top], 0x400` at 0x510878) is raised to 0x800. Runs last, after
// every self-test.
namespace {
unsigned char* g_low_items = nullptr;
}  // namespace

// The first thing InjectAll does, before any clone or fuzz array is placed:
// the item array's room below 16 MB is easiest to find then (by the end of
// the injects the range is cut up by our own allocations - measured
// 2026-09-27: the largest gap left was 192 KB).
void DrawPool_Reserve() {
    using namespace draw_pool_room;
    unsigned char* items = nullptr;
    // The launcher's block, stamped, at one of the candidates (launcher.cpp).
    for (std::uint32_t at : kCandidates) {
        MEMORY_BASIC_INFORMATION mbi;
        auto* p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(at));
        if (VirtualQuery(p, &mbi, sizeof mbi) == 0) continue;
        if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE || mbi.BaseAddress != p || mbi.RegionSize < kBytes) continue;
        if (std::memcmp(p, kStamp, sizeof kStamp) != 0) continue;
        items = p;
        break;
    }
    // Started without the launcher: the first free region of the size below
    // 16 MB, if the address space still has one.
    for (std::uint32_t at = 0x10000; at + kBytes <= 0x1000000 && !items;) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(at)), &mbi, sizeof mbi) == 0) break;
        const auto region = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(mbi.BaseAddress));
        const std::uint32_t end = region + static_cast<std::uint32_t>(mbi.RegionSize);
        if (mbi.State == MEM_FREE && end - at >= kBytes) {
            const std::uint32_t want = (at + 0xFFFF) & ~0xFFFFu;   // allocation granularity
            if (want + kBytes <= end)
                items = static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(want)), kBytes,
                                                                 MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
        at = end;
    }
    if (items) std::memset(items, 0, kBytes);
    g_low_items = items;
    if (items)
        bof3::Log("DIV-0062    draw-item pool's room at 0x%08X (%u KB, below 16 MB)", (unsigned)reinterpret_cast<std::uintptr_t>(items),
                  kBytes / 1024);
}

void DrawPool_Grow() {
    using draw_pool_room::kBytes;
    using draw_pool_room::kCount;
    // The items are linked into the ordering table by 24-bit addresses (the
    // PlayStation's tag; d3d_list.cpp masks a link to 24 bits), so the array
    // must sit below 16 MB, as everything of Capcom's does. DrawPool_Reserve
    // found the room (the launcher's stamped block, or a free region below
    // 16 MB); none leaves the original's pool, said loudly, rather than a link
    // that truncates (the first build of this crashed in Gfx_DrawOTag).
    unsigned char* const items = g_low_items;
    if (!items) {
        bof3::Log("DIV-0062    no free %u KB below 16 MB for the draw-item pool: the original's 1,024 items stay", kBytes / 1024);
        return;
    }
    // The switch is all or nothing. kSites below re-aims only Capcom code we
    // do not own; the functions here are ours, and each one's original names
    // the item array (0x905E80) or the free queue (0x7E09E0, ring 0x3FF) as an
    // immediate - they are every owned function whose reimplementation reads
    // draw_pool:: (grep, 2026-09-29). One of them left as Capcom's code by
    // BOF3X_ORIGINAL would work the old arrays while the rest of the game works
    // the new ones: two cells handed one item, half the array never primed.
    // So any such one keeps the original's pool for everybody, said loudly.
    static const std::uint32_t kOwnedUsers[] = {
        bof3::addr::DrawItemPool_Alloc,  bof3::addr::DrawItemPool_Release, bof3::addr::DrawItemPool_ReleaseCell,
        bof3::addr::Field_ViewReset,     bof3::addr::Weretiger_ResetMapView, bof3::addr::MapView_Build,
        bof3::addr::MapView_CellTextures, bof3::addr::MapView_ItemHalfAt,  bof3::addr::AreaMap_ApplyPatch,
        bof3::addr::MapCell_FlatOverlay, bof3::addr::Sprite_DrawPass,      bof3::addr::Area40_DrawGrid,
    };
    for (const std::uint32_t user : kOwnedUsers) {
        if (bof3::IsOwned(user) && !bof3::IsEnabled(user)) {
            bof3::Log("DIV-0062    0x%06X is the original's (BOF3X_ORIGINAL) and names the draw-item pool: "
                      "the original's 1,024 items stay",
                      (unsigned)user);
            return;
        }
    }
    static unsigned short free_list[kCount];
    for (unsigned i = 0; i < kCount; ++i) free_list[i] = static_cast<unsigned short>(i);
    const auto base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(items));
    struct Site {
        std::uint32_t at;       // where the 4-byte immediate sits
        std::uint32_t was;      // the original's value there
        std::uint32_t offset;   // into the item: 0, 0x7E or 0x8E
    };
    static const Site kSites[] = {
        {0x486EBD, 0x905E80, 0},    {0x509930, 0x905E80, 0},    {0x5109FC, 0x905E80, 0},
        {0x51242A, 0x905E80, 0},    {0x512607, 0x905E80, 0},    {0x5139C2, 0x905E80, 0},
        {0x513BAC, 0x905E80, 0},    {0x513C41, 0x905E80, 0},    {0x5098F4, 0x905EFE, 0x7E},
        {0x5098FB, 0x905EFE, 0x7E}, {0x513B9B, 0x905EFE, 0x7E}, {0x509909, 0x905F0E, 0x8E},
        {0x509910, 0x905F0E, 0x8E},
    };
    for (const Site& s : kSites) {
        std::uint8_t was[4], is[4];
        const std::uint32_t now = base + s.offset;
        std::memcpy(was, &s.was, 4);
        std::memcpy(is, &now, 4);
        bof3::PatchBytes("DrawPool", s.at, was, is, 4);
    }
    static const std::uint8_t was16[2] = {0x00, 0x04}, is16[2] = {0x00, 0x08};
    bof3::PatchBytes("DrawPool", 0x51087F, was16, is16, 2);
    // BOF3X_ORIGINAL=DrawPool: PatchBytes left every site alone (and said so);
    // read the first back, and leave the arrays the original's too.
    std::uint32_t first;
    std::memcpy(&first, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(kSites[0].at)), 4);
    if (first == kSites[0].was) return;
    draw_pool::g_items = items;
    draw_pool::g_free = free_list;
    draw_pool::g_count = kCount;
    DrawItemPool_Top = 1;
    bof3::Log("DIV-0062    draw-item pool %u items (was %u), the item array at 0x%08X", kCount,
              (unsigned)DrawItemPool_Free_count, (unsigned)base);
}
