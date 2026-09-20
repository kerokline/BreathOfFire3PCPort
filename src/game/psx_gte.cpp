#include "game/psx_gte.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr long kStackDepth = 20;
static_assert(Gte_MatrixStack_count == kStackDepth * Gte_Matrix_count);

// --- BOF3X_SHADOW=psx_gte: a differential fuzz, once at start-up ---------------
// Fifteen functions - fourteen leaves and one whose only call is to another of
// them - with jumps internal or none: byte-copies run in place
// against the same register globals. One round: randomise every register
// block the twelve touch and a scratch area of arguments, run one function -
// theirs, then from the same state ours - and compare all the blocks and the
// scratch. The blocks are put back afterwards.

struct State {
    unsigned long matrix[8], matrix2[8], stack[160], vertices[6], screen[6];
    long depth, otz;
    unsigned long scratch[16];
};

void Capture(State& s, const unsigned long* scratch) {
    std::memcpy(s.matrix, Gte_Matrix, sizeof s.matrix);
    std::memcpy(s.matrix2, Gte_Matrix2, sizeof s.matrix2);
    std::memcpy(s.stack, Gte_MatrixStack, sizeof s.stack);
    std::memcpy(s.vertices, Gte_Vertices, sizeof s.vertices);
    std::memcpy(s.screen, Gte_ScreenXY, sizeof s.screen);
    s.depth = Gte_MatrixDepth;
    s.otz = Gte_Depth;
    if (scratch) std::memcpy(s.scratch, scratch, sizeof s.scratch);
}
void Apply(const State& s, unsigned long* scratch) {
    std::memcpy(Gte_Matrix, s.matrix, sizeof s.matrix);
    std::memcpy(Gte_Matrix2, s.matrix2, sizeof s.matrix2);
    std::memcpy(Gte_MatrixStack, s.stack, sizeof s.stack);
    std::memcpy(Gte_Vertices, s.vertices, sizeof s.vertices);
    std::memcpy(Gte_ScreenXY, s.screen, sizeof s.screen);
    Gte_MatrixDepth = s.depth;
    Gte_Depth = s.otz;
    if (scratch) std::memcpy(scratch, s.scratch, sizeof s.scratch);
}

using Fn = void (__cdecl*)(void*, void*, void*);
struct Entry {
    const char* name;
    std::uint32_t original, size;
    Fn ours;
    Fn theirs;
    std::uint32_t call_at;   // offset of its one E8 that leaves, to Gte_ApplyMatrix; 0 for none
};

template <class F> Fn AsFn(F* f) { return reinterpret_cast<Fn>(reinterpret_cast<void*>(f)); }

