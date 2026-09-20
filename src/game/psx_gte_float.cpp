#include "game/psx_gte_float.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The original of every function in this file goes through x87. What x87
// computes depends on the precision-control field of the control word: each
// fmul, fdiv and fiadd rounds to 24, 53 or 64 bits before the next one starts.
// The game runs them at 53 (kGameControlWord, measured - see
// docs/psx-library-layer.md section 3), where every x87 operation on values
// this size is the IEEE double operation, so these are written in `double`,
// which the toolchain compiles to SSE2: the same rounding, whatever the x87
// control word says. `long double` is not needed and not used.
//
// Two of them depend on that precision, Gte_DepthRamp and Gte_Perspective.
// The depth stores do not: an s32 times a power of two is exact at any of the
// three, and the store to a float rounds once.

namespace {

constexpr unsigned short kGameControlWord = 0x027F;

// 2^-14, the float at 0x5C466C: a depth as the primitives' per-vertex float.
constexpr double kDepthScale = 1.0 / 16384.0;

float DepthAsFloat(long depth) { return static_cast<float>(static_cast<double>(depth) * kDepthScale); }

void StoreAt(void* prim, unsigned offset, float value) {
    std::memcpy(static_cast<unsigned char*>(prim) + offset, &value, sizeof value);
}

// The depths oldest first are Gte_DepthOlder[2], [1], [0], then Gte_Depth.
void PrimDepths3(void* prim, unsigned stride) {
    StoreAt(prim, 0x10, DepthAsFloat(Gte_DepthOlder[1]));
    StoreAt(prim, 0x10 + stride, DepthAsFloat(Gte_DepthOlder[0]));
    StoreAt(prim, 0x10 + 2 * stride, DepthAsFloat(Gte_Depth));
}
void PrimDepths4(void* prim, unsigned stride) {
    StoreAt(prim, 0x10, DepthAsFloat(Gte_DepthOlder[2]));
    StoreAt(prim, 0x10 + stride, DepthAsFloat(Gte_DepthOlder[1]));
    StoreAt(prim, 0x10 + 2 * stride, DepthAsFloat(Gte_DepthOlder[0]));
    StoreAt(prim, 0x10 + 3 * stride, DepthAsFloat(Gte_Depth));
}
// The newest depth at all four vertices, converted once. The original stores
// two from the x87 register and copies the last two out of the primitive as
// integers; the same four dwords either way (a control that stored all four
// from the register was not refused by the fuzz).
void PrimDepthFlat4(void* prim, unsigned stride) {
    const float depth = DepthAsFloat(Gte_Depth);
    for (unsigned v = 0; v < 4; ++v) StoreAt(prim, 0x10 + v * stride, depth);
}

long DepthRamp(long value) {
    const long lo = Gte_RampNear;
    if (value < lo) return 0;
    const long hi = Gte_RampFar;
    if (value > hi) return 0x1000;
    // Both differences wrap in 32 bits, as the original's do.
    const std::int32_t num = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) - static_cast<std::uint32_t>(lo));
    const std::int32_t den = static_cast<std::int32_t>(static_cast<std::uint32_t>(hi) - static_cast<std::uint32_t>(lo));
    // lo == hi == value: 0 / 0, and _ftol of a NaN is the integer indefinite,
    // 0x8000000000000000, whose low dword - what the caller gets - is 0.
    if (den == 0) return 0;
    const double scaled = static_cast<double>(num) / static_cast<double>(den) * 4096.0;
    // _ftol 0x5B9550 truncates to 64 bits; eax is the low half. |scaled| is at
    // most 2^43.
    return static_cast<long>(static_cast<std::uint32_t>(static_cast<std::int64_t>(scaled)));
}

