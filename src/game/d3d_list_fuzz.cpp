// Part of BOF3X_SHADOW=d3d_draw: a differential fuzz of the draw Gfx_DrawOTag
// 0x59EE50 against a byte-copy of Capcom's, once at start-up. docs/d3d-draw.md
// section 5.
//
// The copy is the body and both jump tables (0x6C8 bytes); the tables' entries
// and the two `jmp [ecx*4 + table]` operands are moved into the copy, and every
// call out - the 42 handler sites, Gfx_MoveImage twice, D3d_SetAlphaModulate and
// the tail jump to D3d_AfterDraw - is re-aimed at a recorder that names the
// table entry it stands for. Ours is put on the same recorders through
// d3d_list::g; the device is the harness's fake. Per round: a random ordering
// table in memory below 16 MB (links are 24-bit), codes seeded so that every
// entry of both byte tables is reached, links with and without a top byte, the
// render flag, the draw-enable byte and the after-draw word random. The
// recorders sometimes cut or re-route the link of the node they were handed,
// set the after-draw word, flip the render flag or the draw-enable byte - the
// draw reads the first two after each handler, and must not re-read the others.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/d3d_fuzz.h"
#include "game/d3d_list_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace d3d_list {
namespace {

using d3d_fuzz::Next;
using d3d_fuzz::Record;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
U GetLong(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

// --- the table in memory ---------------------------------------------------

constexpr U kNodeBytes = 0x40;
constexpr U kMaxNodes = 16;   // node 0 is the head the draw is handed
constexpr U kKeyBytes = 0x40;
constexpr U kBufferBytes = kMaxNodes * kNodeBytes + kKeyBytes;
U g_base;          // below 0x1000000
U g_nodes;         // this round's count, head included

struct Region {
    U address, bytes;
};
Region g_regions[6];
constexpr U kStateBytes = kBufferBytes + 12 + 2 + 1 + 0xA00 + 0x1400;
struct State {
    unsigned char bytes[kStateBytes];
};
State g_saved, g_start, g_theirs_state, g_ours_state;

void Capture(State& s) {
    U at = 0;
    for (const Region& r : g_regions) {
        std::memcpy(s.bytes + at, At(r.address), r.bytes);
        at += r.bytes;
    }
}
void Restore(const State& s) {
    U at = 0;
    for (const Region& r : g_regions) {
        std::memcpy(At(r.address), s.bytes + at, r.bytes);
        at += r.bytes;
    }
}
bool SameState(const State& a, const State& b, U* where) {
    U at = 0;
    for (const Region& r : g_regions) {
        for (U i = 0; i < r.bytes; ++i)
            if (a.bytes[at + i] != b.bytes[at + i]) {
                *where = r.address + i;
                return false;
            }
        at += r.bytes;
    }
    return true;
}

// --- the recorders -----------------------------------------------------------

U g_round;

U Mix(U salt) {
    U h = (g_round * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu) ^ (d3d_fuzz::g_log->n * 0xC2B2AE35u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

// What a handler may do to the draw's state: its node's link cut (-1),
// re-routed forward, or given a top byte; the after-draw word set; the render
// flag or the draw-enable byte flipped.
void Disturb(U node, U salt) {
    const U h = Mix(salt);
    if (h % 3) return;
    switch ((h >> 4) % 6) {
    case 0: PutLong(node, 0xFFFFFFFFu); break;
    case 1:
    case 2: {
        const U index = (node - g_base) / kNodeBytes + 1 + (h >> 8) % 3;
        PutLong(node, index < g_nodes ? g_base + index * kNodeBytes : 0xFFFFFFFFu);
        if (index < g_nodes && (h >> 12) % 2) PutLong(node, GetLong(node) | ((h >> 16) & 0xFF000000u) | 0x01000000u);
        break;
    }
    case 3: At(kAfterDrawFlag)[(h >> 8) % 2] = static_cast<unsigned char>(h >> 16); break;
    case 4: At(kRenderFlags)[0] ^= 1; break;
    default: At(kDrawEnable)[0] ^= 1; break;
    }
}

template <int K> long __cdecl StubEntry(const unsigned char* prim) {
    Record(0x40 + K, Addr(prim) - g_base);
    Disturb(Addr(prim), 0x100 + K);
    return static_cast<long>(Mix(0x200 + K));
}
void __cdecl StubMove(short* rect, int x, int y) {
    Record(0x30, Addr(rect) - g_base, static_cast<U>(x), static_cast<U>(y));
    Disturb(Addr(rect) - 8, 0x300);
}
void __cdecl StubAlpha(unsigned on) { Record(0x31, on); }
void __cdecl StubAfter() { Record(0x32); }

template <int... K> Callees MakeStandIns(std::integer_sequence<int, K...>) {
    return Callees{{&StubEntry<K>...}, {&StubEntry<32 + K>...}, StubMove, StubAlpha, StubAfter};
}
const Callees kStandIns = MakeStandIns(std::make_integer_sequence<int, kHandlers>{});

// --- the copy ------------------------------------------------------------------

constexpr U kBody = 0x59EE50, kCloneBytes = 0x6C8;
// The original's call sites, from the disassembly (docs/d3d-draw.md section 1):
// the first table's case blocks in the order they lie (entries 15 and 16, and
// 17..19, are not in entry order), then the second table's.
struct Site {
    U offset;
    int entry;   // 0..20 first table, 32..52 second
    U target;
};
const Site kSites[] = {
    {0x6F, 0, 0x5A3A60},   {0x7D, 1, 0x5A3B60},   {0x8B, 2, 0x5A3D70},   {0x99, 3, 0x5A3E90},
    {0xA7, 4, 0x5A3FB0},   {0xB5, 5, 0x5A40C0},   {0xC3, 6, 0x5A41A0},   {0xD1, 7, 0x5A42E0},
    {0xDF, 8, kRetOnly},   {0xED, 9, kRetOnly},   {0xFB, 10, kRetOnly},  {0x109, 11, kRetOnly},
    {0x117, 12, kRetOnly}, {0x125, 13, kRetOnly}, {0x133, 14, 0x5A4400}, {0x141, 16, 0x5A4500},
    {0x14F, 15, 0x5A45B0}, {0x15D, 18, 0x5A46E0}, {0x16B, 19, 0x5A47F0}, {0x179, 17, 0x5A4900},
    {0x184, 20, 0x5A4C40},
    {0x25A, 32, 0x59FA50}, {0x268, 33, 0x59FDB0}, {0x276, 34, 0x5A0AB0}, {0x284, 35, 0x5A0C40},
    {0x292, 36, 0x5A0E80}, {0x2A0, 37, 0x5A1050}, {0x2AE, 38, 0x5A1290}, {0x2BC, 39, 0x5A14C0},
    {0x2CA, 40, 0x5A17A0}, {0x2D8, 41, 0x5A1A00}, {0x2E6, 42, 0x5A1D10}, {0x2F4, 43, 0x5A18B0},
    {0x302, 44, 0x5A1B50}, {0x310, 45, 0x5A1EA0}, {0x31E, 46, 0x5A20D0}, {0x32C, 48, 0x5A2220},
    {0x33A, 47, 0x5A2300}, {0x348, 50, 0x5A2520}, {0x356, 51, 0x5A2710}, {0x364, 49, 0x5A2900},
    {0x372, 52, 0x5A2EB0},
};

void Relocate(unsigned char* code, U disp_at, U table_at, U entries) {
    const U moved = Addr(code) - kBody;
    for (U i = 0; i < entries; ++i) {
        U target;
        std::memcpy(&target, code + table_at + 4 * i, 4);
        if (target < kBody || target >= kBody + 0x450)
            bof3::Fatal("Gfx_DrawOTag: jump table entry %u at +0x%X is 0x%X", (unsigned)i, (unsigned)table_at,
                        (unsigned)target);
        target += moved;
        std::memcpy(code + table_at + 4 * i, &target, 4);
    }
    U disp;
    std::memcpy(&disp, code + disp_at, 4);
    if (disp != kBody + table_at) bof3::Fatal("Gfx_DrawOTag: no jump table operand at +0x%X", (unsigned)disp_at);
    disp += moved;
    std::memcpy(code + disp_at, &disp, 4);
}

using DrawFn = void(__cdecl*)(unsigned long*);

DrawFn CloneDraw() {
    static const void* const kStubs[] = {
#define S(k) reinterpret_cast<const void*>(kStandIns.soft[k])
        S(0),  S(1),  S(2),  S(3),  S(4),  S(5),  S(6),  S(7),  S(8),  S(9),  S(10),
        S(11), S(12), S(13), S(14), S(15), S(16), S(17), S(18), S(19), S(20),
#undef S
    };
    bof3::CloneCall calls[sizeof kSites / sizeof kSites[0] + 4];
    int n = 0;
    for (const Site& s : kSites) {
        const void* stub = s.entry < 32 ? kStubs[s.entry]
                                        : reinterpret_cast<const void*>(kStandIns.d3d[s.entry - 32]);
        calls[n++] = {s.offset, stub, s.target};
    }
    calls[n++] = {0x1D6, reinterpret_cast<const void*>(&StubMove), 0x59E9A0};
    calls[n++] = {0x3C7, reinterpret_cast<const void*>(&StubMove), 0x59E9A0};
    calls[n++] = {0x3EF, reinterpret_cast<const void*>(&StubAlpha), 0x59F520};
    calls[n++] = {0x44A, reinterpret_cast<const void*>(&StubAfter), 0x59F580};   // the tail jmp
    auto* code = static_cast<unsigned char*>(bof3::CloneOriginal("Gfx_DrawOTag", kBody, kCloneBytes, calls, n));
    if (!code) bof3::Fatal("Gfx_DrawOTag: CloneOriginal returned null");
    Relocate(code, 0x6A, 0x450, 25);    // jmp [ecx*4 + 0x59F2A0] at +0x67
    Relocate(code, 0x255, 0x588, 26);   // jmp [ecx*4 + 0x59F3D8] at +0x252
    FlushInstructionCache(GetCurrentProcess(), code, kCloneBytes);
    return reinterpret_cast<DrawFn>(code);
}

// A buffer the 24-bit links can reach: inside the game's VRAM shadow
// (Gfx_VramShadow 0x6C9F44, 1 MiB), saved and put back around the fuzz. Low
// memory outside the image is not reliably free at start-up.
constexpr U kLowBuffer = 0x6D0000;

// --- a round -----------------------------------------------------------------

void RandomTable(unsigned* coverage) {
    for (U i = 0; i < kBufferBytes; ++i) At(g_base)[i] = static_cast<unsigned char>(Next());
    g_nodes = 1 + Next() % kMaxNodes;
    const U key = g_base + kMaxNodes * kNodeBytes;
    for (U i = 0; i < g_nodes; ++i) {
        const U node = g_base + i * kNodeBytes;
        U link = i + 1 < g_nodes ? node + kNodeBytes : 0xFFFFFFFFu;
        if (link != 0xFFFFFFFFu && Next() % 5 == 0) link |= (Next() | 1u) << 24;
        PutLong(node, link);
        // The head is never drawn; its code and +8 are seeded all the same.
        // The code: every index of both byte tables, now and then out of range.
        unsigned char code;
        switch (Next() % 8) {
        case 0: code = static_cast<unsigned char>(Next()); break;
        case 1: code = static_cast<unsigned char>(0xE0 + Next() % 0x20); break;
        default: code = static_cast<unsigned char>(0x20 + Next() % 0xD8); break;
        }
        At(node)[7] = code;
        if (Next() % 4 == 0) At(node)[4] = At(node)[5] = 0;   // a draw mode with tpage 0
        const U index = (code & 0xFCu) - 0x20u;
        if (index <= 0xD4 && i > 0) ++coverage[index / 4];
        const bool mode = (code & 0xF4) == 0xE0;   // E0..E3, E8..EB
        PutLong(node + 8, mode && Next() % 3 == 0 ? 0 : key + (Next() % 8) * 4);
    }
    // The draw's own state.
    for (U i = 0; i < 12; ++i) At(kTexKey)[i] = static_cast<unsigned char>(Next());
    At(kDrawEnable)[0] = static_cast<unsigned char>(Next() % 8 ? (Next() | 1) : 0);
    At(kAfterDrawFlag)[0] = static_cast<unsigned char>(Next() % 2 ? 0 : Next());
    At(kAfterDrawFlag)[1] = static_cast<unsigned char>(Next() % 2 ? 0 : Next());
    At(kRenderFlags)[0] = static_cast<unsigned char>(Next());
    for (U i = 0; i < 0xA00; ++i) At(0x7C9F50)[i] = static_cast<unsigned char>(Next());
    for (U i = 0; i < 0x1400; i += 4) PutLong(0x7CAE38 + i, Next());
}

d3d_fuzz::Log g_theirs, g_ours;

}  // namespace

void SelfTest() {
    const DrawFn theirs = CloneDraw();
    g_base = kLowBuffer;
    g_regions[0] = {g_base, kBufferBytes};
    g_regions[1] = {kTexKey, 12};
    g_regions[2] = {kAfterDrawFlag, 2};
    g_regions[3] = {kRenderFlags, 1};
    g_regions[4] = {0x7C9F50, 0xA00};
    g_regions[5] = {0x7CAE38, 0x1400};
    Capture(g_saved);
    const Callees saved = g;
    g = kStandIns;
    d3d_fuzz::Seed(0xEE50EE50u);

    constexpr unsigned kRounds = 30000;
    unsigned bad = 0, software = 0, disabled = 0, after = 0, calls = 0;
    unsigned coverage[0xD4 / 4 + 1] = {};
    {
        d3d_fuzz::DeviceSwap swap;
        for (unsigned r = 0; r < kRounds; ++r) {
            g_round = r;
            RandomTable(coverage);
            Capture(g_start);
            auto* ot = reinterpret_cast<unsigned long*>(At(g_base));

            g_theirs.Clear();
            d3d_fuzz::g_log = &g_theirs;
            theirs(ot);
            Capture(g_theirs_state);

            Restore(g_start);
            g_ours.Clear();
            d3d_fuzz::g_log = &g_ours;
            Gfx_DrawOTag(ot);
            Capture(g_ours_state);
            d3d_fuzz::g_log = nullptr;

            if (g_start.bytes[kBufferBytes + 12 + 2] & 1) ++software;
            if (g_start.bytes[kBufferBytes + 11] == 0) ++disabled;
            if (g_theirs.n && g_theirs.calls[(g_theirs.n < d3d_fuzz::kMaxCalls ? g_theirs.n : d3d_fuzz::kMaxCalls) - 1].what == 0x32)
                ++after;
            calls += g_theirs.n;

            char why[200] = "";
            U where = 0;
            bool same = d3d_fuzz::SameLog(g_ours, g_theirs, why);
            if (same && !SameState(g_ours_state, g_theirs_state, &where)) {
                same = false;
                std::snprintf(why, sizeof why, "memory at 0x%X", (unsigned)where);
            }
            if (!same) {
                if (bad < 4) bof3::Log("shadow      d3d_draw MISMATCH: Gfx_DrawOTag round %u (%u nodes): %s", r,
                                       (unsigned)g_nodes, why);
                ++bad;
            }
        }
    }
    g = saved;
    Restore(g_saved);

    unsigned reached = 0;
    for (unsigned c : coverage) reached += c != 0;
    bof3::Log("shadow      d3d_draw self-test: Gfx_DrawOTag %u rounds (software %u, draw disabled %u, after-draw %u, "
              "%u calls out, %u of %u table indices reached)",
              kRounds, software, disabled, after, calls, reached, (unsigned)(sizeof coverage / sizeof coverage[0]));
    if (bad) bof3::Fatal("the draw differs from the original in %u self-test rounds", bad);
}

}  // namespace d3d_list