void SelfTest(Entry* entries, unsigned n) {
    constexpr unsigned kRounds = 24000;
    static State saved, input, their_out, our_out;
    static unsigned long scratch[16];
    Capture(saved, nullptr);

    std::uint32_t rng = 0x1B873593u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    unsigned bad = 0, full = 0, empty = 0, aliased = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < sizeof input; ++i) bytes[i] = static_cast<unsigned char>(next());
        // depth: mostly in range, the edges often, out of range sometimes
        static const long kEdge[] = {0, 1, 19, 20, 21, -1};
        input.depth = next() % 3 == 0 ? kEdge[next() % 6] : static_cast<long>(next() % 20);
        if (input.depth >= kStackDepth) ++full;
        if (input.depth <= 0) ++empty;
        // Three argument cells inside the scratch, a quarter of the time
        // overlapping, which is where the order of reads and writes shows.
        unsigned at[3] = {0, 5, 10};
        if (next() % 4 == 0) { at[1] = next() % 3; at[2] = next() % 3; ++aliased; }
        const Entry& e = entries[round % n];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, scratch);
            (pass ? e.ours : e.theirs)(scratch + at[0], scratch + at[1], scratch + at[2]);
            Capture(pass ? our_out : their_out, scratch);
        }
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 8)
            bof3::Log("shadow      psx_gte self-test MISMATCH: %s, round %u, depth %ld", e.name, round, input.depth);
    }
    Apply(saved, nullptr);
    bof3::Log("shadow      psx_gte self-test: %u functions, %u rounds (%u with the stack full or past it, %u with it "
              "empty or below, %u with overlapping arguments), %u MISMATCHES; every register block and the "
              "arguments compared", n, kRounds, full, empty, aliased, bad);
    if (bad) bof3::Fatal("the GTE layer differs from the originals in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

// original 0x5A7BF0. out = (matrix . vector) >> 12: the rotation part of a PSX
// MATRIX, nine s16 at 4096 to the unit, times an SVECTOR. The most-called
// function of the attract run, 2,625,115 times.
//
// As the original has it: sums in 32 bits, wrapping; arithmetic shifts;
// everything read before anything is written, so out may be the vector.
extern "C" void __cdecl Gte_ApplyMatrix(const short* matrix, const short* vector, long* out) {
    const std::int32_t x = vector[0], y = vector[1], z = vector[2];
    std::int32_t row[3];
    for (int r = 0; r < 3; ++r) {
        const std::uint32_t sum = static_cast<std::uint32_t>(matrix[3 * r] * x) +
                                  static_cast<std::uint32_t>(matrix[3 * r + 1] * y) +
                                  static_cast<std::uint32_t>(matrix[3 * r + 2] * z);
        row[r] = static_cast<std::int32_t>(sum) >> 12;
    }
    out[0] = row[0];
    out[1] = row[1];
    out[2] = row[2];
}

// original 0x5A7CF0: the same with a VECTOR in - 32-bit components, so here the
// products wrap too. libgte ApplyMatrixLV.
extern "C" void __cdecl Gte_ApplyMatrixLV(const short* matrix, const long* vector, long* out) {
    const std::uint32_t x = static_cast<std::uint32_t>(vector[0]), y = static_cast<std::uint32_t>(vector[1]),
                        z = static_cast<std::uint32_t>(vector[2]);
    std::int32_t row[3];
    for (int r = 0; r < 3; ++r) {
        const std::uint32_t sum = static_cast<std::uint32_t>(static_cast<std::int32_t>(matrix[3 * r])) * x +
                                  static_cast<std::uint32_t>(static_cast<std::int32_t>(matrix[3 * r + 1])) * y +
                                  static_cast<std::uint32_t>(static_cast<std::int32_t>(matrix[3 * r + 2])) * z;
        row[r] = static_cast<std::int32_t>(sum) >> 12;
    }
    out[0] = row[0];
    out[1] = row[1];
    out[2] = row[2];
}

// original 0x5A81B0: libgte TransposeMatrix, one s16 at a time, each read and
// then stored, in this order - so that `out == in` gives what the original
// gives, which is not the transpose.
extern "C" void __cdecl Gte_TransposeMatrix(const short* in, short* out) {
    static const unsigned char kFrom[9] = {0, 3, 6, 1, 4, 7, 2, 5, 8};
    for (unsigned i = 0; i < 9; ++i) {
        const short v = in[kFrom[i]];
        out[i] = v;
    }
}

// original 0x5A8200: the current matrix applied to an SVECTOR, rotation and
// then translation. libgte RotTrans, without its flag argument.
extern "C" void __cdecl Gte_RotTrans(const short* vector, long* out) {
    Gte_ApplyMatrix(reinterpret_cast<const short*>(Gte_Matrix), vector, out);
    const long rotated[3] = {out[0], out[1], out[2]};
    out[0] = rotated[0] + static_cast<long>(Gte_Matrix[5]);
    out[1] = rotated[1] + static_cast<long>(Gte_Matrix[6]);
    out[2] = rotated[2] + static_cast<long>(Gte_Matrix[7]);
}

// originals 0x5A8DE0, 0x5A8E00, 0x5A8DA0: libgte SetRotMatrix and
// SetTransMatrix - the two halves of Gte_Matrix from a PSX MATRIX - and the
// same for the second matrix, whole.
extern "C" void __cdecl Gte_SetRotMatrix(const unsigned long* matrix) { std::memmove(Gte_Matrix, matrix, 5 * 4); }
extern "C" void __cdecl Gte_SetTransMatrix(const unsigned long* matrix) {
    Gte_Matrix[5] = matrix[5];
    Gte_Matrix[6] = matrix[6];
    Gte_Matrix[7] = matrix[7];
}
extern "C" void __cdecl Gte_SetMatrix2(const unsigned long* matrix) { std::memmove(Gte_Matrix2, matrix, 8 * 4); }

// original 0x5A8100: libgte TransMatrix - a MATRIX's translation from a VECTOR.
extern "C" void __cdecl Gte_TransMatrix(unsigned long* matrix, const unsigned long* vector) {
    matrix[5] = vector[0];
    matrix[6] = vector[1];
    matrix[7] = vector[2];
}

// originals 0x5A7B90, 0x5A7BC0: libgte PushMatrix and PopMatrix, with room for
// twenty where the PSX had one. A push onto a full stack and a pop from an
// empty one do nothing, silently.
extern "C" void __cdecl Gte_PushMatrix(void) {
    const long depth = Gte_MatrixDepth;
    if (depth >= kStackDepth) return;
    // A negative depth passes the signed test and indexes below the stack, as
    // the original's does.
    std::memcpy(reinterpret_cast<unsigned char*>(Gte_MatrixStack) + static_cast<std::uint32_t>(depth) * 0x20u,
                Gte_Matrix, 0x20);
    Gte_MatrixDepth = depth + 1;
}
extern "C" void __cdecl Gte_PopMatrix(void) {
    long depth = Gte_MatrixDepth;
    if (depth <= 0) return;
    Gte_MatrixDepth = --depth;
    std::memcpy(Gte_Matrix, reinterpret_cast<unsigned char*>(Gte_MatrixStack) + static_cast<std::uint32_t>(depth) * 0x20u,
                0x20);
}

// originals 0x5A8E30, 0x5A8E50. One vertex into the middle slot; or three,
// the first into the middle slot, the second into the first, the third into
// the last.
extern "C" void __cdecl Gte_LoadVertex(const unsigned long* vertex) {
    Gte_Vertices[2] = vertex[0];
    Gte_Vertices[3] = vertex[1];
}
extern "C" void __cdecl Gte_LoadVertices3(const unsigned long* vertices) {
    Gte_Vertices[2] = vertices[0];
    Gte_Vertices[3] = vertices[1];
    Gte_Vertices[0] = vertices[2];
    Gte_Vertices[1] = vertices[3];
    Gte_Vertices[4] = vertices[4];
    Gte_Vertices[5] = vertices[5];
}

// originals 0x5A90B0, 0x5A90D0: the newest screen point; or all three, oldest
// first, a dword at a time in this order - which matters if the outs overlap.
extern "C" void __cdecl Gte_StoreScreenXY(unsigned long* out) {
    out[0] = Gte_ScreenXY[4];
    out[1] = Gte_ScreenXY[5];
}
extern "C" void __cdecl Gte_StoreScreenXY3(unsigned long* out0, unsigned long* out1, unsigned long* out2) {
    out0[0] = Gte_ScreenXY[0];
    out0[1] = Gte_ScreenXY[1];
    out1[0] = Gte_ScreenXY[2];
    out1[1] = Gte_ScreenXY[3];
    out2[0] = Gte_ScreenXY[4];
    out2[1] = Gte_ScreenXY[5];
}

// original 0x5A94B0.
extern "C" void __cdecl Gte_StoreDepthQuarter(long* out) { *out = Gte_Depth >> 2; }

void PsxGte_Inject() {
    // Every one: no calls, jumps internal or none (disasm 2026-09-20).
    if (bof3::WantsShadow("psx_gte")) {
#define E(name, size) {#name, bof3::addr::name, size, AsFn(&name), nullptr, 0}
#define E_CALLS_APPLY(name, size, at) {#name, bof3::addr::name, size, AsFn(&name), nullptr, at}
        static Entry entries[] = {
            E(Gte_ApplyMatrix, 0x7B), E(Gte_SetRotMatrix, 0x15), E(Gte_SetTransMatrix, 0x21),
            E(Gte_SetMatrix2, 0x15), E(Gte_TransMatrix, 0x1A), E(Gte_PushMatrix, 0x2C),
            E(Gte_PopMatrix, 0x2B), E(Gte_LoadVertex, 0x16), E(Gte_LoadVertices3, 0x3A),
            E(Gte_StoreScreenXY, 0x16), E(Gte_StoreScreenXY3, 0x40), E(Gte_StoreDepthQuarter, 0xF),
            E(Gte_ApplyMatrixLV, 0x78), E(Gte_TransposeMatrix, 0x4F), E_CALLS_APPLY(Gte_RotTrans, 0x41, 0x10),
        };
#undef E
#undef E_CALLS_APPLY
        // entries[0] is Gte_ApplyMatrix: cloned first, so that the copy of a
        // function that calls it can be pointed at its copy and never at ours.
        for (auto& e : entries) {
            const bof3::CloneCall call{e.call_at, reinterpret_cast<const void*>(entries[0].theirs)};
            e.theirs = reinterpret_cast<Fn>(
                bof3::CloneOriginal(e.name, e.original, e.size, e.call_at ? &call : nullptr, e.call_at ? 1 : 0));
        }
        SelfTest(entries, sizeof entries / sizeof entries[0]);
    }
    BOF3_INJECT(Gte_ApplyMatrix);
    BOF3_INJECT(Gte_SetRotMatrix);
    BOF3_INJECT(Gte_SetTransMatrix);
    BOF3_INJECT(Gte_SetMatrix2);
    BOF3_INJECT(Gte_TransMatrix);
    BOF3_INJECT(Gte_PushMatrix);
    BOF3_INJECT(Gte_PopMatrix);
    BOF3_INJECT(Gte_LoadVertex);
    BOF3_INJECT(Gte_LoadVertices3);
    BOF3_INJECT(Gte_StoreScreenXY);
    BOF3_INJECT(Gte_StoreScreenXY3);
    BOF3_INJECT(Gte_StoreDepthQuarter);
    BOF3_INJECT(Gte_ApplyMatrixLV);
    BOF3_INJECT(Gte_TransposeMatrix);
    BOF3_INJECT(Gte_RotTrans);
}
