// BOF3X_SHADOW=field_c3: group FC3's 63 functions through the scenario
// harness's field mode (scenario_harness.h, docs/scenario_harness.md section
// 7), once at start-up. docs/field_c3.md section 4.
//
// The clone table is tools/band_rows.py --group FC3 --clones (2026-09-29) with
// the names given and three corrections read from the code: 0x5253E0 ends at
// its tail jmp (0xB3 bytes, not 0x2BB: 0x5254A0 after it is a function of its
// own, reached by that jmp, by 0x5256A0's and by FieldCore_ScriptMoveSteps[1]),
// 0x5254A0 added (0x1FB bytes), and the five FieldCore_State2Steps dispatchers
// no list had (0x525CA0, 0x5261E0, 0x526490, 0x526A90, 0x526B80). Every call
// site was re-listed by the same capstone pass over the final extents. Shapes:
//
//   kState   the mode frames (no Sprite_Current)
//   kCall    MoveCmd_OpE7 (a byte), the approach / avoid picks and the best of
//            four turns (a sprite record; al), FieldCore_TileD0Probe (x, z, a
//            scratch word, the direction; al)
//   kSprite  the rest: the fade cases and the field core's states, run on
//            Sprite_Current (FieldCore_TileD0Slope answers al)
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_c3.h"
#include "game/field_c3_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_c3 {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using sh::Arg;
using sh::ArgAt;
using sh::Shape;
using U = std::uint32_t;

