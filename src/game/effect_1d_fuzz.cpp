// BOF3X_SHADOW=effect_1d: group E1D's 30 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_1d.md section 4. BOF3X_E1D_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E1D --byte-tables --clones
// --harness scenario (2026-09-29), each extent read again to its last
// instruction (capstone) and the names given. Shapes: every dispatcher and
// state kEffect (Sprite_Current one of the 20 Effect_Objects records, +5 the
// kind, a dispatcher's +1 below its table's length); the two draw helpers kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1d.h"
#include "game/effect_1d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_1d {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E1D --clones, 2026-09-29.
constexpr sh::CallSite kCalls46F2F0[] = {{0xA, 0x46F570}, {0x10, 0x46F690}};
constexpr sh::CallSite kCalls46F340[] = {{0xA, 0x46F570}, {0x10, 0x46F690}};
constexpr sh::CallSite kCalls46F390[] = {{0xA, 0x46F570}, {0x10, 0x46F690}};
constexpr sh::CallSite kCalls46F3D0[] = {{0xA, 0x46F570}, {0x10, 0x46F690}};
constexpr sh::CallSite kCalls46F410[] = {{0xA, 0x46F570}, {0x10, 0x46F690}, {0x2F, 0x589840}};
constexpr sh::CallSite kCalls46F470[] = {{0x5, 0x587740}};
constexpr sh::CallSite kCalls46F490[] = {{0xD, 0x589810}};
constexpr sh::CallSite kCalls46F530[] = {{0x33, 0x589840}};
constexpr sh::CallSite kCalls46F7B0[] = {{0x16, 0x587740}};
constexpr sh::CallSite kCalls46F7D0[] = {{0xC, 0x589810}};
constexpr sh::CallSite kCalls46F840[] = {{0x1C, 0x5B93D2}, {0x2A, 0x5B93D2}, {0x43, 0x5B93D2}, {0x50, 0x5B93D2}};
constexpr sh::CallSite kCalls46F8B0[] = {{0xA, 0x46FAE0}};
constexpr sh::CallSite kCalls46F920[] = {{0x54, 0x5A8250}, {0x68, 0x5B9550}, {0x7D, 0x5B9550}};
constexpr sh::CallSite kCalls46F9D0[] = {{0x1C, 0x46FCF0}};
constexpr sh::CallSite kCalls46FA20[] = {{0x25, 0x46FCF0}, {0x3D, 0x587740}};
constexpr sh::CallSite kCalls46FA90[] = {{0x1C, 0x46FCF0}};
constexpr sh::CallSite kCalls46FCF0[] = {{0x13, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x71, 0x5A75F0}, {0x79, 0x5A7780},
                                         {0xA1, 0x5A7A50}, {0xC2, 0x5A7A00}, {0xE5, 0x5A7A50}, {0x102, 0x5A7A00}, {0x141, 0x461E50}};
constexpr sh::CallSite kCalls46FEA0[] = {{0x16, 0x46FFB0}};
constexpr sh::CallSite kCalls46FF20[] = {{0xC, 0x589810}, {0x58, 0x5720C0}};
constexpr sh::CallSite kCalls46FFB0[] = {
    {0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x3D, 0x494060}, {0xC3, 0x5A7610}, {0xCB, 0x5A7780},
    {0xDE, 0x5A7A50}, {0xE8, 0x5A7A00}, {0x10B, 0x5A7A00}, {0x115, 0x5A7A00}, {0x135, 0x5A7A50}, {0x153, 0x494110},
    {0x162, 0x5A7A00}, {0x170, 0x5A7A50}, {0x190, 0x5A7A00}, {0x19E, 0x5A7A00}, {0x1BD, 0x5A7A50}, {0x1DB, 0x494110},
    {0x20B, 0x5A7A50}, {0x215, 0x5A7A00}, {0x239, 0x5A7A00}, {0x243, 0x5A7A00}, {0x263, 0x5A7A50}, {0x281, 0x494110},
    {0x294, 0x5A7A50}, {0x29E, 0x5A7A00}, {0x2C1, 0x5A7A00}, {0x2CB, 0x5A7A00}, {0x2EB, 0x5A7A50}, {0x309, 0x494110},
    {0x31F, 0x461E50}};

