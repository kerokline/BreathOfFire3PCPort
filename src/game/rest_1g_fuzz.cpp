// BOF3X_SHADOW=rest_1g: group R1G's 45 functions through the scenario harness's
// field mode (scenario_harness.h, used unchanged; docs/scenario_harness.md
// sections 7 and 8), once at start-up. docs/rest_1g.md section 4.
// BOF3X_R1G_ONLY=<name> runs the clones whose name contains it (the controls).
//
// The clone table is tools/band_rows.py --group R1G --clones --harness scenario
// (2026-10-04) with the names given; every extent and call site agrees with
// the capstone read. The leader's steps run with Sprite_Current the leader's
// ObjTrio record (as E1E's, docs/effect_1e.md section 4), the fish with
// Sprite_Current and Field_ActiveMember a Sprite_Objects record - both kSprite
// in field mode, not effect mode. The four that take arguments are kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_1g.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_1g {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using sh::Shape;
using U = std::uint32_t;

// band_rows.py's call sites (2026-10-04), each checked against the capstone read.
constexpr sh::CallSite kCalls5289C0[] = {{0x1, 0x52B250}, {0x61, 0x52B2A0}, {0x7E, 0x589200}, {0x91, 0x5891F0}};
constexpr sh::CallSite kCalls528A90[] = {{0x75, 0x52D880}};
constexpr sh::JumpTable kTables528A90[] = {{0x1F, 0xFC, 5}};
constexpr sh::CallSite kCalls528BE0[] = {{0x20, 0x587740}, {0x30, 0x52D5C0}};
constexpr sh::CallSite kCalls528C20[] = {{0x23, 0x52D610}, {0x43, 0x52D5C0}, {0x69, 0x469750}, {0x9F, 0x516B30}};
constexpr sh::CallSite kCalls52ADA0[] = {{0x19, 0x516B30}, {0x3D, 0x587740}, {0x80, 0x587740}, {0xC5, 0x587740}};
constexpr sh::CallSite kCalls52AE80[] = {{0x1B, 0x516B30}, {0x48, 0x587740}};
constexpr sh::CallSite kCalls52AF80[] = {{0x83, 0x5A7650}, {0xB9, 0x5A8250}, {0xC2, 0x5A9110}, {0xED, 0x5A8250}, {0xF6, 0x5A9110}, {0x108, 0x461E50}};
constexpr sh::CallSite kCalls52B100[] = {{0xB, 0x495040}};
constexpr sh::CallSite kCalls52B160[] = {{0x2, 0x495040}};
constexpr sh::CallSite kCalls52B1B0[] = {{0x25, 0x587740}, {0x33, 0x587740}, {0x41, 0x587740}};
constexpr sh::CallSite kCalls52B200[] = {{0x2, 0x589330}, {0xC, 0x587740}, {0x3D, 0x5893A0}};
constexpr sh::CallSite kCalls52B330[] = {{0x13, 0x495040}};
constexpr sh::CallSite kCalls52B370[] = {{0x4, 0x52B460}};
constexpr sh::CallSite kCalls52B480[] = {{0x61, 0x5B93D2}, {0x1C2, 0x5B93D2}};
constexpr sh::CallSite kCalls52B750[] = {{0x1B, 0x589590}, {0x53, 0x5B93D2}, {0xA5, 0x5B93D2}, {0xDE, 0x5B93D2}, {0x114, 0x5B93D2}, {0x13C, 0x5720C0}, {0x178, 0x5891F0}};
constexpr sh::CallSite kCalls52B8F0[] = {{0x11, 0x52CC40}, {0x24, 0x52CA80}, {0x29, 0x5893A0}, {0x30, 0x5B93D2}, {0x86, 0x589590}, {0x9D, 0x587740}, {0xC6, 0x5891F0}, {0x100, 0x52CAC0}, {0x12C, 0x589330}, {0x14F, 0x5720C0}, {0x224, 0x52CA80}, {0x229, 0x5893A0}};
constexpr sh::CallSite kCalls52BB20[] = {{0x0, 0x5893A0}, {0x2B, 0x589590}, {0x6C, 0x5891F0}};
constexpr sh::CallSite kCalls52BBD0[] = {{0x13, 0x52CC40}, {0x26, 0x52CA80}, {0x2B, 0x5893A0}, {0x35, 0x5B93D2}, {0x19A, 0x52CAC0}, {0x1BD, 0x5720C0}, {0x23D, 0x589330}, {0x245, 0x52CCD0}, {0x263, 0x5B93D2}, {0x339, 0x589330}, {0x341, 0x5893A0}, {0x35C, 0x5B93D2}, {0x396, 0x52CA80}, {0x39B, 0x5893A0}};
constexpr sh::JumpTable kTables52BBD0[] = {{0xFF, 0x3A4, 4}};
constexpr sh::CallSite kCalls52BF90[] = {{0x7F, 0x5893A0}, {0xA3, 0x589590}, {0xDB, 0x5891F0}, {0x1D2, 0x52C9E0}, {0x23F, 0x52C9E0}, {0x2EA, 0x5B93D2}, {0x32A, 0x587740}, {0x346, 0x589590}, {0x375, 0x5891F0}, {0x389, 0x52C990}, {0x3A1, 0x5B93D2}, {0x3D3, 0x5B93D2}, {0x408, 0x5B93D2}, {0x4B4, 0x5B93D2}, {0x4BD, 0x5B93D2}, {0x506, 0x5B93D2}, {0x579, 0x5B93D2}, {0x5DB, 0x52C990}, {0x814, 0x589330}, {0x81C, 0x5893A0}};
constexpr sh::CallSite kCalls52C830[] = {{0x6D, 0x5893A0}};
constexpr sh::CallSite kCalls52C8D0[] = {{0x11, 0x587740}, {0x2D, 0x589590}, {0x54, 0x5891F0}};
constexpr sh::CallSite kCalls52C940[] = {{0x1C, 0x5893A0}};
constexpr sh::CallSite kCalls52C9E0[] = {{0x86, 0x5B93D2}};
constexpr sh::CallSite kCalls52CC40[] = {{0x6E, 0x531C70}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define G_FN(name) reinterpret_cast<const void*>(&::name)
#define G_CLONE(name, base, size, calls, shape, ret) #name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
#define G_TABLE(name, base, size, calls, tables) #name, base, size, calls, SH_N(calls), nullptr, 0, tables, SH_N(tables), G_FN(name), 0, false, Shape::kSprite
#define G_LEAF(name, base, size, shape, ret) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
constexpr Shape kSp = Shape::kSprite;
constexpr Shape kCa = Shape::kCall;
const sh::Clone kClones[] = {
    {G_LEAF(LeaderPanel_S1, 0x5289A0, 0x12, kSp, 0)},
    {G_CLONE(LeaderPanel_S1Begin, 0x5289C0, 0xA2, kCalls5289C0, kSp, 0)},
    {G_LEAF(LeaderPanel_S1Wait, 0x528A70, 0x12, kSp, 0)},
    {G_TABLE(LeaderPanel_S1Buttons, 0x528A90, 0x14D, kCalls528A90, kTables528A90)},
    {G_CLONE(LeaderPanel_S1Idle, 0x528BE0, 0x35, kCalls528BE0, kSp, 0)},
    {G_CLONE(LeaderPanel_S1Box3In, 0x528C20, 0xA8, kCalls528C20, kSp, 0)},
    {G_CLONE(ChoiceMenu_DataPage, 0x52ADA0, 0xE0, kCalls52ADA0, kSp, 0)},
    {G_CLONE(ChoiceMenu_RulePage, 0x52AE80, 0xAC, kCalls52AE80, kSp, 0)},
    {G_LEAF(LeaderPanel_S9Back, 0x52AF30, 0x29, kSp, 0)},
    {G_LEAF(LeaderPanel_S10, 0x52AF60, 0x12, kSp, 0)},
    {G_CLONE(LeaderPanel_S10Look, 0x52AF80, 0x12A, kCalls52AF80, kSp, 0)},
    {G_LEAF(LeaderPanel_S10End, 0x52B0B0, 0x29, kSp, 0)},
    {G_LEAF(LeaderPanel_S11, 0x52B0E0, 0x12, kSp, 0)},
    {G_CLONE(LeaderPanel_S11FadeOut, 0x52B100, 0x1C, kCalls52B100, kSp, 0)},
    {G_LEAF(LeaderPanel_S11Switch, 0x52B120, 0x36, kSp, 0)},
    {G_CLONE(LeaderPanel_S11FadeIn, 0x52B160, 0x1A, kCalls52B160, kSp, 0)},
    {G_LEAF(LeaderPanel_S11End, 0x52B180, 0x2A, kSp, 0)},
    {G_CLONE(LeaderPanel_PoseSound, 0x52B1B0, 0x48, kCalls52B1B0, kSp, 0)},
    {G_CLONE(LeaderPanel_UseItemEnd, 0x52B200, 0x42, kCalls52B200, kSp, 0)},
    {G_LEAF(LeaderPanel_SetRecords, 0x52B250, 0x43, kSp, 0)},
    {G_LEAF(LeaderPanel_Effect3Mode, 0x52B2A0, 0x34, kCa, 0)},
    {G_LEAF(LeaderPanel_HoldTest, 0x52B2E0, 0x4A, kSp, 0)},
    {G_CLONE(LeaderPanel_LeaveOnPress, 0x52B330, 0x35, kCalls52B330, kCa, 0xFFu)},
    {G_CLONE(LeaderPanel_EffectsStep, 0x52B370, 0xE2, kCalls52B370, kSp, 0)},
    {G_LEAF(LeaderPanel_PressLatch, 0x52B460, 0x1E, kSp, 0)},
    {G_CLONE(Fish_Spawn, 0x52B480, 0x23D, kCalls52B480, kSp, 0)},
    {G_LEAF(Fish_RunAll, 0x52B6C0, 0x84, kSp, 0)},
    {G_CLONE(Fish_Begin, 0x52B750, 0x192, kCalls52B750, kSp, 0)},
    {G_CLONE(Fish_Swim, 0x52B8F0, 0x230, kCalls52B8F0, kSp, 0)},
    {G_CLONE(Fish_Settle, 0x52BB20, 0xAF, kCalls52BB20, kSp, 0)},
    {G_TABLE(Fish_Approach, 0x52BBD0, 0x3B4, kCalls52BBD0, kTables52BBD0)},
    {G_CLONE(Fish_Hooked, 0x52BF90, 0x828, kCalls52BF90, kSp, 0)},
    {G_LEAF(Fish_S5, 0x52C7C0, 0x12, kSp, 0)},
    {G_LEAF(Fish_S5Center, 0x52C7E0, 0x44, kSp, 0)},
    {G_CLONE(Fish_S5Move, 0x52C830, 0x72, kCalls52C830, kSp, 0)},
    {G_LEAF(Fish_S6, 0x52C8B0, 0x12, kSp, 0)},
    {G_CLONE(Fish_S6Begin, 0x52C8D0, 0x65, kCalls52C8D0, kSp, 0)},
    {G_CLONE(Fish_S6Wait, 0x52C940, 0x21, kCalls52C940, kSp, 0)},
    {G_LEAF(Fish_S6Release, 0x52C970, 0x12, kSp, 0)},
    {G_LEAF(Fish_AdjustStrength, 0x52C990, 0x4F, kCa, 0)},
    {G_CLONE(Fish_Chance, 0x52C9E0, 0x9E, kCalls52C9E0, kCa, 0xFFu)},
    {G_LEAF(Fish_Step, 0x52CA80, 0x40, kSp, 0)},
    {G_LEAF(Fish_Heading, 0x52CAC0, 0x176, kSp, 0)},
    {G_CLONE(Fish_LureInReach, 0x52CC40, 0x8C, kCalls52CC40, kSp, 0xFFu)},
    {G_LEAF(Fish_LureClose, 0x52CCD0, 0x77, kSp, 0xFFu)},
};
#undef G_LEAF
#undef G_TABLE
#undef G_CLONE
#undef G_FN
#undef SH_N

enum : unsigned {
    kS1, kS1Begin, kS1Wait, kS1Buttons, kS1Idle, kS1Box3In, kDataPage, kRulePage, kS9Back, kS10, kS10Look, kS10End,
    kS11, kS11FadeOut, kS11Switch, kS11FadeIn, kS11End, kPoseSound, kUseItemEnd, kSetRecords, kEffect3Mode, kHoldTest,
    kLeaveOnPress, kEffectsStep, kPressLatch, kSpawn, kRunAll, kBegin, kSwim, kSettle, kApproach, kHooked, kS5,
    kS5Center, kS5Move, kS6, kS6Begin, kS6Wait, kS6Release, kAdjust, kChance, kStep, kHeading, kInReach, kClose, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");
bool Fishy(unsigned k) { return k >= kSpawn; }

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char* Sc() { return Sprite_Current; }

// The cells (rest_1g.cpp's).
constexpr U kEff0Flags = 0x7E11E0, kEff0State = 0x7E11E1, kEff0Depth = 0x7E11E7, kEff0X = 0x7E1214, kEff0Z = 0x7E1218,
            kEff0Height = 0x7E121E, kEff1State = 0x7E1261, kEff3State = 0x7E1361, kEff4State = 0x7E13E1,
            kEff5Frame = 0x7E1470, kEff5Level = 0x7E1490, kEff6State = 0x7E14E1, kEff6Dir = 0x7E14E8,
            kEff6Page = 0x7E151C, kEff6Shown = 0x7E151E;
constexpr U kLeaderStage = 0x802D42, kLeaderStep = 0x802D43, kSpriteIndex = 0x939A1C, kRecordA = 0x939A20,
            kRecordB = 0x939A24, kItemB = 0x90412E, kItemA = 0x904130, kWarned = 0x6BC709, kPressNow = 0x6BC717,
            kComboStep = 0x6BC70C, kComboTimer = 0x6BC710, kPoseWas = 0x6BC71C, kPose = 0x6BC71D, kBiteHeld = 0x6BC716,
            kSpot = 0x905B88, kSpotCounts = 0x66A9CC, kActive = 0x9035A4;

unsigned char* Active() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(kActive))))); }
unsigned char* RecordA() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(kRecordA))))); }
unsigned char* RecordB() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(kRecordB))))); }