// band_rows.py's call sites, re-read over the final extents (2026-09-29).
constexpr sh::CallSite kCalls5172C0[] = {{0x0, 0x517350}, {0x5, 0x56E6C0}, {0xA, 0x536F10}, {0xF, 0x57B780}, {0x14, 0x531B60}, {0x19, 0x5372E0}, {0x1E, 0x494030}, {0x23, 0x454AD0}, {0x28, 0x592F00}};
constexpr sh::CallSite kCalls517330[] = {{0x0, 0x42D710}, {0x5, 0x59E230}};
constexpr sh::CallSite kCalls517340[] = {{0x0, 0x57DFF0}, {0x5, 0x59E230}};
constexpr sh::CallSite kCalls518E20[] = {{0x4C, 0x518F80}, {0x92, 0x518DD0}, {0x98, 0x518CA0}};
constexpr sh::CallSite kCalls518ED0[] = {{0x4E, 0x518F80}, {0x94, 0x518DD0}, {0x9A, 0x518CA0}};
constexpr sh::CallSite kCalls518F80[] = {{0x6E, 0x518080}};
constexpr sh::CallSite kCalls5193D0[] = {{0xE, 0x454CC0}};
constexpr sh::CallSite kCalls519410[] = {{0x94, 0x454D60}};
constexpr sh::CallSite kCalls519500[] = {{0xE, 0x454CC0}};
constexpr sh::CallSite kCalls519530[] = {{0x97, 0x454D60}};
constexpr sh::CallSite kCalls525390[] = {{0x36, 0x5893A0}, {0x3B, 0x589410}};
constexpr sh::CallSite kCalls5253E0[] = {{0x5A, 0x5725C0}, {0x75, 0x589330}, {0x98, 0x589330}, {0xA1, 0x536650}, {0xAE, 0x5254A0}};
constexpr sh::CallSite kCalls5254A0[] = {{0x2F, 0x576B50}, {0x79, 0x589330}, {0x8D, 0x534EC0}, {0xEF, 0x589330}, {0x10F, 0x534590}, {0x144, 0x589330}, {0x161, 0x534590}, {0x17A, 0x589330}, {0x1C8, 0x52E140}, {0x1EF, 0x589330}};
constexpr sh::CallSite kCalls5256A0[] = {{0x12, 0x52E140}, {0x27, 0x5254A0}, {0x38, 0x5345E0}, {0x5C, 0x52E140}};
constexpr sh::CallSite kCalls525740[] = {{0x2, 0x534880}};
constexpr sh::CallSite kCalls525770[] = {{0x2, 0x534790}};
constexpr sh::CallSite kCalls5257A0[] = {{0xC0, 0x578EB0}, {0xF8, 0x525390}};
constexpr sh::CallSite kCalls525980[] = {{0xD, 0x589330}, {0x2F, 0x589330}};
constexpr sh::CallSite kCalls5259D0[] = {{0x1, 0x589410}, {0x107, 0x589330}, {0x114, 0x587740}, {0x139, 0x52E140}};
constexpr sh::CallSite kCalls525B20[] = {{0x41, 0x52E140}, {0x57, 0x5725F0}, {0x5F, 0x5893A0}};
constexpr sh::CallSite kCalls525B90[] = {{0x10, 0x52E140}, {0x22, 0x572570}, {0x5D, 0x589330}, {0xAC, 0x5725F0}, {0xB4, 0x5893A0}};
constexpr sh::CallSite kCalls525C50[] = {{0x0, 0x589410}, {0x9, 0x534920}, {0x1B, 0x589330}};
constexpr sh::CallSite kCalls525CE0[] = {{0x0, 0x535FC0}};
constexpr sh::CallSite kCalls525CF0[] = {{0x17, 0x572570}, {0x3C, 0x5893A0}};
constexpr sh::CallSite kCalls525D40[] = {{0xD, 0x572570}, {0x8C, 0x589330}, {0xA0, 0x534590}};
constexpr sh::CallSite kCalls525E30[] = {{0x17, 0x572570}, {0x48, 0x536290}, {0x56, 0x5893A0}};
constexpr sh::CallSite kCalls525E90[] = {{0x0, 0x5362D0}};
constexpr sh::CallSite kCalls525EB0[] = {{0x0, 0x5363C0}};
constexpr sh::CallSite kCalls525ED0[] = {{0x0, 0x536440}};
constexpr sh::CallSite kCalls525F10[] = {{0x0, 0x535FE0}};
constexpr sh::CallSite kCalls525F30[] = {{0x0, 0x536050}};
constexpr sh::CallSite kCalls525F50[] = {{0x0, 0x5360C0}};
constexpr sh::CallSite kCalls525F70[] = {{0x0, 0x536130}};
constexpr sh::CallSite kCalls525F90[] = {{0x0, 0x536170}};
constexpr sh::CallSite kCalls525FB0[] = {{0x17, 0x572570}, {0x3C, 0x5893A0}};
constexpr sh::CallSite kCalls526000[] = {{0xD, 0x572570}, {0x36, 0x589330}, {0x4A, 0x534590}};
constexpr sh::CallSite kCalls5260A0[] = {{0x17, 0x572570}, {0x33, 0x572570}, {0x6A, 0x589330}, {0x7B, 0x5893A0}};
constexpr sh::CallSite kCalls526120[] = {{0x3D, 0x589330}};
constexpr sh::CallSite kCalls5261C0[] = {{0x2, 0x534790}};
constexpr sh::CallSite kCalls526200[] = {{0x13, 0x589330}};
constexpr sh::CallSite kCalls526240[] = {{0x26, 0x5345E0}, {0x33, 0x52E140}, {0x38, 0x5893A0}};
constexpr sh::CallSite kCalls526280[] = {{0xB7, 0x572570}, {0xDF, 0x589330}, {0xF3, 0x534590}};
constexpr sh::CallSite kCalls5263C0[] = {{0x2, 0x534790}};
constexpr sh::CallSite kCalls5263E0[] = {{0x8E, 0x589330}, {0x97, 0x5345E0}, {0xA4, 0x52E140}, {0xA9, 0x5893A0}};
constexpr sh::CallSite kCalls526490[] = {{0x1E, 0x5893A0}};
constexpr sh::CallSite kCalls5264F0[] = {{0x29, 0x535C50}, {0x3A, 0x5266B0}, {0x3F, 0x526880}, {0x52, 0x589330}, {0x6E, 0x526DB0}, {0x7F, 0x526DB0}, {0x132, 0x589330}, {0x13A, 0x5345E0}, {0x147, 0x52E140}, {0x14C, 0x5893A0}, {0x181, 0x589330}};
constexpr sh::CallSite kCalls5266B0[] = {{0x61, 0x535610}, {0x85, 0x535C50}, {0x115, 0x526820}};
constexpr sh::CallSite kCalls526820[] = {{0x12, 0x5725C0}, {0x33, 0x572570}, {0x4F, 0x572570}};
constexpr sh::CallSite kCalls526880[] = {{0x3B, 0x5725C0}, {0xA4, 0x5725C0}, {0x10C, 0x5725C0}, {0x136, 0x5725C0}, {0x19D, 0x5725C0}, {0x1BF, 0x5725C0}, {0x1ED, 0x5725C0}};
constexpr sh::CallSite kCalls526AB0[] = {{0x5, 0x587740}, {0x19, 0x5951D0}, {0x34, 0x589330}};
constexpr sh::CallSite kCalls526B00[] = {{0x34, 0x589330}, {0x46, 0x572570}};
constexpr sh::CallSite kCalls526BA0[] = {{0x1D, 0x5366A0}, {0xB5, 0x5725C0}, {0xE4, 0x56F670}, {0xF9, 0x526DB0}, {0x10D, 0x536650}, {0x114, 0x531DF0}, {0x127, 0x536650}, {0x12E, 0x5345E0}, {0x13B, 0x536670}, {0x149, 0x534C20}, {0x16A, 0x589330}, {0x174, 0x587740}};
constexpr sh::CallSite kCalls526D30[] = {{0xD, 0x536670}, {0x2C, 0x536650}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define C3_FN(name) reinterpret_cast<const void*>(&::name)
#define C3_CLONE(name, base, size, calls) #name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, C3_FN(name)
#define C3_LEAF(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, C3_FN(name)
constexpr Shape kSp = Shape::kSprite;
constexpr Shape kSt = Shape::kState;
constexpr Shape kCa = Shape::kCall;
const sh::Clone kClones[] = {
    {C3_CLONE(Mode11_FieldFrame, 0x5172C0, 0x2D, kCalls5172C0), 0, false, kSt},
    {C3_CLONE(Mode8_Step5, 0x517330, 0xA, kCalls517330), 0, false, kSt},
    {C3_CLONE(Mode8_Step8, 0x517340, 0xA, kCalls517340), 0, false, kSt},
    {C3_LEAF(MoveCmd_OpE7, 0x518B20, 0x19), 0, false, kCa},
    {C3_CLONE(Field_ObjectApproachDirection, 0x518E20, 0xA3, kCalls518E20), 0xFF, false, kCa, ArgAt(0, Arg::kSprite)},
    {C3_CLONE(Field_ObjectAvoidDirection, 0x518ED0, 0xA5, kCalls518ED0), 0xFF, false, kCa, ArgAt(0, Arg::kSprite)},
    {C3_CLONE(Field_ObjectBestDirection, 0x518F80, 0x11C, kCalls518F80), 0xFF, false, kCa, ArgAt(0, Arg::kSprite)},
    {C3_CLONE(Field_ObjectFadeOutStart, 0x5193D0, 0x39, kCalls5193D0), 0, false, kSp},
    {C3_CLONE(Field_ObjectFadeOutStep, 0x519410, 0xC6, kCalls519410), 0, false, kSp},
    {C3_CLONE(Field_ObjectFadeInStart, 0x519500, 0x2D, kCalls519500), 0, false, kSp},
    {C3_CLONE(Field_ObjectFadeInStep, 0x519530, 0xC9, kCalls519530), 0, false, kSp},
    {C3_CLONE(FieldCore_ScriptMove, 0x525390, 0x41, kCalls525390), 0, false, kSp},
    {C3_CLONE(FieldCore_ScriptMoveAlign, 0x5253E0, 0xB3, kCalls5253E0), 0, false, kSp},
    {C3_CLONE(FieldCore_ScriptMoveNext, 0x5254A0, 0x1FB, kCalls5254A0), 0, false, kSp},
    {C3_CLONE(FieldCore_ScriptMoveStep, 0x5256A0, 0x63, kCalls5256A0), 0, false, kSp},
    {C3_LEAF(FieldCore_ScriptMoveWait, 0x525710, 0x27), 0, false, kSp},
    {C3_CLONE(FieldCore_ScriptMoveShadeLower, 0x525740, 0x2C, kCalls525740), 0, false, kSp},
    {C3_CLONE(FieldCore_ScriptMoveShadeFade, 0x525770, 0x2C, kCalls525770), 0, false, kSp},
    {C3_CLONE(FieldCore_Attached, 0x5257A0, 0x101, kCalls5257A0), 0, false, kSp},
    {C3_LEAF(FieldCore_Hop, 0x525960, 0x12), 0, false, kSp},
    {C3_CLONE(FieldCore_HopBegin, 0x525980, 0x4E, kCalls525980), 0, false, kSp},
    {C3_CLONE(FieldCore_HopLaunch, 0x5259D0, 0x14C, kCalls5259D0), 0, false, kSp},
    {C3_CLONE(FieldCore_HopRise, 0x525B20, 0x64, kCalls525B20), 0, false, kSp},
    {C3_CLONE(FieldCore_HopFall, 0x525B90, 0xB9, kCalls525B90), 0, false, kSp},
    {C3_CLONE(FieldCore_HopLand, 0x525C50, 0x4E, kCalls525C50), 0, false, kSp},
    {C3_LEAF(FieldCore_Vertical, 0x525CA0, 0x1E), 0, false, kSp},
    {C3_LEAF(FieldCore_Up, 0x525CC0, 0x12), 0, false, kSp},
    {C3_CLONE(FieldCore_UpBegin, 0x525CE0, 0xE, kCalls525CE0), 0, false, kSp},
    {C3_CLONE(FieldCore_UpOut, 0x525CF0, 0x41, kCalls525CF0), 0, false, kSp},
    {C3_CLONE(FieldCore_UpArrive, 0x525D40, 0xE7, kCalls525D40), 0, false, kSp},
    {C3_CLONE(FieldCore_UpIn, 0x525E30, 0x5B, kCalls525E30), 0, false, kSp},
    {C3_CLONE(FieldCore_UpWait5, 0x525E90, 0x12, kCalls525E90), 0, false, kSp},
    {C3_CLONE(FieldCore_UpWait6, 0x525EB0, 0x12, kCalls525EB0), 0, false, kSp},
    {C3_CLONE(FieldCore_UpEnd, 0x525ED0, 0x1F, kCalls525ED0), 0, false, kSp},
    {C3_LEAF(FieldCore_Down, 0x525EF0, 0x12), 0, false, kSp},
    {C3_CLONE(FieldCore_DownBegin, 0x525F10, 0x16, kCalls525F10), 0, false, kSp},
    {C3_CLONE(FieldCore_DownWait1, 0x525F30, 0x13, kCalls525F30), 0, false, kSp},
    {C3_CLONE(FieldCore_DownWait2, 0x525F50, 0x13, kCalls525F50), 0, false, kSp},
    {C3_CLONE(FieldCore_DownWait3, 0x525F70, 0x13, kCalls525F70), 0, false, kSp},
    {C3_CLONE(FieldCore_DownWait4, 0x525F90, 0x13, kCalls525F90), 0, false, kSp},
    {C3_CLONE(FieldCore_DownOut, 0x525FB0, 0x41, kCalls525FB0), 0, false, kSp},
    {C3_CLONE(FieldCore_DownArrive, 0x526000, 0x91, kCalls526000), 0, false, kSp},
    {C3_CLONE(FieldCore_DownIn, 0x5260A0, 0x80, kCalls5260A0), 0, false, kSp},
    {C3_CLONE(FieldCore_DownLand, 0x526120, 0x95, kCalls526120), 0, false, kSp},
    {C3_CLONE(FieldCore_VerticalShade, 0x5261C0, 0x17, kCalls5261C0), 0, false, kSp},
    {C3_LEAF(FieldCore_JumpExit, 0x5261E0, 0x1E), 0, false, kSp},
    {C3_CLONE(FieldCore_JumpExitBegin, 0x526200, 0x38, kCalls526200), 0, false, kSp},
    {C3_CLONE(FieldCore_JumpExitOut, 0x526240, 0x3D, kCalls526240), 0, false, kSp},
    {C3_CLONE(FieldCore_JumpExitArrive, 0x526280, 0x13A, kCalls526280), 0, false, kSp},
    {C3_CLONE(FieldCore_JumpExitShade, 0x5263C0, 0x17, kCalls5263C0), 0, false, kSp},
    {C3_CLONE(FieldCore_JumpExitIn, 0x5263E0, 0xAE, kCalls5263E0), 0, false, kSp},
    {C3_CLONE(FieldCore_TileD0, 0x526490, 0x23, kCalls526490), 0, false, kSp},
    {C3_LEAF(FieldCore_TileD0Begin, 0x5264C0, 0x2B), 0, false, kSp},
    {C3_CLONE(FieldCore_TileD0Move, 0x5264F0, 0x1B4, kCalls5264F0), 0, false, kSp},
    {C3_CLONE(FieldCore_TileD0Exit, 0x5266B0, 0x162, kCalls5266B0), 0, false, kSp},
    {C3_CLONE(FieldCore_TileD0Probe, 0x526820, 0x60, kCalls526820), 0xFF, false, kCa, ArgAt(2, Arg::kScratch)},
    {C3_CLONE(FieldCore_TileD0Slope, 0x526880, 0x20F, kCalls526880), 0xFF, false, kSp},
    {C3_LEAF(FieldCore_Fall, 0x526A90, 0x1E), 0, false, kSp},
    {C3_CLONE(FieldCore_FallBegin, 0x526AB0, 0x46, kCalls526AB0), 0, false, kSp},
    {C3_CLONE(FieldCore_FallSpin, 0x526B00, 0x7E, kCalls526B00), 0, false, kSp},
    {C3_LEAF(FieldCore_Recoil, 0x526B80, 0x12), 0, false, kSp},
    {C3_CLONE(FieldCore_RecoilBegin, 0x526BA0, 0x18B, kCalls526BA0), 0, false, kSp},
    {C3_CLONE(FieldCore_RecoilBlink, 0x526D30, 0x80, kCalls526D30), 0, false, kSp},
};
#undef C3_LEAF
#undef C3_CLONE
#undef C3_FN
#undef SH_N

enum : unsigned {
    kMode11, kMode8a, kMode8b, kOpE7, kApproach, kAvoid, kBest, kFadeOutStart, kFadeOutStep, kFadeInStart, kFadeInStep,
    kScriptMove, kAlign, kNext, kStep, kWait, kShadeLower, kShadeFade, kAttached, kHop, kHopBegin, kHopLaunch, kHopRise,
    kHopFall, kHopLand, kVertical, kUp, kUpBegin, kUpOut, kUpArrive, kUpIn, kUpWait5, kUpWait6, kUpEnd, kDown,
    kDownBegin, kDownWait1, kDownWait2, kDownWait3, kDownWait4, kDownOut, kDownArrive, kDownIn, kDownLand, kVShade,
    kJumpExit, kJumpBegin, kJumpOut, kJumpArrive, kJumpShade, kJumpIn, kTileD0, kTileBegin, kTileMove, kTileExit,
    kTileProbe, kTileSlope, kFall, kFallBegin, kFallSpin, kRecoil, kRecoilBegin, kRecoilBlink, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define C3_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kAll = 0xFFFFFFFFu;

unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char* Sc() { return Sprite_Current; }

// --- the stand-ins' effects (Noise() and the state only: the same on both passes) ---

// MapView_SlopeAt: AreaMap_Slope's "sloped" byte 0x903850, which every caller
// here reads straight after (docs/field_c3.md section 3).
// Its answer at the steepness boundary 0x40 half the time (the callers test
// > 0x40 as a signed word).
U SlopeEffect(const U*, U answer) {
    const U h = sh::Noise();
    Mem(at::kSloped)[0] = static_cast<unsigned char>(h % 3 ? 1 : 0);
    if ((h >> 4) & 1) return (answer & 0xFFFF0000u) | (0x3F + (h >> 8) % 3);
    return answer;
}
// MapView_GroundAt: two answers in three at a boundary the callers compare the
// height against - Sprite_Current's height word less or plus 0x200, 0x100 or
// 0, give or take one (docs/field_c3.md section 4).
U GroundEffect(const U*, U answer) {
    static const int kDelta[] = {-0x200, 0x200, 0x100, -0x100, 0};
    const U h = sh::Noise();
    if (h % 3 == 0) return answer;
    const int d = kDelta[(h >> 4) % 5] + static_cast<int>((h >> 8) % 3) - 1;
    return (answer & 0xFFFF0000u) | (static_cast<U>(Word(Sc() + 0x3E) + d) & 0xFFFFu);
}
// MoveCmd_AttachOffset: its three words out (FieldCore_Attached adds them).
U OffsetEffect(const U* a, U answer) {
    sh::FillBytes(reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[0])), 12);
    return answer;
}
// FieldCore_TileD0Probe: the word out, which FieldCore_TileD0Exit compares.
U ProbeEffect(const U* a, U answer) {
    // near Sprite_Current's height (the first best), so two probes tie often
    const U h = sh::Noise();
    const auto w = static_cast<std::uint16_t>(h % 4 ? Word(Sc() + 0x3E) - (h >> 4) % 3 : h >> 5);
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[2])), &w, sizeof w);
    return answer;
}
// Callees that turn Sprite_Current (+8), which the callers read back.
U TurnEffect(const U*, U answer) {
    const U h = sh::Noise();
    if (h & 1) Sc()[8] = static_cast<unsigned char>(h >> 8);
    return answer;
}
// MoveScript_Step: the context's flag byte (Field_State +0x124, a[0]), which
// FieldCore_ScriptMoveNext reads after it.
U StepEffect(const U* a, U answer) {
    const U h = sh::Noise();
    auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if ((h & 1) && sh::InRegions(p, 1)) p[0] = static_cast<unsigned char>(h >> 8);
    return answer;
}

