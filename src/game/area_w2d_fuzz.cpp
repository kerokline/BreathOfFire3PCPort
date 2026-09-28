// BOF3X_SHADOW=area_w2d: world 2's areas 95..100 and 103 through the area
// round's shared harness (area_harness.h), once at start-up - one
// area_harness::Run per area, each Group setting its own area number, all
// under the one shadow name. docs/area_w2d.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA095..100
// and AREA103 (2026-09-28), each row read against the disassembly (every
// start, extent, call site and both in-function jump tables agree); the
// shapes are the root table each function hangs from (docs/area_w2d.md
// section 1). The group's own callees (area 95's turn-and-move, area 103's
// member search) are recorders here like any other callee, so each function
// is fuzzed alone; areas 99 and 100's state tables are swapped for recorders
// (DataTable).
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2d.h"
#include "game/area_w2d_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w2d {
namespace {

namespace ah = area_harness;
using S = ah::Shape;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define OURS(f) reinterpret_cast<const void*>(&::f)

// ---- areas 95 and 96 ----
constexpr ah::CallSite kCalls4135B0[] = {{0x39, 0x4137D0}};
constexpr ah::CallSite kCalls4135F0[] = {{0x62, 0x57C7C0}, {0x6B, 0x531F90}};
constexpr ah::CallSite kCalls413670[] = {{0x1, 0x589810}, {0x74, 0x587740}, {0x8D, 0x57C0F0}, {0xA7, 0x57C0F0}, {0xB8, 0x57C0F0}};
constexpr ah::CallSite kCalls413750[] = {{0x13, 0x578C10}, {0x3B, 0x5720C0}};
constexpr ah::CallSite kCalls4137D0[] = {{0x36, 0x5891F0}, {0x57, 0x578C10}, {0x74, 0x578C10}};
constexpr ah::CallSite kCalls413860[] = {{0x39, 0x4137D0}};
constexpr ah::CallSite kCalls4138A0[] = {{0x4, 0x536700}, {0x16, 0x579F00}, {0x25, 0x579F00}};
constexpr ah::CallSite kCalls4138F0[] = {{0xD, 0x5720C0}};
// ---- area 97 ----
constexpr ah::CallSite kCalls413940[] = {{0x18, 0x57C7A0}, {0x33, 0x57C7C0}};
constexpr ah::CallSite kCalls413980[] = {{0x2C, 0x5918E0}};
constexpr ah::CallSite kCalls4139E0[] = {{0x2, 0x5918E0}, {0x15, 0x57C0F0}};
// ---- area 98 ----
constexpr ah::CallSite kCalls413A00[] = {{0x10, 0x57C0F0}, {0x31, 0x57C0F0}};
constexpr ah::CallSite kCalls413A80[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls413AB0[] = {{0x0, 0x589810}, {0xC7, 0x587740}};
constexpr ah::CallSite kCalls413B90[] = {{0x10, 0x579F00}};
constexpr ah::CallSite kCalls413BC0[] = {{0x44, 0x594E00}, {0x4C, 0x57C7A0}};
constexpr ah::CallSite kCalls413C20[] = {{0x0, 0x57C7C0}};
// ---- area 99 ----
constexpr ah::CallSite kCalls413CC0[] = {{0x23, 0x587740}, {0x3C, 0x589330}, {0x50, 0x589810}};
constexpr ah::CallSite kCalls413EC0[] = {{0x61, 0x589330}};
constexpr ah::CallSite kCalls413F50[] = {{0x8, 0x57C160}};
constexpr ah::CallSite kCalls413F70[] = {{0x10, 0x57C140}, {0x23, 0x57C140}, {0x35, 0x579F00}, {0x44, 0x579F00}};
// ---- area 100 ----
constexpr ah::CallSite kCalls413FC0[] = {{0xB, 0x57C140}, {0x25, 0x57C140}, {0x56, 0x594E00}, {0x6B, 0x594E00}, {0x85, 0x594E00}, {0x9F, 0x594E00}};
constexpr ah::CallSite kCalls414070[] = {{0x7, 0x57C0F0}};
constexpr ah::CallSite kCalls4140A0[] = {{0xC, 0x572650}, {0x14, 0x589810}};
constexpr ah::CallSite kCalls4140F0[] = {{0xC, 0x572650}, {0x14, 0x589810}};
constexpr ah::CallSite kCalls414140[] = {{0x31, 0x57C110}, {0x40, 0x57C0F0}, {0x4C, 0x57C0F0}, {0x5F, 0x57C110}, {0x68, 0x5918E0}, {0x7E, 0x57C0F0},
                                         {0x8D, 0x57C0F0}, {0x99, 0x57C0F0}, {0xB2, 0x57C110}, {0xC5, 0x57C0F0}, {0xD4, 0x57C110}, {0xE0, 0x57C0F0}};
constexpr ah::JumpTable kTables414140[] = {{0x23, 0xEC, 4}};
constexpr ah::CallSite kCalls414240[] = {{0x1A, 0x5734F0}, {0x38, 0x589810}, {0xA6, 0x4976D0}, {0xC7, 0x57C7A0}};
constexpr ah::JumpTable kTables414240[] = {{0x14, 0xDC, 5}};
constexpr ah::CallSite kCalls414330[] = {{0x10, 0x57C140}, {0x50, 0x531F90}};
constexpr ah::CallSite kCalls414390[] = {{0x7, 0x57C0F0}, {0x10, 0x572650}, {0x1A, 0x587740}, {0x28, 0x579F00}, {0x36, 0x579F00}, {0x44, 0x579F00}, {0x52, 0x579F00}};
constexpr ah::CallSite kCalls4143F0[] = {{0x7, 0x57C0F0}, {0x10, 0x572650}, {0x18, 0x57C7C0}};
constexpr ah::CallSite kCalls414420[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls414440[] = {{0x7, 0x57C140}, {0x19, 0x579F00}, {0x24, 0x579F00}, {0x2F, 0x579F00}, {0x3A, 0x579F00}};
constexpr ah::CallSite kCalls4144B0[] = {{0x22, 0x4220D0}};
// ---- area 103 ----
constexpr ah::CallSite kCalls4144E0[] = {{0x0, 0x414540}, {0x1D, 0x517E90}};
constexpr ah::CallSite kCalls414620[] = {{0xD, 0x5720C0}};
constexpr ah::CallSite kCalls414640[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls4146A0[] = {{0x0, 0x57C7C0}};

// The shared bodies of areas 95 and 96 run under area 95 (the seed sets 96
// for the one that tests the area); area 96's own copy of handler 4 under 96.
const ah::Clone kClones95[] = {
    {"Area95_TurnAtCell", 0x4135B0, 0x3E, kCalls4135B0, AH_N(kCalls4135B0), nullptr, 0, nullptr, 0, OURS(Area95_TurnAtCell), 0, false, S::kHandler},
    {"Area95_WaitEffectAtCell", 0x4135F0, 0x75, kCalls4135F0, AH_N(kCalls4135F0), nullptr, 0, nullptr, 0, OURS(Area95_WaitEffectAtCell), 0, false, S::kHandler},
    {"Area95_SpawnEffect37", 0x413670, 0xD2, kCalls413670, AH_N(kCalls413670), nullptr, 0, nullptr, 0, OURS(Area95_SpawnEffect37), 0, false, S::kHandler},
    {"Area95_LeapArc", 0x413750, 0x7C, kCalls413750, AH_N(kCalls413750), nullptr, 0, nullptr, 0, OURS(Area95_LeapArc), 0, false, S::kHandler},
    {"Area95_TurnAndMove", 0x4137D0, 0x87, kCalls4137D0, AH_N(kCalls4137D0), nullptr, 0, nullptr, 0, OURS(Area95_TurnAndMove), 0, false, S::kHandler},
};
enum : unsigned { k95Turn, k95Wait, k95Spawn, k95Leap, k95Move };
const ah::Clone kClones96[] = {
    {"Area96_TurnAtCell", 0x413860, 0x3E, kCalls413860, AH_N(kCalls413860), nullptr, 0, nullptr, 0, OURS(Area96_TurnAtCell), 0, false, S::kHandler},
    {"Area96_ToggleCell", 0x4138A0, 0x2E, kCalls4138A0, AH_N(kCalls4138A0), nullptr, 0, nullptr, 0, OURS(Area96_ToggleCell), 0, false, S::kHandler},
    {"Area96_FaceBack", 0x4138D0, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area96_FaceBack), 0, false, S::kHandler},
    {"Area96_SnapElevation", 0x4138F0, 0x4A, kCalls4138F0, AH_N(kCalls4138F0), nullptr, 0, nullptr, 0, OURS(Area96_SnapElevation), 0, false, S::kHandler},
};
enum : unsigned { k96Turn, k96Toggle, k96Face, k96Snap };
const ah::Clone kClones97[] = {
    {"Area97_TailWaitCounter3", 0x413940, 0x40, kCalls413940, AH_N(kCalls413940), nullptr, 0, nullptr, 0, OURS(Area97_TailWaitCounter3), 0, false, S::kTail},
    {"Area97_StepHook", 0x413980, 0x51, kCalls413980, AH_N(kCalls413980), nullptr, 0, nullptr, 0, OURS(Area97_StepHook), 0xFF, false, S::kHook},
    {"Area97_FlagIfKeyItem5", 0x4139E0, 0x20, kCalls4139E0, AH_N(kCalls4139E0), nullptr, 0, nullptr, 0, OURS(Area97_FlagIfKeyItem5), 0xFF, false, S::kTail},
};
enum : unsigned { k97Tail, k97Step, k97Key };
const ah::Clone kClones98[] = {
    {"Area98_ChoiceFlag9", 0x413A00, 0x41, kCalls413A00, AH_N(kCalls413A00), nullptr, 0, nullptr, 0, OURS(Area98_ChoiceFlag9), 0, false, S::kChoice},
    {"Area98_ChoiceAsk2F", 0x413A50, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area98_ChoiceAsk2F), 0, false, S::kChoice},
    {"Area98_ChoiceArmTail45", 0x413A80, 0x26, kCalls413A80, AH_N(kCalls413A80), nullptr, 0, nullptr, 0, OURS(Area98_ChoiceArmTail45), 0, false, S::kChoice},
    {"Area98_SpawnEffect37", 0x413AB0, 0xD2, kCalls413AB0, AH_N(kCalls413AB0), nullptr, 0, nullptr, 0, OURS(Area98_SpawnEffect37), 0, false, S::kHandler},
    {"Area98_InitCells", 0x413B90, 0x21, kCalls413B90, AH_N(kCalls413B90), nullptr, 0, nullptr, 0, OURS(Area98_InitCells), 0, false, S::kInit},
    {"Area98_TailChangeArea62", 0x413BC0, 0x60, kCalls413BC0, AH_N(kCalls413BC0), nullptr, 0, nullptr, 0, OURS(Area98_TailChangeArea62), 0, false, S::kTail},
    {"Area98_Trigger44", 0x413C20, 0x5D, kCalls413C20, AH_N(kCalls413C20), nullptr, 0, nullptr, 0, OURS(Area98_Trigger44), 0xFF, false, S::kCallee},
    {"Area98_CameraDistanceFF00", 0x413C80, 0x11, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area98_CameraDistanceFF00), 0, false, S::kHandler},
    {"Area98_CameraDistance0", 0x413CA0, 0x11, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area98_CameraDistance0), 0, false, S::kHandler},
};
enum : unsigned { k98Flag9, k98Ask, k98Arm, k98Spawn, k98Init, k98Tail, k98Trigger, k98CamFar, k98CamNear };
const ah::Clone kClones99[] = {
    {"Area99_TurnSoundEffect1B", 0x413CC0, 0x8A, kCalls413CC0, AH_N(kCalls413CC0), nullptr, 0, nullptr, 0, OURS(Area99_TurnSoundEffect1B), 0, false, S::kHandler},
    {"Area99_RunDrift", 0x413D50, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area99_RunDrift), 0, false, S::kHandler},
    {"Area99_DriftStart", 0x413D70, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area99_DriftStart), 0, false, S::kState},
    {"Area99_DriftStep", 0x413DA0, 0x45, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area99_DriftStep), 0, false, S::kState},
    {"Area99_RunLeap", 0x413DF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area99_RunLeap), 0, false, S::kHandler},
    {"Area99_LeapStart", 0x413E10, 0xA1, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area99_LeapStart), 0, false, S::kState},
    {"Area99_LeapStep", 0x413EC0, 0x88, kCalls413EC0, AH_N(kCalls413EC0), nullptr, 0, nullptr, 0, OURS(Area99_LeapStep), 0, false, S::kState},
    {"Area99_ToggleRowFlag1F", 0x413F50, 0x11, kCalls413F50, AH_N(kCalls413F50), nullptr, 0, nullptr, 0, OURS(Area99_ToggleRowFlag1F), 0, false, S::kHandler},
    {"Area99_InitCell", 0x413F70, 0x4D, kCalls413F70, AH_N(kCalls413F70), nullptr, 0, nullptr, 0, OURS(Area99_InitCell), 0, false, S::kInit},
};
enum : unsigned { k99Turn, k99RunDrift, k99DriftStart, k99DriftStep, k99RunLeap, k99LeapStart, k99LeapStep, k99Toggle, k99Init };
const ah::Clone kClones100[] = {
    {"Area100_ChangeAreaByFlags", 0x413FC0, 0xA8, kCalls413FC0, AH_N(kCalls413FC0), nullptr, 0, nullptr, 0, OURS(Area100_ChangeAreaByFlags), 0, false, S::kHandler},
    {"Area100_SetFlag45", 0x414070, 0x10, kCalls414070, AH_N(kCalls414070), nullptr, 0, nullptr, 0, OURS(Area100_SetFlag45), 0, false, S::kHandler},
    {"Area100_ShiftCameraUp4", 0x414080, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area100_ShiftCameraUp4), 0, false, S::kHandler},
    {"Area100_ShiftCameraDown4", 0x414090, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area100_ShiftCameraDown4), 0, false, S::kHandler},
    {"Area100_SpawnEffect9B", 0x4140A0, 0x49, kCalls4140A0, AH_N(kCalls4140A0), nullptr, 0, nullptr, 0, OURS(Area100_SpawnEffect9B), 0, false, S::kHandler},
    {"Area100_SpawnEffectB7", 0x4140F0, 0x49, kCalls4140F0, AH_N(kCalls4140F0), nullptr, 0, nullptr, 0, OURS(Area100_SpawnEffectB7), 0, false, S::kHandler},
    {"Area100_ChoiceFlags93", 0x414140, 0xFC, kCalls414140, AH_N(kCalls414140), nullptr, 0, kTables414140, AH_N(kTables414140), OURS(Area100_ChoiceFlags93), 0, false, S::kChoice},
    {"Area100_TailEffect9B", 0x414240, 0xF0, kCalls414240, AH_N(kCalls414240), nullptr, 0, kTables414240, AH_N(kTables414240), OURS(Area100_TailEffect9B), 0, false, S::kTail},
    {"Area100_StepHook", 0x414330, 0x5E, kCalls414330, AH_N(kCalls414330), nullptr, 0, nullptr, 0, OURS(Area100_StepHook), 0xFF, false, S::kHook},
    {"Area100_Trigger16", 0x414390, 0x5D, kCalls414390, AH_N(kCalls414390), nullptr, 0, nullptr, 0, OURS(Area100_Trigger16), 0xFF, false, S::kCallee},
    {"Area100_Trigger13", 0x4143F0, 0x2E, kCalls4143F0, AH_N(kCalls4143F0), nullptr, 0, nullptr, 0, OURS(Area100_Trigger13), 0xFF, false, S::kCallee},
    {"Area100_Trigger30", 0x414420, 0x1D, kCalls414420, AH_N(kCalls414420), nullptr, 0, nullptr, 0, OURS(Area100_Trigger30), 0xFF, false, S::kCallee},
    {"Area100_InitCells", 0x414440, 0x43, kCalls414440, AH_N(kCalls414440), nullptr, 0, nullptr, 0, OURS(Area100_InitCells), 0, false, S::kInit},
    {"Area100_EffectB7Run", 0x414490, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area100_EffectB7Run), 0, false, S::kCallee},
    {"Area100_EffectB7Ring", 0x4144B0, 0x2B, kCalls4144B0, AH_N(kCalls4144B0), nullptr, 0, nullptr, 0, OURS(Area100_EffectB7Ring), 0, false, S::kState},
};
enum : unsigned {
    k100Change, k100Flag45, k100CamUp, k100CamDown, k100Spawn9B, k100SpawnB7, k100Choice, k100Tail, k100Step, k100Trig16, k100Trig13,
    k100Trig30, k100Init, k100EffRun, k100EffRing
};
const ah::Clone kClones103[] = {
    {"Area103_TalkByMember", 0x4144E0, 0x51, kCalls4144E0, AH_N(kCalls4144E0), nullptr, 0, nullptr, 0, OURS(Area103_TalkByMember), 0, false, S::kHandler},
    {"Area103_FirstListedMember", 0x414540, 0x70, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area103_FirstListedMember), 0xFF, false, S::kCallee},
    {"Area103_SkipIfLeader89", 0x4145B0, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area103_SkipIfLeader89), 0, false, S::kHandler},
    {"Area103_CountIfLeader89Zero", 0x4145D0, 0x16, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area103_CountIfLeader89Zero), 0, false, S::kHandler},
    {"Area103_ShakeElevation", 0x4145F0, 0x25, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area103_ShakeElevation), 0, false, S::kHandler},
    {"Area103_Ground", 0x414620, 0x20, kCalls414620, AH_N(kCalls414620), nullptr, 0, nullptr, 0, OURS(Area103_Ground), 0, false, S::kHandler},
    {"Area103_SpawnEffect4D", 0x414640, 0x58, kCalls414640, AH_N(kCalls414640), nullptr, 0, nullptr, 0, OURS(Area103_SpawnEffect4D), 0, false, S::kHandler},
    {"Area103_Trigger35", 0x4146A0, 0x1D, kCalls4146A0, AH_N(kCalls4146A0), nullptr, 0, nullptr, 0, OURS(Area103_Trigger35), 0xFF, false, S::kCallee},
};
enum : unsigned { k103Talk, k103First, k103Skip, k103Count, k103Shake, k103Ground, k103Spawn, k103Trig35 };
#undef AH_N
#undef OURS