std::int32_t MulWrap(long a, long b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

// The globals are read where the original reads them and not before, so that
// an out pointer aimed at one of them gives what the original gives.
void Perspective(float* sx, float* sy, long* sz) {
    if (Gte_Transformed[2] >= Gte_NearZ) {
        // In front of the near plane: x * h / z + offset, the product an
        // integer one that wraps.
        const std::int32_t px = MulWrap(Gte_ProjDistance, Gte_Transformed[0]);
        *sx = static_cast<float>(static_cast<double>(px) / static_cast<double>(Gte_Transformed[2]) +
                                 static_cast<double>(Gte_OffsetX));
        const std::int32_t py = MulWrap(Gte_Transformed[1], Gte_ProjDistance);
        *sy = static_cast<float>(static_cast<double>(py) / static_cast<double>(Gte_Transformed[2]) +
                                 static_cast<double>(Gte_OffsetY));
        *sz = Gte_Transformed[2];
        return;
    }
    if (Gte_Transformed[2] <= 0) {
        // At or behind the eye: projected as if on the near plane, depth 0.
        const std::int32_t px = MulWrap(Gte_ProjDistance, Gte_Transformed[0]);
        *sx = static_cast<float>(static_cast<double>(px) / static_cast<double>(Gte_NearZ) +
                                 static_cast<double>(Gte_OffsetX));
        const std::int32_t py = MulWrap(Gte_Transformed[1], Gte_ProjDistance);
        *sy = static_cast<float>(static_cast<double>(py) / static_cast<double>(Gte_NearZ) +
                                 static_cast<double>(Gte_OffsetY));
        *sz = 0;
        return;
    }
    // Between the eye and the near plane: the same projection onto the near
    // plane, but in floating point throughout - nothing wraps - by way of
    // z / near, with y's product passing through a 32-bit float.
    const double z = static_cast<double>(Gte_Transformed[2]);
    const double ratio = z / static_cast<double>(Gte_NearZ);
    const float y_scaled = static_cast<float>(static_cast<double>(Gte_Transformed[1]) * ratio);
    *sx = static_cast<float>(static_cast<double>(Gte_Transformed[0]) * ratio * static_cast<double>(Gte_ProjDistance) / z +
                             static_cast<double>(Gte_OffsetX));
    *sy = static_cast<float>(static_cast<double>(Gte_ProjDistance) * static_cast<double>(y_scaled) /
                                 static_cast<double>(Gte_Transformed[2]) +
                             static_cast<double>(Gte_OffsetY));
    *sz = Gte_Transformed[2];
}

// --- scaffolding: the x87 control word -------------------------------------------
// Only the checks below touch it; nothing of ours depends on it.
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

// --- BOF3X_SHADOW=psx_gte_float, live ----------------------------------------------
// Every call of the two that depend on precision: a byte-copy of the original
// into temporaries, ours for real, the bit patterns compared - and the control
// word the game was running under at that moment counted, which is the
// measurement kGameControlWord rests on.

using RampFn = long (__cdecl*)(long);
using PerspectiveFn = void (__cdecl*)(float*, float*, long*);
RampFn g_ramp_clone = nullptr;
PerspectiveFn g_perspective_clone = nullptr;

struct {
    unsigned ramp_calls, perspective_calls, mismatches;
    unsigned branch[3];
    unsigned short word[4];
    unsigned word_calls[4], word_other;
} g_live;

void LiveCountWord() {
    const unsigned short cw = GetControlWord();
    for (unsigned i = 0; i < 4; ++i) {
        if (g_live.word_calls[i] == 0) g_live.word[i] = cw;
        if (g_live.word[i] == cw) { ++g_live.word_calls[i]; return; }
    }
    ++g_live.word_other;
}

void LiveReport() {
    bof3::Log("shadow      psx_gte_float live: %u Gte_DepthRamp, %u Gte_Perspective (%u in front of the near plane, "
              "%u behind the eye, %u between), %u MISMATCHES; x87 control word %04X x %u, %04X x %u, %04X x %u, "
              "%04X x %u, others x %u",
              g_live.ramp_calls, g_live.perspective_calls, g_live.branch[0], g_live.branch[1], g_live.branch[2],
              g_live.mismatches, g_live.word[0], g_live.word_calls[0], g_live.word[1], g_live.word_calls[1],
              g_live.word[2], g_live.word_calls[2], g_live.word[3], g_live.word_calls[3], g_live.word_other);
}

void LiveTick() {
    const unsigned calls = g_live.ramp_calls + g_live.perspective_calls;
    if (calls == 1 || calls % 65536 == 0) LiveReport();
}

// --- the same switch, once at start-up: a differential fuzz -----------------------
// One round: randomise the globals these read and a scratch area, run one
// function - the original's copy, then from the same state ours - and compare
// the scratch and the return value as bits. The copies run under a control
// word set here: kGameControlWord, where any mismatch is fatal; and the other
// two precisions, where the depth stores must still match and the two that
// depend on precision are EXPECTED to differ - the count is logged, and is the
// standing proof that this fuzz can see a rounding difference at all.

struct State {
    long transformed[3], near_z, proj, offset_x, offset_y, ramp_near, ramp_far, older[3], depth, near_scale, far_scale, matrix_depth;
    unsigned long scratch[48];
};

void Capture(State& s, const unsigned long* scratch) {
    std::memcpy(s.transformed, Gte_Transformed, sizeof s.transformed);
    s.near_z = Gte_NearZ; s.proj = Gte_ProjDistance;
    s.offset_x = Gte_OffsetX; s.offset_y = Gte_OffsetY;
    s.ramp_near = Gte_RampNear; s.ramp_far = Gte_RampFar;
    std::memcpy(s.older, Gte_DepthOlder, sizeof s.older);
    s.depth = Gte_Depth;
    s.near_scale = Gte_RampNearScale; s.far_scale = Gte_RampFarScale;
    s.matrix_depth = Gte_MatrixDepth;
    if (scratch) std::memcpy(s.scratch, scratch, sizeof s.scratch);
}
void Apply(const State& s, unsigned long* scratch) {
    std::memcpy(Gte_Transformed, s.transformed, sizeof s.transformed);
    Gte_NearZ = s.near_z; Gte_ProjDistance = s.proj;
    Gte_OffsetX = s.offset_x; Gte_OffsetY = s.offset_y;
    Gte_RampNear = s.ramp_near; Gte_RampFar = s.ramp_far;
    std::memcpy(Gte_DepthOlder, s.older, sizeof s.older);
    Gte_Depth = s.depth;
    Gte_RampNearScale = s.near_scale; Gte_RampFarScale = s.far_scale;
    Gte_MatrixDepth = s.matrix_depth;
    if (scratch) std::memcpy(scratch, s.scratch, sizeof s.scratch);
}

using Fn = long (__cdecl*)(void*, void*, void*, void*);
struct Entry {
    const char* name;
    std::uint32_t original, size;
    Fn ours;
    bool by_value;        // its arguments are numbers
    bool returns;         // its result counts
    bool needs_precision; // differs from the original at other control words
    std::uint32_t jmp_at; // offset of its E9 that leaves, to _ftol; 0 for none
    Fn theirs;
};

template <class F> Fn AsFn(F* f) { return reinterpret_cast<Fn>(reinterpret_cast<void*>(f)); }

void SelfTest(Entry* entries, unsigned n) {
    constexpr unsigned kRounds = 48000;
    static State saved, input, their_out, our_out;
    static unsigned long scratch[48];
    Capture(saved, nullptr);
    const unsigned short saved_word = GetControlWord();

    std::uint32_t rng = 0x9E3779B9u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    // A number as the game has them - small - or anything at all, or an edge.
    auto number = [&next]() -> long {
        static const long kEdge[] = {0, 1, -1, 2, 0x7FFFFFFF, static_cast<long>(0x80000000u), 0x1000, 500, 1000};
        switch (next() % 8) {
            case 0: return static_cast<long>(next());
            case 1: return kEdge[next() % 9];
            case 2: return static_cast<long>(next() % 0x2000000u) - 0x1000000;
            default: return static_cast<long>(next() % 0x4000u) - 0x1000;
        }
    };

    static const unsigned short kWords[3] = {kGameControlWord, 0x007F, 0x037F};
    unsigned bad = 0, elsewhere_same = 0, elsewhere_differs = 0, stores_bad_elsewhere = 0;
    unsigned branch[3] = {0, 0, 0}, ramp_inside = 0, ramp_steps = 0, aliased = 0, at_globals = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < sizeof input; ++i) bytes[i] = static_cast<unsigned char>(next());
        for (long& v : input.transformed) v = number();
        input.near_z = number(); input.proj = number();
        input.offset_x = number(); input.offset_y = number();
        input.ramp_near = number(); input.ramp_far = number();
        for (long& v : input.older) v = number();
        input.depth = number();
        input.near_scale = number(); input.far_scale = number();
        // z against the near plane: often right on it or next to it
        if (next() % 4 == 0) input.transformed[2] = input.near_z + static_cast<long>(next() % 3) - 1;
        long value = number();
        const long value2 = number();
        bool stepped = false;
        if (next() % 2 == 0 && input.ramp_far > input.ramp_near) {   // inside the ramp, and its ends
            const std::uint32_t span = static_cast<std::uint32_t>(input.ramp_far) - static_cast<std::uint32_t>(input.ramp_near);
            value = static_cast<long>(static_cast<std::uint32_t>(input.ramp_near) + (span == 0xFFFFFFFFu ? next() : next() % (span + 1u)));
        }
        if (next() % 4 == 0) {
            // A wide ramp and a value one either side of a 4096th of it: the
            // only place the quotient's precision reaches the truncated result.
            input.ramp_near = static_cast<long>(next() % 0x4000u) - 0x1000;
            const std::uint32_t span = 0x2000000u + next() % 0x7DFF0000u;
            input.ramp_far = input.ramp_near + static_cast<long>(span);
            const std::uint64_t step = static_cast<std::uint64_t>(span) * (next() % 4097u) / 4096u;
            std::int64_t v = input.ramp_near + static_cast<std::int64_t>(step) + static_cast<long>(next() % 3) - 1;
            if (v < input.ramp_near) v = input.ramp_near;
            if (v > input.ramp_far) v = input.ramp_far;
            value = static_cast<long>(v);
            stepped = true;
        }
        if (next() % 16 == 0) input.ramp_far = input.ramp_near, value = input.ramp_near;   // 0 / 0

        const Entry& e = entries[round % n];
        // Four argument cells in the scratch - far enough apart for the
        // by-pointer stores, a quarter of the time overlapping. The primitive
        // stores write up to +0x4C of the first.
        unsigned at[4] = {0, 22, 25, 28};
        if (next() % 4 == 0) { for (unsigned& a : at) a = 20 + next() % 3; ++aliased; }
        if (e.original == bof3::addr::Gte_Perspective)
            ++branch[input.transformed[2] >= input.near_z ? 0 : input.transformed[2] <= 0 ? 1 : 2];
        if (e.returns && value >= input.ramp_near && value <= input.ramp_far) ++ramp_inside;
        if (e.returns && stepped && input.ramp_far != input.ramp_near) ++ramp_steps;

        // The perspective division's outs, one time in eight, aimed at one of
        // the globals it reads - which shows where it reads them again.
        void* out[4] = {scratch + at[0], scratch + at[1], scratch + at[2], scratch + at[3]};
        if (e.original == bof3::addr::Gte_Perspective && next() % 8 == 0) {
            void* const globals[] = {&Gte_Transformed[0], &Gte_Transformed[1], &Gte_Transformed[2], &Gte_NearZ,
                                     &Gte_ProjDistance, &Gte_OffsetX, &Gte_OffsetY};
            out[next() % 3] = globals[next() % 7];
            ++at_globals;
        }

        const unsigned word_index = round % 3;
        long result[2] = {0, 0};
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, scratch);
            void* a0 = e.by_value ? reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(value)))
                                  : out[0];
            if (pass == 0) SetControlWord(kWords[word_index]);
            void* a1 = e.by_value ? reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(value2)))
                                  : out[1];
            result[pass] = (pass ? e.ours : e.theirs)(a0, a1, out[2], out[3]);
            if (pass == 0) SetControlWord(saved_word);
            Capture(pass ? our_out : their_out, scratch);
        }
        const bool same = std::memcmp(&their_out, &our_out, sizeof their_out) == 0 &&
                          (!e.returns || result[0] == result[1]);
        if (word_index == 0 || !e.needs_precision) {
            if (!same) {
                if (word_index != 0) ++stores_bad_elsewhere;
                if (++bad <= 8)
                    bof3::Log("shadow      psx_gte_float self-test MISMATCH: %s, round %u, control word %04X, "
                              "z %ld near %ld, results %08lX / %08lX", e.name, round, kWords[word_index],
                              input.transformed[2], input.near_z, static_cast<unsigned long>(result[0]),
                              static_cast<unsigned long>(result[1]));
            }
        } else {
            ++(same ? elsewhere_same : elsewhere_differs);
        }
    }
    Apply(saved, nullptr);
    SetControlWord(saved_word);
    bof3::Log("shadow      psx_gte_float self-test: %u functions, %u rounds, a third under each of the control words "
              "%04X, %04X, %04X; Gte_Perspective %u in front of the near plane, %u behind the eye, %u between; "
              "and %u with an out aimed at a global it reads; Gte_DepthRamp %u inside the ramp, %u of those next to a "
              "4096th of a wide one; %u with overlapping arguments; %u MISMATCHES (%u of them depth "
              "stores under another precision). The two that depend on precision, under the other two: %u rounds "
              "the same, %u DIFFERENT - expected, and the proof that a rounding difference shows",
              n, kRounds, kWords[0], kWords[1], kWords[2], branch[0], branch[1], branch[2], at_globals, ramp_inside, ramp_steps, aliased, bad,
              stores_bad_elsewhere, elsewhere_same, elsewhere_differs);
    if (bad) bof3::Fatal("the GTE's floating-point functions differ from the originals in %u of %u self-test rounds", bad, kRounds);
    if (elsewhere_differs == 0)
        bof3::Fatal("psx_gte_float self-test: no round differed under another precision - the fuzz cannot see rounding");
}

}  // namespace