const sh::Callee kCallees[] = {
    // this group's own, called directly (E8 / E9)
    {C3_OURS(FieldCore_ScriptMove), 0, {}, sh::Answer::kPhase, 0, 0},
    {C3_OURS(FieldCore_ScriptMoveNext), 0, {}, sh::Answer::kPhase, 0, 0},
    {C3_OURS(FieldCore_TileD0Exit), 0, {}, kG, 0, 0, {}, &TurnEffect},
    {C3_OURS(FieldCore_TileD0Slope), 0, {}, kF, 0, 0},
    {C3_OURS(FieldCore_TileD0Probe), 4, {kAll, kAll, 0, kAll}, kF, 0, 0, {}, &ProbeEffect},
    {C3_OURS(Field_ObjectBestDirection), 2, {kAll, 0xFF}, sh::Answer::kByte, 0xFF, 0x07, {}, &TurnEffect},
    // Field_ObjectHandlers' eleven (MoveCmd_OpE7's table): typed, one argument -
    // the object, Field_ActiveMember - which a handler recorder would not log
    {C3_OURS(Field_ObjectWander), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Field_ObjectApproach), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Field_ObjectAvoid), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Field_ObjectWanderHome), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Field_ObjectUpdate), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Field_ObjectWait), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Field_ObjectStill), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Field_ObjectFollow), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Field_ObjectFadeOut), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Field_ObjectFadeIn), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Field_ObjectTurn), 1, {kAll}, kG, 0, 0},
    // FE2's, by address (docs/field_c3.md section 8)
    {"0x536F10", at::kEventObjectFrame, at::kEventObjectFrame, 0, {}, kG, 0, 0},
    {"0x535FC0", at::kUpFace, at::kUpFace, 0, {}, kG, 0, 0},
    {"0x535FE0", at::kDownJumpSetUp, at::kDownJumpSetUp, 0, {}, kG, 0, 0},
    {"0x536290", at::kUpLanded, at::kUpLanded, 0, {}, kG, 0, 0},
    {"0x536050", at::kDownWait1, at::kDownWait1, 0, {}, kF, 0, 0},
    {"0x5360C0", at::kDownWait2, at::kDownWait2, 0, {}, kF, 0, 0},
    {"0x536130", at::kDownWait3, at::kDownWait3, 0, {}, kF, 0, 0},
    {"0x536170", at::kDownWait4, at::kDownWait4, 0, {}, kF, 0, 0},
    {"0x5362D0", at::kUpWait5, at::kUpWait5, 0, {}, kF, 0, 0},
    {"0x5363C0", at::kUpWait6, at::kUpWait6, 0, {}, kF, 0, 0},
    {"0x536440", at::kUpWait7, at::kUpWait7, 0, {}, kF, 0, 0},
    {"0x534C20", at::kRecoilFace, at::kRecoilFace, 1, {0xFF}, kG, 0, 0},   // and eax, 0xFF at +0xE
    // field-standard ones re-listed with the mask the callee reads (the callers
    // push whole registers; docs/field_c3.md section 3 has each read) or a
    // louder stand-in
    {C3_OURS(Sprite_EnsureAnimation), 1, {0xFF}, kF, 0, 0, {}, nullptr, nullptr, true},     // Sprite_SetAnimationAt reads a byte (dl)
    {C3_OURS(MapView_SlopeAt), 3, {kAll, kAll, 0xFF}, kG, 0, 0, {}, &SlopeEffect, nullptr, true},   // AreaMap_Slope's direction byte n
    {C3_OURS(MapView_GroundAt), 2, {kAll, kAll}, kG, 0, 0, {}, &GroundEffect, nullptr, true},     // answers at the height's boundaries
    {C3_OURS(MapView_SetElevation), 1, {0xFFFF}, kG, 0, 0, {}, nullptr, nullptr, true},     // only the low word reaches memory
    {C3_OURS(MoveCmd_AttachOffset), 2, {0, 0xFF}, kG, 0, 0, {}, &OffsetEffect, nullptr, true},   // the out pointer is the caller's frame
    {C3_OURS(Field_WayBlocked), 4, {kAll, kAll, 0xFF, 0xFFFF}, kF, 0, 0, {}, nullptr, nullptr, true},   // raised & 0xFF, ground a word
    {C3_OURS(Area_LinkAt), 2, {0xFF, 0xFF}, kF, 0, 0, {}, nullptr, nullptr, true},          // x & 0xFF, z & 0xFF
    {C3_OURS(MoveScript_Step), 2, {kAll, kAll}, sh::Answer::kByte, 0xFF, 0x0F, {16, 16}, &StepEffect, nullptr, true},   // 0xFF ends the move
    {C3_OURS(Field_CellAhead), 0, {}, sh::Answer::kByte, 0, 4, {}, nullptr, nullptr, true},   // 1 and 3 read by FieldCore_RecoilBegin
};
#undef C3_OURS

