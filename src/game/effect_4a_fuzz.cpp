// BOF3X_SHADOW=effect_4a: group E4A's 51 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_4a.md section 4. BOF3X_E4A_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up); BOF3X_E4A_ROUNDS
// sets the rounds (default 4,000).
//
// The clone table is tools/band_rows.py --group E4A --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given, with three starts added: kind 0x83's state 2 0x4883D0 (code
// no list has, inside the cut's span of 0x488240), kind 0x85's draw tail
// 0x488B90 (its own frame and ret, which the tool counted into 0x488B70) and
// kind 0x86's dispatcher 0x488BE0 (a catalog row no group of the round holds).
// Every function is a kEffect (Sprite_Current one of the 20 Effect_Objects
// records, +5 the kind, a dispatcher's byte below its table's length).
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4a.h"
#include "game/effect_4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_4a {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

constexpr U kInputHeld = 0x7E1BE8;   // Input_Held (u16; the macro of that name is the cell itself)

// tools/band_rows.py --group E4A --clones, 2026-10-03 (0x488B70 cut at its jmp, 0x488B90 its own).
constexpr sh::CallSite kCalls4881C0[] = {{0x4C, 0x589840}};
constexpr sh::CallSite kCalls488240[] = {{0x5C, 0x589840}};
constexpr sh::JumpTable kTables488240[] = {{0x58, 0x70, 5}};
constexpr sh::CallSite kCalls4887B0[] = {{0x27, 0x589840}};
constexpr sh::CallSite kCalls488830[] = {{0xA, 0x589840}, {0x3F, 0x589840}, {0x46, 0x5B93D2}};
constexpr sh::CallSite kCalls4888B0[] = {{0xA, 0x589840}, {0x3F, 0x589840}, {0x4F, 0x5B93D2}, {0xAF, 0x589840}};
constexpr sh::CallSite kCalls488970[] = {{0x9, 0x589840}, {0x40, 0x589840}};
constexpr sh::CallSite kCalls488A00[] = {{0x32, 0x4976D0}};
constexpr sh::CallSite kCalls488A50[] = {{0x1F, 0x48CA90}, {0x24, 0x488B90}};
constexpr sh::CallSite kCalls488A80[] = {{0x47, 0x48CA90}, {0x4C, 0x488B90}};
constexpr sh::CallSite kCalls488AE0[] = {{0x62, 0x4976D0}, {0x7B, 0x48CA90}, {0x80, 0x488B90}};
constexpr sh::CallSite kCalls488B70[] = {{0x9, 0x589840}, {0xE, 0x48CA90}, {0x13, 0x488B90}};
constexpr sh::CallSite kCalls488B90[] = {{0x25, 0x588F20}, {0x3B, 0x588F20}};
constexpr sh::CallSite kCalls488C00[] = {{0x2, 0x57CD90}, {0x38, 0x57CD90}, {0x8B, 0x57A010}, {0xA1, 0x57A010}, {0x160, 0x454CC0}, {0x17F, 0x454CC0}};
constexpr sh::CallSite kCalls488EF0[] = {{0x40, 0x454DC0}, {0x5E, 0x454DC0}, {0x66, 0x589840}};
constexpr sh::CallSite kCalls488F80[] = {{0x15, 0x489030}};
constexpr sh::CallSite kCalls488FB0[] = {{0x0, 0x489220}, {0x9, 0x4891F0}, {0x13, 0x587740}};
constexpr sh::CallSite kCalls488FE0[] = {{0x0, 0x489220}};
constexpr sh::CallSite kCalls489020[] = {{0x0, 0x589840}};

