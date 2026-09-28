// BOF3X_SHADOW=area_w2a: world 2's areas 76..82 and 84 through the area
// round's shared harness (area_harness.h), once at start-up - one
// area_harness::Run per area, each Group setting its own area number, all
// under the one shadow name. docs/area_w2a.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA076..084
// (2026-09-28), each row read against the disassembly (every start, extent,
// call site and the two jump tables agree); the shapes are the root table
// each function hangs from (docs/area_w2a.md section 1). The group's own
// callee (area 77's effect helper) is a recorder here like any other callee,
// and area 79's state table is swapped for recorders, so each function is
// fuzzed alone.
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2a.h"
#include "game/area_w2a_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w2a {
namespace {

namespace ah = area_harness;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])

// ---- area 77 ----
constexpr ah::CallSite kCalls40EBB0[] = {{0x8, 0x57C140}, {0x58, 0x57C140}, {0x75, 0x57C160}, {0x7F, 0x587740}, {0x86, 0x469FE0}};
constexpr ah::CallSite kCalls40EC50[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls40ECC0[] = {{0xC, 0x587740}};
constexpr ah::CallSite kCalls40ECE0[] = {{0xC, 0x587740}};
constexpr ah::CallSite kCalls40ED00[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40ED50[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40EDA0[] = {{0x8, 0x591B60}};
constexpr ah::CallSite kCalls40EDC0[] = {{0x7, 0x57C0F0}};
constexpr ah::CallSite kCalls40EDD0[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40EE20[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40EE70[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls40EEC0[] = {{0x14, 0x587740}};
constexpr ah::CallSite kCalls40EEE0[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40EF30[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls40EF80[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40EFD0[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40F020[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40F070[] = {{0x2, 0x40F140}};
constexpr ah::CallSite kCalls40F080[] = {{0x2, 0x40F140}};
constexpr ah::CallSite kCalls40F090[] = {{0x56, 0x54E790}, {0x83, 0x5720C0}, {0x94, 0x5891F0}};
constexpr ah::CallSite kCalls40F140[] = {{0x1, 0x589810}};
// ---- area 78 ----
constexpr ah::CallSite kCalls40F170[] = {{0x27, 0x57C0F0}, {0x40, 0x57C0F0}};
constexpr ah::CallSite kCalls40F220[] = {{0x28, 0x57C7C0}};
constexpr ah::CallSite kCalls40F280[] = {{0x1, 0x589810}};
// ---- area 79: the two choices' jump tables (six entries each, in .text) ----
constexpr ah::JumpTable kTables40F2E0[] = {{0xF, 0x44, 6}};
constexpr ah::JumpTable kTables40F340[] = {{0x1E, 0x54, 6}};
// ---- area 81 ----
constexpr ah::CallSite kCalls40F5B0[] = {{0x1, 0x589810}};
// ---- area 82 ----
constexpr ah::CallSite kCalls40F6A0[] = {{0x8, 0x57C140}, {0x16, 0x4976D0}, {0x28, 0x4976D0}};
// ---- area 84 ----
constexpr ah::CallSite kCalls40F6E0[] = {{0x24, 0x57C0F0}, {0x2D, 0x572650}};

const ah::Clone kClones76[] = {
    {"Area76_StepDisarmTail5", 0x40EB90, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area76_StepDisarmTail5), 0xFF, false, ah::Shape::kHook},
};
const ah::Clone kClones77[] = {
    {"Area77_CellHook", 0x40EBB0, 0x92, kCalls40EBB0, AH_N(kCalls40EBB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_CellHook), 0xFF, false, ah::Shape::kHook},
    {"Area77_Trigger34", 0x40EC50, 0x1D, kCalls40EC50, AH_N(kCalls40EC50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Trigger34), 0xFF, false, ah::Shape::kCallee},
    {"Area77_ChoiceMessage", 0x40EC70, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_ChoiceMessage), 0x0, false, ah::Shape::kChoice},
    {"Area77_ChoiceCounter0", 0x40EC90, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_ChoiceCounter0), 0x0, false, ah::Shape::kChoice},
    {"Area77_SetByteFE", 0x40ECC0, 0x13, kCalls40ECC0, AH_N(kCalls40ECC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_SetByteFE), 0x0, false, ah::Shape::kHandler},
    {"Area77_ClearByteFE", 0x40ECE0, 0x13, kCalls40ECE0, AH_N(kCalls40ECE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_ClearByteFE), 0x0, false, ah::Shape::kHandler},
    {"Area77_Spawn3AtMember0ListA", 0x40ED00, 0x42, kCalls40ED00, AH_N(kCalls40ED00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Spawn3AtMember0ListA), 0x0, false, ah::Shape::kHandler},
    {"Area77_Spawn4AtMember0ListB", 0x40ED50, 0x42, kCalls40ED50, AH_N(kCalls40ED50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Spawn4AtMember0ListB), 0x0, false, ah::Shape::kHandler},
    {"Area77_RemoveItem47", 0x40EDA0, 0x11, kCalls40EDA0, AH_N(kCalls40EDA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_RemoveItem47), 0x0, false, ah::Shape::kHandler},
    {"Area77_SetFlag4", 0x40EDC0, 0x10, kCalls40EDC0, AH_N(kCalls40EDC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_SetFlag4), 0x0, false, ah::Shape::kHandler},
    {"Area77_Spawn2AtMember0ListC", 0x40EDD0, 0x42, kCalls40EDD0, AH_N(kCalls40EDD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Spawn2AtMember0ListC), 0x0, false, ah::Shape::kHandler},
    {"Area77_Spawn3AtMember1ListD", 0x40EE20, 0x42, kCalls40EE20, AH_N(kCalls40EE20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Spawn3AtMember1ListD), 0x0, false, ah::Shape::kHandler},
    {"Area77_Spawn3AtMember2ListE", 0x40EE70, 0x46, kCalls40EE70, AH_N(kCalls40EE70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Spawn3AtMember2ListE), 0x0, false, ah::Shape::kHandler},
    {"Area77_ShiftCameraDownSound", 0x40EEC0, 0x1B, kCalls40EEC0, AH_N(kCalls40EEC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_ShiftCameraDownSound), 0x0, false, ah::Shape::kHandler},
    {"Area77_Spawn4AtMember1List0", 0x40EEE0, 0x42, kCalls40EEE0, AH_N(kCalls40EEE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Spawn4AtMember1List0), 0x0, false, ah::Shape::kHandler},
    {"Area77_Spawn4AtMember2List0", 0x40EF30, 0x46, kCalls40EF30, AH_N(kCalls40EF30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Spawn4AtMember2List0), 0x0, false, ah::Shape::kHandler},
    {"Area77_Spawn4AtMember0List0", 0x40EF80, 0x42, kCalls40EF80, AH_N(kCalls40EF80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Spawn4AtMember0List0), 0x0, false, ah::Shape::kHandler},
    {"Area77_Spawn1AtMember0ListF", 0x40EFD0, 0x42, kCalls40EFD0, AH_N(kCalls40EFD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Spawn1AtMember0ListF), 0x0, false, ah::Shape::kHandler},
    {"Area77_Spawn3AtMember0ListG", 0x40F020, 0x42, kCalls40F020, AH_N(kCalls40F020), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_Spawn3AtMember0ListG), 0x0, false, ah::Shape::kHandler},
    {"Area77_SpawnEffect4A", 0x40F070, 0x9, kCalls40F070, AH_N(kCalls40F070), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_SpawnEffect4A), 0x0, false, ah::Shape::kHandler},
    {"Area77_SpawnEffect4C", 0x40F080, 0x9, kCalls40F080, AH_N(kCalls40F080), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_SpawnEffect4C), 0x0, false, ah::Shape::kHandler},
    {"Area77_LeapStep", 0x40F090, 0xA9, kCalls40F090, AH_N(kCalls40F090), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_LeapStep), 0x0, false, ah::Shape::kHandler},
    {"Area77_SpawnEffectKind", 0x40F140, 0x2B, kCalls40F140, AH_N(kCalls40F140), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area77_SpawnEffectKind), 0x0, false, ah::Shape::kCallee},
};
enum : unsigned {
    k77Cell, k77Trigger, k77Choice0, k77Choice1, k77SetFE, k77ClearFE, k77SpawnA, k77SpawnB, k77Remove, k77Flag4, k77SpawnC, k77SpawnD,
    k77SpawnE, k77Camera, k77Spawn0M1, k77Spawn0M2, k77Spawn0M0, k77SpawnF, k77SpawnG, k77Effect4A, k77Effect4C, k77Leap, k77EffectKind
};
const ah::Clone kClones78[] = {
    {"Area78_ChoiceRowFlag16or17", 0x40F170, 0x49, kCalls40F170, AH_N(kCalls40F170), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area78_ChoiceRowFlag16or17), 0x0, false, ah::Shape::kChoice},
    {"Area78_ChoiceCounter2Bor2C", 0x40F1C0, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area78_ChoiceCounter2Bor2C), 0x0, false, ah::Shape::kChoice},
    {"Area78_ChoiceCounterAor2", 0x40F1F0, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area78_ChoiceCounterAor2), 0x0, false, ah::Shape::kChoice},
    {"Area78_ChoiceResetScene", 0x40F220, 0x5B, kCalls40F220, AH_N(kCalls40F220), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area78_ChoiceResetScene), 0x0, false, ah::Shape::kChoice},
    {"Area78_SpawnEffect44", 0x40F280, 0x28, kCalls40F280, AH_N(kCalls40F280), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area78_SpawnEffect44), 0x0, false, ah::Shape::kHandler},
};
enum : unsigned { k78RowFlag, k78Counter2B, k78CounterA, k78Reset, k78Effect44 };
const ah::Clone kClones79[] = {
    {"Area79_ChoiceCounter1or14", 0x40F2B0, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area79_ChoiceCounter1or14), 0x0, false, ah::Shape::kChoice},
    {"Area79_ChoiceCounter3", 0x40F2E0, 0x5C, nullptr, 0, nullptr, 0, kTables40F2E0, AH_N(kTables40F2E0), reinterpret_cast<const void*>(&::Area79_ChoiceCounter3), 0x0, false, ah::Shape::kChoice},
    {"Area79_ChoiceMessageCounter3", 0x40F340, 0x6C, nullptr, 0, nullptr, 0, kTables40F340, AH_N(kTables40F340), reinterpret_cast<const void*>(&::Area79_ChoiceMessageCounter3), 0x0, false, ah::Shape::kChoice},
    {"Area79_ChoiceCounter8or9", 0x40F3B0, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area79_ChoiceCounter8or9), 0x0, false, ah::Shape::kChoice},
    {"Area79_ChoiceCounter5orA", 0x40F3E0, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area79_ChoiceCounter5orA), 0x0, false, ah::Shape::kChoice},
    {"Area79_ObjectState", 0x40F410, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area79_ObjectState), 0x0, false, ah::Shape::kHandler},
    {"Area79_StateSlide", 0x40F430, 0x62, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area79_StateSlide), 0x0, false, ah::Shape::kState},
};
enum : unsigned { k79Counter1, k79Counter3, k79Message3, k79Counter8, k79Counter5, k79State, k79Slide };
const ah::Clone kClones80[] = {
    {"Area80_ChoiceMessage0", 0x40F4A0, 0x2E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area80_ChoiceMessage0), 0x0, false, ah::Shape::kChoice},
    {"Area80_ChoiceMessage1", 0x40F4D0, 0x2E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area80_ChoiceMessage1), 0x0, false, ah::Shape::kChoice},
    {"Area80_ChoiceMessage2", 0x40F500, 0x2E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area80_ChoiceMessage2), 0x0, false, ah::Shape::kChoice},
    {"Area80_ResetCameraShift", 0x40F530, 0x11, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area80_ResetCameraShift), 0x0, false, ah::Shape::kHandler},
};
const ah::Clone kClones81[] = {
    {"Area81_ChoiceCounter1or2", 0x40F550, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area81_ChoiceCounter1or2), 0x0, false, ah::Shape::kChoice},
    {"Area81_ChoiceMessageCounter3or4", 0x40F580, 0x2E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area81_ChoiceMessageCounter3or4), 0x0, false, ah::Shape::kChoice},
    {"Area81_SpawnEffect27AtObject", 0x40F5B0, 0x40, kCalls40F5B0, AH_N(kCalls40F5B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area81_SpawnEffect27AtObject), 0x0, false, ah::Shape::kHandler},
    {"Area81_ShiftCameraDown", 0x40F5F0, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area81_ShiftCameraDown), 0x0, false, ah::Shape::kHandler},
    {"Area81_ShiftCameraUp", 0x40F600, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area81_ShiftCameraUp), 0x0, false, ah::Shape::kHandler},
};
const ah::Clone kClones82[] = {
    {"Area82_SkipByPose", 0x40F610, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area82_SkipByPose), 0x0, false, ah::Shape::kHandler},
    {"Area82_SetMargin5", 0x40F640, 0xD, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area82_SetMargin5), 0x0, false, ah::Shape::kHandler},
    {"Area82_SkipByPose456", 0x40F650, 0x4D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area82_SkipByPose456), 0x0, false, ah::Shape::kHandler},
    {"Area82_OpenMessageByRowFlag4", 0x40F6A0, 0x38, kCalls40F6A0, AH_N(kCalls40F6A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area82_OpenMessageByRowFlag4), 0x0, false, ah::Shape::kHandler},
};
enum : unsigned { k82Skip, k82Margin, k82Skip456, k82Message };
const ah::Clone kClones84[] = {
    {"Area84_SetFlag31At5", 0x40F6E0, 0x3E, kCalls40F6E0, AH_N(kCalls40F6E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area84_SetFlag31At5), 0x0, false, ah::Shape::kHandler},
};
#undef AH_N

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(std::uint32_t address) { return *ah::Mem(address); }