// The tables the functions reach out through, swapped for recorders on both
// sides; each count is the code's (docs/field_c3.md section 2).
const sh::DataTable kTables[] = {
    {Key(Field_ObjectHandlers), 11},
    {Key(FieldCore_ScriptMoveSteps), 6},
    {Key(FieldCore_HopSteps), 5},
    {Key(FieldCore_VerticalSteps), 2},
    {Key(FieldCore_UpSteps), 8},
    {Key(FieldCore_DownSteps), 10},
    {Key(FieldCore_JumpExitSteps), 5},
    {Key(FieldCore_TileD0Steps), 2},
    {Key(FieldCore_FallSteps), 2},
    {Key(FieldCore_RecoilSteps), 2},
};

// Beyond field mode's standard regions: Field_ActiveMember, the first 44 tint
// records (the seed keeps the member's +0x9F index below 44; 0x7E0918 is the
// standard Draw_PassFlags), and the point the vertical moves test the ground at.
constexpr unsigned kTints = 44;
const sh::Region kRegions[] = {
    {at::kActiveMember, 4},
    {Key(MoveScript_TintRecords), kTints * 12},
    {at::kTargetX, 8},
};

// --- the seed ---------------------------------------------------------------------------

template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char Small() { return static_cast<unsigned char>(PickOf(0, 0, 1, 1, 2, 3, 4, 5, 7, 8, sh::Next())); }