#define E1D_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E1D_CALLS(a) a, E1D_N(a)
#define E1D_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind21_Run", 0x46F2B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1D_FN(EffectKind21_Run), 0, false, kEf, 0, 6, 0, 0x21},
    {"EffectKind21_Start", 0x46F2D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1D_FN(EffectKind21_Start), 0, false, kEf, 0, 0, 0, 0x21},
    {"EffectKind21_Grow", 0x46F2F0, 0x43, E1D_CALLS(kCalls46F2F0), nullptr, 0, nullptr, 0, E1D_FN(EffectKind21_Grow), 0, false, kEf, 0, 0, 0, 0x21},
    {"EffectKind21_Lift", 0x46F340, 0x44, E1D_CALLS(kCalls46F340), nullptr, 0, nullptr, 0, E1D_FN(EffectKind21_Lift), 0, false, kEf, 0, 0, 0, 0x21},
    {"EffectKind21_Hold", 0x46F390, 0x3F, E1D_CALLS(kCalls46F390), nullptr, 0, nullptr, 0, E1D_FN(EffectKind21_Hold), 0, false, kEf, 0, 0, 0, 0x21},
    {"EffectKind21_Swirl", 0x46F3D0, 0x40, E1D_CALLS(kCalls46F3D0), nullptr, 0, nullptr, 0, E1D_FN(EffectKind21_Swirl), 0, false, kEf, 0, 0, 0, 0x21},
    {"EffectKind21_Fold", 0x46F410, 0x35, E1D_CALLS(kCalls46F410), nullptr, 0, nullptr, 0, E1D_FN(EffectKind21_Fold), 0, false, kEf, 0, 0, 0, 0x21},
    {"EffectKind22_Run", 0x46F450, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1D_FN(EffectKind22_Run), 0, false, kEf, 0, 3, 0, 0x22},
    {"EffectKind22_Sound", 0x46F470, 0x16, E1D_CALLS(kCalls46F470), nullptr, 0, nullptr, 0, E1D_FN(EffectKind22_Sound), 0, false, kEf, 0, 0, 0, 0x22},
    {"EffectKind22_SpawnArms", 0x46F490, 0x99, E1D_CALLS(kCalls46F490), nullptr, 0, nullptr, 0, E1D_FN(EffectKind22_SpawnArms), 0, false, kEf, 0, 0, 0, 0x22},
    {"EffectKind22_WaitArms", 0x46F530, 0x3B, E1D_CALLS(kCalls46F530), nullptr, 0, nullptr, 0, E1D_FN(EffectKind22_WaitArms), 0, false, kEf, 0, 0, 0, 0x22},
    {"EffectKind23_Run", 0x46F790, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1D_FN(EffectKind23_Run), 0, false, kEf, 0, 3, 0, 0x23},
    {"EffectKind23_Start", 0x46F7B0, 0x1D, E1D_CALLS(kCalls46F7B0), nullptr, 0, nullptr, 0, E1D_FN(EffectKind23_Start), 0, false, kEf, 0, 0, 0, 0x23},
    {"EffectKind23_SpawnRays", 0x46F7D0, 0x4F, E1D_CALLS(kCalls46F7D0), nullptr, 0, nullptr, 0, E1D_FN(EffectKind23_SpawnRays), 0, false, kEf, 0, 0, 0, 0x23},
    {"EffectKind24_Run", 0x46F820, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1D_FN(EffectKind24_Run), 0, false, kEf, 0, 3, 0, 0x24},
    {"EffectKind24_Start", 0x46F840, 0x67, E1D_CALLS(kCalls46F840), nullptr, 0, nullptr, 0, E1D_FN(EffectKind24_Start), 0, false, kEf, 0, 0, 0, 0x24},
    {"EffectKind24_Shrink", 0x46F8B0, 0x4A, E1D_CALLS(kCalls46F8B0), nullptr, 0, nullptr, 0, E1D_FN(EffectKind24_Shrink), 0, false, kEf, 0, 0, 0, 0x24},
    {"EffectKind25_Run", 0x46F900, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1D_FN(EffectKind25_Run), 0, false, kEf, 0, 5, 0, 0x25},
    {"EffectKind25_Start", 0x46F920, 0xA2, E1D_CALLS(kCalls46F920), nullptr, 0, nullptr, 0, E1D_FN(EffectKind25_Start), 0, false, kEf, 0, 0, 0, 0x25},
    {"EffectKind25_Grow", 0x46F9D0, 0x4A, E1D_CALLS(kCalls46F9D0), nullptr, 0, nullptr, 0, E1D_FN(EffectKind25_Grow), 0, false, kEf, 0, 0, 0, 0x25},
    {"EffectKind25_Glow", 0x46FA20, 0x6B, E1D_CALLS(kCalls46FA20), nullptr, 0, nullptr, 0, E1D_FN(EffectKind25_Glow), 0, false, kEf, 0, 0, 0, 0x25},
    {"EffectKind25_Shrink", 0x46FA90, 0x41, E1D_CALLS(kCalls46FA90), nullptr, 0, nullptr, 0, E1D_FN(EffectKind25_Shrink), 0, false, kEf, 0, 0, 0, 0x25},
    {"EffectKind25_DrawDisc", 0x46FCF0, 0x16E, E1D_CALLS(kCalls46FCF0), nullptr, 0, nullptr, 0, E1D_FN(EffectKind25_DrawDisc), 0, false, kCa, 0, 0, 0, 0x25},
    {"EffectKind26_Run", 0x46FE60, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1D_FN(EffectKind26_Run), 0, false, kEf, 0, 3, 0, 0x26},
    {"EffectKind26_Start", 0x46FE80, 0x15, nullptr, 0, nullptr, 0, nullptr, 0, E1D_FN(EffectKind26_Start), 0, false, kEf, 0, 0, 0, 0x26},
    {"EffectKind26_Widen", 0x46FEA0, 0x3E, E1D_CALLS(kCalls46FEA0), nullptr, 0, nullptr, 0, E1D_FN(EffectKind26_Widen), 0, false, kEf, 0, 0, 0, 0x26},
    {"EffectKind26_DrawBand", 0x46FFB0, 0x346, E1D_CALLS(kCalls46FFB0), nullptr, 0, nullptr, 0, E1D_FN(EffectKind26_DrawBand), 0, false, kCa, 0, 0, 0, 0x26},
    {"EffectKind27_Run", 0x46FEE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1D_FN(EffectKind27_Run), 0, false, kEf, 0, 3, 0, 0x27},
    {"EffectKind27_Start", 0x46FF00, 0x15, nullptr, 0, nullptr, 0, nullptr, 0, E1D_FN(EffectKind27_Start), 0, false, kEf, 0, 0, 0, 0x27},
    {"EffectKind27_Emit", 0x46FF20, 0x87, E1D_CALLS(kCalls46FF20), nullptr, 0, nullptr, 0, E1D_FN(EffectKind27_Emit), 0, false, kEf, 0, 0, 0, 0x27},
};
#undef E1D_FN
#undef E1D_CALLS
#undef E1D_N

