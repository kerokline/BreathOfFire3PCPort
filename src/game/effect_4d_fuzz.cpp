// BOF3X_SHADOW=effect_4d: group E4D's 51 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_4d.md section 4. BOF3X_E4D_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E4D --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; every extent is the tool's. Shapes: every dispatcher and
// state kEffect (Sprite_Current one of the 20 Effect_Objects records, +5 the
// kind, a dispatcher's byte below its table's length); the tints and the
// kind-0x94 redraw kEffect too (they read Sprite_Current); the helpers with
// arguments kCall; the spark mover answering al with its ret_mask.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4d.h"
#include "game/effect_4d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_4d {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E4D --clones, 2026-10-03.
constexpr sh::CallSite kCalls48C9F0[] = {{0x3D, 0x4976D0}, {0x55, 0x48CA90}};
constexpr sh::CallSite kCalls48CA50[] = {{0x1E, 0x48CA90}};
constexpr sh::CallSite kCalls48CA80[] = {{0x0, 0x48CA90}};
constexpr sh::CallSite kCalls48CA90[] = {{0xE, 0x5A79A0},  {0x26, 0x5A77C0}, {0x2F, 0x461E50}, {0x3B, 0x5A7740},
                                         {0x82, 0x5A7780}, {0x8A, 0x5A77A0}, {0x93, 0x461E50}};
constexpr sh::CallSite kCalls48CB90[] = {{0x55, 0x48CC90}, {0x5D, 0x48CD40}, {0x6A, 0x48CC90}, {0x72, 0x48CD40}};
constexpr sh::CallSite kCalls48CC10[] = {{0x2, 0x48CC90}, {0xA, 0x48CD40}};
constexpr sh::CallSite kCalls48CC20[] = {{0x1F, 0x48CC90}};
constexpr sh::CallSite kCalls48CC60[] = {{0x19, 0x589840}, {0x20, 0x48CC90}};
constexpr sh::CallSite kCalls48CC90[] = {{0x16, 0x5A79A0}, {0x2F, 0x5A77C0}, {0x38, 0x461E50}, {0x44, 0x5A7740},
                                         {0x8B, 0x5A7780}, {0x93, 0x5A77A0}, {0x9C, 0x461E50}};
constexpr sh::CallSite kCalls48CD40[] = {{0x1F, 0x588F20}, {0x69, 0x588F20}};
constexpr sh::CallSite kCalls48CEA0[] = {{0x38, 0x57C7C0}, {0x4B, 0x589840}};
constexpr sh::CallSite kCalls48CF50[] = {{0x5C, 0x5A79A0}, {0x74, 0x5A77C0}, {0x7D, 0x461E50},
                                         {0x89, 0x5A7610}, {0x105, 0x5A7780}, {0x10E, 0x461E50}};
constexpr sh::CallSite kCalls48D0B0[] = {{0x1A, 0x48D490}};
constexpr sh::CallSite kCalls48D100[] = {{0x10, 0x48D490}, {0x23, 0x482050}, {0x2D, 0x5A7B90}, {0x38, 0x48D860}, {0x46, 0x5A7BC0}};
constexpr sh::CallSite kCalls48D180[] = {{0x17, 0x48D650}, {0x2C, 0x48D490}, {0x34, 0x48D980}, {0x39, 0x48D6A0}, {0x8B, 0x495040}};
constexpr sh::CallSite kCalls48D220[] = {{0x17, 0x48D650}, {0x1C, 0x48D980}, {0x21, 0x48D6A0}, {0x32, 0x495040}};
constexpr sh::CallSite kCalls48D270[] = {{0x11, 0x589840}};
constexpr sh::CallSite kCalls48D2E0[] = {{0x18, 0x48DBA0}};
constexpr sh::CallSite kCalls48D360[] = {{0x3, 0x5A7B90}, {0x8, 0x494060}, {0x30, 0x494110}, {0x35, 0x5A7BC0}, {0x5C, 0x48DC40}};
constexpr sh::CallSite kCalls48D3F0[] = {{0x3, 0x5A7B90},  {0x8, 0x494060},  {0x30, 0x494110},
                                         {0x35, 0x5A7BC0}, {0x66, 0x48DC40}, {0x88, 0x589840}};
