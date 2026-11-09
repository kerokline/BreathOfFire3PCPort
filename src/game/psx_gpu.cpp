#include "game/psx_gpu.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr std::uint32_t kLinkFlag = 0x80000000u;

// --- BOF3X_SHADOW=psx_gpu: differential fuzzes, once at start-up ---------------
// Seventeen leaves with no calls and only internal jumps: a byte-copy of each
// runs anywhere. Ours is always called through the signature the game's
// callers assume - whole dwords, noise above whatever the original reads.

std::uint32_t g_rng = 0xB5297A4Du;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}

unsigned g_bad, g_rounds;

template <class Fn> Fn Clone(const char* name, std::uint32_t original, std::uint32_t size) {
    return reinterpret_cast<Fn>(bof3::CloneOriginal(name, original, size));
}
template <class Fn, class Ours> Fn AsGameCalls(Ours* ours) { return reinterpret_cast<Fn>(reinterpret_cast<void*>(ours)); }

void Mismatch(const char* name, unsigned round) {
    if (++g_bad <= 8) bof3::Log("shadow      psx_gpu self-test MISMATCH: %s, round %u", name, round);
}

// A setter of one primitive, with up to four more dword arguments: a random
// 0x50-byte primitive, the whole of it compared.
using PrimFn = void (__cdecl*)(unsigned char*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
void FuzzPrim(const char* name, PrimFn theirs, PrimFn ours, unsigned rounds) {
    for (unsigned round = 0; round < rounds; ++round, ++g_rounds) {
        unsigned char input[0x50], a[0x50], b[0x50];
        for (auto& x : input) x = static_cast<unsigned char>(Rng());
        std::uint32_t arg[4];
        // zero, one, and noise with either low bit: what flag arguments get
        for (auto& x : arg) x = Rng() % 4 == 0 ? 0 : Rng() % 3 == 0 ? 1 : Rng();
        std::memcpy(a, input, sizeof a);
        std::memcpy(b, input, sizeof b);
        theirs(a, arg[0], arg[1], arg[2], arg[3]);
        ours(b, arg[0], arg[1], arg[2], arg[3]);
        if (std::memcmp(a, b, sizeof a) != 0) Mismatch(name, round);
    }
}

using ValueFn = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
void FuzzValue(const char* name, ValueFn theirs, ValueFn ours, unsigned rounds, bool exhaustive13) {
    for (unsigned round = 0; round < rounds; ++round, ++g_rounds) {
        std::uint32_t arg[4];
        for (auto& x : arg) x = Rng() % 3 == 0 ? Rng() % 0x400 : Rng();
        if (exhaustive13) arg[0] = (round & 0x1FFF) | (round % 2 ? Rng() & ~0x1FFFu : 0);   // every angle, twice over
        if (theirs(arg[0], arg[1], arg[2], arg[3]) != ours(arg[0], arg[1], arg[2], arg[3])) Mismatch(name, round);
    }
}

using AddFn = void (__cdecl*)(unsigned long*, unsigned long*, unsigned long*);
void FuzzAddPrim(AddFn theirs, AddFn ours) {
    for (unsigned round = 0; round < 4000; ++round, ++g_rounds) {
        // Three cells of one small buffer, so that ot, prim and link_out
        // coincide as often as not - link_out IS prim in an ordinary AddPrim.
        unsigned long input[4], a[4], b[4];
        for (auto& x : input) x = Rng();
        const unsigned i = Rng() % 4, j = Rng() % 4, k = Rng() % 2 ? j : Rng() % 4;
        std::memcpy(a, input, sizeof a);
        theirs(a + i, a + j, a + k);
        std::memcpy(b, input, sizeof b);
        ours(b + i, b + j, b + k);
        // what was stored is an address in its own buffer: compare as offsets
        for (unsigned c = 0; c < 4; ++c) {
            const auto rebase = [](unsigned long v, const unsigned long* base) {
                const unsigned long low = v & ~kLinkFlag, from = Low32(base);
                return low >= from && low < from + 16 ? (v & kLinkFlag) | (low - from) | 0x40000000ul : v;
            };
            if (rebase(a[c], a) != rebase(b[c], b)) { Mismatch("Gpu_AddPrim", round); break; }
        }
    }
}

using ClearFn = void (__cdecl*)(unsigned long*, int);
void FuzzClearOTagR(ClearFn theirs, ClearFn ours) {
    for (unsigned round = 0; round < 2000; ++round, ++g_rounds) {
        // The same buffer for both, since what is stored are its addresses.
        // n from -1: the original stores the terminator at ot[n - 1] for n < 2.
        static unsigned long table[72], input[72], their_out[72];
        for (auto& x : input) x = Rng();
        const int n = static_cast<int>(Rng() % 68) - 1;
        std::memcpy(table, input, sizeof table);
        theirs(table + 3, n);
        std::memcpy(their_out, table, sizeof table);
        std::memcpy(table, input, sizeof table);
        ours(table + 3, n);
        if (std::memcmp(their_out, table, sizeof table) != 0) Mismatch("Gpu_ClearOTagR", round);
    }
}

}  // namespace