const ah::DataTable kTables99[] = {{at::kArea99DriftStates, at::kArea99StateCount}, {at::kArea99LeapStates, at::kArea99StateCount}};
const ah::DataTable kTables100[] = {{at::kArea100EffectStates, at::kArea100EffectStateCount}};

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(std::uint32_t address) { return *ah::Mem(address); }
unsigned char* EffectRecord(unsigned slot) { return ah::Mem(at::kEffectObjects + slot % at::kEffectCount * at::kEffectStride); }

// Which area and function the round is running (set by the seeds; read by
// Settle and the louder stand-ins).
int g_area = 0;
unsigned g_k = 0;
bool Running(int area, unsigned k) { return g_area == area && g_k == k; }

// A record the active member pointer may name: one of the four party objects
// (Sprite_ObjectsExtra), a field object, a party record, or the running
// object itself (the two alias in the game).
unsigned char* MemberRecord(std::uint32_t v) {
    switch (v % 4) {
    case 0: return ah::Mem(at::kSpriteObjectsExtra + (v >> 2) % 4 * 0xA4);
    case 1: return ah::Object(v >> 2);
    case 2: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return Sprite_Current;
    }
}
unsigned char* ScriptRecord(std::uint32_t v) {
    return v & 1 ? ah::Object(v >> 1) : ah::PartyOf(static_cast<unsigned char>(v >> 1));
}
// The focus object: a field object, or one of Sprite_ObjectsExtra's four
// (whose index from Sprite_Objects is past the thirty).
unsigned char* FocusRecord(std::uint32_t v) {
    return v % 5 == 0 ? ah::Mem(at::kSpriteObjectsExtra + (v >> 3) % 4 * 0xA4) : ah::Object(v >> 3);
}

