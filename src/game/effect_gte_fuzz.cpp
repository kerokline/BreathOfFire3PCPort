// BOF3X_SHADOW=effect_gte: group EGT's four GTE helpers through the scenario
// harness (scenario_harness.h, used unchanged) as kCall helpers, once at
// start-up. docs/effect_gte.md section 3. BOF3X_EGT_ONLY=<name> runs the clones
// whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group EGT --clones (2026-09-29), each
// extent read again to its last instruction (capstone). Every GTE callee is a
// recorder of the group's own that LOGS what it is handed - the stack vectors
// and matrices by content (deref), the pointers into the harness's scratch by
// value, the stack pointers not at all - and then, in its effect, CALLS THE
// REAL FUNCTION (ours, proven against Capcom's by the psx_gte fuzzes) on those
// arguments and answers what it answers. So a wrong vector shows in the log,
// what it leads to shows in the GTE registers and the outs (regions), and the
// disturbance between the calls tests when each cell is read.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_gte.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_gte {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;

// tools/band_rows.py --group EGT --clones, 2026-09-29.
constexpr sh::CallSite kCalls494060[] = {{0xD, 0x5A8060}, {0x47, 0x5A7BF0}, {0x8E, 0x5A8060}, {0x98, 0x5A8DE0}, {0xA2, 0x5A8E00}};
constexpr sh::CallSite kCalls494110[] = {{0x4F, 0x5A8250}, {0x58, 0x5A9110}};
constexpr sh::CallSite kCalls4941E0[] = {{0x4B, 0x5A8200}};

#define EGT_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define EGT_CALLS(a) a, EGT_N(a)
#define EGT_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kScratch0 = sh::ArgAt(0, sh::Arg::kScratch), kScratch1 = sh::ArgAt(1, sh::Arg::kScratch),
            kScratch2 = sh::ArgAt(2, sh::Arg::kScratch);
// Each answers in eax, all 32 bits (the original's ret leaves a defined value:
// docs/effect_gte.md section 2), so each ret_mask is the whole register.
const sh::Clone kAll[] = {
    {"EffectGte_LoadMapCamera", 0x494060, 0xAB, EGT_CALLS(kCalls494060), nullptr, 0, nullptr, 0,
     EGT_FN(EffectGte_LoadMapCamera), 0xFFFFFFFFu, false, kCa, 0},
    {"EffectGte_ProjectPoint", 0x494110, 0x65, EGT_CALLS(kCalls494110), nullptr, 0, nullptr, 0,
     EGT_FN(EffectGte_ProjectPoint), 0xFFFFFFFFu, false, kCa, kScratch0 | kScratch1},
    {"EffectGte_SetDiagonalOne", 0x494180, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, EGT_FN(EffectGte_SetDiagonalOne),
     0xFFFFFFFFu, false, kCa, kScratch0},
    {"EffectGte_ProjectSize", 0x4941E0, 0x91, EGT_CALLS(kCalls4941E0), nullptr, 0, nullptr, 0,
     EGT_FN(EffectGte_ProjectSize), 0xFFFFFFFFu, false, kCa, kScratch0 | kScratch1 | kScratch2},
};
#undef EGT_FN
#undef EGT_CALLS
#undef EGT_N

