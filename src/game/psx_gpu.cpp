#include "game/psx_gpu.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// What the setters leave at +0x10 and on: the float 0.01, as its bits. The
// PSX primitives have no such field; the port's are wider, and unread.
constexpr std::uint32_t kPointZeroOne = 0x3C23D70Au;
constexpr std::uint32_t kLinkFlag = 0x80000000u;

void PutDword(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }

// --- BOF3X_SHADOW=psx_gpu: differential fuzzes, once at start-up ---------------
// Fourteen leaves with no calls and only internal jumps: a byte-copy of each
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
                const unsigned long low = v & ~kLinkFlag, from = static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(base));
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

// original 0x5A7540. Puts prim at the head of the list ot heads: the old head
// goes to link_out - prim's own link, for the game's callers - and the new
// head carries bit 31 of what prim's first dword held. Both reads first.
extern "C" void __cdecl Gpu_AddPrim(unsigned long* ot, unsigned long* prim, unsigned long* link_out) {
    const unsigned long flag = *prim & kLinkFlag;
    const unsigned long head = *ot;
    *link_out = head;
    *ot = flag | static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(prim));
}

// original 0x5A7560. One store: the list's last link now names `item`. The
// caller makes `item` the new tail. 892,889 calls in the attract run.
extern "C" void __cdecl Gpu_LinkPrim(unsigned long* tail, unsigned long item) { *tail = item; }

// originals 0x5A75D0, 0x5A7650, 0x5A7710, 0x5A7760, 0x5A7770. The primitive's
// GPU code to byte +7, and for three of them 0.01 to a float per vertex.
extern "C" void __cdecl Gpu_SetPolyFT4(unsigned char* prim) {
    prim[7] = 0x2C;
    for (unsigned at = 0x10; at <= 0x40; at += 0x10) PutDword(prim + at, kPointZeroOne);
}
extern "C" void __cdecl Gpu_SetLineF2(unsigned char* prim) {
    prim[7] = 0x40;
    PutDword(prim + 0x10, kPointZeroOne);
    PutDword(prim + 0x1C, kPointZeroOne);
}
extern "C" void __cdecl Gpu_SetSprt(unsigned char* prim) {
    prim[7] = 0x64;
    PutDword(prim + 0x10, kPointZeroOne);
}
extern "C" void __cdecl Gpu_SetCode6C(unsigned char* prim) { prim[7] = 0x6C; }
extern "C" void __cdecl Gpu_SetCode84(unsigned char* prim) { prim[7] = 0x84; }

// originals 0x5A7780, 0x5A77A0. Bits 1 and 0 of the GPU code, from bit 0 of the
// argument's low byte.
extern "C" void __cdecl Gpu_SetSemiTrans(unsigned char* prim, unsigned abe) {
    prim[7] = static_cast<unsigned char>((abe & 1) ? prim[7] | 2 : prim[7] & 0xFD);
}
extern "C" void __cdecl Gpu_SetShadeTex(unsigned char* prim, unsigned tge) {
    prim[7] = static_cast<unsigned char>((tge & 1) ? prim[7] | 1 : prim[7] & 0xFE);
}

// original 0x5A77C0. A draw-mode primitive: 0xE8000000 with the texture page
// in the low word and the two flags in bits 16 and 17 - not the PSX GPU's
// 0xE1 packet - and the texture window word after it.
extern "C" void __cdecl Gpu_SetDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    PutDword(prim + 4, 0xE8000000u | (tpage & 0xFFFFu));
    if (dfe) prim[6] |= 1;
    if (dtd) prim[6] |= 2;
    PutDword(prim + 8, tw);
}

// original 0x5A7960. An ordering table of n entries, each linking to the one
// below it and the lowest to the terminator, every link with bit 31 set.
//
// As the original has it: for n < 2 the terminator goes to ot[n - 1], which
// for n < 1 is outside the table.
extern "C" void __cdecl Gpu_ClearOTagR(unsigned long* ot, int n) {
    std::uintptr_t at = reinterpret_cast<std::uintptr_t>(ot) + static_cast<std::uintptr_t>(n) * 4u - 4u;
    for (int k = n - 1; k > 0; --k) {
        *reinterpret_cast<unsigned long*>(at) = static_cast<unsigned long>(at - 4u) | kLinkFlag;
        at -= 4u;
    }
    *reinterpret_cast<unsigned long*>(at) =
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(&Gpu_OtTerminator)) | kLinkFlag;
}

// originals 0x5A79A0, 0x5A79E0: libgpu's getTPage and getClut, bit for bit.
extern "C" unsigned __cdecl Gpu_GetTPage(unsigned tp, unsigned abr, int x, int y) {
    return ((tp & 3u) << 7) | ((abr & 3u) << 5) | ((static_cast<unsigned>(y) & 0x200u) << 2) |
           (static_cast<unsigned>(x >> 6) & 0xFu) | (static_cast<unsigned>(y >> 4) & 0x10u);
}
extern "C" unsigned __cdecl Gpu_GetClut(int x, int y) {
    return (static_cast<unsigned>(x >> 4) & 0x3Fu) | (static_cast<unsigned>(y) << 6);
}

// original 0x5A7A00. Sine of a 4096-step angle, 4096 to the unit, from the
// exe's own quarter-wave table - which is read in place, never copied.
extern "C" int __cdecl Math_Sin(int angle) {
    const int sign = (angle & 0x800) ? -1 : 1;
    const int a = angle & 0x7FF;
    return static_cast<int>(Math_SinTable[a < 0x400 ? a : 0x800 - a]) * sign;
}

void PsxGpu_Inject() {
    // Every one: no calls, jumps internal or none (disasm 2026-09-20).
    if (bof3::WantsShadow("psx_gpu")) {
        g_bad = g_rounds = 0;
#define PRIM(name, size, rounds) \
        FuzzPrim(#name, Clone<PrimFn>(#name, bof3::addr::name, size), AsGameCalls<PrimFn>(&name), rounds)
        PRIM(Gpu_SetPolyFT4, 0x1A, 200);
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
        bof3::Log("shadow      psx_gpu self-test: 14 functions, %u rounds in all, %u MISMATCHES", g_rounds, g_bad);
        if (g_bad) bof3::Fatal("the PSX library layer differs from the originals in %u of %u self-test rounds", g_bad, g_rounds);
    }
    BOF3_INJECT(Gpu_AddPrim);
    BOF3_INJECT(Gpu_LinkPrim);
    BOF3_INJECT(Gpu_SetPolyFT4);
    BOF3_INJECT(Gpu_SetLineF2);
    BOF3_INJECT(Gpu_SetSprt);
    BOF3_INJECT(Gpu_SetCode6C);
    BOF3_INJECT(Gpu_SetCode84);
    BOF3_INJECT(Gpu_SetSemiTrans);
    BOF3_INJECT(Gpu_SetShadeTex);
    BOF3_INJECT(Gpu_SetDrawMode);
    BOF3_INJECT(Gpu_ClearOTagR);
    BOF3_INJECT(Gpu_GetTPage);
    BOF3_INJECT(Gpu_GetClut);
    BOF3_INJECT(Math_Sin);
}
