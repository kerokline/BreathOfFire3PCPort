// BOF3X_SHADOW=area_w0c: world 0's areas 27, 28, 32..37 through the area
// round's shared harness (area_harness.h), once at start-up - one
// area_harness::Run per area, each Group setting its own area number, all
// under the one shadow name. docs/area_w0c.md section 3.
//
// The clone tables are magic_rows.clone_sites over each function's extent
// (the scratch gen.py, 2026-09-27; the same routine tools/area_rows.py
// --clones prints with), each extent checked against magic_rows.descend. The
// shapes are the root table each function hangs from (docs/area_w0c.md
// section 1). Area 29 has no function of this group (its init is round
// eight's).
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0c.h"
#include "game/area_w0c_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w0c {
namespace {

namespace ah = area_harness;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])

// ---- area 27 ----
constexpr ah::CallSite kCalls4034F0[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls403520[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}, {0x32, 0x579F00}, {0x3D, 0x579F00}};
constexpr ah::CallSite kCalls403570[] = {{0x24, 0x57C7C0}, {0x30, 0x57C0F0}, {0x42, 0x531F90}, {0x86, 0x594E00}, {0xB2, 0x57C7A0}, {0xBE, 0x57C110}};
constexpr ah::JumpTable kTables403570[] = {{0x13, 0xD8, 5}};
constexpr ah::CallSite kCalls403660[] = {{0x24, 0x57C7C0}, {0x30, 0x57C0F0}, {0x42, 0x531F90}, {0x89, 0x594E00}, {0xB5, 0x57C7A0}, {0xC1, 0x57C110}};
constexpr ah::JumpTable kTables403660[] = {{0x13, 0xD8, 5}};
// ---- area 28 ----
constexpr ah::CallSite kCalls403750[] = {{0x1, 0x589810}, {0x3F, 0x5720C0}};
// ---- area 32 ---- (0x4038C0 / 0x403960 jump through Area32_StatesA / B: data tables)
constexpr ah::CallSite kCalls403880[] = {{0x28, 0x5891F0}};
constexpr ah::CallSite kCalls4038E0[] = {{0x1, 0x534590}};
constexpr ah::CallSite kCalls403920[] = {{0x2, 0x534880}};
constexpr ah::CallSite kCalls403980[] = {{0xE, 0x454CC0}};
constexpr ah::CallSite kCalls403A20[] = {{0x58, 0x454D60}};
constexpr ah::CallSite kCalls403AF0[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls403B90[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls403C00[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls403C40[] = {{0x0, 0x589810}};
// ---- area 33 ---- (0x404680 / 0x404800 jump through WorldMap33_Record08States / Record04States)
constexpr ah::CallSite kCalls4046A0[] = {{0x2, 0x589590}, {0x133, 0x5891F0}, {0x14F, 0x588F20}};
constexpr ah::CallSite kCalls404820[] = {{0x1C, 0x462A90}, {0x26, 0x589590}, {0x9B, 0x5891F0}, {0xB0, 0x589840}};
// ---- area 34 ----
constexpr ah::CallSite kCalls404D50[] = {{0x38, 0x57C0F0}, {0x5A, 0x57C140}, {0x6B, 0x57C7C0}, {0x72, 0x531F90}};
constexpr ah::CallSite kCalls404E00[] = {{0x0, 0x589810}, {0x4C, 0x5720C0}};
constexpr ah::CallSite kCalls404E60[] = {{0x8, 0x57C0F0}, {0x1B, 0x57C140}, {0x34, 0x57C0F0}};
// ---- area 35 ----
constexpr ah::CallSite kCalls404EA0[] = {{0xC, 0x57C7C0}};
constexpr ah::CallSite kCalls404EE0[] = {{0xC, 0x57C7C0}};
constexpr ah::CallSite kCalls404F20[] = {{0xC, 0x57C7C0}};
// ---- area 36 ---- (0x404FE0 jumps through Area36_EffectStates)
constexpr ah::CallSite kCalls404F80[] = {{0x10, 0x57C140}, {0x50, 0x531F90}};
constexpr ah::CallSite kCalls405000[] = {{0x22, 0x4220D0}};
// ---- area 37 ----
constexpr ah::CallSite kCalls4051B0[] = {{0x1B, 0x531F90}};
constexpr ah::CallSite kCalls4051F0[] = {{0x9, 0x57C7C0}};
constexpr ah::CallSite kCalls405240[] = {{0x2B, 0x57C0F0}, {0x3E, 0x594E00}, {0x68, 0x4976D0}, {0x8F, 0x495040}, {0xB5, 0x57C7A0}};
constexpr ah::JumpTable kTables405240[] = {{0x13, 0xD0, 4}};
constexpr ah::CallSite kCalls405320[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls405340[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls405360[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls405380[] = {{0x0, 0x57C7C0}};

const ah::Clone kClones27[] = {
    {"Area27_ChoiceTail1", 0x403400, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area27_ChoiceTail1), 0x0, false, ah::Shape::kChoice},
    {"Area27_ChoiceTail2", 0x403430, 0x21, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area27_ChoiceTail2), 0x0, false, ah::Shape::kChoice},
    {"Area27_ChoiceCounter5or6", 0x403460, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area27_ChoiceCounter5or6), 0x0, false, ah::Shape::kChoice},
    {"Area27_ChoiceCounter5Bor5A", 0x403490, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area27_ChoiceCounter5Bor5A), 0x0, false, ah::Shape::kChoice},
    {"Area27_ChoiceCounter5Cor5D", 0x4034C0, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area27_ChoiceCounter5Cor5D), 0x0, false, ah::Shape::kChoice},
    {"Area27_SpawnEffect2F", 0x4034F0, 0x28, kCalls4034F0, AH_N(kCalls4034F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area27_SpawnEffect2F), 0x0, false, ah::Shape::kHandler},
    {"Area27_ClearCells", 0x403520, 0x46, kCalls403520, AH_N(kCalls403520), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area27_ClearCells), 0x0, false, ah::Shape::kHandler},
    {"Area27_TailDropIn1", 0x403570, 0xEC, kCalls403570, AH_N(kCalls403570), nullptr, 0, kTables403570, AH_N(kTables403570), reinterpret_cast<const void*>(&::Area27_TailDropIn1), 0x0, false, ah::Shape::kTail},
    {"Area27_TailDropIn2", 0x403660, 0xEC, kCalls403660, AH_N(kCalls403660), nullptr, 0, kTables403660, AH_N(kTables403660), reinterpret_cast<const void*>(&::Area27_TailDropIn2), 0x0, false, ah::Shape::kTail},
};
enum : unsigned { k27Tail1, k27Tail2, k27Counter56, k27Counter5B, k27Counter5C, k27Spawn2F, k27ClearCells, k27DropIn1, k27DropIn2 };
const ah::Clone kClones28[] = {
    {"Area28_SpawnEffect40", 0x403750, 0x53, kCalls403750, AH_N(kCalls403750), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area28_SpawnEffect40), 0x0, false, ah::Shape::kHandler},
};
const ah::Clone kClones32[] = {
    {"Area32_ChoiceLeaderAnim", 0x403880, 0x38, kCalls403880, AH_N(kCalls403880), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_ChoiceLeaderAnim), 0x0, false, ah::Shape::kChoice},
    {"Area32_RunA", 0x4038C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_RunA), 0x0, false, ah::Shape::kHandler},
    {"Area32_ShadeStart", 0x4038E0, 0x38, kCalls4038E0, AH_N(kCalls4038E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_ShadeStart), 0x0, false, ah::Shape::kState},
    {"Area32_ShadeStep", 0x403920, 0x38, kCalls403920, AH_N(kCalls403920), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_ShadeStep), 0x0, false, ah::Shape::kState},
    {"Area32_RunB", 0x403960, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_RunB), 0x0, false, ah::Shape::kHandler},
    {"Area32_TintStart", 0x403980, 0x97, kCalls403980, AH_N(kCalls403980), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_TintStart), 0x0, false, ah::Shape::kState},
    {"Area32_TintStep", 0x403A20, 0xC2, kCalls403A20, AH_N(kCalls403A20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_TintStep), 0x0, false, ah::Shape::kState},
    {"Area32_SpawnEffect50Near", 0x403AF0, 0x9A, kCalls403AF0, AH_N(kCalls403AF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_SpawnEffect50Near), 0x0, false, ah::Shape::kHandler},
    {"Area32_SpawnEffect4F", 0x403B90, 0x67, kCalls403B90, AH_N(kCalls403B90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_SpawnEffect4F), 0x0, false, ah::Shape::kHandler},
    {"Area32_SpawnEffect4E", 0x403C00, 0x3A, kCalls403C00, AH_N(kCalls403C00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_SpawnEffect4E), 0x0, false, ah::Shape::kHandler},
    {"Area32_SpawnEffect50Far", 0x403C40, 0x9A, kCalls403C40, AH_N(kCalls403C40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area32_SpawnEffect50Far), 0x0, false, ah::Shape::kHandler},
};
enum : unsigned { k32Choice, k32RunA, k32ShadeStart, k32ShadeStep, k32RunB, k32TintStart, k32TintStep, k32Near, k32Spawn4F, k32Spawn4E, k32Far };
const ah::Clone kClones33[] = {
    {"Area33_Record08Run", 0x404680, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area33_Record08Run), 0x0, false, ah::Shape::kState},
    {"Area33_Record08Start", 0x4046A0, 0x154, kCalls4046A0, AH_N(kCalls4046A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area33_Record08Start), 0x0, false, ah::Shape::kState},
    {"Area33_Record04Run", 0x404800, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area33_Record04Run), 0x0, false, ah::Shape::kState},
    {"Area33_Record04Start", 0x404820, 0xB5, kCalls404820, AH_N(kCalls404820), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area33_Record04Start), 0x0, false, ah::Shape::kState},
};
enum : unsigned { k33Run08, k33Start08, k33Run04, k33Start04 };
const ah::Clone kClones34[] = {
    {"Area34_ChoiceTail5", 0x404D50, 0x89, kCalls404D50, AH_N(kCalls404D50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area34_ChoiceTail5), 0x0, false, ah::Shape::kChoice},
    {"Area34_SkipScript", 0x404DE0, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area34_SkipScript), 0x0, false, ah::Shape::kHandler},
    {"Area34_SpawnEffect51", 0x404E00, 0x5F, kCalls404E00, AH_N(kCalls404E00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area34_SpawnEffect51), 0x0, false, ah::Shape::kHandler},
    {"Area34_Trigger51", 0x404E60, 0x40, kCalls404E60, AH_N(kCalls404E60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area34_Trigger51), 0xFF, false, ah::Shape::kCallee},
};
enum : unsigned { k34Choice, k34Skip, k34Spawn51, k34Trigger };
const ah::Clone kClones35[] = {
    {"Area35_SetCounter1", 0x404EA0, 0x38, kCalls404EA0, AH_N(kCalls404EA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area35_SetCounter1), 0x0, false, ah::Shape::kHandler},
    {"Area35_SetCounter2", 0x404EE0, 0x38, kCalls404EE0, AH_N(kCalls404EE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area35_SetCounter2), 0x0, false, ah::Shape::kHandler},
    {"Area35_SetCounter3", 0x404F20, 0x38, kCalls404F20, AH_N(kCalls404F20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area35_SetCounter3), 0x0, false, ah::Shape::kHandler},
    {"Area35_HideObject", 0x404F60, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area35_HideObject), 0x0, false, ah::Shape::kHandler},
};
const ah::Clone kClones36[] = {
    {"Area36_StepHook", 0x404F80, 0x5E, kCalls404F80, AH_N(kCalls404F80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area36_StepHook), 0xFF, false, ah::Shape::kHook},
    {"Area36_EffectRun", 0x404FE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area36_EffectRun), 0x0, false, ah::Shape::kState},
    {"Area36_EffectStep", 0x405000, 0x2B, kCalls405000, AH_N(kCalls405000), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area36_EffectStep), 0x0, false, ah::Shape::kState},
};
enum : unsigned { k36Hook, k36Run, k36Step };
const ah::Clone kClones37[] = {
    {"Area37_ChoiceAsk5E", 0x405030, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ChoiceAsk5E), 0x0, false, ah::Shape::kChoice},
    {"Area37_ChoiceConfirm60", 0x405060, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ChoiceConfirm60), 0x0, false, ah::Shape::kChoice},
    {"Area37_ChoiceAsk72", 0x405090, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ChoiceAsk72), 0x0, false, ah::Shape::kChoice},
    {"Area37_ChoiceConfirm74", 0x4050C0, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ChoiceConfirm74), 0x0, false, ah::Shape::kChoice},
    {"Area37_ChoiceAsk86", 0x4050F0, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ChoiceAsk86), 0x0, false, ah::Shape::kChoice},
    {"Area37_ChoiceConfirm88", 0x405120, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ChoiceConfirm88), 0x0, false, ah::Shape::kChoice},
    {"Area37_ChoiceAsk9A", 0x405150, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ChoiceAsk9A), 0x0, false, ah::Shape::kChoice},
    {"Area37_ChoiceConfirm9C", 0x405180, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ChoiceConfirm9C), 0x0, false, ah::Shape::kChoice},
    {"Area37_ChoiceDropIn", 0x4051B0, 0x35, kCalls4051B0, AH_N(kCalls4051B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ChoiceDropIn), 0x0, false, ah::Shape::kChoice},
    {"Area37_ChoiceTail2E", 0x4051F0, 0x49, kCalls4051F0, AH_N(kCalls4051F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ChoiceTail2E), 0x0, false, ah::Shape::kChoice},
    {"Area37_TailLeave", 0x405240, 0xE0, kCalls405240, AH_N(kCalls405240), nullptr, 0, kTables405240, AH_N(kTables405240), reinterpret_cast<const void*>(&::Area37_TailLeave), 0x0, false, ah::Shape::kTail},
    {"Area37_Trigger45", 0x405320, 0x16, kCalls405320, AH_N(kCalls405320), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_Trigger45), 0xFF, false, ah::Shape::kCallee},
    {"Area37_Trigger46", 0x405340, 0x16, kCalls405340, AH_N(kCalls405340), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_Trigger46), 0xFF, false, ah::Shape::kCallee},
    {"Area37_Trigger47", 0x405360, 0x16, kCalls405360, AH_N(kCalls405360), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_Trigger47), 0xFF, false, ah::Shape::kCallee},
    {"Area37_Trigger48", 0x405380, 0x16, kCalls405380, AH_N(kCalls405380), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_Trigger48), 0xFF, false, ah::Shape::kCallee},
    {"Area37_ToggleScriptFlag8", 0x4053A0, 0x9, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area37_ToggleScriptFlag8), 0x0, false, ah::Shape::kHandler},
};
enum : unsigned { k37Ask5E, k37Confirm60, k37Ask72, k37Confirm74, k37Ask86, k37Confirm88, k37Ask9A, k37Confirm9C, k37DropIn, k37Tail2E, k37TailLeave };
#undef AH_N

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The callees the standard set lacks, and Effect_FindFree's answer: a slot
// 0..3 (inside the group's Effect_Objects region) or 0xFF, none, a fifth of
// the time. 0x4220D0 is logged by the three dwords its pointer holds.
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
#define W0C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
const ah::Callee kCallees[] = {
    {W0C_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03},
    {W0C_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W0C_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W0C_OURS(Tint_Release), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W0C_OURS(Transition_Start), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W0C_OURS(Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W0C_OURS(WorldMap_RecordIndex), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"PositionHook_4220D0", at::kPositionHook, at::kPositionHook, 1, {kAll}, ah::Answer::kGarbage, 0, 0, {12}},
};
#undef W0C_OURS

// The areas' own .data state tables, swapped for recorders while the fuzz
// runs (their other entries are other groups' shared bodies).
const ah::DataTable kTables32[] = {{at::kArea32StatesA, 2}, {at::kArea32StatesB, 2}};
const ah::DataTable kTables33[] = {{at::kRecord08States, 3}, {at::kRecord04States, 2}};
const ah::DataTable kTables36[] = {{at::kArea36EffectStates, 2}};

// Beyond the field frame: the effect records the spawns write (slots 0..3),
// the active member and script object pointers and what the areas read of
// the chapter row, the pending kind, area 37's mark, the character byte,
// the wait word and the pass flags.
const ah::Region kRegions[] = {
    {at::kEffectObjects, 4 * at::kEffectStride},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFlagRow, 4},
    {at::kPendingKind, 1},
    {at::kAnswerMark, 1},
    {at::kRecord0Byte9, 1},
    {at::kWaitWord, 2},
    {at::kPassFlags, 1},
};

