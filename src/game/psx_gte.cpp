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

namespace psx_gte {
// DIV-0178 (docs/issue-121-lift-stall.md 8): 1 through
// PatchBytes("ApplyMatrixLVExact") in PsxGte_Inject, after the fuzz, which
// runs Capcom's wrapping products. While 0 Gte_ApplyMatrixLV is the port's;
// set, its products are exact, as the PlayStation's libgte computes them.
std::uint32_t g_lv_exact = 0;
}  // namespace psx_gte

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
    {
        // DIV-0178: armed after the fuzz above, which sees Capcom's wrapping products.
        const std::uint8_t was[] = {0, 0, 0, 0}, is[] = {1, 0, 0, 0};
        bof3::PatchBytes("ApplyMatrixLVExact",
                         Low32(&psx_gte::g_lv_exact), was, is, 4);
        bof3::Log("DIV-0178    Gte_ApplyMatrixLV's products are exact, as the PlayStation's libgte computes them (on unless the line above says OFF)");
    }
}
