// BOF3X_SHADOW=area_w3e: world 3's areas 136 and 139..142 through the area
// round's shared harness (area_harness.h), once at start-up - one
// area_harness::Run per area, each Group setting its own area number, all
// under the one shadow name. docs/area_w3e.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA136 and
// AREA139..142 (2026-09-28), each row read against the disassembly (every
// start, extent, call site and the three in-function jump tables agree); the
// shapes are the root table each function hangs from (docs/area_w3e.md
// section 1). The group's own callees (area 140's blocked test and cell hook,
// area 141's shade steps and cell painter) are recorders here like any other
// callee, so each function is fuzzed alone; the state tables of areas 140,
// 141 and 142 are swapped for recorders (DataTable).
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3e.h"
#include "game/area_w3e_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w3e {
namespace {

namespace ah = area_harness;
using S = ah::Shape;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define OURS(f) reinterpret_cast<const void*>(&::f)

// ---- area 136 ----
constexpr ah::CallSite kCalls41EFE0[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls41F010[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls41F0A0[] = {{0x3F, 0x578C10}};
constexpr ah::CallSite kCalls41F100[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}};
constexpr ah::CallSite kCalls41F130[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls41F180[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls41F1D0[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls41F220[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}};
constexpr ah::CallSite kCalls41F280[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls41F2D0[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls41F340[] = {{0x22, 0x5918E0}, {0x4D, 0x4976D0}, {0x84, 0x57C7A0}, {0x94, 0x57C0F0}, {0xB0, 0x4976D0},
                                         {0xC2, 0x57C160}, {0xE2, 0x531F90}, {0x12A, 0x594E00}, {0x146, 0x57C7A0}, {0x165, 0x57C110}};
constexpr ah::JumpTable kTables41F340[] = {{0x1C, 0x170, 8}};
// ---- area 139 ----
constexpr ah::CallSite kCalls41F510[] = {{0x24, 0x57C0F0}, {0x2D, 0x572650}};
constexpr ah::CallSite kCalls41F550[] = {{0x4C, 0x57C160}, {0x56, 0x587740}};
// ---- area 140 ----
constexpr ah::CallSite kCalls41F5C0[] = {{0x17, 0x57C7C0}};
constexpr ah::CallSite kCalls41F600[] = {{0x2C, 0x57C0F0}, {0x35, 0x572650}};
constexpr ah::CallSite kCalls41F650[] = {{0xA, 0x57C140}, {0x66, 0x41F790}, {0x9F, 0x578C10}, {0xAA, 0x57C4C0}, {0x101, 0x5891F0}, {0x117, 0x41F9B0}};
constexpr ah::CallSite kCalls41F790[] = {{0x6, 0x518080}, {0x1C, 0x536700}};
constexpr ah::CallSite kCalls41F7C0[] = {{0x18, 0x57C7A0}, {0x1F, 0x531F90}, {0x3C, 0x57C0F0}, {0xDA, 0x57C7C0}, {0xEF, 0x57C7C0},
                                         {0xFD, 0x57C7A0}, {0x11A, 0x57C110}, {0x120, 0x52FEB0}, {0x138, 0x4976D0}, {0x159, 0x57C7A0}};
constexpr ah::JumpTable kTables41F7C0[] = {{0x14, 0x170, 12}};
constexpr ah::CallSite kCalls41F960[] = {{0x10, 0x57C140}, {0x33, 0x57C7C0}};
constexpr ah::CallSite kCalls41F9B0[] = {{0x5A, 0x57C140}, {0xFC, 0x57C0F0}, {0x113, 0x572650}, {0x125, 0x589810}, {0x17F, 0x587740}, {0x191, 0x57C7C0}};
constexpr ah::CallSite kCalls41FB60[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls41FBE0[] = {{0xE, 0x57C110}, {0x22, 0x572650}, {0x2A, 0x589840}};
// ---- area 141 ----
constexpr ah::CallSite kCalls41FC10[] = {{0x38, 0x454DC0}};
constexpr ah::CallSite kCalls41FC50[] = {{0x3A, 0x573400}, {0x4E, 0x578C10}};
constexpr ah::CallSite kCalls41FCD0[] = {{0x33, 0x41FD20}};
constexpr ah::CallSite kCalls41FDD0[] = {{0x3E, 0x41FE20}};
constexpr ah::CallSite kCalls41FEE0[] = {{0x7, 0x57C0F0}, {0x18, 0x57C7C0}, {0x1F, 0x5734F0}, {0x2B, 0x420430}};
constexpr ah::CallSite kCalls41FF40[] = {{0x10, 0x420430}, {0x1C, 0x420430}, {0x3A, 0x57C140}, {0x53, 0x420430}, {0x66, 0x420430}, {0x7B, 0x57C140},
                                         {0x94, 0x420430}, {0xA7, 0x420430}, {0xBC, 0x57C140}, {0xD5, 0x420430}, {0xE8, 0x420430}, {0x108, 0x420430}};
constexpr ah::CallSite kCalls420060[] = {{0x2C, 0x57C7A0}, {0x33, 0x531F90}, {0x4C, 0x4976D0}, {0xC9, 0x4976D0}, {0xD7, 0x5734F0}, {0xFB, 0x57C0F0},
                                         {0x107, 0x420430}, {0x113, 0x420430}, {0x173, 0x57C7A0}, {0x197, 0x57C140}, {0x1AE, 0x57C0F0}, {0x1BA, 0x420430},
                                         {0x1C6, 0x420430}, {0x1FB, 0x57C140}, {0x212, 0x57C0F0}, {0x21E, 0x420430}, {0x22A, 0x420430}};
constexpr ah::JumpTable kTables420060[] = {{0x1B, 0x28C, 15}};
constexpr ah::CallSite kCalls420360[] = {{0x3F, 0x57C7C0}};
constexpr ah::CallSite kCalls4203C0[] = {{0x1A, 0x57C7C0}, {0x30, 0x57C7C0}, {0x35, 0x533E50}, {0x52, 0x420430}, {0x5E, 0x420430}};
constexpr ah::CallSite kCalls420430[] = {{0x43, 0x579F00}, {0x85, 0x579F00}};
constexpr ah::CallSite kCalls4204D0[] = {{0x5, 0x57CD90}, {0x2D, 0x57CD90}, {0x74, 0x57AD10}, {0x7D, 0x589200}, {0x93, 0x57AD10}, {0x9C, 0x589200}};
constexpr ah::CallSite kCalls420580[] = {{0x1, 0x57CD90}, {0x37, 0x57AD10}, {0x40, 0x589200}};
constexpr ah::CallSite kCalls4205D0[] = {{0x5, 0x57CD90}, {0x29, 0x57CD90}, {0x70, 0x57AD10}, {0x86, 0x57AD10}};
constexpr ah::CallSite kCalls420670[] = {{0x5, 0x57CD90}, {0x29, 0x57CD90}, {0x70, 0x57A010}, {0x86, 0x57A010}};
constexpr ah::CallSite kCalls420710[] = {{0x1, 0x57CD90}, {0x37, 0x57A010}};
// ---- area 142 ----
constexpr ah::CallSite kCalls4207C0[] = {{0x1E, 0x5891F0}};

const ah::Clone kClones136[] = {
    {"Area136_ChoiceTail19Sub0", 0x41EFE0, 0x2D, kCalls41EFE0, AH_N(kCalls41EFE0), nullptr, 0, nullptr, 0, OURS(Area136_ChoiceTail19Sub0), 0, false, S::kChoice},
    {"Area136_ChoiceTail19Sub1", 0x41F010, 0x2D, kCalls41F010, AH_N(kCalls41F010), nullptr, 0, nullptr, 0, OURS(Area136_ChoiceTail19Sub1), 0, false, S::kChoice},
    {"Area136_ChoiceMessage2", 0x41F040, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area136_ChoiceMessage2), 0, false, S::kChoice},
    {"Area136_ChoiceMessage3", 0x41F070, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area136_ChoiceMessage3), 0, false, S::kChoice},
    {"Area136_MoveUntilLimit", 0x41F0A0, 0x53, kCalls41F0A0, AH_N(kCalls41F0A0), nullptr, 0, nullptr, 0, OURS(Area136_MoveUntilLimit), 0, false, S::kHandler},
    {"Area136_CellsRow51", 0x41F100, 0x25, kCalls41F100, AH_N(kCalls41F100), nullptr, 0, nullptr, 0, OURS(Area136_CellsRow51), 0, false, S::kHandler},
    {"Area136_SpawnMember1Kind2", 0x41F130, 0x42, kCalls41F130, AH_N(kCalls41F130), nullptr, 0, nullptr, 0, OURS(Area136_SpawnMember1Kind2), 0, false, S::kHandler},
    {"Area136_SpawnMember2Kind2", 0x41F180, 0x46, kCalls41F180, AH_N(kCalls41F180), nullptr, 0, nullptr, 0, OURS(Area136_SpawnMember2Kind2), 0, false, S::kHandler},
    {"Area136_SpawnMember1Kind1", 0x41F1D0, 0x42, kCalls41F1D0, AH_N(kCalls41F1D0), nullptr, 0, nullptr, 0, OURS(Area136_SpawnMember1Kind1), 0, false, S::kHandler},
    {"Area136_CellsRow10", 0x41F220, 0x25, kCalls41F220, AH_N(kCalls41F220), nullptr, 0, nullptr, 0, OURS(Area136_CellsRow10), 0, false, S::kHandler},
    {"Area136_Counter0ByTile", 0x41F250, 0x25, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area136_Counter0ByTile), 0, false, S::kHandler},
    {"Area136_SpawnMember1Kind3", 0x41F280, 0x42, kCalls41F280, AH_N(kCalls41F280), nullptr, 0, nullptr, 0, OURS(Area136_SpawnMember1Kind3), 0, false, S::kHandler},
    {"Area136_SpawnLeaderKind1", 0x41F2D0, 0x42, kCalls41F2D0, AH_N(kCalls41F2D0), nullptr, 0, nullptr, 0, OURS(Area136_SpawnLeaderKind1), 0, false, S::kHandler},
    {"Area136_ShiftCameraUp2", 0x41F320, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area136_ShiftCameraUp2), 0, false, S::kHandler},
    {"Area136_Tail19", 0x41F340, 0x19E, kCalls41F340, AH_N(kCalls41F340), nullptr, 0, kTables41F340, AH_N(kTables41F340), OURS(Area136_Tail19), 0, false, S::kTail},
    {"Area136_InitExtraObject", 0x41F4E0, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area136_InitExtraObject), 0, false, S::kInit},
};
enum : unsigned {
    k136Tail0, k136Tail1, k136Msg2, k136Msg3, k136Move, k136Row51, k136Spawn1K2, k136Spawn2K2, k136Spawn1K1, k136Row10, k136Tile,
    k136Spawn1K3, k136SpawnLK1, k136Camera, k136Tail19, k136Init
};
const ah::Clone kClones139[] = {
    {"Area139_FlagIfLeader89Is5", 0x41F510, 0x3E, kCalls41F510, AH_N(kCalls41F510), nullptr, 0, nullptr, 0, OURS(Area139_FlagIfLeader89Is5), 0, false, S::kHandler},
    {"Area139_CellHook", 0x41F550, 0x63, kCalls41F550, AH_N(kCalls41F550), nullptr, 0, nullptr, 0, OURS(Area139_CellHook), 0xFF, false, S::kHook},
};
enum : unsigned { k139Flag, k139Cell };
const ah::Clone kClones140[] = {
    {"Area140_ChoiceArmTail41", 0x41F5C0, 0x34, kCalls41F5C0, AH_N(kCalls41F5C0), nullptr, 0, nullptr, 0, OURS(Area140_ChoiceArmTail41), 0, false, S::kChoice},
    {"Area140_FlagIfLeader89Is567", 0x41F600, 0x46, kCalls41F600, AH_N(kCalls41F600), nullptr, 0, nullptr, 0, OURS(Area140_FlagIfLeader89Is567), 0, false, S::kHandler},
    {"Area140_FollowAndAct", 0x41F650, 0x132, kCalls41F650, AH_N(kCalls41F650), nullptr, 0, nullptr, 0, OURS(Area140_FollowAndAct), 0, false, S::kHandler},
    {"Area140_BlockedAhead", 0x41F790, 0x2E, kCalls41F790, AH_N(kCalls41F790), nullptr, 0, nullptr, 0, OURS(Area140_BlockedAhead), 0xFF, false, S::kCallee},
    {"Area140_Tail41", 0x41F7C0, 0x1A0, kCalls41F7C0, AH_N(kCalls41F7C0), nullptr, 0, kTables41F7C0, AH_N(kTables41F7C0), OURS(Area140_Tail41), 0, false, S::kTail},
    {"Area140_StepHook", 0x41F960, 0x42, kCalls41F960, AH_N(kCalls41F960), nullptr, 0, nullptr, 0, OURS(Area140_StepHook), 0xFF, false, S::kHook},
    {"Area140_CellHook", 0x41F9B0, 0x1AE, kCalls41F9B0, AH_N(kCalls41F9B0), nullptr, 0, nullptr, 0, OURS(Area140_CellHook), 0xFF, false, S::kHook},
    {"Area140_Trigger18", 0x41FB60, 0xF, kCalls41FB60, AH_N(kCalls41FB60), nullptr, 0, nullptr, 0, OURS(Area140_Trigger18), 0xFF, false, S::kCallee},
    {"Area140_Effect71Run", 0x41FB70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area140_Effect71Run), 0, false, S::kCallee},
    {"Area140_Effect71Start", 0x41FB90, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area140_Effect71Start), 0, false, S::kState},
    {"Area140_Effect71Count", 0x41FBB0, 0x30, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area140_Effect71Count), 0, false, S::kState},
    {"Area140_Effect71End", 0x41FBE0, 0x2F, kCalls41FBE0, AH_N(kCalls41FBE0), nullptr, 0, nullptr, 0, OURS(Area140_Effect71End), 0, false, S::kState},
};
enum : unsigned { k140Choice, k140Flag, k140Follow, k140Blocked, k140Tail, k140Step, k140Cell, k140Trig18, k140EffRun, k140EffStart, k140EffCount, k140EffEnd };
const ah::Clone kClones141[] = {
    {"Area141_ShadeOff", 0x41FC10, 0x3F, kCalls41FC10, AH_N(kCalls41FC10), nullptr, 0, nullptr, 0, OURS(Area141_ShadeOff), 0, false, S::kHandler},
    {"Area141_MoveToX45", 0x41FC50, 0x57, kCalls41FC50, AH_N(kCalls41FC50), nullptr, 0, nullptr, 0, OURS(Area141_MoveToX45), 0, false, S::kHandler},
    {"Area141_RunShadeDown", 0x41FCB0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area141_RunShadeDown), 0, false, S::kHandler},
    {"Area141_ShadeDownStart", 0x41FCD0, 0x42, kCalls41FCD0, AH_N(kCalls41FCD0), nullptr, 0, nullptr, 0, OURS(Area141_ShadeDownStart), 0, false, S::kState},
    {"Area141_ShadeDownStep", 0x41FD20, 0x89, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area141_ShadeDownStep), 0, false, S::kState},
    {"Area141_RunShadeUp", 0x41FDB0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area141_RunShadeUp), 0, false, S::kHandler},
    {"Area141_ShadeUpStart", 0x41FDD0, 0x4E, kCalls41FDD0, AH_N(kCalls41FDD0), nullptr, 0, nullptr, 0, OURS(Area141_ShadeUpStart), 0, false, S::kState},
    {"Area141_ShadeUpStep", 0x41FE20, 0xB2, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area141_ShadeUpStep), 0, false, S::kState},
    {"Area141_ChoiceFlag7D", 0x41FEE0, 0x55, kCalls41FEE0, AH_N(kCalls41FEE0), nullptr, 0, nullptr, 0, OURS(Area141_ChoiceFlag7D), 0, false, S::kChoice},
    {"Area141_InitCells", 0x41FF40, 0x111, kCalls41FF40, AH_N(kCalls41FF40), nullptr, 0, nullptr, 0, OURS(Area141_InitCells), 0, false, S::kInit},
    {"Area141_Tail52", 0x420060, 0x2FC, kCalls420060, AH_N(kCalls420060), nullptr, 0, kTables420060, AH_N(kTables420060), OURS(Area141_Tail52), 0, false, S::kTail},
    {"Area141_CellHook", 0x420360, 0x5C, kCalls420360, AH_N(kCalls420360), nullptr, 0, nullptr, 0, OURS(Area141_CellHook), 0xFF, false, S::kHook},
    {"Area141_Trigger57", 0x4203C0, 0x69, kCalls4203C0, AH_N(kCalls4203C0), nullptr, 0, nullptr, 0, OURS(Area141_Trigger57), 0xFF, false, S::kCallee},
    {"Area141_PaintCells", 0x420430, 0x9A, kCalls420430, AH_N(kCalls420430), nullptr, 0, nullptr, 0, OURS(Area141_PaintCells), 0, false, S::kCallee},
    {"Area141_PlacePairAnimated", 0x4204D0, 0xAA, kCalls4204D0, AH_N(kCalls4204D0), nullptr, 0, nullptr, 0, OURS(Area141_PlacePairAnimated), 0, false, S::kCallee},
    {"Area141_PlaceOneAnimated", 0x420580, 0x4A, kCalls420580, AH_N(kCalls420580), nullptr, 0, nullptr, 0, OURS(Area141_PlaceOneAnimated), 0, false, S::kCallee},
    {"Area141_PlacePair6x", 0x4205D0, 0x94, kCalls4205D0, AH_N(kCalls4205D0), nullptr, 0, nullptr, 0, OURS(Area141_PlacePair6x), 0, false, S::kCallee},
    {"Area141_PlacePair0x", 0x420670, 0x94, kCalls420670, AH_N(kCalls420670), nullptr, 0, nullptr, 0, OURS(Area141_PlacePair0x), 0, false, S::kCallee},
    {"Area141_PlaceOne0x", 0x420710, 0x41, kCalls420710, AH_N(kCalls420710), nullptr, 0, nullptr, 0, OURS(Area141_PlaceOne0x), 0, false, S::kCallee},
};
enum : unsigned {
    k141Off, k141Move, k141RunDown, k141DownStart, k141DownStep, k141RunUp, k141UpStart, k141UpStep, k141Choice, k141Init, k141Tail,
    k141Cell, k141Trig57, k141Paint, k141PlacePairA, k141PlaceOneA, k141PlacePair6, k141PlacePair0, k141PlaceOne0
};
const ah::Clone kClones142[] = {
    {"Area142_RunWait", 0x420760, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area142_RunWait), 0, false, S::kHandler},
    {"Area142_WaitRequest3", 0x420780, 0x34, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area142_WaitRequest3), 0, false, S::kState},
    {"Area142_WaitRequestEnd", 0x4207C0, 0x3C, kCalls4207C0, AH_N(kCalls4207C0), nullptr, 0, nullptr, 0, OURS(Area142_WaitRequestEnd), 0, false, S::kState},
};
enum : unsigned { k142Run, k142Wait3, k142End };
#undef AH_N
#undef OURS

