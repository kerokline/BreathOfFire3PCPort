#include "game/gfx_texcache.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// What the cache holds is two COM interfaces per entry, and all this file
// ever does with one is Release it: vtable slot 2, stdcall.
struct ComObject {
    struct Vtbl {
        void* query_interface;
        void* add_ref;
        unsigned long (__stdcall* release)(ComObject*);
    }* vtbl;
};

// One of 1,024 entries at 0x6C3F40: 32 texture pages of 64 x 256 (16 across,
// 2 down, as PSX VRAM), 32 entries a page, filled from slot 0 with no holes.
// Only the fields this file touches are named.
struct TexCacheEntry {
    std::uint8_t state;        // 0 free - and the end of the page's list; 1 built; 2 stale
    std::uint8_t mode;         // +1: PSX colour mode, (tpage >> 7) & 3: 0 4-bit, 1 8-bit, 2 15-bit.
                               //     Non-zero is wider than a page in VRAM, which is why such
                               //     an entry is also dropped when the NEXT page changes.
    std::uint16_t clut;        // +2: PSX CLUT id the texture was built with
    std::uint32_t generation;  // +4: that CLUT row's generation at the time (Gfx_ClutRows)
    std::uint32_t key[2];      // +8: Gfx_TexCacheKey at the time
    ComObject* a;              // +0x10
    ComObject* b;              // +0x14
};
static_assert(sizeof(TexCacheEntry) == 0x18);

constexpr int kPerPage = 32;
constexpr int kEntries = 32 * kPerPage;
static_assert(sizeof(TexCacheEntry) * kEntries == Gfx_TexCache_count);

struct Rect { int left, top, right, bottom; };

// USER32 IntersectRect, which the original calls: right and bottom are
// exclusive, and an empty rect intersects nothing.
bool Intersects(const Rect& p, const Rect& q) {
    const int l = p.left > q.left ? p.left : q.left;
    const int r = p.right < q.right ? p.right : q.right;
    const int t = p.top > q.top ? p.top : q.top;
    const int b = p.bottom < q.bottom ? p.bottom : q.bottom;
    return l < r && t < b;
}

struct Tally { unsigned dropped = 0, closed_up = 0, staled = 0; };

// The whole of the original's effect on the table, with the one thing it does
// outside the table - Release - handed to the caller, so that the shadow check
// runs exactly the code the game runs.
//
// As the original has it: both rects are built INCLUSIVE (x + w - 1, page x +
// 0x3F) and then given to an exclusive intersection. So the last column and
// row of every page, and of the rect, do not count: a rect one cell wide or
// high invalidates nothing, and one that reaches only a page's column 63 or
// row 255 leaves that page alone.
template <class Release>
Tally Invalidate(TexCacheEntry* table, const short* rect, int mode, Release release) {
    Tally tally;
    const Rect dirty{rect[0], rect[1], rect[0] + rect[2] - 1, rect[1] + rect[3] - 1};

    int page = 0;
    for (int py = 0; py < 0x200; py += 0x100) {
        for (int px = 0; px < 0x400; px += 0x40, ++page) {
            if (!Intersects(Rect{px, py, px + 0x3F, py + 0xFF}, dirty)) continue;

            // The page's own entries. Mode 0 stops at the first free slot;
            // mode non-zero visits all 32.
            TexCacheEntry* own = table + page * kPerPage;
            for (int i = 0; i < kPerPage; ++i) {
                TexCacheEntry& e = own[i];
                if (mode != 0) {
                    if (e.state == 1) { e.state = 2; ++tally.staled; }
                    continue;
                }
                if (e.state == 0) break;
                if (e.b) release(e.b);
                if (e.a) release(e.a);
                std::memset(&e, 0, sizeof e);
                ++tally.dropped;
            }

            // Then the page to the left, for its 8- and 15-bit textures, which
            // reach into this one - not for the first page of a row. Here a dropped entry is
            // closed up rather than left as a hole, because a hole would end
            // that page's list for the loop above.
            if ((page & 0xF) == 0) continue;
            TexCacheEntry* left = own - kPerPage;
            for (int i = 0; i < kPerPage;) {
                TexCacheEntry& e = left[i];
                if (mode != 0) {
                    if (e.state == 1 && e.mode != 0) { e.state = 2; ++tally.staled; }
                    ++i;
                    continue;
                }
                if (e.state == 0) break;
                if (e.mode == 0) { ++i; continue; }
                if (e.b) release(e.b);
                if (e.a) release(e.a);
                std::memmove(&e, &e + 1, sizeof e * static_cast<unsigned>(kPerPage - 1 - i));
                std::memset(&left[kPerPage - 1], 0, sizeof e);
                ++tally.closed_up;
                // and look at slot i again: it now holds the next entry
            }
        }
    }
    return tally;
}