constexpr sh::CallSite kCalls48D490[] = {{0x17, 0x5A79A0}, {0x2F, 0x5A77C0}, {0x38, 0x461E50}, {0x3D, 0x494060},
                                         {0x4C, 0x494110}, {0x6B, 0x4941E0}, {0xDC, 0x5A75F0}, {0xE4, 0x5A7780},
                                         {0x10E, 0x5A7A50}, {0x12F, 0x5A7A00}, {0x19B, 0x461E50}};
constexpr sh::CallSite kCalls48D6A0[] = {{0x5, 0x494060}, {0x31, 0x48D6F0}};
constexpr sh::CallSite kCalls48D6F0[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x55, 0x4941E0},
                                         {0x60, 0x494110}, {0x8A, 0x5A75F0}, {0x92, 0x5A7780}, {0xA6, 0x5A7A50},
                                         {0xC1, 0x5A7A00}, {0xE2, 0x5A7A50}, {0xFD, 0x5A7A00}, {0x151, 0x461E50}};
constexpr sh::CallSite kCalls48D860[] = {{0x2D, 0x5B93D2}, {0x3A, 0x5B93D2}, {0x4D, 0x5B93D2}, {0x5C, 0x5A7A50},
                                         {0x67, 0x5A7A00}, {0x78, 0x5A7A50}, {0x83, 0x5A7A00}, {0x97, 0x494180},
                                         {0xA5, 0x5A7F10}, {0xB5, 0x5A7F80}, {0xC3, 0x5A7FF0}, {0xD2, 0x5A7C70},
                                         {0xE1, 0x5A7C70}, {0xE9, 0x5B93D2}};
constexpr sh::CallSite kCalls48D980[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0x4E, 0x48D9F0}};
constexpr sh::CallSite kCalls48D9F0[] = {{0xE, 0x5A75F0},  {0x16, 0x5A7780}, {0x24, 0x494110}, {0x2E, 0x5A7A50},
                                         {0x41, 0x5A7A00}, {0x66, 0x5A7A00}, {0x79, 0x5A7A50}, {0xBE, 0x494110},
                                         {0xC8, 0x5A7A50}, {0xDB, 0x5A7A00}, {0x100, 0x5A7A00}, {0x113, 0x5A7A50},
                                         {0x158, 0x494110}, {0x19B, 0x461E50}};
constexpr sh::CallSite kCalls48DBA0[] = {{0x2A, 0x5A79A0}, {0x42, 0x5A77C0}, {0x4B, 0x461E50},
                                         {0x57, 0x5A7740}, {0x5F, 0x5A7780}, {0x8D, 0x461E50}};
constexpr sh::CallSite kCalls48DC40[] = {{0x4D, 0x5A79A0}, {0x64, 0x5A77C0}, {0x6D, 0x461E50}, {0x8D, 0x5A75F0},
                                         {0x95, 0x5A7780}, {0xB2, 0x5A7A50}, {0xD2, 0x5A7A00}, {0x105, 0x5A7A50},
                                         {0x125, 0x5A7A00}, {0x178, 0x461E50}};
constexpr sh::CallSite kCalls48DE00[] = {{0xA, 0x5B93D2}};
constexpr sh::CallSite kCalls48DF70[] = {{0x9, 0x589840}};