// Area 95's leap divides by the running object's +9 and area 99's leap start
// by Field_MoveSpeeds[3] and 16 / it: the original faults on 0, ours aborts,
// so the fuzz keeps both away from 0 (docs/area_w2d.md section 6) - after
// the harness's disturbance and after a louder stand-in moves the object.
void Settle() {
    if (Running(95, k95Leap) && Sprite_Current[9] == 0) Sprite_Current[9] = 1;
    const unsigned speed = B(at::kMoveSpeed3);
    if (speed == 0 || speed > 16) B(at::kMoveSpeed3) = static_cast<unsigned char>(1 + speed % 16);
}

// ---- the stand-ins the group lists ----

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, on purpose (each only half the time, from
// Noise): after these calls the callers read a cell again - the running
// object after the elevation, the sound, the animations and the effect
// search (areas 95, 99, 103), the script object after MoveCmd_Move,
// Sprite_SetAnimation and MoveScript_SetTurnTarget (area 95's turn, area
// 103's talk), the chapter byte and the area after the sound (area 95's
// effect). The harness's own disturbance reaches a group cell about one call
// in 24 and moves Sprite_Current one call in 24.
void MoveCurrent(std::uint32_t n) {
    Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3);
    Settle();
}
std::uint32_t MovesCurrent(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) MoveCurrent(n);
    return answer;
}
std::uint32_t MovesScriptObject(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kScriptObject, ScriptRecord(n >> 8));
    if (n & 2) MoveCurrent(n >> 4);
    return answer;
}
// The sound: the running object, or the chapter byte and the area number
// (Area95_SpawnEffect37 reads both after it).
std::uint32_t SoundEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) MoveCurrent(n);
    // values drawn from the noise only (Next() is the seed's, not replayed
    // alike in both passes)
    static const signed char kChapters[] = {6, 7, 8, -1, -128};
    static const unsigned short kAreas[] = {0x60, 0x5F, 0x160};
    if (n & 2) Cond_ByteFA = kChapters[(n >> 8) % 5];
    if (n & 4) Game_AreaNumber = kAreas[(n >> 12) % 3];
    return answer;
}
// The elevation: half the time answers MapView_Elevation's low word, so that
// Area96_SnapElevation's "already there" branch is taken; and moves the
// running object.
std::uint32_t ElevationEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) answer = (answer & 0xFFFF0000u) | (static_cast<std::uint32_t>(MapView_Elevation) & 0xFFFFu);
    if (n & 2) MoveCurrent(n >> 4);
    return answer;
}
// The effect search answers a slot of the group's four or none; half the
// time the running object moves.
std::uint32_t FindFreeEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) MoveCurrent(n);
    if (n & 2) ah::SetPointer(ah::at::kFieldState,ah::PartyOf(static_cast<unsigned char>(n >> 8)));
    return answer;
}

