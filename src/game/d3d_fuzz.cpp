// The vertex-block fuzz's harness. See d3d_fuzz.h and docs/glyph-draw.md
// section 5.
#include "game/d3d_fuzz.h"

#include <cstdio>
#include <cstring>
#include <utility>

#include "hook/log.h"

namespace d3d_fuzz {

Log* g_log = nullptr;

void Log::Clear() {
    std::memset(this, 0, sizeof *this);
}

void Record(std::uint32_t what, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3,
            std::uint32_t a4, std::uint32_t a5, std::uint32_t a6) {
    if (!g_log) bof3::Fatal("d3d_fuzz: a call was recorded with no log set");
    if (g_log->n < kMaxCalls) g_log->calls[g_log->n] = {what, {a0, a1, a2, a3, a4, a5, a6}, -1};
    ++g_log->n;
}

std::uint32_t ComResult(unsigned position) {
    std::uint32_t h = (position + 1u) * 0x9E3779B1u;
    return h ^ (h >> 15);
}

namespace {

using U = std::uint32_t;
constexpr int kSlots = 64;

// IDirect3DDevice3's methods the handlers are known to call, with their
// argument counts including `this`. Everything else traps.
constexpr int Arity(int slot) {
    switch (slot) {
    case 9:     // BeginScene(this)
    case 10:    // EndScene(this)
        return 1;
    case 22:    // SetRenderState(this, state, value)
    case 38:    // SetTexture(this, stage, texture)
        return 3;
    case 40:    // SetTextureStageState(this, stage, type, value)
        return 4;
    case 28:    // DrawPrimitive(this, type, fvf, vertices, count, flags) - its own recorder
        return 6;
    default:
        return 0;
    }
}
constexpr int kDrawPrimitive = 28;

U Returned() { return ComResult(g_log ? g_log->n - 1u : 0u); }

template <int S> long __stdcall Com1(void*) {
    Record(0x100 + S);
    return static_cast<long>(Returned());
}
template <int S> long __stdcall Com3(void*, U a, U b) {
    Record(0x100 + S, a, b);
    return static_cast<long>(Returned());
}
template <int S> long __stdcall Com4(void*, U a, U b, U c) {
    Record(0x100 + S, a, b, c);
    return static_cast<long>(Returned());
}
template <int S> long __stdcall Trap(void*) {
    bof3::Fatal("d3d_fuzz: the fake device's slot %d (vtable +0x%X) was called, and nothing records it", S,
                S * 4);
}

long __stdcall DrawPrimitive(void*, U type, U fvf, U vertices, U count, U flags) {
    Record(0x100 + kDrawPrimitive, type, fvf, vertices, count, flags);
    if (g_log->n <= kMaxCalls && g_log->n_snaps < kMaxSnaps) {
        U bytes = count * 0x20u;
        if (bytes > kSnapBytes || count > 0x100u) bytes = kSnapBytes;
        std::memcpy(g_log->snaps[g_log->n_snaps], reinterpret_cast<const void*>(static_cast<std::uintptr_t>(vertices)),
                    bytes);
        g_log->calls[g_log->n - 1].snap = static_cast<int>(g_log->n_snaps);
        ++g_log->n_snaps;
    }
    return static_cast<long>(Returned());
}

template <int S> void* Entry() {
    if constexpr (S == kDrawPrimitive) return reinterpret_cast<void*>(&DrawPrimitive);
    else if constexpr (Arity(S) == 1) return reinterpret_cast<void*>(&Com1<S>);
    else if constexpr (Arity(S) == 3) return reinterpret_cast<void*>(&Com3<S>);
    else if constexpr (Arity(S) == 4) return reinterpret_cast<void*>(&Com4<S>);
    else return reinterpret_cast<void*>(&Trap<S>);
}

void* g_vtbl[kSlots];
struct Fake {
    void** vtbl;
} g_fake = {g_vtbl};

template <int... S> void Fill(std::integer_sequence<int, S...>) {
    ((g_vtbl[S] = Entry<S>()), ...);
}

std::uint32_t& DeviceSlot() {
    return *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(kDevice));
}

std::uint32_t g_rand = 0x2545F491u;

}  // namespace

void* FakeDevice() { return &g_fake; }

DeviceSwap::DeviceSwap() : saved_(DeviceSlot()) {
    Fill(std::make_integer_sequence<int, kSlots>{});
    DeviceSlot() = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_fake));
}

DeviceSwap::~DeviceSwap() { DeviceSlot() = saved_; }

bool SameLog(const Log& ours, const Log& theirs, char* why, SnapCompare same_snap) {
    if (ours.n != theirs.n) {
        std::snprintf(why, 160, "%u calls out, the original %u", ours.n, theirs.n);
        return false;
    }
    const unsigned n = ours.n < kMaxCalls ? ours.n : kMaxCalls;
    for (unsigned i = 0; i < n; ++i) {
        const Call& a = ours.calls[i];
        const Call& b = theirs.calls[i];
        if (a.what != b.what || std::memcmp(a.a, b.a, sizeof a.a) != 0) {
            std::snprintf(why, 160, "call %u: 0x%X(%X %X %X %X %X), the original 0x%X(%X %X %X %X %X)", i,
                          (unsigned)a.what, (unsigned)a.a[0], (unsigned)a.a[1], (unsigned)a.a[2], (unsigned)a.a[3],
                          (unsigned)a.a[4], (unsigned)b.what, (unsigned)b.a[0], (unsigned)b.a[1], (unsigned)b.a[2],
                          (unsigned)b.a[3], (unsigned)b.a[4]);
            return false;
        }
        if ((a.snap < 0) != (b.snap < 0)) {
            std::snprintf(why, 160, "call %u: one side has a vertex snapshot, the other none", i);
            return false;
        }
        if (a.snap >= 0) {
            const unsigned char* sa = ours.snaps[a.snap];
            const unsigned char* sb = theirs.snaps[b.snap];
            const bool same = same_snap ? same_snap(sa, sb) : std::memcmp(sa, sb, kSnapBytes) == 0;
            if (!same) {
                std::snprintf(why, 160, "call %u: the vertices handed to DrawPrimitive differ", i);
                return false;
            }
        }
    }
    return true;
}

void Seed(std::uint32_t seed) { g_rand = seed ? seed : 0x2545F491u; }

std::uint32_t Next() {
    std::uint32_t x = g_rand;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g_rand = x;
}

std::uint32_t Pick(const std::uint32_t* values, unsigned n) { return values[Next() % n]; }

}  // namespace d3d_fuzz