// --- the stand-ins' effects (Noise() and the state only: both passes the same) -----

// AreaMap_Elevation: two answers in three at Sprite_Current's height word, give
// or take one (the fish compare the two as s16).
U ElevationEffect(const U*, U answer) {
    const U h = sh::Noise();
    unsigned char* const s = Sc();
    if (h % 3 == 0 || !sh::InRegions(s + 0x3E, 2)) return answer;
    return (answer & 0xFFFF0000u) | ((Word(s + 0x3E) + (h >> 4) % 3 - 1) & 0xFFFFu);
}
// Fish_Heading: the direction +8 it reads is logged (the real one turns it into
// the steps), and half the time it writes a new one, which its callers read
// after it.
U HeadingEffect(const U*, U answer) {
    unsigned char* const s = Sc();
    if (!sh::InRegions(s + 8, 1)) return answer;
    sh::Note(s[8]);
    const U n = sh::Noise();
    if (n & 1) s[8] = static_cast<unsigned char>((n >> 1) % 9);
    return answer;
}
// LeaderPanel_PressLatch: 0x6BC717, which LeaderPanel_EffectsStep reads after it.
U PressLatchEffect(const U*, U answer) {
    Mem(kPressNow)[0] = static_cast<unsigned char>(sh::Noise() % 3);
    return answer;
}
// Louder on Fish_Hooked's and Fish_Swim's paths (2026-10-05, debt 18): their
// re-reads after a Rand or Fish_LureInReach follow one to four calls, and the
// group's case alone (about one call in 430) needed 60,000 rounds a function
// to refuse D09..D11 and D13 (docs/rest_1g.md section 6). Rand, the harness's
// kRand row re-listed, a quarter of the time moves one of the three cells
// read again after it - the member's strength word +0x98 / +0x9A, record 5's
// frame, the leader's stage (3 and 4, Fish_Swim's test) - as the group's cases
// 6, 12 and 9 do; Fish_LureInReach a quarter of the time the leader's stage.
void FlipStage() { Mem(kLeaderStage)[0] = static_cast<unsigned char>(Mem(kLeaderStage)[0] == 4 ? 3 : 4); }
U RandEffect(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 4 != 0) return answer;
    const auto b = static_cast<unsigned char>(n >> 24);
    switch ((n >> 2) % 3) {
    case 0: {
        unsigned char* const am = Active();
        if (sh::InRegions(am + 0x98, 4)) SetWord(am + (b & 1 ? 0x98 : 0x9A), (n >> 8) & 0x1FF);
        break;
    }
    case 1: SetLong(Mem(kEff5Frame), b & 1 ? -7 : static_cast<std::int32_t>(b >> 1)); break;
    default: FlipStage(); break;
    }
    return answer;
}
U LureInReachEffect(const U*, U answer) {
    if (sh::Noise() % 4 == 0) FlipStage();
    return answer;
}
// Gfx_CommitPrim: the packet cursor on by the primitive's 0x20, as the real one
// moves it (so each line is drawn into a packet of its own).
U CommitEffect(const U*, U answer) {
    unsigned char* const p = sh::Pointer(sh::at::kPacketNext);
    if (sh::InRegions(p + 0x20, 0x20)) sh::SetPointer(sh::at::kPacketNext, p + 0x20);
    return answer;
}