enum : unsigned { kCamera, kPoint, kDiagonal, kSize, kCount };
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename T> T* P(U a) { return reinterpret_cast<T*>(static_cast<std::uintptr_t>(a)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the effects: the real GTE function on what the recorder was handed ---------------
//
// Deterministic functions of the state and their arguments, so both passes get
// the same; none draws from the harness's streams.

U FxRotMatrix(const U* a, U) { return Key(::Gte_RotMatrix(P<const short>(a[0]), P<short>(a[1]))); }
U FxApplyMatrix(const U* a, U answer) {
    ::Gte_ApplyMatrix(P<const short>(a[0]), P<const short>(a[1]), P<long>(a[2]));
    return answer;
}
U FxSetRotMatrix(const U* a, U answer) {
    ::Gte_SetRotMatrix(P<const unsigned long>(a[0]));
    return answer;
}
// Capcom's 0x5A8E00 leaves eax the translation's z, its last load; the callers'
// own eax at their ret is that (0x494060), so the stand-in answers it.
U FxSetTransMatrix(const U* a, U) {
    ::Gte_SetTransMatrix(P<const unsigned long>(a[0]));
    return P<const U>(a[0])[7];
}
U FxRotTransPers(const U* a, U) {
    return static_cast<U>(::Gte_RotTransPers(P<const short>(a[0]), P<unsigned long>(a[1]), P<long>(a[2])));
}
// Capcom's 0x5A9110 leaves eax its argument (0x494110 answers it).
U FxStoreDepthF(const U* a, U) {
    ::Gte_StoreDepthF(P<float>(a[0]));
    return a[0];
}
U FxRotTrans(const U* a, U answer) {
    ::Gte_RotTrans(P<const short>(a[0]), P<long>(a[1]));
    return answer;
}

// Masks: a pointer into the caller's stack differs between the passes (0: not
// logged); what it points at is logged by content where the callee reads it
// (deref: 6 bytes of an SVECTOR - its fourth word is never written by either
// side -, 20 of a MATRIX's rotation and padding, 32 of a whole MATRIX once its
// translation is written). Camera_Angles and the scratch outs are the same
// address on both passes: logged by value.
#define EGT_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // (angles, matrix): the angles logged by value and content, the matrix unlogged (not yet written)
    {EGT_OURS(Gte_RotMatrix), 2, {kW, 0}, kG, 0, 0, {6, 0}, &FxRotMatrix},
    // (matrix, vector, out): the rotation and the vector by content
    {EGT_OURS(Gte_ApplyMatrix), 3, {0, 0, 0}, kG, 0, 0, {20, 6, 0}, &FxApplyMatrix},
    {EGT_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {20}, &FxSetRotMatrix},
    {EGT_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {32}, &FxSetTransMatrix},
    // (vector, sxy, p, flag): the original pushes four; p and flag are its own argument slots
    {EGT_OURS(Gte_RotTransPers), 4, {0, kW, 0, 0}, kG, 0, 0, {6, 0, 0, 0}, &FxRotTransPers},
    {EGT_OURS(Gte_StoreDepthF), 1, {kW}, kG, 0, 0, {}, &FxStoreDepthF},
    // (vector, out, flag): three pushed
    {EGT_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6, 0, 0}, &FxRotTrans},
};
#undef EGT_OURS

// Beyond field mode's standard regions (which hold Camera_Angles, the view
// focus and Camera_Distance): the GTE's registers the callees read and write -
// 0x7DE420..0x7DE500 (the far ramp, the colour matrix, the back colour, the
// depth-stack word, the vertices, the view-space vertex, the near plane, the
// rotation / translation matrix, the screen FIFO, the second matrix) and
// 0x7DE780..0x7DE7A8 (the projection distance, Gte_Otz, the offsets, the depth
// FIFO and the newest depth) - and Camera_ShiftX / Camera_ShiftY.
const sh::Region kRegions[] = {
    {0x7DE420, 0xE0},
    {0x7DE780, 0x28},
    {0x903800, 4},
};

// --- the seed -------------------------------------------------------------------------

std::int32_t Signed(U v) { return static_cast<std::int32_t>(v); }