// Area 141's shade tables are one run of four code entries (the shade-down
// dispatcher reads all four; the shade-up one the last two).
const ah::DataTable kTables140[] = {{at::kArea140EffectStates, at::kArea140EffectStateCount}};
const ah::DataTable kTables141[] = {{at::kArea141ShadeDownStates, at::kArea141ShadeDownReach}};
const ah::DataTable kTables142[] = {{at::kArea142States, at::kArea142StateCount}};

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(std::uint32_t address) { return *ah::Mem(address); }
unsigned char* EffectRecord(unsigned slot) { return ah::Mem(at::kEffectObjects + slot % at::kEffectCount * at::kEffectStride); }

// A record the active member pointer may name: one of the four party objects
// (Sprite_ObjectsExtra), a field object, a party record, or the running
// object itself.
unsigned char* MemberRecord(std::uint32_t v) {
    switch (v % 4) {
    case 0: return ah::Mem(at::kSpriteObjectsExtra + (v >> 2) % 4 * at::kObjectStride);
    case 1: return ah::Object(v >> 2);
    case 2: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return Sprite_Current;
    }
}
unsigned char* ScriptRecord(std::uint32_t v) { return v & 1 ? ah::Object(v >> 1) : ah::PartyOf(static_cast<unsigned char>(v >> 1)); }

