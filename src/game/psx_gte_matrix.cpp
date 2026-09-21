#include "game/psx_gte_matrix.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The matrix product and what is built on it: libgte's MulMatrix0, the three
// one-axis rotations, RotMatrix and its Y-X-Z twin, and the load of an
// object's matrix composed with the camera's. Integer throughout.
//
// One divergence, DIV-0021: the original product builds its nine results on
// the stack and copies FIVE dwords out, so bytes 18 and 19 of every out - a
// MATRIX's alignment hole - receive stale stack. Ours writes zeros there,
// which is what the original leaves three calls in four
// (docs/psx-library-layer.md section 4).

// original 0x5A7D70: libgte MulMatrix0 by shape - out = a * b, each element
// the three products summed in 32 bits and >> 12. Every element of a and b is
// read before anything is stored, so out may be either of them.
extern "C" short* __cdecl Gte_MulMatrix0(const short* a, const short* b, short* out) {
    short result[10];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            const std::uint32_t sum = static_cast<std::uint32_t>(a[3 * r] * b[c]) +
                                      static_cast<std::uint32_t>(a[3 * r + 1] * b[3 + c]) +
                                      static_cast<std::uint32_t>(a[3 * r + 2] * b[6 + c]);
            result[3 * r + c] = static_cast<short>(static_cast<std::int32_t>(sum) >> 12);
        }
    result[9] = 0;   // DIV-0021: the original's is whatever its stack held
    std::memcpy(out, result, 20);
    return out;
}

namespace {

// The rotations premultiply: matrix = R * matrix. The original's R is a stack
// matrix whose padding it never writes; the product never reads it.
short* Premultiply(short r0, short r1, short r2, short r3, short r4, short r5, short r6, short r7, short r8,
                   short* matrix) {
    const short r[9] = {r0, r1, r2, r3, r4, r5, r6, r7, r8};
    Gte_MulMatrix0(r, matrix, matrix);
    return matrix;
}

constexpr short kOne = 0x1000;

}  // namespace

// originals 0x5A7F10, 0x5A7F80, 0x5A7FF0: libgte RotMatrixX, Y and Z by
// shape. The cosine is taken first, then the sine, as the original.
extern "C" short* __cdecl Gte_RotMatrixX(int angle, short* matrix) {
    const short c = static_cast<short>(Math_Cos(angle));
    const short s = static_cast<short>(Math_Sin(angle));
    return Premultiply(kOne, 0, 0, 0, c, static_cast<short>(-s), 0, s, c, matrix);
}
extern "C" short* __cdecl Gte_RotMatrixY(int angle, short* matrix) {
    const short c = static_cast<short>(Math_Cos(angle));
    const short s = static_cast<short>(Math_Sin(angle));
    return Premultiply(c, 0, s, 0, kOne, 0, static_cast<short>(-s), 0, c, matrix);
}
extern "C" short* __cdecl Gte_RotMatrixZ(int angle, short* matrix) {
    const short c = static_cast<short>(Math_Cos(angle));
    const short s = static_cast<short>(Math_Sin(angle));
    return Premultiply(c, static_cast<short>(-s), 0, s, c, 0, 0, 0, kOne, matrix);
}

// originals 0x5A8060, 0x5A80B0: libgte RotMatrix - Rx Ry Rz - and the twin
// that makes Ry Rx Rz. The rotation part and its padding start as the
// identity; the translation is left alone. Each angle is read just before its
// rotation, after the matrix has been written, as the original reads them.
extern "C" short* __cdecl Gte_RotMatrix(const short* angles, short* matrix) {
    std::memcpy(matrix, Gte_IdentityRotation, 20);
    Gte_RotMatrixZ(angles[2], matrix);
    Gte_RotMatrixY(angles[1], matrix);
    Gte_RotMatrixX(angles[0], matrix);
    return matrix;
}
extern "C" short* __cdecl Gte_RotMatrixYXZ(const short* angles, short* matrix) {
    std::memcpy(matrix, Gte_IdentityRotation, 20);
    Gte_RotMatrixZ(angles[2], matrix);
    Gte_RotMatrixX(angles[0], matrix);
    Gte_RotMatrixY(angles[1], matrix);
    return matrix;
}