// A coordinate on the grid, half off it, or anything (16.16).
U Coordinate() {
    const U v = sh::Next();
    switch (sh::Next() % 5) {
    case 0: return v & 0xFFFF0000u;
    case 1: return (v & 0xFFFF0000u) | 0x8000u;
    case 2: return v & 0xFFFF8000u;
    default: return v;
    }
}

void SeedSprite(unsigned char* s) {
    s[5] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
    s[7] = static_cast<unsigned char>(sh::Next());
    s[8] = static_cast<unsigned char>(PickOf(3, 5, 7, 2, 6, sh::Next() & 7, sh::Next()));
    s[9] = Small();
    s[0xA] = Small();
    s[0xB] = static_cast<unsigned char>(sh::Next() & 7);
    SetLong(s + 0x14, static_cast<std::int32_t>(PickOf(0, 0xFFFFFF7Fu, 0xFFFFFF80u, 0xFFFFFF81u, 0x40, sh::Next())));
    SetLong(s + 0x18, static_cast<std::int32_t>(sh::Next() % 4));   // Sprite_ObjectsExtra's four
    SetLong(s + 0x34, static_cast<std::int32_t>(Coordinate()));
    SetLong(s + 0x38, static_cast<std::int32_t>(Coordinate()));
    if (sh::Half()) s[0x24] = static_cast<unsigned char>(s[0x24] & 0xDF);
    s[0x5C] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
    s[0x70] = static_cast<unsigned char>(PickOf(0, 0, 1, 0xFF, sh::Next()));
    s[0x9F] = static_cast<unsigned char>(sh::Next() % kTints);   // any record may be Field_ActiveMember (the disturbance moves it)
}