void PsxGpu_Inject() {
    // Every one: no calls, jumps internal or none (disasm 2026-09-20).
    if (bof3::WantsShadow("psx_gpu")) {
        g_bad = g_rounds = 0;
#define PRIM(name, size, rounds) \
        FuzzPrim(#name, Clone<PrimFn>(#name, bof3::addr::name, size), AsGameCalls<PrimFn>(&name), rounds)
        PRIM(Gpu_SetPolyFT4, 0x1A, 200);
        PRIM(Gpu_SetPolyGT4, 0x1A, 200);
        PRIM(Gpu_SetLineF4, 0x1A, 200);
        PRIM(Gpu_SetTile, 0x10, 200);
        PRIM(Gpu_SetLineF2, 0x14, 200);
        PRIM(Gpu_SetSprt, 0x10, 200);
        PRIM(Gpu_SetCode6C, 0x9, 200);
        PRIM(Gpu_SetCode84, 0x9, 200);
        PRIM(Gpu_SetSemiTrans, 0x1C, 2000);
        PRIM(Gpu_SetShadeTex, 0x17, 2000);
        PRIM(Gpu_SetDrawMode, 0x41, 4000);
        PRIM(Gpu_LinkPrim, 0xB, 500);   // its "primitive" is the tail cell
#undef PRIM
        FuzzValue("Gpu_GetTPage", Clone<ValueFn>("Gpu_GetTPage", bof3::addr::Gpu_GetTPage, 0x3C),
                  AsGameCalls<ValueFn>(&Gpu_GetTPage), 8000, false);
        FuzzValue("Gpu_GetClut", Clone<ValueFn>("Gpu_GetClut", bof3::addr::Gpu_GetClut, 0x14),
                  AsGameCalls<ValueFn>(&Gpu_GetClut), 8000, false);
        FuzzValue("Math_Sin", Clone<ValueFn>("Math_Sin", bof3::addr::Math_Sin, 0x42),
                  AsGameCalls<ValueFn>(&Math_Sin), 0x4000, true);
        FuzzAddPrim(Clone<AddFn>("Gpu_AddPrim", bof3::addr::Gpu_AddPrim, 0x20), &Gpu_AddPrim);
        FuzzClearOTagR(Clone<ClearFn>("Gpu_ClearOTagR", bof3::addr::Gpu_ClearOTagR, 0x31), &Gpu_ClearOTagR);
        bof3::Log("shadow      psx_gpu self-test: 17 functions, %u rounds in all, %u MISMATCHES", g_rounds, g_bad);
        if (g_bad) bof3::Fatal("the PSX library layer differs from the originals in %u of %u self-test rounds", g_bad, g_rounds);
    }
}