// original 0x57C070: an object's rotation composed with the camera's, in
// place, then the whole matrix - that rotation, the object's own translation -
// loaded into the GTE.
extern "C" void __cdecl Camera_LoadMatrix(short* matrix) {
    Gte_MulMatrix0(Camera_Matrix, matrix, matrix);
    Gte_SetRotMatrix(reinterpret_cast<const unsigned long*>(matrix));
    Gte_SetTransMatrix(reinterpret_cast<const unsigned long*>(matrix));
}

namespace {

// --- BOF3X_SHADOW=psx_gte_matrix: a differential fuzz, once at start-up -------
// Byte-copies of the originals, their calls re-aimed at byte-copies of what
// they call (never at ours). One round: a scratch area of arguments, the
// camera's matrix and Gte_Matrix randomised; theirs, then from the same state
// ours; all of it compared - except the two padding bytes DIV-0021 changes,
// which are compared separately: ours must be zero there.

struct State {
    unsigned char scratch[96];
    short camera[9];
    unsigned long gte[8];
};

void Capture(State& s, const unsigned char* scratch) {
    std::memcpy(s.scratch, scratch, sizeof s.scratch);
    std::memcpy(s.camera, Camera_Matrix, sizeof s.camera);
    std::memcpy(s.gte, Gte_Matrix, sizeof s.gte);
}
void Apply(const State& s, unsigned char* scratch) {
    std::memcpy(scratch, s.scratch, sizeof s.scratch);
    std::memcpy(Camera_Matrix, s.camera, sizeof s.camera);
    std::memcpy(Gte_Matrix, s.gte, sizeof s.gte);
}

using Fn = void* (__cdecl*)(void*, void*, void*);
template <class F> Fn AsFn(F* f) { return reinterpret_cast<Fn>(reinterpret_cast<void*>(f)); }

enum Shape { kProduct, kRotate, kCompose, kLoad };

struct Entry {
    const char* name;
    std::uint32_t original, size;
    Fn ours;
    Shape shape;
    unsigned n_calls;
    struct { std::uint32_t offset; unsigned callee; } calls[3];
    Fn theirs;
};

void SelfTest(const Entry* entries, unsigned n) {
    constexpr unsigned kRounds = 30000;
    static State saved, input, their_out, our_out;
    alignas(4) static unsigned char scratch[96];
    Capture(saved, scratch);

    std::uint32_t rng = 0x5A7D7021u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    // An even offset into the scratch with room for `bytes` after it.
    auto cell = [&next](unsigned bytes) { return next() % ((96 - bytes) / 2 + 1) * 2; };
    unsigned bad = 0, pad_nonzero = 0, stale = 0, overlapping = 0, skipped = 0, run = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const Entry& e = entries[round % n];
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < sizeof input; ++i) bytes[i] = static_cast<unsigned char>(next());
        // Half the time the matrices hold rotation-sized values, not noise.
        if (next() % 2 == 0) {
            for (unsigned i = 0; i + 1 < sizeof input.scratch; i += 2) {
                const short v = static_cast<short>(static_cast<int>(next() % 0x2001) - 0x1000);
                std::memcpy(input.scratch + i, &v, 2);
            }
            for (short& v : input.camera) v = static_cast<short>(static_cast<int>(next() % 0x2001) - 0x1000);
        }