#define W2D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W2D_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    // the map: words x, z and a value byte, as the originals pass them (area
    // 98's init pushes a dword whose high half is the previous answer)
    {W2D_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2D_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},
    {W2D_OURS(AreaMap_Elevation), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ElevationEffect},
    {"FlagsToggle_57C160", at::kFlagsToggle, at::kFlagsToggle, 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    // the point three dwords on the caller's stack: its bytes, not its address
    {"RingAt_4220D0", at::kRingAt, at::kRingAt, 1, {0}, ah::Answer::kGarbage, 0, 0, {12}},
    // slots inside the group's four effect records, or none
    {W2D_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03, {}, &FindFreeEffect},
    {W2D_OURS(Sound_PlayEffect), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &SoundEffect},
    {W2D_OURS(Sprite_EnsureAnimation), 1, {kU8}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
    {W2D_OURS(Sprite_SetAnimation), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W2D_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W2D_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W2D_THEIRS(MoveCmd_Move), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W2D_OURS(MoveScript_SetTurnTarget), 1, {kAll}, ah::Answer::kFlag, 0, 0, {}, &MovesScriptObject},
    {W2D_OURS(KeyItem_Has), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    {W2D_OURS(Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W2D_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},
    // the group's own, called directly
    {W2D_OURS(Area95_TurnAndMove), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2D_OURS(Area103_FirstListedMember), 0, {}, ah::Answer::kByte, 0xFF, 0x02},
};
#undef W2D_OURS
#undef W2D_THEIRS

// Beyond the field frame: all twenty effect records, the active member,
// script object and chapter row pointers, Camera_ShiftY with the focus
// object pointer after it, the mark, MoveScript_PartyRecords record 0,
// Field_MoveSpeeds[3], Cond_ByteFE.
const ah::Region kRegions[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFlagRow, 4},
    {at::kCameraShiftY, 6},
    {at::kAnswerMark, 1},
    {at::kPartyRecord0, 16},
    {at::kMoveSpeed3, 1},
    {0x905E20, 1},   // Cond_ByteFE
};

// Every round: the pointers the areas follow put back inside the regions,
// the speed inside 1..16.
void Common(int area, unsigned k) {
    g_area = area;
    g_k = k;
    ah::SetPointer(at::kActiveMember, MemberRecord(ah::Next()));
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
    ah::SetPointer(at::kFocusObject, FocusRecord(ah::Next()));
    B(at::kMoveSpeed3) = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 2, 3, 4, 5, 8, 15, 16) : 1 + ah::Next() % 16);
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(std::uint32_t h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 9) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 6 : v); break;
    case 1: B(at::kCounter0) = static_cast<unsigned char>(h & 0x100 ? v % 3 : v); break;
    case 2: B(at::kCounter3) = static_cast<unsigned char>(h & 0x100 ? v % 4 : v); break;
    case 3: ah::SetPointer(at::kActiveMember, MemberRecord(h >> 16)); break;
    case 4: ah::SetPointer(at::kScriptObject, ScriptRecord(h >> 16)); break;
    case 5: ah::SetPointer(at::kFocusObject, FocusRecord(h >> 16)); break;
    case 6: move_script::SetWord(ah::Mem(at::kTailTimer), h & 0x100 ? v % 3 : v); break;
    case 7: B(at::kMoveSpeed3) = static_cast<unsigned char>(1 + v % 16); break;
    default: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 6); break;
    }
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte (tested signed by area 100's), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 4, 5, 0xFF, 0x80, 0x81, 0x7F));
}
// A 16.16 word with the high word `high` and any low word.
std::uint32_t At16(std::uint32_t high, std::uint32_t low) { return (high & 0xFFFF) << 16 | (low & 0xFFFF); }
// A high word on or beside [lo, lo + 3): each inside, one either side, a
// high byte above it (the compares are 16-bit, not 8), anything.
std::uint32_t Around3(std::uint32_t lo) {
    switch (ah::Next() % 5) {
    case 0: case 1: return lo + ah::Next() % 3;
    case 2: return ah::Half() ? lo - 1 : lo + 3;
    case 3: return (lo + ah::Next() % 3) | 0x100u;
    default: return ah::Next();
    }
}