#define G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kAll = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called directly (E8) by the group's
    {G_OURS(LeaderPanel_SetRecords), 0, {}, kG, 0, 0},
    {G_OURS(LeaderPanel_Effect3Mode), 1, {0xFF}, kG, 0, 0},                       // mov al, [esp + 4]
    {G_OURS(LeaderPanel_PressLatch), 0, {}, kG, 0, 0, {}, &PressLatchEffect},
    {G_OURS(Fish_LureInReach), 0, {}, kF, 0, 0, {}, &LureInReachEffect},
    {G_OURS(Fish_LureClose), 0, {}, kF, 0, 0},
    {G_OURS(Fish_Step), 0, {}, kG, 0, 0},
    {G_OURS(Fish_Heading), 0, {}, kG, 0, 0, {}, &HeadingEffect},
    {G_OURS(Fish_AdjustStrength), 1, {0xFF}, kG, 0, 0},                           // movsx cx, byte [esp + 4]
    {G_OURS(Fish_Chance), 2, {0xFF, 0xFF}, kF, 0, 0},                              // and ecx, 0xFF; imul byte [esp + 8]
    // ours, with the width each reads
    {G_OURS(Sound_PlayEffect), 1, {0xFFFF}, kG, 0, 0},
    {G_OURS(Sprite_ScriptTick), 0, {}, kF, 0, 0},                                  // two callers test al
    {G_OURS(FieldPanel_DrawShade), 0, {}, kG, 0, 0},
    {G_OURS(FieldPanel_DrawBox3), 2, {kAll, kAll}, kG, 0, 0},
    {G_OURS(Panel_DrawWindow), 5, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},   // 0x469790 / 0x469960 read & 0xFFFF (E1E)
    {G_OURS(Text_DrawAt), 5, {0xFFFF, 0xFFFF, 0xFF, kAll, kAll}, kG, 0, 0},      // x, y words, colour & 0xFF (E1E)
    {G_OURS(Sprite_SetAnimationBank), 1, {0xFFFF}, kG, 0, 0},                      // unsigned short: the callers push cx / ax over leftovers
    {G_OURS(Sprite_SetAnimation), 1, {0xFF}, kG, 0, 0},
    {G_OURS(Sprite_SetAnimationAt), 2, {0xFF, 0xFFFF}, kG, 0, 0},
    {G_OURS(Sprite_EnsureAnimation), 1, {0xFF}, kG, 0, 0},
    {G_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0, {}, &ElevationEffect},
    {G_OURS(Sprite_PointInReach), 5, {kAll, kAll, 0xFFFF, kAll, kAll}, kF, 0, 0},  // z a short: Fish_LureInReach pushes edx over its caller's
    {G_OURS(Inventory_Holds38To4DAt99), 0, {}, kF, 0, 0},
    {G_OURS(Transition_Start), 1, {0xFF}, kG, 0, 0},
    {G_OURS(Gpu_SetLineF2), 1, {kAll}, kG, 0, 0},
    {G_OURS(Gte_RotTransPers), 3, {0, kAll, 0}, kG, 0, 0, {6, 0, 0}},             // the vertex (three shorts, on the stack: hashed), the packet's sxy; p a stack cell never read
    {G_OURS(Gte_StoreDepthF), 1, {kAll}, kG, 0, 0},
    {G_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    // Capcom's, the standard kRand row with an effect (above)
    {"Rand", KeyOf(Rand), KeyOf(Rand), 0, {}, sh::Answer::kRand, 0, 0, {}, &RandEffect},
};
#undef G_OURS