#define E4D_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E4D_CALLS(a) a, E4D_N(a)
#define E4D_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kAl = 0xFFu;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind91_Run", 0x48C990, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind91_Run), 0, false, kEf, 0, 4, 0, 0x91},
    {"EffectKind91_Start", 0x48C9B0, 0x31, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind91_Start), 0, false, kEf, 0, 0, 0, 0x91},
    {"EffectKind91_Brighten", 0x48C9F0, 0x5A, E4D_CALLS(kCalls48C9F0), nullptr, 0, nullptr, 0, E4D_FN(EffectKind91_Brighten), 0, false, kEf, 0, 0, 0, 0x91},
    {"EffectKind91_WaitMessage", 0x48CA50, 0x23, E4D_CALLS(kCalls48CA50), nullptr, 0, nullptr, 0, E4D_FN(EffectKind91_WaitMessage), 0, false, kEf, 0, 0, 0, 0x91},
    {"EffectKind91_Hold", 0x48CA80, 0x5, E4D_CALLS(kCalls48CA80), nullptr, 0, nullptr, 0, E4D_FN(EffectKind91_Hold), 0, false, kEf, 0, 0, 0, 0x91},
    {"Effect_DrawScreenTint", 0x48CA90, 0x9D, E4D_CALLS(kCalls48CA90), nullptr, 0, nullptr, 0, E4D_FN(Effect_DrawScreenTint), 0, false, kEf, 0, 0, 0, -1},
    {"EffectKind94_Run", 0x48CB30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind94_Run), 0, false, kEf, 0, 5, 0, 0x94},
    {"EffectKind94_Start", 0x48CB50, 0x31, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind94_Start), 0, false, kEf, 0, 0, 0, 0x94},
    {"EffectKind94_Fade", 0x48CB90, 0x79, E4D_CALLS(kCalls48CB90), nullptr, 0, nullptr, 0, E4D_FN(EffectKind94_Fade), 0, false, kEf, 0, 0, 0, 0x94},
    {"EffectKind94_Hold", 0x48CC10, 0xF, E4D_CALLS(kCalls48CC10), nullptr, 0, nullptr, 0, E4D_FN(EffectKind94_Hold), 0, false, kEf, 0, 0, 0, 0x94},
    {"EffectKind94_Flash", 0x48CC20, 0x3C, E4D_CALLS(kCalls48CC20), nullptr, 0, nullptr, 0, E4D_FN(EffectKind94_Flash), 0, false, kEf, 0, 0, 0, 0x94},
    {"EffectKind94_FlashOut", 0x48CC60, 0x27, E4D_CALLS(kCalls48CC60), nullptr, 0, nullptr, 0, E4D_FN(EffectKind94_FlashOut), 0, false, kEf, 0, 0, 0, 0x94},
    {"Effect_DrawScreenTintMode", 0x48CC90, 0xA6, E4D_CALLS(kCalls48CC90), nullptr, 0, nullptr, 0, E4D_FN(Effect_DrawScreenTintMode), 0, false, kCa, 0, 0, 0, -1},
    {"EffectKind94_RedrawSprites", 0x48CD40, 0x89, E4D_CALLS(kCalls48CD40), nullptr, 0, nullptr, 0, E4D_FN(EffectKind94_RedrawSprites), 0, false, kEf, 0, 0, 0, 0x94},
    {"EffectKind95_Run", 0x48CDD0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind95_Run), 0, false, kEf, 0, 3, 0, 0x95},
    {"EffectKind95_Start", 0x48CDF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind95_Start), 0, false, kEf, 0, 0, 0, 0x95},
    {"EffectKind95_Clock", 0x48CE10, 0x8F, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind95_Clock), 0, false, kEf, 0, 0, 0, 0x95},
    {"EffectKind95_Finish", 0x48CEA0, 0x51, E4D_CALLS(kCalls48CEA0), nullptr, 0, nullptr, 0, E4D_FN(EffectKind95_Finish), 0, false, kEf, 0, 0, 0, 0x95},
    {"EffectKind96_Run", 0x48CF00, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind96_Run), 0, false, kEf, 0, 2, 0, 0x96},
    {"EffectKind96_Start", 0x48CF20, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind96_Start), 0, false, kEf, 0, 0, 0, 0x96},
    {"EffectKind96_Pulse", 0x48CF50, 0x119, E4D_CALLS(kCalls48CF50), nullptr, 0, nullptr, 0, E4D_FN(EffectKind96_Pulse), 0, false, kEf, 0, 0, 0, 0x96},
    {"EffectKind97_Run", 0x48D070, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_Run), 0, false, kEf, 0, 6, 0, 0x97},
    {"EffectKind97_Start", 0x48D090, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_Start), 0, false, kEf, 0, 0, 0, 0x97},
    {"EffectKind97_Grow", 0x48D0B0, 0x48, E4D_CALLS(kCalls48D0B0), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_Grow), 0, false, kEf, 0, 0, 0, 0x97},
    {"EffectKind97_WaitCue", 0x48D100, 0x73, E4D_CALLS(kCalls48D100), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_WaitCue), 0, false, kEf, 0, 0, 0, 0x97},
    {"EffectKind97_Burst", 0x48D180, 0x9C, E4D_CALLS(kCalls48D180), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_Burst), 0, false, kEf, 0, 0, 0, 0x97},
    {"EffectKind97_Fade", 0x48D220, 0x4A, E4D_CALLS(kCalls48D220), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_Fade), 0, false, kEf, 0, 0, 0, 0x97},
    {"EffectKind97_End", 0x48D270, 0x17, E4D_CALLS(kCalls48D270), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_End), 0, false, kEf, 0, 0, 0, 0x97},
    {"EffectKind98_Run", 0x48D290, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind98_Run), 0, false, kEf, 0, 4, 0, 0x98},
    {"EffectKind98_Start", 0x48D2B0, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind98_Start), 0, false, kEf, 0, 0, 0, 0x98},
    {"EffectKind98_Flash", 0x48D2E0, 0x75, E4D_CALLS(kCalls48D2E0), nullptr, 0, nullptr, 0, E4D_FN(EffectKind98_Flash), 0, false, kEf, 0, 0, 0, 0x98},
    {"EffectKind98_Rise", 0x48D360, 0x8D, E4D_CALLS(kCalls48D360), nullptr, 0, nullptr, 0, E4D_FN(EffectKind98_Rise), 0, false, kEf, 0, 0, 0, 0x98},
    {"EffectKind98_Fall", 0x48D3F0, 0x91, E4D_CALLS(kCalls48D3F0), nullptr, 0, nullptr, 0, E4D_FN(EffectKind98_Fall), 0, false, kEf, 0, 0, 0, 0x98},
    {"EffectKind97_DrawRing", 0x48D490, 0x1BA, E4D_CALLS(kCalls48D490), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_DrawRing), 0, false, kCa, 0, 0, 0, 0x97},
    {"EffectKind97_EmitSpark", 0x48D650, 0x4C, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_EmitSpark), 0, false, kEf, 0, 0, 0, 0x97},
    {"EffectKind97_MoveSparks", 0x48D6A0, 0x45, E4D_CALLS(kCalls48D6A0), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_MoveSparks), kAl, false, kEf, 0, 0, 0, 0x97},
    {"EffectKind97_DrawSpark", 0x48D6F0, 0x170, E4D_CALLS(kCalls48D6F0), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_DrawSpark), 0, false, kCa, 0, 0, 0, 0x97},
    {"EffectKind97_DebrisInit", 0x48D860, 0x112, E4D_CALLS(kCalls48D860), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_DebrisInit), 0, false, kCa, 0, 0, 0, 0x97},
    {"EffectKind97_DrawDebris", 0x48D980, 0x63, E4D_CALLS(kCalls48D980), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_DrawDebris), 0, false, kEf, 0, 0, 0, 0x97},
    {"EffectKind97_DebrisDraw", 0x48D9F0, 0x1AB, E4D_CALLS(kCalls48D9F0), nullptr, 0, nullptr, 0, E4D_FN(EffectKind97_DebrisDraw), 0, false, kCa, 0, 0, 0, 0x97},
    {"EffectKind98_DrawFlash", 0x48DBA0, 0x98, E4D_CALLS(kCalls48DBA0), nullptr, 0, nullptr, 0, E4D_FN(EffectKind98_DrawFlash), 0, false, kCa, 0, 0, 0, 0x98},
    {"EffectKind98_DrawBurst", 0x48DC40, 0x195, E4D_CALLS(kCalls48DC40), nullptr, 0, nullptr, 0, E4D_FN(EffectKind98_DrawBurst), 0, false, kCa, 0, 0, 0, 0x98},
    {"EffectKind9A_Run", 0x48DDE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind9A_Run), 0, false, kEf, 0, 8, 0, 0x9A},
    {"EffectKind9A_Start", 0x48DE00, 0x2F, E4D_CALLS(kCalls48DE00), nullptr, 0, nullptr, 0, E4D_FN(EffectKind9A_Start), 0, false, kEf, 0, 0, 0, 0x9A},
    {"EffectKind9A_NudgeY", 0x48DE30, 0x3C, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind9A_NudgeY), 0, false, kEf, 0, 0, 0, 0x9A},
    {"EffectKind9A_HoldY", 0x48DE70, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind9A_HoldY), 0, false, kEf, 0, 0, 0, 0x9A},
    {"EffectKind9A_BackY", 0x48DE90, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind9A_BackY), 0, false, kEf, 0, 0, 0, 0x9A},
    {"EffectKind9A_NudgeX", 0x48DED0, 0x3C, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind9A_NudgeX), 0, false, kEf, 0, 0, 0, 0x9A},
    {"EffectKind9A_HoldX", 0x48DF10, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind9A_HoldX), 0, false, kEf, 0, 0, 0, 0x9A},
    {"EffectKind9A_BackX", 0x48DF30, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, E4D_FN(EffectKind9A_BackX), 0, false, kEf, 0, 0, 0, 0x9A},
    {"EffectKind9A_Repeat", 0x48DF70, 0x18, E4D_CALLS(kCalls48DF70), nullptr, 0, nullptr, 0, E4D_FN(EffectKind9A_Repeat), 0, false, kEf, 0, 0, 0, 0x9A},
};
#undef E4D_FN
#undef E4D_CALLS
#undef E4D_N