unsigned char& B(std::uint32_t address) { return *ah::Mem(address); }

// A record the active member pointer may name: one of the four party objects
// (Sprite_ObjectsExtra, what ops D4..D6 divide it into), a field object, a
// party record, or the running object itself (the two alias in the game).
unsigned char* MemberRecord(std::uint32_t v) {
    switch (v % 4) {
    case 0: return ah::Mem(at::kSpriteObjectsExtra + (v >> 2) % 4 * 0xA4);
    case 1: return ah::Object(v >> 2);
    case 2: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return Sprite_Current;
    }
}

// Every round: the pointers the areas follow put back inside the regions.
void Common() {
    ah::SetPointer(at::kActiveMember, MemberRecord(ah::Next()));
    ah::SetPointer(at::kScriptObject, ah::Half() ? ah::Object(ah::Next()) : ah::PartyOf(static_cast<unsigned char>(ah::Next())));
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(std::uint32_t h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 6) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(v % 7 - 1); break;
    case 1: B(at::kCounter3) = static_cast<unsigned char>(h & 0x100 ? v : (h & 0x200 ? 0x31 : 0x20)); break;
    case 2: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 4); break;
    case 3: ah::SetPointer(at::kActiveMember, MemberRecord(h >> 16)); break;
    case 4: B(at::kFlagRow + (h >> 16) % 4) = v; break;
    default: move_script::SetWord(ah::Mem(at::kTailTimer), h & 0x100 ? 1 : v); break;
    }
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte (tested signed by some), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 0xFF, 0x80, 0x81, 0x7F, 4));
}
// The mode tail's state, each case and the two out-of-range sides.
void SeedTailState(unsigned last) {
    if (ah::Often()) B(at::kTailState) = static_cast<unsigned char>(ah::Next() % (last + 3) - 1);
    else if (ah::Half()) B(at::kTailState) = static_cast<unsigned char>(last);
}