TexCacheEntry* Table() { return reinterpret_cast<TexCacheEntry*>(Gfx_TexCache); }

// --- BOF3X_SHADOW=Gfx_InvalidateTextures --------------------------------------
// Every call: plan our result on a copy of the table, releasing nothing; let a
// byte-copy of the original do the real work on the real table; compare. The
// original's Releases are the ones that happen, so the game runs as original.
// What this cannot see is the ORDER of Releases - only which entries went.

using Original = void (__cdecl*)(const short*, int);
Original g_original_clone = nullptr;
TexCacheEntry g_plan[kEntries];

struct {
    unsigned calls, mode0, mode1, mismatches;
    unsigned dropped, closed_up, staled, would_release;
} g_shadow;

void ShadowReport(const char* why) {
    bof3::Log("shadow      Gfx_InvalidateTextures %s: %u calls (%u mode 0, %u mode non-zero), "
              "%u MISMATCHES; ours dropped %u, closed up %u, staled %u, would release %u",
              why, g_shadow.calls, g_shadow.mode0, g_shadow.mode1, g_shadow.mismatches,
              g_shadow.dropped, g_shadow.closed_up, g_shadow.staled, g_shadow.would_release);
}

void ShadowCall(const short* rect, int mode) {
    const short r[4] = {rect[0], rect[1], rect[2], rect[3]};
    std::memcpy(g_plan, Table(), sizeof g_plan);
    unsigned releases = 0;
    const Tally t = Invalidate(g_plan, r, mode, [&](ComObject*) { ++releases; });

    g_original_clone(rect, mode);

    ++g_shadow.calls;
    ++(mode == 0 ? g_shadow.mode0 : g_shadow.mode1);
    g_shadow.dropped += t.dropped;
    g_shadow.closed_up += t.closed_up;
    g_shadow.staled += t.staled;
    g_shadow.would_release += releases;
    if (std::memcmp(g_plan, Table(), sizeof g_plan) != 0) {
        if (++g_shadow.mismatches <= 8) {
            int first = 0;
            while (std::memcmp(&g_plan[first], &Table()[first], sizeof(TexCacheEntry)) == 0) ++first;
            bof3::Log("shadow      Gfx_InvalidateTextures MISMATCH call %u: rect {%d,%d,%d,%d} mode %d, "
                      "first differing entry %d (page %d slot %d): ours state %u, original state %u",
                      g_shadow.calls, r[0], r[1], r[2], r[3], mode, first, first / kPerPage,
                      first % kPerPage, g_plan[first].state, Table()[first].state);
        }
    }
    if (g_shadow.calls == 1 || g_shadow.calls % 256 == 0) ShadowReport("so far");
}

// --- the same switch, once at start-up: a differential fuzz ---------------------
// The attract sequence only ever calls with mode 0 and never makes the second
// pass close anything up (shadow run 2026-09-19), so those paths are driven
// here instead. The DLL is injected before the game has run, when the table is
// still all zero: fill it with random entries whose COM pointers are fakes that
// log their Release, run the original's clone, put the same input back, run
// ours, and compare the table AND the order of Releases. The table is left
// zero again, as found.

struct FakeObject {
    ComObject::Vtbl* vtbl;
    unsigned id;
};
unsigned g_release_log[2 * kEntries];
unsigned g_release_n;

unsigned long __stdcall FakeRelease(ComObject* o) {
    if (g_release_n < 2 * kEntries) g_release_log[g_release_n] = reinterpret_cast<FakeObject*>(o)->id;
    ++g_release_n;
    return 0;
}