void Seed(unsigned k) {
    unsigned char* const s = Sc();
    for (unsigned i = 0; i < 4; ++i) SeedSprite(sh::SpriteRecord(i));
    // Field_State: its move-script context +0x124.. and the pace
    unsigned char* const f = Field_State;
    f[0x124] = static_cast<unsigned char>(sh::Half() ? 1u << (sh::Next() % 8) : sh::Next());
    f[0x125] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
    f[0x12B] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
    f[0x128] = static_cast<unsigned char>(1 + sh::Next() % 5);   // Field_MoveSpeeds 1..5: none of 0 (the hop divides)
    // the active member one of the four records, its tint index inside the region
    Field_ActiveMember = sh::SpriteRecord(sh::Next());
    Field_ActiveMember[0x9F] = static_cast<unsigned char>(sh::Next() % kTints);
    Mem(at::kSloped)[0] = static_cast<unsigned char>(sh::Half() ? 0 : 1);
    Field_Request = static_cast<unsigned char>(PickOf(2, 5, 0, sh::Next()));
    Field_StatusBits = static_cast<unsigned char>(sh::Next());
    SetWord(reinterpret_cast<unsigned char*>(&Input_Held),
            PickOf(0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x8000, 0x9000, 0xC000, sh::Next()) | (sh::Next() & 0xFFF));
    switch (k) {
    // the dispatchers: the index byte inside the table (the harness's
    // sprite_span is one span for the group; these differ)
    case kScriptMove: s[3] = static_cast<unsigned char>(sh::Next() % 6); break;
    case kHop: s[3] = static_cast<unsigned char>(sh::Next() % 5); break;
    case kVertical: s[3] = static_cast<unsigned char>(sh::Next() % 2); break;
    case kUp: s[4] = static_cast<unsigned char>(sh::Next() % 8); break;
    case kDown: s[4] = static_cast<unsigned char>(sh::Next() % 10); break;
    case kJumpExit: s[3] = static_cast<unsigned char>(sh::Next() % 5); break;
    case kTileD0:
    case kFall:
    case kRecoil: s[3] = static_cast<unsigned char>(sh::Next() % 2); break;
    case kApproach:
    case kAvoid: {
        // the leader within the half-widths about half the time
        for (unsigned i = 0; i < 4; ++i) {
            unsigned char* const r = sh::SpriteRecord(i);
            SetWord(r + 0x98, PickOf(0, 1, 2, 4, 8, 0xFFFF, sh::Next()));
            SetWord(r + 0x9A, PickOf(0, 1, 2, 4, 8, 0xFFFF, sh::Next()));
        }
        const U dx = PickOf(0, 0x10000, 0x18000, 0x30000, 0xFFFF0000u, sh::Next());
        const U dz = PickOf(0, 0x10000, 0x18000, 0x30000, 0xFFFF0000u, sh::Next());
        SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(static_cast<U>(Long(s + 0x34)) + dx));
        SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(static_cast<U>(Long(s + 0x38)) + dz));
        break;
    }
    case kBest:
        Mem(at::kLeaderPace)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 8, sh::Next()));
        break;
    case kFadeOutStep:
    case kFadeInStep: {
        // the member's record near its end: 0 / 1 for the fade out, 0x1E / 0x1F for the fade in
        unsigned char* const r = MoveScript_TintRecords + Field_ActiveMember[0x9F] * 12u;
        for (unsigned i = 2; i <= 4; ++i)
            r[i] = static_cast<unsigned char>(sh::Often() ? (k == kFadeOutStep ? PickOf(0, 1) : PickOf(0x1E, 0x1F, 0x20))
                                                          : sh::Next());
        break;
    }
    case kHopFall:
        SetLong(s + 0x20, static_cast<std::int32_t>(PickOf(0, 0xFFFFFFF8u, 0xFFFFFFF0u, sh::Next())));
        break;
    case kDownLand:
        s[9] = static_cast<unsigned char>(PickOf(1, 1, 2, sh::Next()));
        s[0xA] = static_cast<unsigned char>(PickOf(3, 3, 2, sh::Next()));
        break;
    case kJumpIn:
        s[0xA] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, sh::Next()));
        break;
    case kJumpOut:
        s[0xA] = static_cast<unsigned char>(PickOf(2, 2, 3, sh::Next()));
        break;
    case kTileSlope:
        s[8] = static_cast<unsigned char>(PickOf(3, 5, 3, 5, sh::Next()));
        if (sh::Half()) SetWord(s + 0x34, 0);
        if (sh::Half()) SetWord(s + 0x38, 0);
        break;
    case kTileExit:
    case kTileMove:
        s[0x70] = static_cast<unsigned char>(PickOf(0, 1, 0xFF, sh::Next()));
        break;
    default: break;
    }
}