// ---- area 27 ----
void Seed27(unsigned k) {
    Common();
    switch (k) {
    case k27Tail1: case k27Tail2: case k27Counter56: case k27Counter5B: case k27Counter5C: SeedAnswer(); break;
    case k27DropIn1: case k27DropIn2:
        SeedTailState(4);
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 0, 1, 3));
        if (ah::Often()) B(at::kCounter3) = static_cast<unsigned char>(AH_PICK(0x20, 0x1F, 0x21, 0x10, 0x0F, 0x11, 0x31, 0x30, 0x32));
        if (ah::Often()) move_script::SetWord(ah::Mem(at::kTailTimer), AH_PICK(1, 1, 1, 2, 0, 0x10, 0xFFFF));
        break;
    default: break;
    }
}

// ---- area 32 ----
void Seed32(unsigned k) {
    Common();
    switch (k) {
    case k32Choice: SeedAnswer(); break;
    case k32RunA: case k32RunB: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % 2); break;
    case k32Spawn4F:
        // only subtracted and divided here, never followed: any value, below
        // Sprite_Objects too (the division is signed)
        if (ah::Half()) move_script::SetLong(ah::Mem(at::kActiveMember), static_cast<std::int32_t>(ah::Next()));
        break;
    case k32TintStep: {
        // all three at 0xC0, or one step short, or either side of the signed compare
        const bool done = ah::Half();
        for (unsigned off = 0x5D; off <= 0x5F; ++off)
            if (done ? ah::Often() : ah::Half())
                Sprite_Current[off] = static_cast<unsigned char>(done ? AH_PICK(0xC0, 0xBC) : AH_PICK(0xC0, 0xBC, 0xBF, 0xC1, 0x7F, 0x80, 0x3F, 0xFF));
        break;
    }
    default: break;
    }
}