enum : unsigned {
    k21Run, k21Start, k21Grow, k21Lift, k21Hold, k21Swirl, k21Fold, k22Run, k22Sound, k22Spawn, k22Wait, k23Run,
    k23Start, k23Spawn, k24Run, k24Start, k24Shrink, k25Run, k25Start, k25Grow, k25Glow, k25Shrink, k25Disc,
    k26Run, k26Start, k26Widen, k26Band, k27Run, k27Start, k27Emit, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the effects -----------------------------------------------------------------------

// The caller's stack from just below this frame to its base (the TIB's
// StackBase): where the originals' and ours' locals are.
bool OnStack(const void* p, unsigned n) {
    std::uint32_t base;
    __asm__("movl %%fs:4, %0" : "=r"(base));
    const auto here = static_cast<U>(reinterpret_cast<std::uintptr_t>(&base));
    const auto at = static_cast<U>(reinterpret_cast<std::uintptr_t>(p));
    return at > here && at + n > at && at + n <= base;
}
bool Writable(U at, unsigned n) { return sh::InRegions(P(at), n) || OnStack(P(at), n); }
// A float with a random mantissa and an exponent 2^-17..2^18 (no NaN, no
// infinity): what the projection's screen words and depth are.
void FillFloat(unsigned char* at) {
    const U n = sh::Noise();
    const U bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    std::memcpy(at, &bits, 4);
}
// EffectGte_ProjectPoint: out[0..2] the projected x, y and depth (floats) - the
// band's vertices in the packet, compared.
U FxProjectPoint(const U* a, U answer) {
    if (Writable(a[1], 12))
        for (unsigned i = 0; i < 3; ++i) FillFloat(P(a[1] + 4 * i));
    return answer;
}

// Gte_RotTransPers: the screen point, two floats EffectKind25_Start takes
// through _ftol - fractions (the effect-mode row fills whole numbers, which
// cannot tell truncation from rounding), and one time in eight a value _ftol
// answers with the integer indefinite (NaN, or past 2^63 either way); the
// depth-cue word the callee writes through p filled too.
U FxRotTransPers(const U* a, U answer) {
    if (Writable(a[1], 8))
        for (unsigned i = 0; i < 2; ++i) {
            unsigned char* const at = P(a[1] + 4 * i);
            const U n = sh::Noise();
            if (n % 8 == 0) {
                const U odd[] = {0x7FC00000u, 0xFFC00000u, 0x5F000000u, 0xDF000001u, 0x7F7FFFFFu, 0xFF7FFFFFu, 0x5EFFFFFFu};
                std::memcpy(at, &odd[(n >> 3) % 7], 4);
            } else {
                FillFloat(at);
            }
        }
    if (Writable(a[2], 4)) sh::FillBytes(P(a[2]), 4);
    return answer;
}

#define E1D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name: x, y and the radius read as
    // s16 (movsx), the two shades as bytes; the band's point as three dwords,
    // t as its word - the callers push whole registers (docs/effect_1d.md section 3)
    {E1D_OURS(EffectKind25_DrawDisc), 5, {k16, k16, k16, k8, k8}, kG, 0, 0},
    {E1D_OURS(EffectKind26_DrawBand), 4, {kW, kW, kW, k16}, kG, 0, 0},
    // nobody's this round (catalog part 6), by address: the arm's draw reads
    // the four points 0x46F570 wrote and the words after them - hashed to
    // +0x54 where the standard row logs the pointer only
    {"0x46F690", at::kArmDraw, at::kArmDraw, 1, {0}, kG, 0, 0, {0x54}, nullptr, nullptr, true},
    // standard rows re-listed: EffectGte_ProjectPoint's point is a stack local
    // (its address differs between the copy and ours): hashed, not logged; out
    // (the packet) logged and filled
    {E1D_OURS(EffectGte_ProjectPoint), 2, {0, kW}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    // the vertex and the screen point are Prim_VertexScratch and MapView_ScreenXY
    // (fixed cells, logged); p is a local, not logged, filled
    {E1D_OURS(Gte_RotTransPers), 3, {kW, kW, 0}, kG, 0, 0, {8, 0, 0}, &FxRotTransPers, nullptr, true},
};
#undef E1D_OURS

// The seven state tables the dispatchers jump through, read in place; each
// table's length to the next kind's (none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x654284, 6}, {0x65429C, 3}, {0x6542A8, 3}, {0x6542B4, 3}, {0x6542C0, 5}, {0x6542D4, 3}, {0x6542E0, 3},
};
const std::uint8_t kKinds[] = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27};