void Args(unsigned k, U* a) {
    switch (k) {
    case kOpE7: a[0] = (a[0] & 0xFFFFFF00u) | (sh::Next() % 11); break;   // inside Field_ObjectHandlers' eleven
    case kBest: a[1] = (a[1] & 0xFFFFFF00u) | (sh::Half() ? 0u : sh::Next() & 0xFF); break;
    case kTileProbe: a[3] = PickOf(1, 3, 5, sh::Next()); break;
    default: break;
    }
}

// --- the disturbance: a cell these read again after a call ------------------------------
void Disturb(U h) {
    unsigned char* const s = Sc();
    const auto b = static_cast<unsigned char>(h >> 24);
    switch (sh::DisturbCase(h, 12)) {
    case 0: s[9] = static_cast<unsigned char>(b & 3); break;
    case 1: s[0xA] = static_cast<unsigned char>(b % 5); break;
    case 2: s[8] = b; break;
    case 3: s[5] = static_cast<unsigned char>(b & 1 ? 0 : b); break;
    case 4: Field_State[0x124] = b; break;
    case 5: Field_State[0x12B] = static_cast<unsigned char>(b & 1); break;
    case 6: Mem(at::kSloped)[0] = static_cast<unsigned char>(b & 1); break;
    case 7: SetWord(s + 0x3E, h >> 8); break;
    case 8: SetLong(s + 0x14, static_cast<std::int32_t>(h >> 4)); break;
    case 9: SetWord(s + ((h >> 8) & 1 ? 0x34 : 0x38), b & 1 ? 0u : h >> 12); break;
    case 10: Field_ActiveMember = sh::SpriteRecord(b); break;
    case 11: s[0x70] = static_cast<unsigned char>(b & 1 ? 0 : b); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    sh::Group g = {"field_c3", kClones, kCount, kCallees, sizeof kCallees / sizeof kCallees[0],
                   kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   &Seed, &Disturb, 3000};
    g.args = &Args;
    g.field = true;   // kSprite / kCall put it on anyway
    sh::Run(g);
}

}  // namespace field_c3