// ---- area 33 ----
void Seed33(unsigned k) {
    Common();
    // the map's width byte small enough that any (x, z) byte pair lands in
    // the harness's 8 KiB of the area block (AreaMap_Bytes is its +0x800)
    AreaMap_Header[0] = static_cast<unsigned char>(1 + ah::Next() % 0x16);
    switch (k) {
    case k33Run08: Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % 3); break;
    case k33Run04: Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    case k33Start08:
        if (ah::Often()) Sprite_Current[8] = static_cast<unsigned char>(ah::Next() % 4);
        if (ah::Often()) Sprite_Current[6] = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0xFF));
        break;
    case k33Start04:
        if (ah::Often()) B(at::kRecord0Byte9) = static_cast<unsigned char>(AH_PICK(9, 8, 10));
        if (ah::Often()) Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & ~1u);
        if (ah::Often()) Sprite_Current[0xB] = static_cast<unsigned char>(ah::Next() % 3);
        break;
    default: break;
    }
}

// ---- area 34 ----
void Seed34(unsigned k) {
    Common();
    switch (k) {
    case k34Choice:
        SeedAnswer();
        if (ah::Often()) Cond_ByteFA = static_cast<signed char>(AH_PICK(8, 8, 8, 7, 9, 0x88));
        break;
    case k34Skip:
        if (ah::Often()) Field_State[0x89] = static_cast<unsigned char>(AH_PICK(7, 6, 8, 0x87));
        break;
    default: break;
    }
}