// original 0x5A8340: where a depth lies between two planes, 0 to 0x1000 - the
// GTE's depth-cue interpolation factor by its shape. Truncated, not rounded.
extern "C" long __cdecl Gte_DepthRamp(long value) {
    if (!g_ramp_clone) return DepthRamp(value);
    LiveCountWord();
    const long theirs = g_ramp_clone(value);
    const long ours = DepthRamp(value);
    ++g_live.ramp_calls;
    if (theirs != ours && ++g_live.mismatches <= 8)
        bof3::Log("shadow      Gte_DepthRamp MISMATCH: value %ld in %ld..%ld, ours %ld, original %ld", value,
                  Gte_RampNear, Gte_RampFar, ours, theirs);
    LiveTick();
    return ours;
}

// original 0x5A8380: the perspective division. Gte_Transformed, the vertex in
// view space, to a screen position in FLOATS - where the PSX had two s16 - and
// a depth. 2.5 million calls an attract run.
extern "C" void __cdecl Gte_Perspective(float* sx, float* sy, long* sz) {
    if (!g_perspective_clone) { Perspective(sx, sy, sz); return; }
    LiveCountWord();
    ++g_live.branch[Gte_Transformed[2] >= Gte_NearZ ? 0 : Gte_Transformed[2] <= 0 ? 1 : 2];
    std::uint32_t their_bits[3] = {0, 0, 0}, our_bits[3];
    g_perspective_clone(reinterpret_cast<float*>(&their_bits[0]), reinterpret_cast<float*>(&their_bits[1]),
                        reinterpret_cast<long*>(&their_bits[2]));
    Perspective(sx, sy, sz);
    std::memcpy(&our_bits[0], sx, 4);
    std::memcpy(&our_bits[1], sy, 4);
    std::memcpy(&our_bits[2], sz, 4);
    ++g_live.perspective_calls;
    // Outs that overlap each other would differ here without being wrong; the
    // game's never do, and the start-up fuzz is where overlap is compared.
    if (std::memcmp(their_bits, our_bits, sizeof our_bits) != 0 && ++g_live.mismatches <= 8)
        bof3::Log("shadow      Gte_Perspective MISMATCH: (%ld, %ld, %ld) near %ld h %ld: ours %08X %08X %08X, "
                  "original %08X %08X %08X", Gte_Transformed[0], Gte_Transformed[1], Gte_Transformed[2], Gte_NearZ,
                  Gte_ProjDistance, (unsigned)our_bits[0], (unsigned)our_bits[1], (unsigned)our_bits[2],
                  (unsigned)their_bits[0], (unsigned)their_bits[1], (unsigned)their_bits[2]);
    LiveTick();
}