// ---- areas 95 and 96 ----

// Handler 4's cell: z word 0x3A, x word 0x40 or 0x42, and their neighbours.
void SeedTurnCell() {
    unsigned char* const cur = Sprite_Current;
    if (ah::Often()) move_script::SetWord(cur + 0x3A, ah::Often() ? 0x3A : AH_PICK(0x39, 0x3B, 0x13A));
    if (ah::Often()) move_script::SetWord(cur + 0x36, AH_PICK(0x40, 0x42, 0x40, 0x42, 0x41, 0x3F, 0x43, 0x44, 0x140, 0x142));
}
// The twenty effect records: one standing at the cell (x word 0x41, z word
// 0x3A, live) at a random slot two times in three, others near it (one field
// off), the rest random.
void SeedEffectsAtCell() {
    for (unsigned i = 0; i < at::kEffectCount; ++i) {
        unsigned char* const e = EffectRecord(i);
        if (!ah::Half()) continue;
        e[0] = static_cast<unsigned char>(ah::Next() | 1);
        move_script::SetWord(e + 0x36, ah::Half() ? 0x41 : AH_PICK(0x40, 0x42, 0x141));
        move_script::SetWord(e + 0x3A, ah::Half() ? 0x3A : AH_PICK(0x39, 0x3B, 0x13A));
    }
    for (unsigned i = 0; i < at::kEffectCount; ++i) {
        unsigned char* const e = EffectRecord(i);
        if ((e[0] & 1) && move_script::Word(e + 0x36) == 0x41 && move_script::Word(e + 0x3A) == 0x3A) e[0] = static_cast<unsigned char>(e[0] & ~1u);
    }
    if (ah::Often()) {
        unsigned char* const e = EffectRecord(ah::Next());
        e[0] = static_cast<unsigned char>(ah::Next() | 1);
        move_script::SetWord(e + 0x36, 0x41);
        move_script::SetWord(e + 0x3A, 0x3A);
        switch (ah::Next() % 4) {   // one field off, a quarter of the time
        case 0: e[0] = static_cast<unsigned char>(e[0] & ~1u); break;
        case 1: move_script::SetWord(e + 0x36, AH_PICK(0x40, 0x42, 0x141)); break;
        case 2: move_script::SetWord(e + 0x3A, AH_PICK(0x39, 0x3B, 0x13A)); break;
        default: break;
        }
    }
    if (ah::Half()) B(at::kLeader137) = 0;
}