// ---- the stand-ins the group lists ----

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, on purpose (each only half the time, from
// Noise): after these calls the callers read Sprite_Current again (area
// 136's spawns store the slot through it, area 140's handler 1 reads its
// direction and sets its +7 / +0x2A, area 142's end state sets its +4, area
// 140's effect end reads its cell) or the script object (area 136's handler 0
// moves its position). The harness's own disturbance moves Sprite_Current
// about one call in 24.
void MoveCurrent(std::uint32_t n) {
    Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3);
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
// The map byte: 0x89 a third of the time (area 140's blocked test compares
// it), else the recorder's.
std::uint32_t ByteAtEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n % 3 == 0) answer = (answer & 0xFFFFFF00u) | 0x89;
    return answer;
}
// Flags_Test also moves the running object (area 140's handler 1 reads it
// after the test) and the chapter's flag row is untouched.
std::uint32_t FlagsTestEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if ((n & 7) == 1) MoveCurrent(n >> 4);
    return answer;
}

// The event ops take their object from the word 0x903850 (scena_se.cpp):
// the value each call sees is part of what it does.
std::uint32_t NotesObjectWord(const std::uint32_t*, std::uint32_t answer) {
    ah::Note(Word(ah::Mem(at::kScratch850)));
    return answer;
}

#define W3E_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W3E_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    // the map: words x, z and a value byte (area 141's painter pushes x and z
    // with the high halves of other registers)
    {W3E_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0},
    {W3E_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0, {}, &ByteAtEffect},
    // an effect slot of the group's four records or none; Sprite_Current moved
    {W3E_OURS(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFF, 0x03, {}, &MovesCurrent},
    {W3E_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03},
    {W3E_OURS(Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},
    // any of the thirty field objects, or none
    {W3E_OURS(Sprite_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x1D},
    // the ops read the object word 0x903850: logged with each call
    {W3E_OURS(EventOp_6x), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &NotesObjectWord},
    {W3E_OURS(EventOp_0x), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &NotesObjectWord},
    {W3E_OURS(Flags_Toggle), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W3E_OURS(Flags_Test), 2, {kAll, kU8}, ah::Answer::kBool, 0, 0, {}, &FlagsTestEffect},
    {W3E_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},
    {W3E_THEIRS(MoveCmd_Move), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W3E_OURS(MoveCmd_MoveKind2), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W3E_OURS(Sprite_FaceDirection), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W3E_OURS(Sprite_SetAnimation), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W3E_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3E_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3E_OURS(KeyItem_Has), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    {W3E_OURS(Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W3E_OURS(Party_HealJoined), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3E_OURS(Field_ZoneCounterRoll), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    // the group's own, called directly
    // answers 0 or 1 (its two returns)
    {W3E_OURS(Area140_BlockedAhead), 2, {kU16, kU16}, ah::Answer::kByte, 0x00, 0x01},
    {W3E_OURS(Area140_CellHook), 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0},
    {W3E_OURS(Area141_ShadeDownStep), 0, {}, ah::Answer::kPhase, 0, 0},
    {W3E_OURS(Area141_ShadeUpStep), 0, {}, ah::Answer::kPhase, 0, 0},
    {W3E_OURS(Area141_PaintCells), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
};
#undef W3E_OURS
#undef W3E_THEIRS

// Beyond the field frame: all twenty effect records, the active member,
// script object and chapter row pointers, Camera_ShiftY, Input_Held /
// Input_Pressed, the action button word, the tile word, Field_Kind2Hold,
// Cond_ByteFE, Sprite_Kind2.
const ah::Region kRegions[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFlagRow, 4},
    {at::kCameraShiftY, 2},
    {at::kInputHeld, 8},
    {at::kButtonMap6, 2},
    {at::kTile, 2},
    {at::kKind2Hold, 1},
    {at::kCondByteFE, 1},
    {at::kSpriteKind2, at::kObjectStride},
    // effect "slot 0xFF": where a spawn whose none test failed would write
    // (Effect_Objects + 0xFF << 7), so such a write is compared
    {at::kEffectObjects + 0xFFu * at::kEffectStride, at::kEffectStride},
};

// Which area and function the round is running (set by the seeds; read by
// the args hooks).
int g_area = 0;
unsigned g_k = 0;
// A pick the seed makes that the args hook follows (a cell entry, an object).
unsigned g_pick = 0;
bool g_off = false;

// Every round: the pointers the areas follow put back inside the regions.
void Common(int area, unsigned k) {
    g_area = area;
    g_k = k;
    g_pick = 0;
    g_off = false;
    ah::SetPointer(at::kActiveMember, MemberRecord(ah::Next()));
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(std::uint32_t h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 9) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 14 : v); break;
    case 1: B(at::kCounter0) = static_cast<unsigned char>(h & 0x100 ? v % 3 : v); break;
    case 2: B(at::kCounter3) = static_cast<unsigned char>(h & 0x100 ? (v & 1 ? 0x28 : 0) : v); break;
    case 3: ah::SetPointer(at::kActiveMember, MemberRecord(h >> 16)); break;
    case 4: ah::SetPointer(at::kScriptObject, ScriptRecord(h >> 16)); break;
    case 5: SetWord(ah::Mem(at::kTailTimer), h & 0x100 ? v % 3 : v); break;
    case 6: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 3); break;
    case 7: B(at::kTailSub) = static_cast<unsigned char>(v & 1); break;
    default: SetWord(ah::Mem(at::kInputHeld + 4), h & 0x100 ? 0 : v); break;
    }
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte (read signed by area 136's message choices), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0xFF, 0x80, 0x7F, 3));
}
// A field request: 2 (a message up) often, each other value a state tests.
void SeedRequest() {
    if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 0, 3, 5, 1, 0x82));
}
// An object trigger is called (a field object, 0x904030).
void ArgsTrigger(std::uint32_t* a, unsigned obj) {
    a[0] = Key(ah::Object(obj));
    a[1] = at::kStoryFlags;
}
// A 16.16 word with the high word `high` and any low word.
std::uint32_t At16(std::uint32_t high, std::uint32_t low) { return (high & 0xFFFF) << 16 | (low & 0xFFFF); }

// A cell hook's entry: one of `count` entries (x, z, direction nibble), its
// cell as the arguments and the leader's direction byte its nibble, one field
// off a quarter of the time (the direction with a high nibble, or one off).
void SeedCellPose(std::uint32_t table, unsigned count, unsigned stride) {
    g_pick = ah::Next() % count;
    g_off = ah::Next() % 4 == 0;
    const unsigned char* const e = ah::Mem(table + g_pick * stride);
    unsigned char pose = static_cast<unsigned char>(e[2] & 0xF);
    if (g_off && ah::Half()) pose = static_cast<unsigned char>(ah::Half() ? pose | 0x10 : pose + 1);
    if (ah::Often()) B(at::kLeaderPose) = pose;
}
void ArgsCell(std::uint32_t table, unsigned stride, std::uint32_t* a) {
    if (!ah::Often()) return;
    const unsigned char* const e = ah::Mem(table + g_pick * stride);
    unsigned x = e[0], z = e[1];
    if (g_off) {
        if (ah::Half()) x = x + 1;
        else z = z - 1;
    }
    // the high bytes are not read: any
    a[0] = (ah::Next() & 0xFFFFFF00u) | (x & 0xFF);
    a[1] = (ah::Next() & 0xFFFFFF00u) | (z & 0xFF);
}

// ---- area 136 ----
void Seed136(unsigned k) {
    Common(136, k);
    switch (k) {
    case k136Tail0: case k136Tail1: case k136Msg2: case k136Msg3: SeedAnswer(); break;
    case k136Move: {
        // the z ahead on, beside or far from the limit for sub-kind 0 or 1
        B(at::kTailSub) = static_cast<unsigned char>(ah::Often() ? ah::Next() % 2 : ah::Next());
        unsigned char* const cur = Sprite_Current;
        if (ah::Often()) {
            const std::uint32_t step = static_cast<std::uint32_t>(Long(ah::Mem(at::kDirectionSteps + (cur[8] & 7u) * 8 + 4)));
            const std::uint32_t limit = static_cast<std::uint32_t>(B(at::kArea136Limits + B(at::kTailSub))) << 16;
            const std::uint32_t d = AH_PICK(0, 0, 1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u);
            SetLong(cur + 0x38, static_cast<std::int32_t>(limit - step + d));
        }
        break;
    }
    case k136Spawn1K2: case k136Spawn2K2: case k136Spawn1K1: case k136Spawn1K3: case k136SpawnLK1:
        if (ah::Often()) B(at::kPartyList1) = static_cast<unsigned char>(ah::Next() % 12);
        if (ah::Often()) B(at::kPartyList2) = static_cast<unsigned char>(ah::Next() % 12);
        break;
    case k136Tile:
        if (ah::Often()) SetWord(ah::Mem(at::kTile), (ah::Next() & 0xFF00u) | AH_PICK(0x61, 0x62, 0x64, 0x66, 0x67, 0xE2));
        break;
    case k136Tail19:
        if (ah::Often()) B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 10, 11, 12, 13, 0, 1, 2, 10, 11, 12, 13, 3, 9, 14, 0xFF, 0x80, 0x8A));
        SeedRequest();
        if (ah::Often()) B(at::kCounter3) = static_cast<unsigned char>(AH_PICK(0x28, 0x28, 0, 0, 0x27, 0x29, 1, 0xA8));
        if (ah::Often()) B(at::kTailSub) = static_cast<unsigned char>(AH_PICK(0, 1, 0, 1, 0x80, 2));
        break;
    case k136Init:
        if (ah::Often()) B(at::kLastZone) = static_cast<unsigned char>(AH_PICK(1, 4, 1, 4, 0, 5, 2, 0x81));
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(4, 1, 4, 1, 0, 5, 3, 0x84));
        break;
    default: break;
    }
}