// A record the active member pointer may name: one of the four party objects
// (Sprite_ObjectsExtra), a field object, a party record, or the running
// object itself (area_w1c_fuzz.cpp's).
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

// ---- the stand-ins the group lists ----

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, on purpose (each only half the time, from
// Noise): after these calls the callers read a cell again - Sprite_Current
// after Effect_Spawn (the ten spawns), after Effect_FindFree (area 81's
// spawn at the object), after AreaMap_Elevation (the leap) and after
// MoveCmd_TestFB (area 84); MoveScript_Object after Scena06_Leap and
// Sprite_SetAnimation (the leap); the leader's pose after Flags_Test (area
// 77's cell hook reads it after the row flag's test, a quarter of the time
// so that the switch still matches). The harness's own disturbance reaches a
// group cell about one call in 24.
std::uint32_t MovesCurrent(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3);
    return answer;
}
std::uint32_t MovesScriptObject(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kScriptObject, ScriptRecord(n >> 8));
    return answer;
}
std::uint32_t MovesPose(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if ((n & 3) == 0) B(at::kLeaderPose) = static_cast<unsigned char>(n >> 8);
    return answer;
}

#define W2A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W2A_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    {"FlagsToggle_57C160", at::kFlagsToggle, at::kFlagsToggle, 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {"SpawnKind4_469FE0", at::kSpawnKind4, at::kSpawnKind4, 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W2A_OURS(Flags_Test), 2, {kAll, kU8}, ah::Answer::kBool, 0, 0, {}, &MovesPose},
    // slots inside the group's four effect records, or none
    {W2A_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03, {}, &MovesCurrent},
    {W2A_THEIRS(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02, {}, &MovesCurrent},
    {W2A_OURS(Inventory_Remove), 3, {kAll, kAll, kAll}, ah::Answer::kFlag, 0, 0},
    {W2A_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W2A_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
    {W2A_OURS(Scena06_Leap), 7, {kAll, kAll, kAll, kAll, kAll, kAll, kAll}, ah::Answer::kFlag, 0, 0, {}, &MovesScriptObject},
    {W2A_OURS(AreaMap_Elevation), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W2A_OURS(Sprite_SetAnimation), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    // the group's own, called directly
    {W2A_OURS(Area77_SpawnEffectKind), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
};
#undef W2A_OURS
#undef W2A_THEIRS

// Area 79's state table, read in place by Area79_ObjectState: its two
// entries swapped for recorders while the fuzz runs.
const ah::DataTable kTables79[] = {{at::kArea79States, at::kArea79StateCount}};

// Beyond the field frame: the effect records (slots 0..3), the active member
// and script object pointers, the chapter row pointer, Camera_ShiftY,
// Cond_ByteFE.
const ah::Region kRegions[] = {
    {at::kEffectObjects, 4 * at::kEffectStride},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFlagRow, 4},
    {at::kCameraShiftY, 2},
    {at::kByteFE, 1},
    // Effect_Objects "record" 0xFF, past the 20: where a helper that took
    // Effect_FindFree's none for a slot would write (+0, +5, +0x34, +0x38).
    // The originals never do; a region here lets the fuzz see one that did.
    {at::kEffectObjects + 0xFF * at::kEffectStride, 0x40},
};

// Every round: the pointers the areas follow put back inside the regions.
void Common() {
    ah::SetPointer(at::kActiveMember, MemberRecord(ah::Next()));
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(std::uint32_t h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 8) {
    case 0: B(at::kTailKind) = static_cast<unsigned char>(h & 0x100 ? 5 : v); break;
    case 1: ah::SetPointer(at::kActiveMember, MemberRecord(h >> 16)); break;
    case 2: ah::SetPointer(at::kScriptObject, ScriptRecord(h >> 16)); break;
    case 3: B(at::kLeaderPose) = v; break;
    case 4: B(at::kLeaderByte89) = static_cast<unsigned char>(h & 0x100 ? 5 : v); break;
    case 5: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 8); break;
    case 6: B(at::kPartyList0 + (h >> 16) % 3) = static_cast<unsigned char>(v % 8); break;
    default: B(at::kByteFE) = v; break;
    }
}

// A choice answer: each value a handler tests (0..5 for area 79's jump
// tables), their neighbours, a negative byte (tested signed), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 1, 0, 1, 2, 3, 4, 5, 6, 0xFF, 0x80, 0x81, 0x7F));
}