// --- the seed ------------------------------------------------------------------------

// The frame count +9 at every compare the states make: 1 (down to 0), 0 (down
// to 0xFF), bit 2, 0x2C..0x2E (+0xF against 0x3C), 0x3C, 0x7F..0x81 (signed),
// 0xD7 (the glow's sound).
U FrameCount() { return PickOf(0, 1, 2, 4, 5, 8, 0x2C, 0x2D, 0x2E, 0x3C, 0x3D, 0x7F, 0x80, 0x81, 0xD7, 0xD8, 0xFF, sh::Next()); }

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    s[9] = static_cast<unsigned char>(FrameCount());
    switch (k) {
    case k21Grow:
    case k21Lift:
    case k21Hold:
    case k21Swirl:
    case k21Fold:
        // the angle word +0x58 at the fold's compare: -8 lands on 0, -1, 0x7FFF, 0x8000
        SetWord(s + 0x58, PickOf(0, 7, 8, 9, 0x8007, 0x8008, 0x8009, 0xFFFF, sh::Next()));
        break;
    case k22Wait:
        // every record's five cells at records (the original reads +6 through
        // each), each record's +6 done or not - all five done about half the time
        for (unsigned r = 0; r < at::kEffects; ++r) {
            unsigned char* const e = sh::EffectRecord(r);
            for (unsigned i = 0; i < 5; ++i)
                SetLong(e + 0xC + 4 * i, static_cast<std::int32_t>(Key(sh::EffectRecord(sh::Next() % at::kEffects))));
        }
        for (unsigned r = 0; r < at::kEffects; ++r)
            sh::EffectRecord(r)[6] = static_cast<unsigned char>(sh::Next() % 8 == 0 ? 0 : PickOf(1, 0x80, 0xFF, sh::Next() | 1));
        break;
    case k24Shrink:
        // the first scale +0x12 at 0x10 (to 0) and 0x90 (to 0x80) and their
        // neighbours; the second +0x14 at 0x10 (to 0)
        SetWord(s + 0x12, PickOf(0, 0xF, 0x10, 0x11, 0x8F, 0x90, 0x91, 0x800F, 0x8010, 0x8011, sh::Next()));
        SetWord(s + 0x14, PickOf(0, 0xF, 0x10, 0x11, 0x800F, 0x8010, 0x8011, sh::Next()));
        break;
    case k26Widen:
        // +0xC + 0x10 at 0x500 and past it, and across the sign
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0, 0x4EF, 0x4F0, 0x4F1, 0x7FFFFFEF, 0x7FFFFFF0, 0xFFFFFFF0, sh::Next())));
        break;
    case k27Emit:
        // +0xC's low nibble 0 or not, and 1 (down to 0)
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0, 1, 2, 0x10, 0x11, 0x1000, 0xFFF, 0xF0, sh::Next())));
        break;
    default: break;
    }
}