// ---- area 139 ----
void Seed139(unsigned k) {
    Common(139, k);
    switch (k) {
    case k139Flag:
        if (ah::Often()) B(at::kLeader89) = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6, 0x85, 0));
        break;
    case k139Cell: SeedCellPose(at::kArea139Cells, at::kArea139CellCount, 4); break;
    default: break;
    }
}
void Args139(unsigned k, std::uint32_t* a) {
    if (k == k139Cell) ArgsCell(at::kArea139Cells, 4, a);
}

// ---- area 140 ----
// Area 140's handler 1: the running object's cell placed so the cell ahead
// (by the leader's direction, which it takes) is on, inside or beside the
// rectangle x 0x44..0x50, z 6..0x10.
void SeedFollow() {
    unsigned char* const cur = Sprite_Current;
    const unsigned char pose = static_cast<unsigned char>(ah::Often() ? ah::Next() % 8 : ah::Next());
    B(at::kLeaderPose) = pose;
    const unsigned char* const delta = ah::Mem(at::kCellDelta + pose * 2u);
    if (ah::Often()) {
        const unsigned x = AH_PICK(0x43, 0x44, 0x4A, 0x50, 0x51, 0x44, 0x50);
        const unsigned z = AH_PICK(5, 6, 0xA, 0x10, 0x11, 6, 0x10);
        SetWord(cur + 0x36, x - static_cast<unsigned>(static_cast<int>(static_cast<signed char>(delta[0]))));
        SetWord(cur + 0x3A, z - static_cast<unsigned>(static_cast<int>(static_cast<signed char>(delta[1]))));
    }
    if (ah::Often()) B(at::kLeaderSteps) = static_cast<unsigned char>(ah::Half() ? 0 : 1 + ah::Next() % 0x20);
    if (ah::Often()) B(at::kLeader137) = static_cast<unsigned char>(AH_PICK(1, 1, 0, 2, 0x81));
    // the action button in Input_Pressed half the time
    const unsigned button = 1u << (ah::Next() % 16);
    SetWord(ah::Mem(at::kButtonMap6), ah::Often() ? button : ah::Next());
    SetWord(ah::Mem(at::kInputHeld + 4), ah::Half() ? (ah::Next() | button) : (ah::Next() & ~button));
}
// Area 140's cell hook: an entry's cell and pose, and with its rectangle the
// members' timed step landing on, beside or away from it.
void SeedCell140() {
    SeedCellPose(at::kArea140Cells, at::kArea140CellCount, at::kArea140CellStride);
    const unsigned char* const e = ah::Mem(at::kArea140Cells + g_pick * at::kArea140CellStride);
    const unsigned rect = e[5];
    if (rect == 0 || !ah::Often()) return;
    const unsigned char* const r = ah::Mem(at::kArea140Rects + rect * 4);
    const unsigned rx = Word(r), rz = Word(r + 2);
    const unsigned m = ah::Next() % 3;
    unsigned char* const p = ah::PartyOf(static_cast<unsigned char>(m));
    const unsigned n = ah::Next() % 4;
    p[9] = static_cast<unsigned char>(n);
    const unsigned dx = AH_PICK(0, 1, 0, 1, 2, 0xFFFF);
    const unsigned dz = AH_PICK(0, 1, 0, 1, 2, 0xFFFF);
    const std::uint32_t sx = static_cast<std::uint32_t>(Long(p + 0xC)) * n, sz = static_cast<std::uint32_t>(Long(p + 0x10)) * n;
    SetLong(p + 0x34, static_cast<std::int32_t>(At16(rx + dx, ah::Next()) - sx));
    SetLong(p + 0x38, static_cast<std::int32_t>(At16(rz + dz, ah::Next()) - sz));
}
void Seed140(unsigned k) {
    Common(140, k);
    switch (k) {
    case k140Choice: SeedAnswer(); break;
    case k140Flag:
        if (ah::Often()) B(at::kLeader89) = static_cast<unsigned char>(AH_PICK(5, 6, 7, 4, 8, 0x85, 0));
        break;
    case k140Follow: SeedFollow(); break;
    case k140Tail:
        if (ah::Often()) B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 5, 6, 7, 10, 11, 1, 2, 6, 3, 4, 8, 12, 0xFF));
        if (ah::Often()) Camera_Distance = static_cast<short>(AH_PICK(0x680, 0x640, 0, 0x40, 0x6C0, 0x10680, 0x680, 0));
        SeedRequest();
        if (ah::Half()) Field_Request = 0;
        break;
    case k140Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(0, 0, 1, 0x80));
        break;
    case k140Cell: SeedCell140(); break;
    case k140EffRun:
        Sprite_Current = EffectRecord(ah::Next() % 4);
        Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % at::kArea140EffectStateCount);
        break;
    case k140EffStart: case k140EffEnd: Sprite_Current = EffectRecord(ah::Next() % 4); break;
    case k140EffCount:
        Sprite_Current = EffectRecord(ah::Next() % 4);
        if (ah::Often()) Sprite_Current[9] = static_cast<unsigned char>(AH_PICK(1, 1, 2, 0, 0xD2));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 4, 6, 0x85));
        break;
    default: break;
    }
}
void Args140(unsigned k, std::uint32_t* a) {
    switch (k) {
    case k140Cell: ArgsCell(at::kArea140Cells, at::kArea140CellStride, a); break;
    case k140Step:
        if (!ah::Often()) return;
        a[0] = AH_PICK(0x558000, 0x558000, 0x558001, 0x557FFF, 0x548000, 0x1558000);
        a[1] = At16(AH_PICK(0xA, 0xB, 0xA, 0xB, 9, 0xC, 0x10A), a[1]);
        break;
    case k140Trig18: ArgsTrigger(a, a[0]); break;
    case k140Blocked:
        // the high halves are the caller's registers: any
        a[0] = (a[0] & 0xFFFF0000u) | (ah::Next() & 0xFF);
        a[1] = (a[1] & 0xFFFF0000u) | (ah::Next() & 0xFF);
        break;
    default: break;
    }
}