        // Argument cells. One round in four they overlap.
        const bool overlap = next() % 4 == 0;
        unsigned at[3] = {0, 24, 48};
        std::uintptr_t value = 0;
        std::uint32_t pad_at = 0;   // offset of the out's padding in the scratch
        switch (e.shape) {
        case kProduct:
            if (overlap) { at[0] = cell(18); at[1] = cell(18); at[2] = cell(20); }
            pad_at = at[2] + 18;
            break;
        case kRotate:
            value = next() % 2 ? next() : static_cast<std::uint32_t>(static_cast<int>(next() % 0x4001) - 0x2000);
            if (overlap) at[1] = cell(20);
            pad_at = at[1] + 18;
            break;
        case kCompose:
            if (overlap) { at[0] = cell(6); at[1] = cell(20); }
            pad_at = at[1] + 18;
            // An angle stored in the matrix's padding would be read after
            // the first rotation has written it: stale stack against zero.
            // That is DIV-0021 itself, not a difference to find.
            if (at[0] < pad_at + 2 && pad_at < at[0] + 6) { ++skipped; continue; }
            break;
        case kLoad:
            if (overlap) at[0] = cell(32);
            pad_at = at[0] + 18;
            break;
        }
        if (overlap) ++overlapping;
        ++run;

        void* result[2] = {nullptr, nullptr};
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, scratch);
            void* a[3] = {scratch + at[0], scratch + at[1], scratch + at[2]};
            if (e.shape == kRotate) a[0] = reinterpret_cast<void*>(value);
            result[pass] = (pass ? e.ours : e.theirs)(a[0], a[1], a[2]);
            Capture(pass ? our_out : their_out, scratch);
        }
        // DIV-0021: ours zero in the out's padding, and in Gte_Matrix's when
        // the out went there; theirs is taken for the comparison of the rest.
        auto* our_gte = reinterpret_cast<unsigned char*>(our_out.gte);
        auto* their_gte = reinterpret_cast<unsigned char*>(their_out.gte);
        bool pad_ok = our_out.scratch[pad_at] == 0 && our_out.scratch[pad_at + 1] == 0;
        if (their_out.scratch[pad_at] != 0 || their_out.scratch[pad_at + 1] != 0) ++stale;
        std::memcpy(our_out.scratch + pad_at, their_out.scratch + pad_at, 2);
        if (e.shape == kLoad) {
            pad_ok = pad_ok && our_gte[18] == 0 && our_gte[19] == 0;
            std::memcpy(our_gte + 18, their_gte + 18, 2);
        }
        if (!pad_ok) ++pad_nonzero;
        const bool same = pad_ok && std::memcmp(&their_out, &our_out, sizeof their_out) == 0 &&
                          (e.shape == kLoad || result[0] == result[1]);
        if (!same && ++bad <= 8)
            bof3::Log("shadow      psx_gte_matrix self-test MISMATCH: %s, round %u, cells %u %u %u, value %08lX, "
                      "padding %s", e.name, round, at[0], at[1], at[2], static_cast<unsigned long>(value),
                      pad_ok ? "zero" : "NOT ZERO");
    }
    Apply(saved, scratch);
    bof3::Log("shadow      psx_gte_matrix self-test: %u functions, %u rounds (%u with overlapping arguments, %u "
              "skipped: an angle in the padding), the original left non-zero padding in %u, ours non-zero in %u; "
              "%u MISMATCHES; the scratch, the camera's matrix, Gte_Matrix and the results compared",
              n, run, overlapping, skipped, stale, pad_nonzero, bad);
    if (bad) bof3::Fatal("the matrix functions differ from the originals in %u of %u self-test rounds", bad, run);
}

}  // namespace