// The tables the dispatchers read in place, swapped for recorders on both sides;
// each count is the run of code pointers to the next table (docs/rest_1g.md
// section 1).
const sh::DataTable kTables[] = {
    {Key(LeaderPanel_Stage1Steps), LeaderPanel_Stage1Steps_count},
    {Key(LeaderPanel_Stage10Steps), LeaderPanel_Stage10Steps_count},
    {Key(LeaderPanel_Stage11Steps), LeaderPanel_Stage11Steps_count},
    {Key(Fish_States), Fish_States_count},
    {Key(Fish_S5Steps), Fish_S5Steps_count},
    {Key(Fish_S6Steps), Fish_S6Steps_count},
};

// Beyond field mode's standard regions: the cells 0x939A00.. (the sprite
// index, the two record pointers), 0x6BC700.. (the press latch, the sequences,
// the nibble, the poses), Game_Mode / Game_Step, Field_ActiveMember, the four
// draw-mode bytes 0x93985C.
const sh::Region kRegions[] = {
    {0x939A00, 0x30},
    {0x6BC700, 0x20},
    {0x66C7E8, 4},
    {kActive, 4},
    {0x93985C, 4},
};

// --- the seed ---------------------------------------------------------------------------

// A spot whose counts fit the 30 records (the original writes past them; ours
// aborts): 0..15 by the table, read now (the same on both passes).
unsigned Spot() {
    for (int tries = 0; tries < 8; ++tries) {
        const unsigned spot = sh::Next() % 16;
        unsigned sum = 0;
        for (unsigned k = 0; k < 23; ++k) sum += Mem(kSpotCounts + 23 * spot + k)[0];
        if (sum <= 30) return spot;
    }
    return 0;
}