// ---- area 141 ----
// A run record for the painter beyond the image's nine: any count 0..127, a
// direction, values (read only; the same for both passes).
unsigned char g_run[5];
void Seed141(unsigned k) {
    Common(141, k);
    unsigned char* const cur = Sprite_Current;
    switch (k) {
    case k141Move:
        if (ah::Half()) Sprite_Current = ah::Mem(at::kSpriteKind2);
        if (ah::Often())
            SetLong(Sprite_Current + 0x34,
                    static_cast<std::int32_t>(0x450000u + AH_PICK(0, 0, 0x7FFF, 0x8000, 0xFFFF8000u, 0xFFFF7FFFu, 0xFFFFFFFFu, 0x10000)));
        break;
    case k141RunDown: cur[4] = static_cast<unsigned char>(ah::Next() % at::kArea141ShadeDownReach); break;
    case k141RunUp: cur[4] = static_cast<unsigned char>(ah::Next() % at::kArea141StateCount); break;
    case k141DownStep:
        for (unsigned i = 0x5D; i <= 0x5F; ++i)
            if (ah::Often()) cur[i] = static_cast<unsigned char>(AH_PICK(0x80, 0x80, 0x82, 0x81, 0x7F, 0xC4));
        break;
    case k141UpStep:
        for (unsigned i = 0x5D; i <= 0x5F; ++i)
            if (ah::Often()) cur[i] = static_cast<unsigned char>(AH_PICK(0xC0, 0xC0, 0xBC, 0xBF, 0xC1, 0x80, 0x7F, 0xC4));
        break;
    case k141Choice: SeedAnswer(); break;
    case k141Init:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0, 1, 2, 3, 0x81));
        break;
    case k141Tail: {
        static const std::uint32_t kStates[] = {0, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x14, 0x1E, 0x1F, 0x28, 0x29, 0x32, 0x33,
                                                0, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x14, 0x1E, 0x1F, 0x28, 0x29, 0x32, 0x33,
                                                1, 9, 0x11, 0x15, 0x20, 0x31, 0x34, 0xFF, 0x80};
        B(at::kTailState) = static_cast<unsigned char>(ah::Pick(kStates, sizeof kStates / sizeof kStates[0]));
        SeedRequest();
        if (ah::Often()) SetWord(ah::Mem(at::kTailTimer), AH_PICK(1, 1, 0, 2, 0x1C1, 0x1C0, 0x1C2, 0xFFFF, 0x3C));
        if (ah::Often()) SetWord(ah::Mem(at::kInputHeld + 4), ah::Half() ? 0 : 1u << (ah::Next() % 16));
        if (ah::Often()) B(at::kCounter0) = static_cast<unsigned char>(AH_PICK(1, 1, 0, 2, 0x81));
        if (ah::Often()) B(at::kKind2Hold) = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0x80));
        break;
    }
    case k141Cell: SeedCellPose(at::kArea141Cells, at::kArea141CellCount, 4); break;
    case k141Trig57:
        g_pick = ah::Next() % at::kObjectCount;
        if (ah::Often()) SetWord(ah::Object(g_pick) + 0x88, AH_PICK(0xC000, 0xC001, 0xC002, 0xC000, 0xC001, 0xC002, 0xC003, 0xBFFF, 0x4000));
        break;
    case k141Paint:
        g_pick = ah::Next() % 10;
        g_run[0] = static_cast<unsigned char>(ah::Next());
        g_run[1] = static_cast<unsigned char>(ah::Next());
        g_run[2] = static_cast<unsigned char>(ah::Half() ? AH_PICK(0, 0x80, 1, 0x81, 0x7F, 0xFF, 5, 0x85) : ah::Next());
        g_run[3] = static_cast<unsigned char>(ah::Next());
        g_run[4] = static_cast<unsigned char>(ah::Next());
        break;
    default: break;
    }
}
void Args141(unsigned k, std::uint32_t* a) {
    switch (k) {
    case k141Cell: ArgsCell(at::kArea141Cells, 4, a); break;
    case k141Trig57: ArgsTrigger(a, g_pick); break;
    case k141Paint:
        a[0] = g_pick < 9 ? at::Run(g_pick) : Key(g_run);
        // `on` a byte: 0, 1, a high byte over 0 (read as 0), anything
        a[1] = AH_PICK(0, 1, 0, 1, 0x100, 0x80, 0xFFFFFF00u, 2);
        break;
    default: break;
    }
}