ComObject::Vtbl g_fake_vtbl{nullptr, nullptr, &FakeRelease};
FakeObject g_fakes[2 * kEntries];
TexCacheEntry g_input[kEntries], g_theirs[kEntries];
unsigned g_their_log[2 * kEntries];

std::uint32_t g_rng = 0x2545F491u;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}
int RngIn(int lo, int hi) { return lo + static_cast<int>(Rng() % static_cast<unsigned>(hi - lo + 1)); }

void SelfTest() {
    constexpr unsigned kRounds = 4000;
    static const std::uint8_t zero[sizeof g_input] = {};
    if (std::memcmp(Table(), zero, sizeof zero) != 0) {
        bof3::Log("shadow      Gfx_InvalidateTextures self-test SKIPPED: the table is already in use");
        return;
    }
    // The clone calls IntersectRect through the game's import slot.
    if (*reinterpret_cast<void**>(static_cast<std::uintptr_t>(0x5C4168)) == nullptr) {
        bof3::Log("shadow      Gfx_InvalidateTextures self-test SKIPPED: import slot 0x5C4168 not bound yet");
        return;
    }
    for (unsigned i = 0; i < 2 * kEntries; ++i) g_fakes[i] = {&g_fake_vtbl, i};

    Tally total;
    unsigned mode0 = 0, mode1 = 0, bad = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        std::memset(g_input, 0, sizeof g_input);
        const bool holes = round % 10 == 9;   // not a state the game makes; defined all the same
        for (int page = 0; page < 32; ++page) {
            const int used = Rng() % 4 == 0 ? kPerPage : RngIn(0, 12);
            for (int i = 0; i < used; ++i) {
                TexCacheEntry& e = g_input[page * kPerPage + i];
                e.clut = static_cast<std::uint16_t>(Rng());
                e.generation = Rng();
                e.key[0] = Rng();
                e.key[1] = Rng();
                e.state = holes && Rng() % 6 == 0 ? 0 : static_cast<std::uint8_t>(RngIn(1, 2));
                e.mode = Rng() % 3 == 0 ? static_cast<std::uint8_t>(RngIn(1, 255)) : 0;
                const unsigned slot = static_cast<unsigned>(page * kPerPage + i);
                e.a = Rng() % 5 ? reinterpret_cast<ComObject*>(&g_fakes[2 * slot]) : nullptr;
                e.b = Rng() % 5 ? reinterpret_cast<ComObject*>(&g_fakes[2 * slot + 1]) : nullptr;
            }
        }
        // Rects of every size, a third of them snapped to within a cell of a
        // page edge, where the inclusive/exclusive quirk lives.
        short rect[4];
        rect[0] = static_cast<short>(RngIn(-16, 0x410));
        rect[1] = static_cast<short>(RngIn(-16, 0x210));
        rect[2] = static_cast<short>(Rng() % 4 == 0 ? RngIn(0, 3) : RngIn(0, 0x180));
        rect[3] = static_cast<short>(Rng() % 4 == 0 ? RngIn(0, 3) : RngIn(0, 0x180));
        if (Rng() % 3 == 0) rect[0] = static_cast<short>(RngIn(0, 16) * 0x40 + RngIn(-2, 1));
        if (Rng() % 3 == 0) rect[1] = static_cast<short>(RngIn(0, 2) * 0x100 + RngIn(-2, 1));
        const int mode = Rng() % 2 ? RngIn(1, 3) : 0;
        ++(mode == 0 ? mode0 : mode1);

        std::memcpy(Table(), g_input, sizeof g_input);
        g_release_n = 0;
        g_original_clone(rect, mode);
        std::memcpy(g_theirs, Table(), sizeof g_theirs);
        const unsigned their_n = g_release_n;
        std::memcpy(g_their_log, g_release_log, sizeof g_their_log);

        std::memcpy(Table(), g_input, sizeof g_input);
        g_release_n = 0;
        const Tally t = Invalidate(Table(), rect, mode, [](ComObject* o) { o->vtbl->release(o); });
        total.dropped += t.dropped; total.closed_up += t.closed_up; total.staled += t.staled;

        const bool same_table = std::memcmp(g_theirs, Table(), sizeof g_theirs) == 0;
        const bool same_log = their_n == g_release_n &&
            std::memcmp(g_their_log, g_release_log, sizeof(unsigned) * (their_n < 2 * kEntries ? their_n : 2 * kEntries)) == 0;
        if ((!same_table || !same_log) && ++bad <= 8)
            bof3::Log("shadow      Gfx_InvalidateTextures self-test MISMATCH round %u: rect {%d,%d,%d,%d} mode %d, "
                      "table %s, releases %u vs ours %u (%s)", round, rect[0], rect[1], rect[2], rect[3], mode,
                      same_table ? "same" : "DIFFERS", their_n, g_release_n, same_log ? "same order" : "DIFFER");
    }
    std::memset(Table(), 0, sizeof g_input);
    bof3::Log("shadow      Gfx_InvalidateTextures self-test: %u rounds (%u mode 0, %u mode non-zero), %u MISMATCHES; "
              "dropped %u, closed up %u, staled %u; tables and Release order compared",
              kRounds, mode0, mode1, bad, total.dropped, total.closed_up, total.staled);
    if (bad) bof3::Fatal("Gfx_InvalidateTextures differs from the original in %u of %u self-test rounds", bad, kRounds);
}