void Seed95(unsigned k) {
    Common(95, k);
    switch (k) {
    case k95Turn: SeedTurnCell(); break;
    case k95Wait: SeedEffectsAtCell(); break;
    case k95Spawn:
        if (ah::Often()) Game_AreaNumber = static_cast<unsigned short>(AH_PICK(0x60, 0x5F, 0x60, 0x160, 0x61));
        if (ah::Often()) Cond_ByteFA = static_cast<signed char>(AH_PICK(6, 7, 8, 0, 0xFF, 0x80, 0x7F));
        break;
    case k95Leap:
        if (ah::Often()) Sprite_Current[9] = static_cast<unsigned char>(AH_PICK(1, 2, 0x10, 0xFF, 0x80, 3));
        if (ah::Half()) move_script::SetWord(Sprite_Current + 0x3E, AH_PICK(0, 0xFF20, 0x7FFF, 0x8000));
        Settle();
        break;
    case k95Move:
        if (ah::Often()) Sprite_Current[8] = static_cast<unsigned char>(AH_PICK(7, 7, 0xF, 0x87, 3, 6, 0));
        break;
    default: break;
    }
}
void Seed96(unsigned k) {
    Common(96, k);
    switch (k) {
    case k96Turn: SeedTurnCell(); break;
    case k96Face:
        if (ah::Often()) Sprite_Current[8] = static_cast<unsigned char>(AH_PICK(7, 7, 3, 6, 8, 0x87, 0xF));
        break;
    case k96Snap:
        // MapView_Elevation a sign-extended word half the time (the ground
        // answers its low word half the time), else anything
        if (ah::Half()) MapView_Elevation = static_cast<std::int16_t>(ah::Next());
        break;
    default: break;
    }
}