// ---- area 142 ----
void Seed142(unsigned k) {
    Common(142, k);
    switch (k) {
    case k142Run: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea142StateCount); break;
    case k142Wait3: case k142End:
        if (ah::Often()) B(at::kCounter0) = static_cast<unsigned char>(AH_PICK(1, 0, 2, 0x81));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(3, 3, 2, 4, 0x83));
        break;
    default: break;
    }
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables, void (*seed)(unsigned),
             void (*args)(unsigned, std::uint32_t*), unsigned rounds) {
    ah::Group g{"area_w3e", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                kRegions, sizeof kRegions / sizeof kRegions[0], seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    RunArea(136, kClones136, sizeof kClones136 / sizeof kClones136[0], nullptr, 0, &Seed136, nullptr, kRounds);
    RunArea(139, kClones139, sizeof kClones139 / sizeof kClones139[0], nullptr, 0, &Seed139, &Args139, kRounds);
    RunArea(140, kClones140, sizeof kClones140 / sizeof kClones140[0], kTables140, sizeof kTables140 / sizeof kTables140[0], &Seed140,
            &Args140, kRounds);
    RunArea(141, kClones141, sizeof kClones141 / sizeof kClones141[0], kTables141, sizeof kTables141 / sizeof kTables141[0], &Seed141,
            &Args141, kRounds);
    RunArea(142, kClones142, sizeof kClones142 / sizeof kClones142[0], kTables142, sizeof kTables142 / sizeof kTables142[0], &Seed142,
            nullptr, kRounds);
}

}  // namespace area_w3e
