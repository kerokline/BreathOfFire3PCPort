#include "game/psx_gte_transform.h"

#include <emmintrin.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The GTE's transform commands, and libgte's functions over them: a vertex
// through the current matrix, its translation added, the perspective division
// - Gte_ApplyMatrix, Gte_Perspective and Gte_DepthRamp, which are ours
// already - and the results pushed through the coprocessor's FIFOs or handed
// to the caller. Integer but for two vector normalisations, which are x87 in
// the original and `double` here for the reason psx_gte_float.cpp gives.

namespace {

// --- BOF3X_SHADOW=psx_gte_transform: a differential fuzz, once at start-up --------
// Byte-copies of the originals, their calls re-aimed at byte-copies of what
// they call (never at ours), run under the control word the game runs under.
// One round: every register block these touch and a scratch area of arguments
// randomised; theirs, then from the same state ours; all of it compared.

constexpr unsigned short kGameControlWord = 0x027F;   // measured: psx_gte_float.cpp
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

struct State {
    unsigned long matrix[8], matrix2[8], color_matrix[8], vertices[6], screen[6];
    short back_color[4];
    long transformed[3], near_z, proj, offset_x, offset_y, ramp_near, ramp_far, older[3], depth, ir0, otz;
    unsigned long scratch[40];
};

void Capture(State& s, const unsigned long* scratch) {
    std::memcpy(s.matrix, Gte_Matrix, sizeof s.matrix);
    std::memcpy(s.matrix2, Gte_Matrix2, sizeof s.matrix2);
    std::memcpy(s.color_matrix, Gte_ColorMatrix, sizeof s.color_matrix);
    std::memcpy(s.back_color, Gte_BackColor, 3 * sizeof(short));
    s.back_color[3] = 0;
    std::memcpy(s.vertices, Gte_Vertices, sizeof s.vertices);
    std::memcpy(s.screen, Gte_ScreenXY, sizeof s.screen);
    std::memcpy(s.transformed, Gte_Transformed, sizeof s.transformed);
    s.near_z = Gte_NearZ; s.proj = Gte_ProjDistance;
    s.offset_x = Gte_OffsetX; s.offset_y = Gte_OffsetY;
    s.ramp_near = Gte_RampNear; s.ramp_far = Gte_RampFar;
    std::memcpy(s.older, Gte_DepthOlder, sizeof s.older);
    s.depth = Gte_Depth; s.ir0 = Gte_Ir0; s.otz = Gte_Otz;
    if (scratch) std::memcpy(s.scratch, scratch, sizeof s.scratch);
}
void Apply(const State& s, unsigned long* scratch) {
    std::memcpy(Gte_Matrix, s.matrix, sizeof s.matrix);
    std::memcpy(Gte_Matrix2, s.matrix2, sizeof s.matrix2);
    std::memcpy(Gte_ColorMatrix, s.color_matrix, sizeof s.color_matrix);
    std::memcpy(Gte_BackColor, s.back_color, 3 * sizeof(short));
    std::memcpy(Gte_Vertices, s.vertices, sizeof s.vertices);
    std::memcpy(Gte_ScreenXY, s.screen, sizeof s.screen);
    std::memcpy(Gte_Transformed, s.transformed, sizeof s.transformed);
    Gte_NearZ = s.near_z; Gte_ProjDistance = s.proj;
    Gte_OffsetX = s.offset_x; Gte_OffsetY = s.offset_y;
    Gte_RampNear = s.ramp_near; Gte_RampFar = s.ramp_far;
    std::memcpy(Gte_DepthOlder, s.older, sizeof s.older);
    Gte_Depth = s.depth; Gte_Ir0 = s.ir0; Gte_Otz = s.otz;
    if (scratch) std::memcpy(scratch, s.scratch, sizeof s.scratch);
}

using Fn = long (__cdecl*)(void*, void*, void*, void*, void*, void*, void*, void*, void*);
template <class F> Fn AsFn(F* f) { return reinterpret_cast<Fn>(reinterpret_cast<void*>(f)); }

enum Callee { kApply, kApplyLV, kPerspective, kRamp, kSin, kFtol, kNormal, kCallees };
struct CallAt { std::uint32_t offset; Callee callee; };
struct Entry {
    const char* name;
    std::uint32_t original, size;
    Fn ours;
    bool by_value, returns;   // by_value: all its arguments are numbers
    unsigned n_calls;
    CallAt calls[9];
    Fn theirs;
};

void SelfTest(Entry* entries, unsigned n) {
    constexpr unsigned kRounds = 36000;
    static State saved, input, their_out, our_out;
    static unsigned long scratch[40];
    Capture(saved, nullptr);
    const unsigned short saved_word = GetControlWord();

    std::uint32_t rng = 0x7F4A7C15u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    auto number = [&next]() -> long {
        static const long kEdge[] = {0, 1, -1, 2, 0x7FFFFFFF, static_cast<long>(0x80000000u), 0x1000, 500, 1000};
        switch (next() % 8) {
            case 0: return static_cast<long>(next());
            case 1: return kEdge[next() % 9];
            case 2: return static_cast<long>(next() % 0x2000000u) - 0x1000000;
            default: return static_cast<long>(next() % 0x4000u) - 0x1000;
        }
    };

    unsigned bad = 0, overlapping = 0, zero_vectors = 0, byte_overlaps = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < sizeof input; ++i) bytes[i] = static_cast<unsigned char>(next());
        input.matrix[5] = static_cast<unsigned long>(number());
        input.matrix[6] = static_cast<unsigned long>(number());
        input.matrix[7] = static_cast<unsigned long>(number());
        input.near_z = number(); input.proj = number();
        input.offset_x = number(); input.offset_y = number();
        input.ramp_near = number(); input.ramp_far = number();
        for (long& v : input.older) v = number();
        input.depth = number();
        // Half the time the scratch holds numbers rather than noise: a vector
        // to normalise is three of them, and all zero now and then.
        if (next() % 2 == 0)
            for (unsigned long& v : input.scratch) v = static_cast<unsigned long>(number());
        const bool zero = next() % 16 == 0;
        if (zero) { std::memset(input.scratch, 0, sizeof input.scratch); ++zero_vectors; }