// ---- area 97 ----
void Seed97(unsigned k) {
    Common(97, k);
    switch (k) {
    case k97Tail:
        if (ah::Often()) B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 1, 0, 1, 2, 0xFF, 0x80, 0x81));
        if (ah::Often()) B(at::kCounter3) = static_cast<unsigned char>(AH_PICK(2, 2, 1, 3, 0, 0x82));
        break;
    case k97Step:
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 7, 0x80, 0x81));
        break;
    default: break;
    }
}
// The step hook's (x, z): x's high word on or beside 0x30..0x32, z at or
// beside 0x3A0000 (a signed compare), negative, anything.
void Args97(unsigned k, std::uint32_t* a) {
    if (k != k97Step || !ah::Often()) return;
    a[0] = At16(Around3(0x30), a[0]);
    a[1] = AH_PICK(0x3A0000, 0x3A0001, 0x39FFFF, 0x100000, 0x80000000u, 0xFFFF0000u, 0x7FFFFFFF, 0x3A0000);
}

// ---- area 98 ----
void Seed98(unsigned k) {
    Common(98, k);
    switch (k) {
    case k98Flag9: case k98Ask: case k98Arm: SeedAnswer(); break;
    case k98Tail:
        if (ah::Often()) B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 0, 1, 0xFF));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 0, 1, 3, 0x82));
        break;
    default: break;
    }
}
// An object trigger is called (a field object, 0x904030).
void ArgsTrigger(unsigned, std::uint32_t* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}

// ---- area 99 ----
void Seed99(unsigned k) {
    Common(99, k);
    switch (k) {
    case k99Turn:
        if (ah::Often()) Sprite_Current[8] = static_cast<unsigned char>(ah::Next() % 9);
        break;
    case k99RunDrift: case k99RunLeap: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea99StateCount); break;
    case k99DriftStep:
        if (ah::Often()) Sprite_Current[0xA] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0x10, 0x11, 0x48, 0xFF));
        break;
    case k99LeapStep: {
        // the rise on each side of 0 before and after the 0x700 step
        if (ah::Often()) move_script::SetLong(Sprite_Current + 0x14, AH_PICK(0x700, 0x6FF, 0x701, 1, 0, 0xFFFFFFFFu, 0x4200, 0x80000000u, 0x800006FFu));
        if (ah::Often()) Sprite_Current[0xA] = static_cast<unsigned char>(AH_PICK(1, 2, 0, 0xE, 0xFF));
        break;
    }
    case k99Init:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(1, 1, 0, 2, 0x81));
        break;
    default: break;
    }
}