std::int32_t Signed(U v) { return static_cast<std::int32_t>(v); }

void SeedFish(unsigned char* r) {
    r[0] = static_cast<unsigned char>(PickOf(0x21, 0x21, 0x01, 0xA1, 0x81, 0x20, sh::Next()));
    r[1] = static_cast<unsigned char>(sh::Next() % 7);
    r[2] = static_cast<unsigned char>(sh::Next() % 3);
    r[4] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3, 3, sh::Next()));
    r[5] = static_cast<unsigned char>(sh::Next() % 30);
    r[6] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 23 : PickOf(0x15, 0x16, 0x16, 9, 0x14));
    r[8] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : PickOf(8, 9, 0xFF, sh::Next()));
    r[9] = static_cast<unsigned char>(PickOf(0, 0, 0, 1, 2, 0x10, sh::Next()));
    r[0xA] = static_cast<unsigned char>(PickOf(0, 1, 0xFF, 0x30, 0x31, 0x2F, 0xD0, 0xCF, 0xD1, 0x10, 0xF0, sh::Next()));
    r[0xB] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, sh::Next()));
    static const U kSteps[] = {0, 0, 0x800, 0xFFFFF800u, 0x801, 0xFFFFF7FFu, 0x1000, 0xFFFFF000u, 0x400, 0xFFFFFC00u,
                               0x401, 1, 0xFFFFFFFFu, 2, 5, 6, 0x80000000u, 0x7FFFFFFFu};
    SetLong(r + 0xC, Signed(sh::Half() ? sh::Pick(kSteps, sizeof kSteps / sizeof kSteps[0]) : sh::Next()));
    SetLong(r + 0x10, Signed(sh::Often() ? sh::Pick(kSteps, sizeof kSteps / sizeof kSteps[0]) : sh::Next()));
    SetLong(r + 0x14, Signed(PickOf(0, 2, 0xFFFFFFFEu, 1, sh::Next())));
    r[0x1C] = static_cast<unsigned char>(PickOf(0, 1, 0xFF, sh::Next()));
    SetLong(r + 0x34, Signed(PickOf(0x60000, 0x60001, 0x5FFFF, 0x80000, 0x80001, 0x140000, 0x13FFFF, 0xF0000,
                                    sh::Next() % 0x200000, sh::Next() % 0x200000, sh::Next())));
    SetLong(r + 0x38, Signed(PickOf(0x8000, 0x8001, 0x150000, 0x150001, 0x3C0000, 0x3C0001, 0x1A0000, 0x19FFFF,
                                    0x3E0000, 0x3DFFFF, 0x110000, 0x320000, 0x3E8000, 0x3F0000, sh::Next() % 0x400000,
                                    sh::Next() % 0x400000, sh::Next())));
    // 0, -0x20 (the landed / jump height), -0x40 (the jump's floor), -0x200
    // (the shade's floor); -0x100 L - 0x20 and -0x100 (L + 1) + 0x20 (the
    // rise's and the dive's bounds for the kind levels L 0..3), each +/- 1
    static const U kRise[] = {0xFFE0, 0xFEE0, 0xFDE0, 0xFCE0, 0xFF20, 0xFE20, 0xFD20, 0xFC20};
    const U edge = kRise[sh::Next() % 8] + PickOf(0, 0, 1, 0xFFFF);
    SetWord(r + 0x3E, PickOf(0, 0, 0xFFE0, 0xFFC0, 0xFFC1, 0xFFBF, 0xFE00, 0xFDFF, 0xFE07, 1, 8, 0x7FFF, 0x8000, edge, edge,
                             sh::Next() % 0x400 - 0x200, sh::Next()));
    SetWord(r + 0x58, PickOf(0x14, 0x15, 0x13, 0x10, 0x12, 0xE, sh::Next() & 0x1F, sh::Next()));
    // what Field_ActiveMember is read for (+0x81, +0x98..+0x9E)
    const U most = PickOf(sh::Next() % 0x100, sh::Next() % 0x100, 0, 0x40, 0x7FFF, 0xFFF0, 0x8000, sh::Next());
    SetWord(r + 0x98, most);
    const auto m = static_cast<short>(most);
    SetWord(r + 0x9A, PickOf(m, m >> 1, (m >> 1) - 1, m >> 2, (m >> 2) - 1, (m >> 2) * 3, (m >> 2) * 3 - 1, m >> 4,
                             (m >> 4) + 1, 0, sh::Next()));
    r[0x81] = static_cast<unsigned char>(PickOf(0, 1, 2, 0xFF, 0xFE, 0xFD, sh::Next()));
    SetWord(r + 0x9C, PickOf(sh::Next() & 0xFF, sh::Next()));
    r[0x9E] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
}