// ---- area 76 ----
void Seed76(unsigned) {
    Common();
    if (ah::Often()) B(at::kTailKind) = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6, 0x85, 0));
}

// ---- area 77 ----

// A cell switch's (x, z) and the leader's pose: record i's exactly, or one
// field off by one, a high byte above the cell byte half the time. The seed
// draws the record and sets the pose (the arguments are drawn after the
// round's state is captured, so a write there would be lost); the arguments
// then follow the seed's draw (area_w1c_fuzz.cpp's).
const unsigned char* g_switch = nullptr;   // the record drawn, or none
unsigned g_switch_off = 0;                 // 0 none off, 1 x off, 2 z off, 3 the pose off
void SeedSwitch() {
    g_switch = nullptr;
    if (!ah::Often()) return;
    g_switch = ah::Mem(at::kArea77CellSwitches + (ah::Next() % at::kArea77CellSwitchCount) * 4u);
    g_switch_off = ah::Half() ? 1 + ah::Next() % 3 : 0;
    const unsigned char pose = g_switch[2];
    B(at::kLeaderPose) = g_switch_off == 3 ? static_cast<unsigned char>(pose + (ah::Half() ? 1 : 0xFF)) : pose;
}
void CellArgs(std::uint32_t* a) {
    if (!g_switch) return;
    const unsigned char* const r = g_switch;
    a[0] = (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0) | r[0];
    a[1] = (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0) | r[1];
    if (g_switch_off == 1) a[0] = (a[0] & ~0xFFu) | static_cast<unsigned char>(r[0] + (ah::Half() ? 1 : 0xFF));
    if (g_switch_off == 2) a[1] = (a[1] & ~0xFFu) | static_cast<unsigned char>(r[1] + (ah::Half() ? 1 : 0xFF));
}

