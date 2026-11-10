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
        psx_gte_float::g_ramp_clone = reinterpret_cast<RampFn>(reinterpret_cast<void*>(entries[0].theirs));
        psx_gte_float::g_perspective_clone = reinterpret_cast<PerspectiveFn>(reinterpret_cast<void*>(entries[1].theirs));
    }
}