enum : unsigned {
    k91Run, k91Start, k91Brighten, k91Wait, k91Hold, kTint,
    k94Run, k94Start, k94Fade, k94Hold, k94Flash, k94FlashOut, kTintMode, k94Redraw,
    k95Run, k95Start, k95Clock, k95Finish,
    k96Run, k96Start, k96Pulse,
    k97Run, k97Start, k97Grow, k97WaitCue, k97Burst, k97Fade, k97End,
    k98Run, k98Start, k98Flash, k98Rise, k98Fall,
    k97Ring, k97Emit, k97MoveSparks, k97DrawSpark, k97DebrisInit, k97DrawDebris, k97DebrisDraw,
    k98DrawFlash, k98DrawBurst,
    k9ARun, k9AStart, k9ANudgeY, k9AHoldY, k9ABackY, k9ANudgeX, k9AHoldX, k9ABackX, k9ARepeat, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Spark(unsigned i) { return EffectKind30_Shards + at::kSparkStride * i; }
unsigned char* Debris(unsigned i) { return P(at::kDebris + at::kDebrisStride * i); }

// --- the effects ---------------------------------------------------------------------

// The tints read Sprite_Current's colour +0x5D..+0x5F: its address and those
// bytes logged (E3C's FxTile).
U FxTint(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    sh::Note(Key(s), sh::InRegions(s + 0x5C, 4) ? static_cast<U>(Long(s + 0x5C)) : 0u);
    return answer;
}
// DrawBurst reads Sprite_Current's projected point +0x74..+0x7F: logged.
U FxBurst(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    if (sh::InRegions(s + 0x74, 12)) sh::NoteBytes(s + 0x74, 12);
    sh::Note(Key(s));
    return answer;
}

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
// infinity); one time in eight a NaN quiet or signalling or an extreme (E3C's
// FillFloat): the rings and the debris copy the projected depth with a mov,
// and add to the projected x and y through the FPU - only a NaN tells a mov
// from an FPU copy.
void FillFloat(U at) {
    if (!Writable(at, 4)) return;
    const U n = sh::Noise();
    U bits;
    if (n % 8 == 0) {
        static const U kOdd[] = {0x7FC00000u, 0xFFC00000u, 0x7FC00001u, 0x7F800001u, 0xFF800002u, 0x5F000000u, 0xDF000001u, 0x7F7FFFFFu};
        bits = kOdd[(n >> 3) % 8];
    } else {
        bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    }
    std::memcpy(P(at), &bits, 4);
}
// EffectGte_ProjectPoint: out[0..2] the screen x, y and depth, which every
// caller reads; the point hashed.
U FxProjectPoint(const U* a, U answer) {
    for (unsigned i = 0; i < 3; ++i) FillFloat(a[1] + 4 * i);
    return answer;
}

#define E4D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {E4D_OURS(Effect_DrawScreenTint), 0, {}, kG, 0, 0, {}, &FxTint},
    // the blend read as the low byte (and eax, 0xFF); the callers push 1 or 2
    {E4D_OURS(Effect_DrawScreenTintMode), 1, {k8}, kG, 0, 0, {}, &FxTint},
    {E4D_OURS(EffectKind94_RedrawSprites), 0, {}, kPh, 0, 0},
    // the point (the record's +0x34) hashed; the size read as its low s16 (the
    // callers push a register whose upper half is left over), the shade's low
    // byte (bits 2..0)
    {E4D_OURS(EffectKind97_DrawRing), 3, {kW, k16, k8}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {E4D_OURS(EffectKind97_EmitSpark), 0, {}, kPh, 0, 0},
    {E4D_OURS(EffectKind97_MoveSparks), 0, {}, kF, 0, 0},
    {E4D_OURS(EffectKind97_DrawSpark), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    {E4D_OURS(EffectKind97_DebrisInit), 1, {kW}, kG, 0, 0},
    {E4D_OURS(EffectKind97_DrawDebris), 0, {}, kPh, 0, 0},
    {E4D_OURS(EffectKind97_DebrisDraw), 1, {kW}, kG, 0, 0, {0x2C}, nullptr, nullptr, true},
    // the level read as its low s16 (movsx-free `mov bx, [esp + 8]`)
    {E4D_OURS(EffectKind98_DrawFlash), 1, {k16}, kG, 0, 0},
    // three words, each read as its low s16; the callers push registers whose
    // upper halves are left over (Sprite_Current's, a point's)
    {E4D_OURS(EffectKind98_DrawBurst), 3, {k16, k16, k16}, kG, 0, 0, {}, &FxBurst},
    // E3A's (wave three, ours): the eight sparks' +0 cleared; nothing of it read after
    {E4D_OURS(EffectKind64_ClearSparks), 0, {}, kPh, 0, 0},
    // re-listed: the standard row's fractional floats never reach a NaN, which
    // alone tells the depth's mov copies from an FPU copy (E3C's row)
    {E4D_OURS(EffectGte_ProjectPoint), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
};
#undef E4D_OURS

// The state tables the dispatchers jump through, read in place; each table's
// own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x655100, 4}, {0x655110, 5}, {0x65512C, 3}, {0x655138, 2}, {0x655140, 6}, {0x655158, 4}, {0x655170, 8},
};
const std::uint8_t kKinds[] = {0x91, 0x94, 0x95, 0x96, 0x97, 0x98, 0x9A};

// Beyond effect mode's standard regions: kind 0x96's level and index and kind
// 0x95's clock word.
const sh::Region kRegions[] = {
    {at::kKind96Shade, 4},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }

// A word-sized level at the clamps' boundaries.
U Level() { return PickOf(0, 1, 0xFF, 0x100, 0xFFFF, 0x8000, 0x7FFF, 0xFE, 0x80, sh::Next()); }

// A float for a projected point the burst reads (+0x74..+0x7C): fractions, a
// NaN quiet or signalling now and then.
U PointFloat() {
    const U n = sh::Next();
    if (n % 6 == 0) return PickOf(0x7F800001u, 0xFF800002u, 0x7FC00000u, 0x7FA00000u);
    return (n & 0x807FFFFFu) | ((0x6Eu + (sh::Next() % 0x24u)) << 23);
}

void Seed(unsigned k) {
    // every record the disturbance may move Sprite_Current to: +9 at its
    // boundaries, +6 at its fourth-frame boundary, +0x5D round 0x40 and the
    // sign, the burst's projected point
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        e[9] = static_cast<unsigned char>(PickOf(1, 2, 0, 0x10, 0x11, 0xF, 0xFF, 0x5A, sh::Next()));
        e[6] = static_cast<unsigned char>(PickOf(0, 4, 3, 7, sh::Next()));
        e[0x5D] = static_cast<unsigned char>(PickOf(0x3F, 0x40, 0x7F, 0x80, 0xFF, 0, sh::Next()));
        for (unsigned i = 0; i < 3; ++i) SetLong(e + 0x74 + 4 * i, static_cast<std::int32_t>(PointFloat()));
        SetWord(e + 0x32, Level());
        SetLong(e + 0xC, static_cast<std::int32_t>(Level()));
        SetLong(e + 0x10, static_cast<std::int32_t>(Level()));
    }
    // ObjTrio has three records: Field_MemberCount 0..3 (ours aborts past)
    Field_MemberCount = static_cast<unsigned char>(sh::Next() % 4);
    // kind 0x96's index inside its table's eight (ours aborts past)
    Mem(at::kKind96Index)[0] = static_cast<unsigned char>(PickOf(0, 6, 7, sh::Next() % 8));
    if (sh::Half()) Mem(at::kCounter)[0] = static_cast<unsigned char>(PickOf(at::kKind97Cue, at::kKind9AEnd));
    if (sh::Half()) MoveScript_WaitWordDA = 0;
    switch (k) {
    case k91Wait:
        if (sh::Half()) Field_Request = 2;
        break;
    case k94Redraw:
        for (unsigned m = 0; m < at::kTrioCount; ++m)
            ObjTrio[at::kTrioStride * m + at::kTrioBusy] = static_cast<unsigned char>(PickOf(7, 7, 6, 8, sh::Next()));
        break;
    case k95Clock:
    case k95Finish: {
        // a third of the time nothing holds it; else one hold at a time
        Field_Request = static_cast<unsigned char>(PickOf(0, 0, 3, 1, 2, sh::Next()));
        Game_Mode = static_cast<unsigned short>(PickOf(0, 0, 5, 3, 4, sh::Next()));
        Mem(at::kTrio2Mode)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next()));
        Mem(at::kTrio0Mode)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next()));
        if (sh::Often()) Input_Pressed = static_cast<unsigned short>(Input_Pressed & ~Field_MenuButton);
        // the frames at 0x1D / 0x27 (the next tips over), the ticks at 0xE
        const U mode = sh::Half() ? 0x8000u : 0u;
        const U frames = PickOf(0x1D, 0x27, 0x1E, 0x28, 0x7F, 0, sh::Next() & 0x7F) << 8;
        const U ticks = PickOf(0xE, 0xF, 0xD, 0xFF, 0, sh::Next() & 0xFF);
        SetWord(Mem(at::kKind95Clock), mode | frames | ticks);
        break;
    }
    case k97Emit:
    case k97MoveSparks:
        // a pool some full, some free, lives at 1
        for (unsigned i = 0; i < at::kSparkCount; ++i) {
            unsigned char* const r = Spark(i);
            r[0] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? 0 : PickOf(1, 0x80, sh::Next() | 1));
            r[2] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, sh::Next()));
        }
        break;
    case k97DebrisDraw:
        for (unsigned i = 0; i < at::kDebrisCount; ++i) SetWord(Debris(i) + 0x2A, Level());
        break;
    default: break;
    }
}