// The leap: the running area's +0x10 script [object +3] (area 77's seven)
// and the object's position word +0xA. Half the time the position is one
// whose operand byte (two past it) is 4 in that script, when the first 0x100
// bytes hold one; else any position in them, or any word (every script lies
// in .data, and 0xFFFF past it still does).
constexpr unsigned kArea77Scripts = 7;
void SeedLeap() {
    unsigned char* const object = MoveScript_Object;
    const unsigned index = ah::Next() % kArea77Scripts;
    object[3] = static_cast<unsigned char>(index);
    const std::uint32_t descriptor = static_cast<std::uint32_t>(move_script::Long(ah::Mem(at::kDescriptors + 77 * 4)));
    const std::uint32_t scripts = static_cast<std::uint32_t>(move_script::Long(ah::Mem(descriptor + 0x10)));
    const std::uint32_t script = static_cast<std::uint32_t>(move_script::Long(ah::Mem(scripts + index * 4)));
    unsigned position = ah::Next() % 0x100;
    if (ah::Half()) {
        const unsigned start = ah::Next() % 0x100;
        for (unsigned i = 0; i < 0x100; ++i) {
            const unsigned p = (start + i) % 0x100;
            if (*ah::Mem(script + p + 2) == 4) {
                position = p;
                break;
            }
        }
    } else if (ah::Next() % 8 == 0) {
        position = ah::Next() & 0xFFFF;
    }
    move_script::SetWord(object + 0xA, position);
}