#define E4A_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E4A_CALLS(a) a, E4A_N(a)
#define E4A_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind82_Check11", 0x488020, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind82_Check11), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Restart", 0x488060, 0x25, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind82_Restart), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Nudge13", 0x488090, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind82_Nudge13), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Nudge15", 0x4880D0, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind82_Nudge15), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Nudge17", 0x488110, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind82_Nudge17), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Count18", 0x488150, 0x29, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind82_Count18), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Again", 0x488180, 0x40, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind82_Again), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_End", 0x4881C0, 0x51, E4A_CALLS(kCalls4881C0), nullptr, 0, nullptr, 0, E4A_FN(EffectKind82_End), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind83_Run", 0x488220, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Run), 0, false, kEf, 0, 24, 0, 0x83},
    {"EffectKind83_Wait", 0x488240, 0x181, E4A_CALLS(kCalls488240), nullptr, 0, E4A_CALLS(kTables488240), E4A_FN(EffectKind83_Wait), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Push2", 0x4883D0, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Push2), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Check3", 0x488410, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Check3), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Push4", 0x488450, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Push4), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Check5", 0x488490, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Check5), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Push6", 0x4884D0, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Push6), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Check7", 0x488510, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Check7), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Push8", 0x488550, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Push8), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Check9", 0x488590, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Check9), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Push10", 0x4885D0, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Push10), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Check11", 0x488610, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Check11), 0, false, kEf, 0, 0, 0, 0x83},
    {"Sprite_StateRestart", 0x433640, 0xA, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(Sprite_StateRestart), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Nudge13", 0x488650, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Nudge13), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Hold14", 0x488690, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Hold14), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Nudge15", 0x4886B0, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Nudge15), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Hold16", 0x4886F0, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Hold16), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Nudge17", 0x488710, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Nudge17), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Count18", 0x488750, 0x5E, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Count18), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind83_Finish", 0x4887B0, 0x2C, E4A_CALLS(kCalls4887B0), nullptr, 0, nullptr, 0, E4A_FN(EffectKind83_Finish), 0, false, kEf, 0, 0, 0, 0x83},
    {"EffectKind84_Run", 0x4887E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind84_Run), 0, false, kEf, 0, 5, 0, 0x84},
    {"EffectKind84_WaitPress", 0x488800, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind84_WaitPress), 0, false, kEf, 0, 0, 0, 0x84},
    {"EffectKind84_Pick", 0x488830, 0x72, E4A_CALLS(kCalls488830), nullptr, 0, nullptr, 0, E4A_FN(EffectKind84_Pick), 0, false, kEf, 0, 0, 0, 0x84},
    {"EffectKind84_Mash", 0x4888B0, 0xB6, E4A_CALLS(kCalls4888B0), nullptr, 0, nullptr, 0, E4A_FN(EffectKind84_Mash), 0, false, kEf, 0, 0, 0, 0x84},
    {"EffectKind84_Pause", 0x488970, 0x6C, E4A_CALLS(kCalls488970), nullptr, 0, nullptr, 0, E4A_FN(EffectKind84_Pause), 0, false, kEf, 0, 0, 0, 0x84},
    {"EffectKind85_Run", 0x4889E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind85_Run), 0, false, kEf, 0, 5, 0, 0x85},
    {"EffectKind85_Start", 0x488A00, 0x4C, E4A_CALLS(kCalls488A00), nullptr, 0, nullptr, 0, E4A_FN(EffectKind85_Start), 0, false, kEf, 0, 0, 0, 0x85},
    {"EffectKind85_WaitMessage", 0x488A50, 0x29, E4A_CALLS(kCalls488A50), nullptr, 0, nullptr, 0, E4A_FN(EffectKind85_WaitMessage), 0, false, kEf, 0, 0, 0, 0x85},
    {"EffectKind85_Brighten", 0x488A80, 0x51, E4A_CALLS(kCalls488A80), nullptr, 0, nullptr, 0, E4A_FN(EffectKind85_Brighten), 0, false, kEf, 0, 0, 0, 0x85},
    {"EffectKind85_NextMessage", 0x488AE0, 0x85, E4A_CALLS(kCalls488AE0), nullptr, 0, nullptr, 0, E4A_FN(EffectKind85_NextMessage), 0, false, kEf, 0, 0, 0, 0x85},
    {"EffectKind85_Wait", 0x488B70, 0x18, E4A_CALLS(kCalls488B70), nullptr, 0, nullptr, 0, E4A_FN(EffectKind85_Wait), 0, false, kEf, 0, 0, 0, 0x85},
    {"EffectKind85_ShowObjects", 0x488B90, 0x48, E4A_CALLS(kCalls488B90), nullptr, 0, nullptr, 0, E4A_FN(EffectKind85_ShowObjects), 0, false, kEf, 0, 0, 0, 0x85},
    {"EffectKind86_Run", 0x488BE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind86_Run), 0, false, kEf, 0, 4, 0, 0x86},
    {"EffectKind86_Spawn", 0x488C00, 0x19D, E4A_CALLS(kCalls488C00), nullptr, 0, nullptr, 0, E4A_FN(EffectKind86_Spawn), 0, false, kEf, 0, 0, 0, 0x86},
    {"EffectKind86_Slide", 0x488DA0, 0x61, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind86_Slide), 0, false, kEf, 0, 0, 0, 0x86},
    {"EffectKind86_Tint", 0x488E10, 0xD4, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind86_Tint), 0, false, kEf, 0, 0, 0, 0x86},
    {"EffectKind86_End", 0x488EF0, 0x6D, E4A_CALLS(kCalls488EF0), nullptr, 0, nullptr, 0, E4A_FN(EffectKind86_End), 0, false, kEf, 0, 0, 0, 0x86},
    {"EffectKind87_Run", 0x488F60, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind87_Run), 0, false, kEf, 0, 9, 0, 0x87},
    {"EffectKind87_Start", 0x488F80, 0x2A, E4A_CALLS(kCalls488F80), nullptr, 0, nullptr, 0, E4A_FN(EffectKind87_Start), 0, false, kEf, 0, 0, 0, 0x87},
    {"EffectKind87_Step1", 0x488FB0, 0x24, E4A_CALLS(kCalls488FB0), nullptr, 0, nullptr, 0, E4A_FN(EffectKind87_Step1), 0, false, kEf, 0, 0, 0, 0x87},
    {"EffectKind87_Step2", 0x488FE0, 0x12, E4A_CALLS(kCalls488FE0), nullptr, 0, nullptr, 0, E4A_FN(EffectKind87_Step2), 0, false, kEf, 0, 0, 0, 0x87},
    {"EffectKind87_Restore", 0x489000, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, E4A_FN(EffectKind87_Restore), 0, false, kEf, 0, 0, 0, 0x87},
    {"EffectKind87_End", 0x489020, 0xF, E4A_CALLS(kCalls489020), nullptr, 0, nullptr, 0, E4A_FN(EffectKind87_End), 0, false, kEf, 0, 0, 0, 0x87},
};
#undef E4A_FN
#undef E4A_CALLS
#undef E4A_N