// A dword of the world point: random, or near where the shift-and-subtract
// crosses 0, -1, the s16 limits, or the dword's own limits.
U Coordinate() {
    return PickOf(sh::Next(), sh::Next() % 0x2000000u, 0, 0xFFFFFFFFu, 0x7FFFFFFFu, 0x80000000u,
                  0x800000u, 0x7FFE00u, 0x8001FFu, 0x7FFFFFu, 0x1000000u, 0x17FFE00u, 0x1800000u, 0x18001FFu,
                  0xFFFFFE00u, 0x1FFu, 0x200u, 0x10800000u, 0xFF800000u, 0x800000u + (sh::Next() % 0x20000u) - 0x10000u);
}
// The height dword: its integer part (>> 16) odd and negative (where halving
// toward zero and a shift differ), even, 0, -1, the limits, and random.
U Height() {
    return PickOf(sh::Next(), 0, 0xFFFFFFFFu, 0xFFFF0000u, 0xFFFE0000u, 0xFFFD0000u, 0xFFFDFFFFu, 0x10000u, 0x30000u,
                  0x2FFFFu, 0xFFFFu, 0x7FFFFFFFu, 0x80000000u, 0x80010000u, 0x7FFF0000u, (sh::Next() % 0x400u) << 16,
                  (0u - (sh::Next() % 0x400u)) << 16, sh::Next() | 0x80010000u);
}
U Word16() { return PickOf(sh::Next(), 0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x40, 0xFFC0, 0x1000, sh::Next() % 0x200); }

void Point(unsigned char* p) {
    U v[3] = {Coordinate(), Coordinate(), Height()};
    std::memcpy(p, v, sizeof v);
}

// The camera vector exactly as the originals build it - the fuzz's own copy,
// so a plant in ours cannot steer the seed.
void CameraVectorOf(const unsigned char* p, short* v) {
    std::int32_t d[3];
    std::memcpy(d, p, sizeof d);
    v[0] = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(d[0] >> 9) - 0x4000u));
    v[1] = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(d[1] >> 9) - 0x4000u));
    v[2] = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(-((d[2] >> 16) / 2))));
    v[3] = 0;
}

// The GTE state: a rotation of moderate entries or random words, a translation
// of numbers, the projection's registers plausible or random.
void Gte() {
    if (sh::Half()) {
        for (unsigned i = 0; i < 9; ++i) {
            const U w = PickOf(0x1000, 0, 0xF000, 0x0B50, 0xF4B0, sh::Next() % 0x2001u - 0x1000u, sh::Next());
            reinterpret_cast<short*>(Gte_Matrix)[i] = static_cast<short>(static_cast<std::uint16_t>(w));
        }
    }
    for (unsigned i = 5; i < 8; ++i)
        Gte_Matrix[i] = PickOf(sh::Next(), 0, 0x1000, 0xFFFFF000u, sh::Next() % 0x20000u - 0x10000u, 0x7FFFFFFFu, 0x80000000u);
    if (sh::Half()) {
        Gte_NearZ = Signed(PickOf(0x10, 1, 0x100, sh::Next() % 0x1000u));
        Gte_ProjDistance = Signed(PickOf(0x200, 0x100, 0x400, sh::Next() % 0x1000u));
        Gte_OffsetX = Signed(PickOf(0xA0, 0, 0x140, sh::Next() % 0x400u));
        Gte_OffsetY = Signed(PickOf(0x78, 0, 0xF0, sh::Next() % 0x400u));
        Gte_RampNear = Signed(PickOf(0x100, 0, sh::Next() % 0x4000u));
        Gte_RampFar = Signed(PickOf(0x4000, 0x8000, sh::Next() % 0x10000u));
    }
}

// The depth the size is divided by: never 0 (the original's fault, ours' abort),
// half the time steered onto a boundary - 1, -1, small, the size's own scale,
// where the quotient overflows 16 bits, and large.
void SteerDepth(const unsigned char* point) {
    short v[4];
    CameraVectorOf(point, v);
    long turned[3];
    ::Gte_ApplyMatrix(reinterpret_cast<const short*>(Gte_Matrix), v, turned);
    if (sh::Half()) {
        const U want = PickOf(1, 0xFFFFFFFFu, 2, 0xFFFFFFFEu, 7, 1000, 0xFFFFFC18u, 0x3E8u * 0x40u, 0x7FFF, 0x10000,
                              0x7FFFFFFFu, 0x80000000u, 0x80000001u, sh::Next() % 0x800u + 1);
        Gte_Matrix[7] = want - static_cast<U>(turned[2]);
    }
    if (static_cast<U>(turned[2]) + Gte_Matrix[7] == 0) Gte_Matrix[7] += 1;
}