void Seed77(unsigned k) {
    Common();
    switch (k) {
    case k77Cell: SeedSwitch(); break;
    case k77Choice0: case k77Choice1: SeedAnswer(); break;
    case k77SpawnA: case k77SpawnB: case k77SpawnC: case k77SpawnD: case k77SpawnE: case k77Spawn0M1: case k77Spawn0M2:
    case k77Spawn0M0: case k77SpawnF: case k77SpawnG:
        for (unsigned m = 0; m < 3; ++m)
            if (ah::Often()) B(at::kPartyList0 + m) = static_cast<unsigned char>(ah::Next() % 8);
        break;
    case k77Leap: SeedLeap(); break;
    default: break;
    }
}
// An object trigger is called (a field object, 0x904030); the cell hook its
// switch's cell; the effect helper a kind word.
void Args77(unsigned k, std::uint32_t* a) {
    if (k == k77Cell) {
        CellArgs(a);
    } else if (k == k77Trigger) {
        a[0] = Key(ah::Object(a[0]));
        a[1] = at::kStoryFlags;
    }
}

// ---- area 78 ----
void Seed78(unsigned k) {
    Common();
    if (k != k78Effect44) SeedAnswer();
}

// ---- area 79 ----
void Seed79(unsigned k) {
    Common();
    switch (k) {
    case k79State:
        // a state inside the table's two (a larger one aborts ours and jumps
        // through the next table's bytes in the original)
        Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea79StateCount);
        break;
    case k79Slide:
        // the count 0, 1, at a table edge (0xF, 0x10: the & 0xF wraps), or any
        if (ah::Often()) Sprite_Current[0xA] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0xF, 0x10, 0x11, 0xFF, 0x80));
        break;
    default: SeedAnswer(); break;
    }
}