        const Entry& e = entries[round % n];
        // Nine argument cells: four vertices, four outs of three dwords, one
        // more. One round in eight they land anywhere, overlapping.
        unsigned at[9] = {0, 2, 4, 6, 8, 11, 14, 17, 20};
        if (next() % 8 == 0) { for (unsigned& a : at) a = next() % 22; ++overlapping; }
        const long value = number(), value2 = number(), value3 = number();
        // The colours are bytes: when cells overlap, in and out a few bytes apart.
        unsigned char* color_out = nullptr;
        if (e.original == bof3::addr::Gte_NormalColor && next() % 4 == 0) {
            color_out = reinterpret_cast<unsigned char*>(scratch + at[1]) + static_cast<int>(next() % 7) - 3;
            if (color_out < reinterpret_cast<unsigned char*>(scratch)) color_out = nullptr; else ++byte_overlaps;
        }

        long result[2] = {0, 0};
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, scratch);
            void* a[9];
            for (unsigned i = 0; i < 9; ++i) a[i] = scratch + at[i];
            if (e.by_value) {
                a[0] = reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(value)));
                a[1] = reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(value2)));
                a[2] = reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(value3)));
            }
            if (color_out) a[2] = color_out;
            if (pass == 0) SetControlWord(kGameControlWord);
            result[pass] = (pass ? e.ours : e.theirs)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]);
            if (pass == 0) SetControlWord(saved_word);
            Capture(pass ? our_out : their_out, scratch);
        }
        const bool same = std::memcmp(&their_out, &our_out, sizeof their_out) == 0 &&
                          (!e.returns || result[0] == result[1]);
        if (!same && ++bad <= 8)
            bof3::Log("shadow      psx_gte_transform self-test MISMATCH: %s, round %u, cells %u %u %u %u %u %u %u %u %u, "
                      "results %08lX / %08lX", e.name, round, at[0], at[1], at[2], at[3], at[4], at[5], at[6], at[7],
                      at[8], static_cast<unsigned long>(result[0]), static_cast<unsigned long>(result[1]));
    }
    Apply(saved, nullptr);
    SetControlWord(saved_word);
    bof3::Log("shadow      psx_gte_transform self-test: %u functions, %u rounds under control word %04X (%u with "
              "overlapping arguments, %u with an all-zero scratch, %u with colours a few bytes apart), %u MISMATCHES; "
              "every register block, the arguments and the results compared", n, kRounds, kGameControlWord,
              overlapping, zero_vectors, byte_overlaps, bad);
    if (bad) bof3::Fatal("the GTE's transforms differ from the originals in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void PsxGteTransform_Inject() {
    // Called BEFORE the modules that own what these call, because a function
    // can only be cloned before its entry is patched.
    if (bof3::WantsShadow("psx_gte_transform")) {
        const void* callee[kCallees];
        callee[kApply] = bof3::CloneOriginal("Gte_ApplyMatrix", bof3::addr::Gte_ApplyMatrix, 0x7B);
        callee[kApplyLV] = bof3::CloneOriginal("Gte_ApplyMatrixLV", bof3::addr::Gte_ApplyMatrixLV, 0x78);
        callee[kNormal] = nullptr;   // filled in below, once Gte_VectorNormal is cloned
        callee[kPerspective] = bof3::CloneOriginal("Gte_Perspective", bof3::addr::Gte_Perspective, 0x111);
        const bof3::CloneCall ftol{0x3B, nullptr};
        callee[kRamp] = bof3::CloneOriginal("Gte_DepthRamp", bof3::addr::Gte_DepthRamp, 0x40, &ftol, 1);
        callee[kSin] = bof3::CloneOriginal("Math_Sin", bof3::addr::Math_Sin, 0x42);
        callee[kFtol] = nullptr;   // the C runtime's, never ours: where the original called

        // Offsets and targets of every call: capstone over each function, 2026-09-20.
        // No jump leaves any of them.
        static Entry entries[] = {
            {"Gte_Rtps", bof3::addr::Gte_Rtps, 0xC6, AsFn(&Gte_Rtps), false, false, 3,
             {{0xF, kApply}, {0xAC, kPerspective}, {0xB8, kRamp}}, nullptr},
            {"Gte_Rtpt", bof3::addr::Gte_Rtpt, 0x145, AsFn(&Gte_Rtpt), false, false, 7,
             {{0x19, kApply}, {0x66, kPerspective}, {0x7A, kApply}, {0xC7, kPerspective}, {0xDB, kApply},
              {0x128, kPerspective}, {0x137, kRamp}}, nullptr},
            {"Gte_RotTransPers", bof3::addr::Gte_RotTransPers, 0xE4, AsFn(&Gte_RotTransPers), false, true, 3,
             {{0xF, kApply}, {0xAC, kPerspective}, {0xB8, kRamp}}, nullptr},
            {"Gte_RotTransPers3", bof3::addr::Gte_RotTransPers3, 0x14B, AsFn(&Gte_RotTransPers3), false, true, 7,
             {{0x19, kApply}, {0x63, kPerspective}, {0x77, kApply}, {0xC3, kPerspective}, {0xD7, kApply},
              {0x125, kPerspective}, {0x134, kRamp}}, nullptr},
            {"Gte_RotTransPers4", bof3::addr::Gte_RotTransPers4, 0x1A7, AsFn(&Gte_RotTransPers4), false, true, 9,
             {{0xF, kApply}, {0x5B, kPerspective}, {0x6F, kApply}, {0xBD, kPerspective}, {0xD1, kApply},
              {0x11F, kPerspective}, {0x136, kApply}, {0x184, kPerspective}, {0x190, kRamp}}, nullptr},
            {"Gte_RotAverage3", bof3::addr::Gte_RotAverage3, 0x1A6, AsFn(&Gte_RotAverage3), false, true, 7,
             {{0x1C, kApply}, {0x6A, kPerspective}, {0x7E, kApply}, {0xCC, kPerspective}, {0xE0, kApply},
              {0x12C, kPerspective}, {0x168, kRamp}}, nullptr},
            {"Gte_RotAverage4", bof3::addr::Gte_RotAverage4, 0x208, AsFn(&Gte_RotAverage4), false, true, 9,
             {{0x13, kApply}, {0x61, kPerspective}, {0x75, kApply}, {0xC3, kPerspective}, {0xD7, kApply},
              {0x125, kPerspective}, {0x13C, kApply}, {0x188, kPerspective}, {0x1D0, kRamp}}, nullptr},
            {"Gte_VectorNormal", bof3::addr::Gte_VectorNormal, 0x9C, AsFn(&Gte_VectorNormal), false, true, 4,
             {{0x59, kFtol}, {0x72, kFtol}, {0x86, kFtol}, {0x92, kFtol}}, nullptr},
            {"Gte_VectorNormalS", bof3::addr::Gte_VectorNormalS, 0x9F, AsFn(&Gte_VectorNormalS), false, true, 4,
             {{0x59, kFtol}, {0x73, kFtol}, {0x88, kFtol}, {0x95, kFtol}}, nullptr},
            {"Gte_ScaleMatrix", bof3::addr::Gte_ScaleMatrix, 0x8B, AsFn(&Gte_ScaleMatrix), false, true, 0, {}, nullptr},
            {"Math_Cos", bof3::addr::Math_Cos, 0x13, AsFn(&Math_Cos), true, true, 1, {{0xA, kSin}}, nullptr},
            {"Gte_NormalColor", bof3::addr::Gte_NormalColor, 0xFA, AsFn(&Gte_NormalColor), false, false, 7,
             {{0x14, kApply}, {0x23, kNormal}, {0x37, kApplyLV}, {0x46, kNormal}, {0x9B, kFtol}, {0xBD, kFtol},
              {0xDC, kFtol}}, nullptr},
            {"Gte_SetColorMatrix", bof3::addr::Gte_SetColorMatrix, 0x15, AsFn(&Gte_SetColorMatrix), false, false, 0, {},
             nullptr},
            {"Gte_SetBackColor", bof3::addr::Gte_SetBackColor, 0x2A, AsFn(&Gte_SetBackColor), true, false, 0, {}, nullptr},
        };
        for (auto& e : entries) {
            bof3::CloneCall calls[9];
            for (unsigned i = 0; i < e.n_calls; ++i) calls[i] = {e.calls[i].offset, callee[e.calls[i].callee]};
            e.theirs = reinterpret_cast<Fn>(
                bof3::CloneOriginal(e.name, e.original, e.size, calls, static_cast<int>(e.n_calls)));
            if (e.original == bof3::addr::Gte_VectorNormal) callee[kNormal] = reinterpret_cast<const void*>(e.theirs);
        }
        SelfTest(entries, sizeof entries / sizeof entries[0]);
    }
}