void SeedLeader(unsigned char* r) {
    r[2] = static_cast<unsigned char>(sh::Next());
    r[3] = static_cast<unsigned char>(sh::Next());
    r[4] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
    r[6] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    r[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 4, 5, sh::Next()));
    r[0xA] = static_cast<unsigned char>(PickOf(0, 1, 8, sh::Next()));
}

// The dispatchers' table lengths (their index drawn below it, as each reads it
// before any call).
unsigned Entries(unsigned k) {
    switch (k) {
    case kS1: return LeaderPanel_Stage1Steps_count;
    case kS10: return LeaderPanel_Stage10Steps_count;
    case kS11: return LeaderPanel_Stage11Steps_count;
    case kS5: return Fish_S5Steps_count;
    case kS6: return Fish_S6Steps_count;
    default: return 0;
    }
}

unsigned char KindByte(unsigned kind, unsigned at) { return Mem(0x66A690 + 36 * kind + at)[0]; }

// Fish_AdjustStrength's delta, drawn by the seed (so that the strength word can
// be planted at its negation) and handed over by Args.
unsigned char g_delta;

// Each function's own boundaries, two times in three (after the general seed):
// the conditions its branches stand behind, which the general draws meet only
// rarely together.
void SeedFor(unsigned k, unsigned char* s) {
    if (!sh::Often()) return;
    unsigned char* const am = Active();
    switch (k) {
    case kSwim:
        s[9] = 0;
        if (sh::Half()) SetWord(s + 0x3E, 0);
        Mem(kBiteHeld)[0] = 0;
        break;
    case kApproach:
    case kInReach:
    case kClose: {
        s[9] = 0;
        Mem(kEff0State)[0] = 4;
        Mem(kSpriteIndex)[0] = 0xFF;
        Mem(kEff4State)[0] = 0;
        Mem(kLeaderStage)[0] = 3;
        // the lure's level against the kind's: -2..4 around it
        const unsigned nib = RecordB()[0xE] & 0xF;
        Mem(kEff0Depth)[0] = static_cast<unsigned char>(KindByte(s[6], 0x14 + nib) + PickOf(0xFE, 0xFF, 0, 1, 2, 3, 4));
        break;
    }
    case kHooked: {
        s[4] = static_cast<unsigned char>(PickOf(0, 1, 2, 3));
        Mem(kEff4State)[0] = 0;
        if (sh::Half()) s[9] = 0;
        // record 5's level at the tension's gap boundaries (-0x30, -0x18)
        const auto t = static_cast<signed char>(s[0xA]);
        SetWord(Mem(kEff5Level), static_cast<U>(t + static_cast<int>(PickOf(0x30, 0x2F, 0x31, 0x18, 0x17, 0x19, 0, 1, 0xFFFFFFFFu))));
        if (sh::Half()) SetWord(s + 0x3E, PickOf(0xFFC0, 0xFFC1, 0xFFBF));
        break;
    }
    case kAdjust:
        if (sh::InRegions(am + 0x98, 4)) {
            const auto d = static_cast<signed char>(g_delta);
            SetWord(am + 0x9A, static_cast<U>(-d + static_cast<int>(PickOf(0, 0, 1, 0xFFFFFFFFu))));
            if (sh::Half()) SetWord(am + 0x98, PickOf(0xFFF0, 0x8000, 0xFFFF, 0, 1));
        }
        break;
    default: break;
    }
}