// ---- area 36 ----
void Seed36(unsigned k) {
    Common();
    switch (k) {
    case k36Hook:
        // half the rounds every test passes but at most one, drawn to fail
        if (ah::Half()) {
            Cond_ByteFD = 2;
            B(at::kLeaderByte8) = static_cast<unsigned char>(AH_PICK(0, 6, 7));
            ah::Mem(at::kStoryFlags)[0x33 >> 3] |= 1u << (0x33 & 7);   // for the record: Flags_Test is a recorder
            if (ah::Half()) {
                if (ah::Half()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(1, 3, 0x82));
                else B(at::kLeaderByte8) = static_cast<unsigned char>(AH_PICK(1, 5, 8, 0x80, 0x86));
            }
            break;
        }
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(2, 1, 3, 0x82));
        if (ah::Often()) B(at::kLeaderByte8) = static_cast<unsigned char>(AH_PICK(0, 6, 7, 1, 5, 8, 0x80));
        break;
    case k36Run: Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    default: break;
    }
}
// The hook's (x, z): 16.16 positions whose high words sit on and beside the
// 3 x 3 rectangle (0x46..0x48, 0x23..0x25) two times in three; each side
// drawn alone so one edge can be off while the other is on.
void Args36(unsigned, std::uint32_t* a) {
    if (ah::Half()) {
        // inside, or one edge just off (x 0x45 / 0x49, z 0x22 / 0x26)
        a[0] = (0x46 + ah::Next() % 3) << 16 | (a[0] & 0xFFFF);
        a[1] = (0x23 + ah::Next() % 3) << 16 | (a[1] & 0xFFFF);
        if (ah::Half()) {
            if (ah::Half()) a[0] = static_cast<std::uint32_t>(AH_PICK(0x45, 0x49, 0x145, 0xFF46)) << 16 | (a[0] & 0xFFFF);
            else a[1] = static_cast<std::uint32_t>(AH_PICK(0x22, 0x26, 0x123, 0xFF23)) << 16 | (a[1] & 0xFFFF);
        }
        return;
    }
    if (ah::Often()) a[0] = (0x45 + ah::Next() % 5) << 16 | (a[0] & 0xFFFF);
    if (ah::Often()) a[1] = (0x22 + ah::Next() % 5) << 16 | (a[1] & 0xFFFF);
}