// The helpers with arguments: the ring's point the record's +0x34 (as its
// callers hand it), the spark and debris records of their pools, the levels at
// their clamps.
void Args(unsigned k, U* a) {
    switch (k) {
    case k97Ring:
        a[0] = Key(Sprite_Current + 0x34);
        a[2] = PickOf(7, 7, 1, 2, 4, sh::Next());
        break;
    case k97DrawSpark: a[0] = Key(Spark(sh::Next() % at::kSparkCount)); break;
    case k97DebrisInit:
    case k97DebrisDraw: a[0] = Key(Debris(sh::Next() % at::kDebrisCount)); break;
    case k98DrawFlash: a[0] = (sh::Next() & 0xFFFF0000u) | Level(); break;
    case k98DrawBurst:
        a[1] = (sh::Next() & 0xFFFF0000u) | Level();
        a[2] = (sh::Next() & 0xFFFF0000u) | Level();
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count +9, +6, the
// shade +0x5D, the counter 0x903848 at its cues, kind 0x96's index inside its
// table, kind 0x95's clock, the burst's projected point.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 7) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1); break;
    case 1: s[6] = static_cast<unsigned char>(v); break;
    case 2: s[0x5D] = static_cast<unsigned char>((v & 1) ? 0x3Fu + ((v >> 1) & 1) : v >> 1); break;
    case 3: Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? ((v & 2) ? at::kKind97Cue : at::kKind9AEnd) : v >> 2); break;
    case 4: Mem(at::kKind96Index)[0] = static_cast<unsigned char>(v % 8); break;
    case 5: SetWord(Mem(at::kKind95Clock), v); break;
    case 6: SetLong(s + 0x74 + 4 * (v % 3), static_cast<std::int32_t>(v >> 2)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E4D_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E4D_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_4d: BOF3X_E4D_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_4d", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_4d