// originals 0x5A9110, 0x5A9130, 0x5A9170: the newest depth as a float, depth
// / 16384; or the newest three, or all four, oldest first - one store at a
// time in this order, which is what overlapping outs would show.
extern "C" void __cdecl Gte_StoreDepthF(float* out) { *out = DepthAsFloat(Gte_Depth); }
extern "C" void __cdecl Gte_StoreDepthF3(float* out0, float* out1, float* out2) {
    *out0 = DepthAsFloat(Gte_DepthOlder[1]);
    *out1 = DepthAsFloat(Gte_DepthOlder[0]);
    *out2 = DepthAsFloat(Gte_Depth);
}
extern "C" void __cdecl Gte_StoreDepthF4(float* out0, float* out1, float* out2, float* out3) {
    *out0 = DepthAsFloat(Gte_DepthOlder[2]);
    *out1 = DepthAsFloat(Gte_DepthOlder[1]);
    *out2 = DepthAsFloat(Gte_DepthOlder[0]);
    *out3 = DepthAsFloat(Gte_Depth);
}

// originals 0x5A91C0..0x5A9460: the same depths straight into a primitive's
// per-vertex float, the first at +0x10 and the rest a stride apart - the
// field the primitive setters fill with 0.01 (psx_gpu.cpp). One function per
// primitive layout in the original, several of them byte for byte the same;
// they stay separate entries because they are separate addresses.
extern "C" void __cdecl Gte_PrimDepths3_0C(void* prim) { PrimDepths3(prim, 0x0C); }
extern "C" void __cdecl Gte_PrimDepths3_10(void* prim) { PrimDepths3(prim, 0x10); }
extern "C" void __cdecl Gte_PrimDepths4_0C(void* prim) { PrimDepths4(prim, 0x0C); }
extern "C" void __cdecl Gte_PrimDepths4_10(void* prim) { PrimDepths4(prim, 0x10); }
extern "C" void __cdecl Gte_PrimDepthFlat4_10(void* prim) { PrimDepthFlat4(prim, 0x10); }
extern "C" void __cdecl Gte_PrimDepths3_10B(void* prim) { PrimDepths3(prim, 0x10); }
extern "C" void __cdecl Gte_PrimDepths4_10B(void* prim) { PrimDepths4(prim, 0x10); }
extern "C" void __cdecl Gte_PrimDepths4_14(void* prim) { PrimDepths4(prim, 0x14); }
extern "C" void __cdecl Gte_PrimDepthFlat4_14(void* prim) { PrimDepthFlat4(prim, 0x14); }
extern "C" void __cdecl Gte_PrimDepths3_10C(void* prim) { PrimDepths3(prim, 0x10); }
extern "C" void __cdecl Gte_PrimDepths4_10C(void* prim) { PrimDepths4(prim, 0x10); }