void PsxGteMatrix_Inject() {
    // Called BEFORE the modules that own what these call (Math_Sin, Math_Cos,
    // Gte_SetRotMatrix, Gte_SetTransMatrix), because a function can only be
    // cloned before its entry is patched.
    if (bof3::WantsShadow("psx_gte_matrix")) {
        enum { kSin, kCos, kProduct_, kRotX, kRotY, kRotZ, kSetRot, kSetTrans, kCallees };
        const void* callee[kCallees] = {};
        callee[kSin] = bof3::CloneOriginal("Math_Sin", bof3::addr::Math_Sin, 0x42);
        const bof3::CloneCall to_sin{0xA, callee[kSin]};
        callee[kCos] = bof3::CloneOriginal("Math_Cos", bof3::addr::Math_Cos, 0x13, &to_sin, 1);
        callee[kSetRot] = bof3::CloneOriginal("Gte_SetRotMatrix", bof3::addr::Gte_SetRotMatrix, 0x15);
        callee[kSetTrans] = bof3::CloneOriginal("Gte_SetTransMatrix", bof3::addr::Gte_SetTransMatrix, 0x21);

        // Offsets and targets of every call: capstone over each function,
        // 2026-09-21. No jump leaves any of them. Ordered so that each
        // callee is cloned before its callers.
        static Entry entries[] = {
            {"Gte_MulMatrix0", bof3::addr::Gte_MulMatrix0, 0x193, AsFn(&Gte_MulMatrix0), kProduct, 0, {}, nullptr},
            {"Gte_RotMatrixX", bof3::addr::Gte_RotMatrixX, 0x67, AsFn(&Gte_RotMatrixX), kRotate, 3,
             {{0xA, kCos}, {0x12, kSin}, {0x57, kProduct_}}, nullptr},
            {"Gte_RotMatrixY", bof3::addr::Gte_RotMatrixY, 0x65, AsFn(&Gte_RotMatrixY), kRotate, 3,
             {{0xA, kCos}, {0x12, kSin}, {0x55, kProduct_}}, nullptr},
            {"Gte_RotMatrixZ", bof3::addr::Gte_RotMatrixZ, 0x67, AsFn(&Gte_RotMatrixZ), kRotate, 3,
             {{0xA, kCos}, {0x12, kSin}, {0x57, kProduct_}}, nullptr},
            {"Gte_RotMatrix", bof3::addr::Gte_RotMatrix, 0x42, AsFn(&Gte_RotMatrix), kCompose, 3,
             {{0x1F, kRotZ}, {0x2A, kRotY}, {0x34, kRotX}}, nullptr},
            {"Gte_RotMatrixYXZ", bof3::addr::Gte_RotMatrixYXZ, 0x42, AsFn(&Gte_RotMatrixYXZ), kCompose, 3,
             {{0x1F, kRotZ}, {0x29, kRotX}, {0x34, kRotY}}, nullptr},
            {"Camera_LoadMatrix", bof3::addr::Camera_LoadMatrix, 0x22, AsFn(&Camera_LoadMatrix), kLoad, 3,
             {{0xC, kProduct_}, {0x12, kSetRot}, {0x18, kSetTrans}}, nullptr},
        };
        for (auto& e : entries) {
            bof3::CloneCall calls[3];
            for (unsigned i = 0; i < e.n_calls; ++i) calls[i] = {e.calls[i].offset, callee[e.calls[i].callee]};
            e.theirs = reinterpret_cast<Fn>(
                bof3::CloneOriginal(e.name, e.original, e.size, calls, static_cast<int>(e.n_calls)));
            if (e.original == bof3::addr::Gte_MulMatrix0) callee[kProduct_] = reinterpret_cast<const void*>(e.theirs);
            if (e.original == bof3::addr::Gte_RotMatrixX) callee[kRotX] = reinterpret_cast<const void*>(e.theirs);
            if (e.original == bof3::addr::Gte_RotMatrixY) callee[kRotY] = reinterpret_cast<const void*>(e.theirs);
            if (e.original == bof3::addr::Gte_RotMatrixZ) callee[kRotZ] = reinterpret_cast<const void*>(e.theirs);
        }
        SelfTest(entries, sizeof entries / sizeof entries[0]);
    }
    BOF3_INJECT(Gte_MulMatrix0);
    BOF3_INJECT(Gte_RotMatrixX);
    BOF3_INJECT(Gte_RotMatrixY);
    BOF3_INJECT(Gte_RotMatrixZ);
    BOF3_INJECT(Gte_RotMatrix);
    BOF3_INJECT(Gte_RotMatrixYXZ);
    BOF3_INJECT(Camera_LoadMatrix);
}