constexpr unsigned kCount = sizeof kAll / sizeof kAll[0];
static_assert(kCount == 51, "the cut's 48 and the three added");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the effects -----------------------------------------------------------------------

// E4D's 0x48CA90: a full-screen tile coloured by Sprite_Current +0x5D..+0x5F -
// Sprite_Current and the three bytes logged.
U FxFade(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    sh::Note(Key(s), sh::InRegions(s + 0x5D, 3) ? static_cast<U>(s[0x5D] | s[0x5E] << 8 | s[0x5F] << 16) : 0u);
    return answer;
}
// EventOp_0x(op): as the real one leaves them, Sprite_Current the object the
// count word 0x903850 names (below 30: Sprite_Objects' record) and the count
// word up one - so a caller that reads Sprite_Current after it, not the record
// it kept, reads another record. The op's 16 bytes are hashed (deref).
U FxPlace(const U*, U answer) {
    unsigned char* const count = Mem(at::kObjectCount);
    const U n = Word(count);
    if (n < at::kSpriteCount) Sprite_Current = Sprite_Objects + at::kSpriteStride * n;
    SetWord(count, n + 1);
    return answer;
}

#define E4A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase, kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called by name: kind 0x85's tail
    {E4A_OURS(EffectKind85_ShowObjects), 0, {}, kPh, 0, 0},
    // later groups' of the round, by address (docs/effect_4a.md section 8)
    {"0x48CA90 (E4D)", at::kFade, at::kFade, 0, {}, kG, 0, 0, {}, &FxFade},
    {"0x489030 (E4B)", at::kKind87Setup, at::kKind87Setup, 0, {}, kPh, 0, 0},
    {"0x489220 (E4B)", at::kKind87Step, at::kKind87Step, 0, {}, kF, 0, 0},
    {"0x4891F0 (E4B)", at::kKind87Reset, at::kKind87Reset, 0, {}, kPh, 0, 0},
    // re-listed: the field-standard row leaves Sprite_Current where it was
    {E4A_OURS(EventOp_0x), 1, {kW}, kG, 0, 0, {16}, &FxPlace, nullptr, true},
};
#undef E4A_OURS

// The state tables the dispatchers jump through, read in place; each table's
// own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x654CE8, 24}, {0x654D48, 5}, {0x654D60, 5}, {0x654D74, 4}, {0x654E88, 9},
};
const std::uint8_t kKinds[] = {0x82, 0x83, 0x84, 0x85, 0x86, 0x87};