// The fuzz for Gfx_TexCacheFind, same switch. Few distinct CLUTs, keys and
// generations, so that hits, misses and stale hits all happen.
using FindFn = int (__cdecl*)(int, int, int);
TexCacheEntry g_find_in[kEntries], g_find_theirs[kEntries];

void SelfTestFind(FindFn theirs) {
    constexpr unsigned kRounds = 6000;
    struct Row { std::uint32_t generation; void* pixels; };
    auto* rows = reinterpret_cast<Row*>(Gfx_ClutRows);
    unsigned bad = 0, hits = 0, misses = 0, staled = 0, direct = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        if (round % 50 == 0) {
            std::memset(g_find_in, 0, sizeof g_find_in);
            for (int page = 0; page < 32; ++page)
                for (int i = 0, used = Rng() % 5 == 0 ? kPerPage : RngIn(0, 10); i < used; ++i) {
                    TexCacheEntry& e = g_find_in[page * kPerPage + i];
                    e.state = static_cast<std::uint8_t>(RngIn(1, 2));
                    e.mode = static_cast<std::uint8_t>(RngIn(0, 2));
                    e.clut = static_cast<std::uint16_t>((RngIn(480, 483) << 6) | RngIn(0, 3));
                    e.generation = static_cast<std::uint32_t>(RngIn(0, 1));
                    e.key[0] = static_cast<std::uint32_t>(RngIn(0, 2));
                    e.key[1] = static_cast<std::uint32_t>(RngIn(0, 1));
                }
            for (int y = 480; y < 484; ++y) rows[y].generation = static_cast<std::uint32_t>(RngIn(0, 1));
        }
        Gfx_TexCacheKey[0] = static_cast<unsigned long>(RngIn(0, 2));
        Gfx_TexCacheKey[1] = static_cast<unsigned long>(RngIn(0, 1));
        const int page = RngIn(0, 31), mode = Rng() % 8 == 0 ? RngIn(3, 255) : RngIn(0, 2);
        // one in ten with high bits set, which the original compares too
        const int clut = ((RngIn(480, 483) << 6) | RngIn(0, 3)) | (Rng() % 10 == 0 ? 0x10000 : 0);
        int result[2];
        for (int pass = 0; pass < 2; ++pass) {
            std::memcpy(Table(), g_find_in, sizeof g_find_in);
            result[pass] = (pass ? &Gfx_TexCacheFind : theirs)(page, clut, mode);
            if (pass == 0) std::memcpy(g_find_theirs, Table(), sizeof g_find_theirs);
        }
        if (result[1] == kPerPage) ++misses; else ++hits;
        if (mode & 2) ++direct;
        if (std::memcmp(g_find_in, Table(), sizeof g_find_in) != 0) ++staled;
        if ((result[0] != result[1] || std::memcmp(g_find_theirs, Table(), sizeof g_find_theirs) != 0) && ++bad <= 8)
            bof3::Log("shadow      Gfx_TexCacheFind self-test MISMATCH round %u: page %d clut 0x%X mode %d: %d vs ours %d",
                      round, page, clut, mode, result[0], result[1]);
    }
    std::memset(Table(), 0, sizeof g_find_in);
    for (int y = 480; y < 484; ++y) rows[y].generation = 0;
    Gfx_TexCacheKey[0] = Gfx_TexCacheKey[1] = 0;
    bof3::Log("shadow      Gfx_TexCacheFind self-test: %u rounds (%u hits, %u misses, %u in 15-bit mode, %u that marked "
              "an entry stale), %u MISMATCHES; results and tables compared", kRounds, hits, misses, direct, staled, bad);
    if (bad) bof3::Fatal("Gfx_TexCacheFind differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

// original 0x5A0830. The slot, 0..31, of the page's entry for this texture, or
// 32 if it has none. A texture is identified by Gfx_TexCacheKey - eight bytes
// the draw sets first - and, unless it is 15-bit, by its CLUT id.
//
// It is also where a palette change reaches a texture: a hit whose CLUT row
// has been converted again since the entry was built (Gfx_ClutRows generation)
// is marked stale on the way out. Not for 15-bit textures, which have no CLUT.
//
// As the original has it: only bit 1 of mode is looked at, so 3 is 15-bit too;
// 4-bit and 8-bit entries are not told apart; the CLUT id is compared as 32
// bits against a 16-bit field, so an id with high bits set never hits; and
// clut >> 6 indexes Gfx_ClutRows unchecked, up to row 1,023 of 512.
extern "C" int __cdecl Gfx_TexCacheFind(int page, int clut, int mode) {
    TexCacheEntry* own = Table() + page * kPerPage;
    for (int i = 0; i < kPerPage; ++i) {
        TexCacheEntry& e = own[i];
        if (e.state == 0) return kPerPage;
        if (std::memcmp(e.key, Gfx_TexCacheKey, sizeof e.key) != 0) continue;
        if (mode & 2) {
            if (e.mode == 2) return i;
            continue;
        }
        if (static_cast<int>(e.clut) != clut) continue;
        struct Row { std::uint32_t generation; void* pixels; };
        if (e.generation != reinterpret_cast<const Row*>(Gfx_ClutRows)[clut >> 6].generation) e.state = 2;
        return i;
    }
    return kPerPage;
}

// original 0x59E700. Throws away (mode 0) or marks stale (mode non-zero) every
// cached texture built from the VRAM cells under rect: four s16, x y w h.
// Callers: Gfx_LoadImage and 0x59E6EA (the end of a row-by-row fill of the
// shadow) with mode 0; 0x59EA5C, after 0x5AA5D6 has been given the shadow and a
// rect, with mode 1.
extern "C" void __cdecl Gfx_InvalidateTextures(const short* rect, int mode) {
    if (g_original_clone) {
        ShadowCall(rect, mode);
        return;
    }
    Invalidate(Table(), rect, mode, [](ComObject* o) { o->vtbl->release(o); });
}

void GfxTexCache_Inject() {
    // 0x59E700..0x59E925: every jump stays inside, and its only calls are
    // through the import slot 0x5C4168 and through vtables (disasm 2026-09-19).
    if (bof3::WantsShadow("Gfx_InvalidateTextures"))
        g_original_clone = reinterpret_cast<Original>(bof3::CloneOriginal(
            "Gfx_InvalidateTextures", bof3::addr::Gfx_InvalidateTextures, 0x226));
    if (g_original_clone) SelfTest();
    BOF3_INJECT(Gfx_InvalidateTextures);

    // 0x5A0830..0x5A0901: every jump stays inside, and it calls nothing
    // (disasm 2026-09-19).
    if (bof3::WantsShadow("Gfx_TexCacheFind"))
        SelfTestFind(reinterpret_cast<FindFn>(
            bof3::CloneOriginal("Gfx_TexCacheFind", bof3::addr::Gfx_TexCacheFind, 0xD2)));
    BOF3_INJECT(Gfx_TexCacheFind);
}