void Seed(unsigned k) {
    Gte();
    switch (k) {
    case kCamera: {
        for (unsigned i = 0; i < 3; ++i)
            Camera_Angles[i] = static_cast<short>(PickOf(0, 0x400, 0x800, 0xC00, 0xFFF, 0x1000, 0xF800, sh::Next()));
        MapView_FocusX = Signed(PickOf(sh::Next(), 0, 1, 2, 0xFFFF, 0x10000, 0x1FFFE, 0x1FFFF, 0x7FFFFFFF, 0x80000000u,
                                       0xFFFFFFFFu, sh::Next() % 0x100000u));
        MapView_FocusZ = Signed(PickOf(sh::Next(), 0, 1, 0x10000, 0x1FFFF, 0x80000000u, 0xFFFFFFFFu, sh::Next() % 0x100000u));
        MapView_Elevation = Signed(PickOf(sh::Next(), 0, 1, 0x10000, 0x1FFFF, 0xFFFFFFFFu, sh::Next() % 0x10000u));
        Camera_ShiftX = static_cast<short>(Word16());
        Camera_ShiftY = static_cast<short>(Word16());
        Camera_Distance = static_cast<short>(PickOf(0, 0x7FFF, 0x8000, 0xEE6C, 0xEE6B, 0xFFFF, 0x200, sh::Next()));
        break;
    }
    case kPoint: Point(sh::Scratch(0)); break;
    case kDiagonal: break;
    case kSize: {
        Point(sh::Scratch(0));
        for (unsigned i = 0; i < 4; ++i) {
            const auto w = static_cast<std::uint16_t>(Word16());
            std::memcpy(sh::Scratch(1) + 4 + 2 * i, &w, 2);
        }
        SteerDepth(sh::Scratch(0));
        break;
    }
    default: break;
    }
}

// The argument words, after the seed: the outs sometimes over the inputs.
void Args(unsigned k, U* a) {
    switch (k) {
    case kPoint:
        // out apart, out over the point itself, or a dword into it
        a[1] = Key(PickOf(0, 0, 1, 2) == 0 ? sh::Scratch(1) : sh::Scratch(0) + 4 * (sh::Next() % 2));
        break;
    case kDiagonal: a[0] = Key(sh::Scratch(0) + 2 * (sh::Next() % 8)); break;
    case kSize: {
        // size at Scratch(1) + 4; out apart, in place (as FC2's caller), a word either side
        const U size = Key(sh::Scratch(1) + 4);
        a[1] = size;
        a[2] = PickOf(Key(sh::Scratch(2)), size, size, size + 2, size - 2, Key(sh::Scratch(0)));
        break;
    }
    default: break;
    }
}

// What the functions read after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the camera's angles, shifts,
// distance and focus (0x494060 reads the focus after its first call, the shifts
// and distance after its second), the point and the size (read before and after
// the one call of 0x494110 / 0x4941E0).
void Disturb(U h) {
    const U v = h >> 8;
    switch (h % 8) {
    case 0: Camera_Angles[v % 3] = static_cast<short>(v >> 4); break;
    case 1: Camera_ShiftX = static_cast<short>(v); break;
    case 2: Camera_ShiftY = static_cast<short>(v); break;
    case 3: Camera_Distance = static_cast<short>(v); break;
    case 4:
        if (v & 1) MapView_FocusX = Signed(v << 4); else if (v & 2) MapView_FocusZ = Signed(v << 3); else MapView_Elevation = Signed(v << 2);
        break;
    case 5: {
        const U w = v << 6;
        std::memcpy(sh::Scratch(0) + 4 * (v % 3), &w, 4);
        break;
    }
    case 6: {
        const auto w = static_cast<std::uint16_t>(v >> 3);
        std::memcpy(sh::Scratch(1) + 4 + 2 * (v % 2), &w, 2);
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_EGT_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_EGT_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_gte: BOF3X_EGT_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_gte", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], nullptr, 0,
                   kRegions, sizeof kRegions / sizeof kRegions[0], [](unsigned k) { Seed(s_index[k]); }, &Disturb,
                   20000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace effect_gte