// ---- area 100 ----
void Seed100(unsigned k) {
    Common(100, k);
    switch (k) {
    case k100Choice: SeedAnswer(); break;
    case k100Tail: {
        static const std::uint32_t kStates[] = {0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 5, 0xFF, 0x80, 0x7F};
        const auto state = static_cast<unsigned char>(ah::Pick(kStates, sizeof kStates / sizeof kStates[0]));
        B(at::kTailState) = state;
        const bool on = ah::Often();   // the cell the state waits on, at its value or beside it
        switch (state) {
        case 1: B(at::kCounter0) = static_cast<unsigned char>(on ? 1 : AH_PICK(0, 2, 0x81)); break;
        case 2: move_script::SetWord(ah::Mem(at::kTailTimer), on ? 1 : AH_PICK(0, 2, 0x101, 0xFFFF)); break;
        case 3: B(at::kCounter0) = static_cast<unsigned char>(on ? 0 : AH_PICK(1, 0xFF, 0x80)); break;
        case 4: Field_Request = static_cast<unsigned char>(on ? AH_PICK(0, 1, 3, 0x82) : 2); break;
        default: break;
        }
        if (state == 2 && ah::Half()) B(at::kCounter0) = static_cast<unsigned char>(AH_PICK(0, 0xFF, 1));
        break;
    }
    case k100Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(3, 3, 2, 4, 0x83));
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(0, 7, 6, 1, 5, 8, 0x80));
        break;
    case k100EffRun:
        Sprite_Current = EffectRecord(ah::Next() % 4);
        Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % at::kArea100EffectStateCount);
        break;
    case k100EffRing: Sprite_Current = EffectRecord(ah::Next() % 4); break;
    default: break;
    }
}
// The step hook's (x, z): each high word on or beside its three.
void Args100(unsigned k, std::uint32_t* a) {
    if (k == k100Step) {
        if (!ah::Often()) return;
        a[0] = At16(Around3(0x45), a[0]);
        a[1] = At16(Around3(0x33), a[1]);
    } else if (k == k100Trig16 || k == k100Trig13 || k == k100Trig30) {
        ArgsTrigger(k, a);
    }
}

// ---- area 103 ----
void Seed103(unsigned k) {
    Common(103, k);
    switch (k) {
    case k103First: {
        // each member's +0x89 one of the three keys (or beside one), the count
        // 1..3, 0 a tenth of the time
        const unsigned char* const keys = ah::Mem(at::kArea103MemberKeys);
        for (unsigned m = 0; m < 3; ++m) {
            unsigned char* const record = ah::PartyOf(static_cast<unsigned char>(m));
            if (ah::Often()) record[0x89] = static_cast<unsigned char>(keys[ah::Next() % 3] + (ah::Half() ? 0 : 1));
        }
        if (ah::Next() % 10 == 0) Field_MemberCount = 0;
        break;
    }
    case k103Skip: case k103Count:
        if (ah::Half()) Field_State[0x89] = 0;
        break;
    case k103Talk:
        if (ah::Half()) Sprite_Current[7] = static_cast<unsigned char>(ah::Next() & ~4u);
        break;
    default: break;
    }
}
void Args103(unsigned k, std::uint32_t* a) {
    if (k == k103Trig35) ArgsTrigger(k, a);
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables, void (*seed)(unsigned),
             void (*args)(unsigned, std::uint32_t*), unsigned rounds) {
    ah::Group g{"area_w2d", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                kRegions, sizeof kRegions / sizeof kRegions[0], seed, &Disturb, rounds};
    g.settle = &Settle;
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    RunArea(95, kClones95, sizeof kClones95 / sizeof kClones95[0], nullptr, 0, &Seed95, nullptr, kRounds);
    RunArea(96, kClones96, sizeof kClones96 / sizeof kClones96[0], nullptr, 0, &Seed96, nullptr, kRounds);
    RunArea(97, kClones97, sizeof kClones97 / sizeof kClones97[0], nullptr, 0, &Seed97, &Args97, kRounds);
    RunArea(98, kClones98, sizeof kClones98 / sizeof kClones98[0], nullptr, 0, &Seed98, &ArgsTrigger, kRounds);
    RunArea(99, kClones99, sizeof kClones99 / sizeof kClones99[0], kTables99, sizeof kTables99 / sizeof kTables99[0], &Seed99, nullptr, kRounds);
    RunArea(100, kClones100, sizeof kClones100 / sizeof kClones100[0], kTables100, sizeof kTables100 / sizeof kTables100[0], &Seed100,
            &Args100, kRounds);
    RunArea(103, kClones103, sizeof kClones103 / sizeof kClones103[0], nullptr, 0, &Seed103, &Args103, kRounds);
}

}  // namespace area_w2d