// originals 0x5A7AA0, 0x5A7AE0, 0x5A7B00: what the functions above read, set.
// Integer all three, and here because the fuzz that owns those globals is.
// By shape libgte's InitGeom, SetGeomOffset and SetGeomScreen - with the near
// plane at half the projection distance, as the GTE clips, and the depth
// ramp's two ends scaled with it: scale * h / 1000, the product wrapping, the
// division toward zero.
extern "C" void __cdecl Gte_InitGeom(void) {
    Gte_ProjDistance = 1000;
    Gte_NearZ = 500;
    Gte_RampNearScale = 0x3333;
    Gte_RampFarScale = 0x10000;
    Gte_RampNear = 0x3333;
    Gte_RampFar = 0x10000;
    Gte_MatrixDepth = 0;
}
extern "C" void __cdecl Gte_SetGeomOffset(long x, long y) {
    Gte_OffsetX = x;
    Gte_OffsetY = y;
}
extern "C" void __cdecl Gte_SetGeomScreen(long h) {
    Gte_ProjDistance = h;
    Gte_NearZ = h / 2;
    Gte_RampNear = MulWrap(Gte_RampNearScale, h) / 1000;
    Gte_RampFar = MulWrap(Gte_RampFarScale, h) / 1000;
}

void PsxGteFloat_Inject() {
    // Every one: no calls; jumps internal or none, but for Gte_DepthRamp's
    // tail jump to _ftol 0x5B9550 at +0x3B, which the copy keeps aimed at the
    // original (disasm 2026-09-20).
    if (bof3::WantsShadow("psx_gte_float")) {
#define E(name, size, by_value, precision, jmp) \
    {#name, bof3::addr::name, size, AsFn(&name), by_value, by_value && precision, precision, jmp, nullptr}
        static Entry entries[] = {
            E(Gte_DepthRamp, 0x40, true, true, 0x3B),      E(Gte_Perspective, 0x111, false, true, 0),
            E(Gte_StoreDepthF, 0x13, false, false, 0),     E(Gte_StoreDepthF3, 0x37, false, false, 0),
            E(Gte_StoreDepthF4, 0x49, false, false, 0),    E(Gte_PrimDepths3_0C, 0x32, false, false, 0),
            E(Gte_PrimDepths3_10, 0x32, false, false, 0),  E(Gte_PrimDepths4_0C, 0x41, false, false, 0),
            E(Gte_PrimDepths4_10, 0x41, false, false, 0),  E(Gte_PrimDepthFlat4_10, 0x22, false, false, 0),
            E(Gte_PrimDepths3_10B, 0x32, false, false, 0), E(Gte_PrimDepths4_10B, 0x41, false, false, 0),
            E(Gte_PrimDepths4_14, 0x41, false, false, 0),  E(Gte_PrimDepthFlat4_14, 0x22, false, false, 0),
            E(Gte_PrimDepths3_10C, 0x32, false, false, 0), E(Gte_PrimDepths4_10C, 0x41, false, false, 0),
            E(Gte_InitGeom, 0x3F, true, false, 0),         E(Gte_SetGeomOffset, 0x14, true, false, 0),
            E(Gte_SetGeomScreen, 0x57, true, false, 0),
        };
#undef E
        for (auto& e : entries) {
            const bof3::CloneCall jmp{e.jmp_at, nullptr};
            e.theirs = reinterpret_cast<Fn>(
                bof3::CloneOriginal(e.name, e.original, e.size, e.jmp_at ? &jmp : nullptr, e.jmp_at ? 1 : 0));
        }
        SelfTest(entries, sizeof entries / sizeof entries[0]);
        // From here on the two wrappers above compare every live call.
        g_ramp_clone = reinterpret_cast<RampFn>(reinterpret_cast<void*>(entries[0].theirs));
        g_perspective_clone = reinterpret_cast<PerspectiveFn>(reinterpret_cast<void*>(entries[1].theirs));
    }
    BOF3_INJECT(Gte_DepthRamp);
    BOF3_INJECT(Gte_Perspective);
    BOF3_INJECT(Gte_StoreDepthF);
    BOF3_INJECT(Gte_StoreDepthF3);
    BOF3_INJECT(Gte_StoreDepthF4);
    BOF3_INJECT(Gte_PrimDepths3_0C);
    BOF3_INJECT(Gte_PrimDepths3_10);
    BOF3_INJECT(Gte_PrimDepths4_0C);
    BOF3_INJECT(Gte_PrimDepths4_10);
    BOF3_INJECT(Gte_PrimDepthFlat4_10);
    BOF3_INJECT(Gte_PrimDepths3_10B);
    BOF3_INJECT(Gte_PrimDepths4_10B);
    BOF3_INJECT(Gte_PrimDepths4_14);
    BOF3_INJECT(Gte_PrimDepthFlat4_14);
    BOF3_INJECT(Gte_PrimDepths3_10C);
    BOF3_INJECT(Gte_PrimDepths4_10C);
    BOF3_INJECT(Gte_InitGeom);
    BOF3_INJECT(Gte_SetGeomOffset);
    BOF3_INJECT(Gte_SetGeomScreen);
}