void Seed(unsigned k) {
    g_delta = static_cast<unsigned char>(PickOf(0xFB, 5, 1, 0xFF, 0x80, 0x7F, sh::Next()));
    for (unsigned i = 0; i < 3; ++i) SeedLeader(sh::ObjectOf(i));
    for (unsigned i = 0; i < 4; ++i) SeedFish(sh::SpriteRecord(i));
    // Fish_RunAll walks all 30: each in use or not, its state below the table's 7
    for (unsigned i = 4; i < 30; ++i) {
        unsigned char* const r = Sprite_Objects + 0xA4 * i;
        r[0] = static_cast<unsigned char>(PickOf(0, 0, 0x21, 0xA1, 1, sh::Next()));
        r[1] = static_cast<unsigned char>(sh::Next() % 7);
    }
    if (Fishy(k)) {
        Sprite_Current = sh::SpriteRecord(sh::Next());
        SetLong(Mem(kActive), Signed(Key(sh::Often() ? Sc() : sh::SpriteRecord(sh::Next()))));
    } else {
        Sprite_Current = sh::Often() ? sh::ObjectOf(0) : sh::SpriteRecord(sh::Next());
        SetLong(Mem(kActive), Signed(Key(sh::SpriteRecord(sh::Next()))));
    }
    unsigned char* const s = Sc();
    if (const unsigned n = Entries(k)) s[k == kS5 || k == kS6 ? 2 : 3] = static_cast<unsigned char>(sh::Next() % n);
    // the leader's stage and step as the fish see them (ObjTrio record 0 +2 / +3;
    // the leader's own dispatchers have their index from the line above)
    if (Fishy(k)) {
        Mem(kLeaderStage)[0] = static_cast<unsigned char>(PickOf(3, 3, 3, 4, 6, 7, sh::Next()));
        Mem(kLeaderStep)[0] = static_cast<unsigned char>(PickOf(0, 9, 10, 0xA, sh::Next()));
    }
    Mem(kSpriteIndex)[0] = static_cast<unsigned char>(PickOf(0xFF, 0xFF, 0xFF, sh::Next() % 30, sh::Next()));
    // the record pointers into the scratch buffers (random bytes, compared)
    sh::SetPointer(kRecordA, sh::Scratch(0) + sh::Next() % 0x30);
    sh::SetPointer(kRecordB, sh::Scratch(1) + sh::Next() % 0x28);
    unsigned char* const a = RecordA();
    unsigned char* const b = RecordB();
    a[3] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
    a[8] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, s[0xB], s[0xB] + 1u, sh::Next()));
    a[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, s[0xB], s[0xB] + 1u, sh::Next()));
    b[0xE] = static_cast<unsigned char>(PickOf(4, 0x84, 0x80, sh::Next() % 16, sh::Next()));
    b[0xF] = static_cast<unsigned char>(PickOf(0, 1, 0xFF, 2, 0xFE, sh::Next()));
    // the item bytes, the warning, the press latch and the sequences, the poses
    Mem(kItemB)[0] = static_cast<unsigned char>(PickOf(0, 0x1C, 0x1D, 0x20, sh::Next()));
    Mem(kItemA)[0] = static_cast<unsigned char>(PickOf(0, 0x2E, 0x2F, 0x31, sh::Next()));
    Mem(kWarned)[0] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    Mem(kPressNow)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
    for (unsigned i = 0; i < 4; ++i) {
        Mem(kComboStep + i)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3, sh::Next() % 8));
        Mem(kComboTimer + i)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, 3, 4, 5, 0xC, sh::Next()));
    }
    Mem(kPoseWas)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
    Mem(kPose)[0] = static_cast<unsigned char>(sh::Half() ? Mem(kPoseWas)[0] : PickOf(0, 1, 2, 3, sh::Next()));
    Mem(kBiteHeld)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    // the effect records' cells, at their boundaries; record 0 near the fish
    Mem(kEff0Flags)[0] = static_cast<unsigned char>(sh::Next());
    Mem(kEff0State)[0] = static_cast<unsigned char>(PickOf(4, 4, 5, 0, sh::Next()));
    Mem(kEff0Depth)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 0xFF, sh::Next()));
    const U dxz[] = {0, 0, 1, 0xFFFFFFFFu, 0x7FFF, 0x8000, 0xFFFF8001u, 0xFFFF8000u, 0x10000, 0xFFFF0000u};
    SetLong(Mem(kEff0X), Signed(static_cast<U>(Long(s + 0x34)) + (sh::Often() ? sh::Pick(dxz, 10) : sh::Next())));
    SetLong(Mem(kEff0Z), Signed(static_cast<U>(Long(s + 0x38)) + (sh::Often() ? sh::Pick(dxz, 10) : sh::Next())));
    SetWord(Mem(kEff0Height), Word(s + 0x3E) + PickOf(0, 0, 1, 0xFFFF, 0x3FF, 0x400, 0xFC01, 0xFC00, 0x10, sh::Next()));
    Mem(kEff1State)[0] = static_cast<unsigned char>(PickOf(1, 1, 0, 4, sh::Next()));
    Mem(kEff3State)[0] = static_cast<unsigned char>(PickOf(0, 0, 4, 6, 7, sh::Next()));
    Mem(kEff4State)[0] = static_cast<unsigned char>(PickOf(0, 0, 3, 1, sh::Next()));
    SetLong(Mem(kEff5Frame), Signed(PickOf(0xFFFFFFF9u, 0xFFFFFFFFu, 0, 1, sh::Next())));
    SetWord(Mem(kEff5Level), PickOf(0, 0x10, 0xFFF0, 0x20, s[0xA], s[0xA] + 1u, static_cast<U>(static_cast<signed char>(s[0xA])) - 1u,
                                    sh::Next() & 0x3F, sh::Next()));
    Mem(kEff6State)[0] = static_cast<unsigned char>(PickOf(8, 0xE, 0, 3, 8, 0xE, sh::Next()));
    Mem(kEff6Dir)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, 0xFF, sh::Next()));
    SetWord(Mem(kEff6Page), PickOf(0, 4, 5, 6, 0x16, 0x17, 0xFFFF, sh::Next()));
    SetWord(Mem(kEff6Shown), PickOf(0, 0x15, 0x16, 0x17, 0xFFFF, 0x8000, sh::Next()));
    Mem(kSpot)[0] = static_cast<unsigned char>(Spot());
    // the field's cells
    Field_State[0x89] = static_cast<unsigned char>(PickOf(0, sh::Next()));
    Field_Kind2Hold = static_cast<unsigned char>(PickOf(0, 0, sh::Next()));
    Field_Kind2Z = static_cast<long>(PickOf(0x150000, 0x150000, 0x150001, 0x3C0000, sh::Next()));
    Frame_Counter = sh::Half() ? sh::Next() & ~0xFu : sh::Next();
    MoveScript_WaitWordDA = static_cast<unsigned short>(PickOf(0, 0, sh::Next()));
    Input_Pressed = static_cast<unsigned short>(PickOf(0x04, 0x10, 0x20, 0x40, 0x74, 0x24, 0x84, 0x2000, 0x8000, 0x4000,
                                                       0x1000, 0xA000, 0xE020, 0x60, 0, sh::Next()));
    Input_Held = static_cast<unsigned short>(PickOf(0x40, 0x4000, 0x4040, 0, sh::Next()));
    Field_CancelButtons = static_cast<unsigned short>(PickOf(0x10, 0x20, 0x40, 0x2000, sh::Next()));
    sh::SetRandHint(PickOf(0, 1, 2, 3, 4, 8, 0xC, 0xF, 0x10, 0x20, 0x40, 0x80, sh::Next()));
    if (sh::Half()) Frame_Counter = (Frame_Counter & ~0xFu) | PickOf(0, 8, 1, 4);
    SeedFor(k, s);
}