// Beyond effect mode's standard regions: kind 0x87's flag byte.
const sh::Region kRegions[] = {
    {at::kKind87Flag, 4},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }

// Every one of the 20 records the disturbance may move Sprite_Current among:
// the sprite bytes +3, +4 (kind 0x86) and +6 (kind 0x85) below 30 - past
// them both sides would write past Sprite_Objects, outside the regions (ours
// aborts there; section 7).
void Records() {
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        e[3] = static_cast<unsigned char>(sh::Next() % at::kSpriteCount);
        e[4] = static_cast<unsigned char>(sh::Next() % at::kSpriteCount);
        e[6] = static_cast<unsigned char>(sh::Next() % at::kSpriteCount);
    }
}

void Seed(unsigned) {
    Records();
    unsigned char* const s = Sprite_Current;
    // the counters against their compares
    Mem(at::kCounter)[0] = static_cast<unsigned char>(PickOf(0x35, 0x34, 0x36, 0, sh::Next()));
    Mem(at::kCounterB)[0] = static_cast<unsigned char>(PickOf(3, 4, 5, 0xFF, 0xFE, 0, 6, 2, sh::Next()));
    Mem(at::kCounterC)[0] = static_cast<unsigned char>(PickOf(0x80, 0, 0x10, 0xF, 0x11, 0x81, sh::Next()));
    // the frame count, the sub-count, the message
    s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 8, 0x1E, 0xFF, sh::Next()));
    s[2] = static_cast<unsigned char>(PickOf(0xE, 0xF, 0x10, 0, 0xFF, sh::Next()));
    s[0xB] = static_cast<unsigned char>(PickOf(0x36, 0x37, 0x35, 0x33, 0xFF, sh::Next()));
    // records 0 and 1 at, past and short of the leader's x
    const U leader = sh::Next();
    SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(leader));
    SetLong(Mem(at::kObject0X), static_cast<std::int32_t>(leader + PickOf(0, 1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u, 0x80000000u, sh::Next())));
    SetLong(Mem(at::kObject1X), static_cast<std::int32_t>(leader + PickOf(0, 1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u, 0x80000000u, sh::Next())));
    // the third member's x about the line (signed)
    SetLong(Mem(at::kObjTrio2X), static_cast<std::int32_t>(PickOf(0x340000, 0x33FFFF, 0x340001, 0, 0x80000000u, 0xFFFFFFFFu, sh::Next())));
    // the held word: the three that count, near ones, none
    SetWord(Mem(kInputHeld), PickOf(0x3000, 0x6000, 0x2000, 0x2001, 0x7000, 0x1000, 0, sh::Next()));
    Field_Request = static_cast<unsigned char>(PickOf(2, 0, 1, 3, sh::Next()));
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +9, the sprite bytes +3 / +4 /
// +6 (below 30), the counters, the third member's x, the message, the held
// word, Field_Request.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (sh::DisturbCase(h, 9)) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1); break;
    case 1: s[3 + (v & 1)] = static_cast<unsigned char>((v >> 1) % at::kSpriteCount); break;
    case 2: Mem(at::kCounterB)[0] = static_cast<unsigned char>((v & 3) == 0 ? 0xFEu : (v & 3) == 1 ? 0u : v >> 2); break;
    case 3: SetLong(Mem(at::kObjTrio2X), static_cast<std::int32_t>((v & 1) ? 0x340000u - 1 + ((v >> 1) & 1) : v)); break;
    case 4: s[0xB] = static_cast<unsigned char>((v & 1) ? 0x37u : v >> 1); break;
    case 5: SetWord(Mem(kInputHeld), (v & 1) ? 0x3000u : v >> 1); break;
    case 6: Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? 0x35u : v >> 1); break;
    case 7: Field_Request = static_cast<unsigned char>((v & 1) ? 2u : v >> 1); break;
    case 8: s[6] = static_cast<unsigned char>(v % at::kSpriteCount); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E4A_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    const char* const only = std::getenv("BOF3X_E4A_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) chosen[n++] = kAll[k];
    if (n == 0) bof3::Fatal("effect_4a: BOF3X_E4A_ONLY=%s names no clone", only);
    const char* const rounds = std::getenv("BOF3X_E4A_ROUNDS");
    sh::Group g = {"effect_4a", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   &Seed, &Disturb,
                   rounds && *rounds ? static_cast<unsigned>(std::strtoul(rounds, nullptr, 0)) : 4000u};
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_4a