// ---- area 80, 81 ----
void Seed80(unsigned) {
    Common();
    SeedAnswer();
}
void Seed81(unsigned) {
    Common();
    SeedAnswer();
}

// ---- area 82 ----
void Seed82(unsigned k) {
    Common();
    if (k == k82Skip || k == k82Skip456)
        if (ah::Often()) Field_State[0x89] = static_cast<unsigned char>(AH_PICK(4, 5, 6, 8, 3, 7, 9, 0x84, 0x88));
}

// ---- area 84 ----
void Seed84(unsigned) {
    Common();
    if (ah::Often()) B(at::kLeaderByte89) = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6, 0x85));
}

void RunArea(int area, const ah::Clone* clones, unsigned n, void (*seed)(unsigned), void (*args)(unsigned, std::uint32_t*),
             const ah::DataTable* tables, unsigned n_tables, unsigned rounds) {
    ah::Group g{"area_w2a", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                kRegions, sizeof kRegions / sizeof kRegions[0], seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
#define W2A_RUN(area, seed, args, tables, n_tables) \
    RunArea(area, kClones##area, sizeof kClones##area / sizeof kClones##area[0], seed, args, tables, n_tables, kRounds)
    W2A_RUN(76, &Seed76, nullptr, nullptr, 0);
    W2A_RUN(77, &Seed77, &Args77, nullptr, 0);
    W2A_RUN(78, &Seed78, nullptr, nullptr, 0);
    W2A_RUN(79, &Seed79, nullptr, kTables79, 1);
    W2A_RUN(80, &Seed80, nullptr, nullptr, 0);
    W2A_RUN(81, &Seed81, nullptr, nullptr, 0);
    W2A_RUN(82, &Seed82, nullptr, nullptr, 0);
    W2A_RUN(84, &Seed84, nullptr, nullptr, 0);
#undef W2A_RUN
}

}  // namespace area_w2a