void Args(unsigned k, U* a) {
    if (k == k26Band) {
        // t's low word at every compare (0x100, 0x400, 0x500, the sign), the
        // high word leftovers as the callers push them
        a[3] = (sh::Next() & 0xFFFF0000u) |
               PickOf(0, 0x80, 0xFF, 0x100, 0x101, 0x3FF, 0x400, 0x401, 0x4FF, 0x500, 0x501, 0x7FFF, 0x8000, 0x80FF,
                      0x8100, 0xFFFF, sh::Next() & 0xFFFF);
    } else if (k == k25Disc) {
        // a radius of the callers' range half the time (0..0x4B), else any s16
        if (sh::Half()) a[2] = (sh::Next() & 0xFFFF0000u) | (sh::Next() % 0x4C);
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count, the arm's angle
// and length words, the ray's scales, the dword +0xC.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 6) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? PickOf(1, 0x2D, 0xD7, 0x80) : v >> 1); break;
    case 1: SetWord(s + 0x58, (v & 1) ? 8u : v >> 1); break;
    case 2: SetWord(s + 0x5E, v); break;
    case 3: SetWord(s + 0x12, (v & 1) ? 0x10u : v >> 1); break;
    case 4: SetWord(s + 0x14, (v & 1) ? 0x10u : v >> 1); break;
    case 5: SetLong(s + 0xC, static_cast<std::int32_t>((v & 1) ? 0x4F0u : v >> 1)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E1D_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E1D_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_1d: BOF3X_E1D_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_1d", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], nullptr, 0,
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_1d