// ---- area 37 ----
void Seed37(unsigned k) {
    Common();
    if (k <= k37Tail2E) {
        SeedAnswer();
        return;
    }
    if (k == k37TailLeave) {
        SeedTailState(3);
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 0, 1, 3));
        if (ah::Often()) MoveScript_WaitWordDA = static_cast<unsigned short>(AH_PICK(0, 1, 0x100, 0xFFFF));
    }
}
// Area 34's and 37's object triggers are called (object, story flags).
void ArgsTrigger(unsigned, std::uint32_t* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}

void Seed28(unsigned) { Common(); }
void Seed35(unsigned) { Common(); }

void RunArea(const char* shadow, int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables,
             void (*seed)(unsigned), void (*args)(unsigned, std::uint32_t*), unsigned rounds) {
    ah::Group g{shadow, clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                kRegions, sizeof kRegions / sizeof kRegions[0], seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    RunArea("area_w0c", 27, kClones27, sizeof kClones27 / sizeof kClones27[0], nullptr, 0, &Seed27, nullptr, kRounds);
    RunArea("area_w0c", 28, kClones28, sizeof kClones28 / sizeof kClones28[0], nullptr, 0, &Seed28, nullptr, kRounds);
    RunArea("area_w0c", 32, kClones32, sizeof kClones32 / sizeof kClones32[0], kTables32, sizeof kTables32 / sizeof kTables32[0],
            &Seed32, nullptr, kRounds);
    RunArea("area_w0c", 33, kClones33, sizeof kClones33 / sizeof kClones33[0], kTables33, sizeof kTables33 / sizeof kTables33[0],
            &Seed33, nullptr, kRounds);
    RunArea("area_w0c", 34, kClones34, sizeof kClones34 / sizeof kClones34[0], nullptr, 0, &Seed34, &ArgsTrigger, kRounds);
    RunArea("area_w0c", 35, kClones35, sizeof kClones35 / sizeof kClones35[0], nullptr, 0, &Seed35, nullptr, kRounds);
    RunArea("area_w0c", 36, kClones36, sizeof kClones36 / sizeof kClones36[0], kTables36, sizeof kTables36 / sizeof kTables36[0],
            &Seed36, &Args36, kRounds);
    RunArea("area_w0c", 37, kClones37, sizeof kClones37 / sizeof kClones37[0], nullptr, 0, &Seed37, &ArgsTrigger, kRounds);
}

}  // namespace area_w0c