// The arguments of the four kCall functions, after the seed.
void Args(unsigned k, U* a) {
    switch (k) {
    case kLeaveOnPress: a[0] = PickOf(0x60, 0x20, 0x40, 0x10000, a[0]); break;
    case kAdjust: a[0] = (a[0] & 0xFFFFFF00u) | g_delta; break;
    case kChance: {
        const auto t = static_cast<signed char>(Sc()[0xA]);
        a[0] = (a[0] & 0xFFFFFF00u) | (PickOf(t, t + 1, t - 1, t + 0x10, t - 0x10, 0x30, 0xD0, a[0]) & 0xFF);
        a[1] = (a[1] & 0xFFFFFF00u) | PickOf(1, 2, 1, 2, a[1] & 0xFF);
        break;
    }
    default: break;
    }
}

// --- the disturbance: a cell these read again after a call ------------------------------
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 24);
    const U v = h >> 8;
    unsigned char* const s = Sc();
    const bool sc = sh::InRegions(s, 0xA4);
    unsigned char* const am = Active();
    switch (sh::DisturbCase(h, 18)) {
    case 0: if (sc) s[9] = static_cast<unsigned char>(b % 3); break;
    case 1: if (sc) s[0xA] = static_cast<unsigned char>(b & 1 ? b : (b >> 1) % 0x62 - 0x31); break;
    case 2: if (sc) s[8] = static_cast<unsigned char>(b & 1 ? (b >> 1) & 7 : b); break;
    case 3: if (sc) SetWord(s + 0x3E, b & 1 ? 0 : v & 0x3FF); break;
    case 4: if (sc) s[4] = static_cast<unsigned char>(b % 4); break;
    case 5: if (sc) SetLong(s + 0x10, static_cast<std::int32_t>(b % 8) - 1); break;
    case 6: if (sh::InRegions(am + 0x98, 4)) SetWord(am + (b & 1 ? 0x98 : 0x9A), v & 0x1FF); break;
    case 7: Mem(kEff0State)[0] = static_cast<unsigned char>(b & 1 ? 4 : b); break;
    case 8: Mem(kSpriteIndex)[0] = static_cast<unsigned char>(b & 1 ? 0xFF : b % 30); break;
    case 9: Mem(kLeaderStage)[0] = static_cast<unsigned char>(b & 1 ? 3 : b % 8); break;
    case 10: Mem(kBiteHeld)[0] = static_cast<unsigned char>(b & 1); break;
    case 11: Mem(kPressNow)[0] = static_cast<unsigned char>(b % 3); break;
    case 12: SetLong(Mem(kEff5Frame), b & 1 ? -7 : static_cast<std::int32_t>(b >> 1)); break;
    case 13: Mem(kEff4State)[0] = static_cast<unsigned char>(b & 1 ? 0 : b); break;
    case 14: Field_Kind2Hold = static_cast<unsigned char>(b & 1 ? 0 : b); break;
    case 15: if (sc) s[6] = static_cast<unsigned char>(b % 23); break;
    case 16: SetWord(Mem(kEff5Level), v & 0x7F); break;
    case 17: {
        unsigned char* const rb = RecordB();
        if (sh::InRegions(rb + 0xE, 2)) rb[0xE + (b & 1)] = static_cast<unsigned char>(b >> 1);
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R1G_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R1G_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kClones[k].name, only)) {
            index[n] = k;
            chosen[n++] = kClones[k];
        }
    if (n == 0) bof3::Fatal("rest_1g: BOF3X_R1G_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    // 6,000 rounds a function (BOF3X_R1G_ROUNDS for a control's run): 60,000 on
    // 2026-10-05 for D09..D11 and D13, back to 6,000 with the louder Rand and
    // Fish_LureInReach above (docs/rest_1g.md section 6)
    const char* const rounds = std::getenv("BOF3X_R1G_ROUNDS");
    sh::Group g = {"rest_1g", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb,
                   rounds && *rounds ? static_cast<unsigned>(std::strtoul(rounds, nullptr, 10)) : 6000u};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    g.sprite_span = 7;   // +1..+4 below Fish_States' 7, which Fish_RunAll reads after a call
    sh::Run(g);
}

}  // namespace rest_1g
