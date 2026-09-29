// BOF3X_SHADOW=battle_e6: group BE6's 39 battle-engine functions through the
// boss harness's engine frame (boss_harness.h, docs/boss_harness.md section
// 10), once at start-up: one boss_harness::Run per unit - the transformation,
// the stats and rolls, the three effect tasks, the field slots, BMAGIC's four
// cells. docs/battle_e6.md section 5.
//
// The clone rows are tools/band_rows.py's (--group BE6 --clones, 2026-09-29),
// each read against the disassembly: 0x4523C0's extent is the tool's 0x1A4
// (its case table inside), the cut's 0x452460 is dropped (a case of it). The
// helpers are kHelper (the arguments this file's Args), the tasks kTask
// (Sprite_Current a task slot; the dispatchers' state byte +1 drawn below
// their tables, the tables' cells swapped for recorders), the history's step
// kStep. The functions of the group each other calls are recorders here
// (their own clone rows test them), louder where the caller reads a cell
// again after the call (FormEffect).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e6.h"
#include "game/battle_e6_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"

namespace battle_e6 {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using S = bh::Shape;
using A = bh::Answer;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)
#define BE6_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)

bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret = 0,
            std::uint8_t state_at = 1, std::uint8_t states = 0) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, shape};
    c.state_at = state_at;
    c.states = states;
    return c;
}

// ===========================================================================
// The clone rows (tools/band_rows.py --group BE6 --clones)
// ===========================================================================

constexpr bh::CallSite kCalls4514A0[] = {{0x52, 0x453300}, {0x83, 0x451670}, {0x98, 0x452080}, {0xFF, 0x4517A0}, {0x1BA, 0x453300}};
constexpr bh::CallSite kCalls451670[] = {{0x11, 0x451750}, {0x2F, 0x4516F0}};
constexpr bh::CallSite kCalls451750[] = {{0x3F, 0x4523C0}};
constexpr bh::CallSite kCalls4517A0[] = {{0x15C, 0x5B93D2}, {0x16F, 0x5B93D2}, {0x178, 0x5B93D2}, {0x1E2, 0x451C50}, {0x21A, 0x446F50},
                                         {0x281, 0x451C50}, {0x2B8, 0x446F20}, {0x2F7, 0x451C50}, {0x32E, 0x446F20}, {0x36D, 0x451C50},
                                         {0x3A4, 0x446F20}, {0x3E3, 0x451C50}, {0x41A, 0x446F20}, {0x49E, 0x451C80}, {0x4A3, 0x451DA0}};
constexpr bh::CallSite kCalls451DA0[] = {{0x81, 0x451FE0},  {0x9E, 0x451FE0},  {0xB9, 0x451FE0},  {0xD3, 0x451FE0},  {0xF0, 0x451FE0},
                                         {0x10B, 0x451FE0}, {0x128, 0x451FE0}, {0x143, 0x451FE0}, {0x160, 0x451FE0}, {0x17B, 0x451FE0},
                                         {0x198, 0x451FE0}, {0x1B3, 0x451FE0}, {0x1CE, 0x451FE0}, {0x1EC, 0x451FE0}};
constexpr bh::CallSite kCalls452080[] = {{0x86, 0x446F50}, {0xF2, 0x446F20}, {0x138, 0x446F20}, {0x17D, 0x446F20}, {0x1C3, 0x446F20}};
constexpr bh::CallSite kCalls4523C0[] = {{0x22, 0x4456C0}, {0xE9, 0x452570}, {0x116, 0x452570}, {0x13B, 0x452570}, {0x160, 0x452570}};
constexpr bh::JumpTable kTables4523C0[] = {{0x76, 0x184, 8}};
constexpr bh::CallSite kCalls452680[] = {{0x1D, 0x588F20}};
constexpr bh::CallSite kCalls4526B0[] = {{0x1D, 0x5891F0}};
constexpr bh::CallSite kCalls4526F0[] = {{0x0, 0x4411B0}, {0x15, 0x5891F0}};
constexpr bh::CallSite kCalls452720[] = {{0xAD, 0x5891F0}, {0xBD, 0x4411B0}};
constexpr bh::CallSite kCalls4527F0[] = {{0x7, 0x435180},   {0xB3, 0x4FB9F0},  {0xD8, 0x4FBBD0},  {0xFC, 0x4FB9F0},
                                         {0x121, 0x4FBBD0}, {0x139, 0x5891F0}, {0x145, 0x4530D0}, {0x155, 0x4411B0}};
constexpr bh::CallSite kCalls452950[] = {{0x0, 0x4411B0}, {0xC2, 0x4411B0}};
constexpr bh::CallSite kCalls452A20[] = {{0x5B, 0x4411B0}};
constexpr bh::CallSite kCalls452A80[] = {{0x3B, 0x5891F0}, {0x49, 0x4351F0}};
constexpr bh::CallSite kCalls452AD0[] = {{0x1D, 0x588F20}};
constexpr bh::CallSite kCalls452B00[] = {{0xC, 0x5891F0}, {0x14, 0x4411B0}};
constexpr bh::CallSite kCalls452B40[] = {{0x0, 0x4411B0}, {0x10, 0x4351F0}};
constexpr bh::CallSite kCalls452B60[] = {{0x1D, 0x588F20}};
constexpr bh::CallSite kCalls452B90[] = {{0x52, 0x589410}};
constexpr bh::CallSite kCalls453300[] = {{0x27, 0x453560}, {0x68, 0x446F20},  {0x8A, 0x446F20},  {0xAC, 0x446F20},
                                         {0xCE, 0x446F20}, {0xEF, 0x446F80},  {0x10F, 0x446F80}, {0x12F, 0x446F80},
                                         {0x14F, 0x446F80}, {0x208, 0x446F20}, {0x220, 0x446F20}};
constexpr bh::CallSite kCalls453560[] = {{0x2E, 0x446F20}, {0x4A, 0x446F20}, {0x66, 0x446F20},  {0x82, 0x446F20},
                                         {0x9D, 0x446F80}, {0xB7, 0x446F80}, {0xD1, 0x446F80},  {0xEB, 0x446F80},
                                         {0x1BC, 0x446F80}, {0x1D2, 0x446F80}, {0x210, 0x446F20}, {0x228, 0x446F20}};
constexpr bh::CallSite kCalls453910[] = {{0x7, 0x4456C0}, {0x58, 0x453A90}, {0x6A, 0x453AC0}, {0xD8, 0x5B93D2}};
constexpr bh::CallSite kCalls453EB0[] = {{0x7, 0x435180}};
constexpr bh::CallSite kCalls454A80[] = {{0x13, 0x454A50}};
constexpr bh::CallSite kCalls4CEB40[] = {{0xE, 0x56FF00},   {0x3F, 0x4CF4B0},  {0x99, 0x4CF4B0},  {0xAC, 0x5A8E30},  {0xB1, 0x5A8E90},
                                         {0xC0, 0x5A90B0},  {0x117, 0x5A75D0}, {0x11F, 0x5A77A0}, {0x165, 0x4CF4B0}, {0x18A, 0x5A8E50},
                                         {0x195, 0x572A00}, {0x19D, 0x5A8F60}, {0x1CD, 0x461E50}, {0x1E1, 0x5A90D0}, {0x1E7, 0x5A9290}};
constexpr bh::CallSite kCalls4CED60[] = {{0xC, 0x56FF00},   {0x3D, 0x4CF4B0},  {0x8B, 0x5A77C0},  {0x99, 0x461E50},
                                         {0xA5, 0x5A7610},  {0x105, 0x4CF4B0}, {0x154, 0x5A85F0}, {0x169, 0x5A9170},
                                         {0x175, 0x5A7780}, {0x209, 0x461E50}, {0x236, 0x5A77C0}, {0x244, 0x461E50}};
constexpr bh::CallSite kCalls4CEFC0[] = {{0xC, 0x56FF00},   {0x5D, 0x4CF4B0},  {0xDC, 0x5A7B90},  {0x145, 0x5A8200}, {0x154, 0x5A8060},
                                         {0x168, 0x5A7D70}, {0x172, 0x5A8DE0}, {0x17C, 0x5A8E00}, {0x188, 0x5A75D0}, {0x190, 0x5A77A0},
                                         {0x240, 0x5A85F0}, {0x255, 0x5A9170}, {0x261, 0x572A00}, {0x272, 0x461E50}, {0x28B, 0x5A7BC0}};
constexpr bh::CallSite kCalls4CF270[] = {{0xD, 0x56FF00},   {0x73, 0x5720C0},  {0x9C, 0x5A8250},  {0x14C, 0x5A75D0},
                                         {0x154, 0x5A77A0}, {0x15A, 0x5A92E0}, {0x216, 0x572A00}, {0x224, 0x461E50}};

// the transformation, the genes, the history's step
const bh::Clone kForm[] = {
    C("DragonHistory_Leave", 0x451480, 0x1B, nullptr, 0, BH_FN(DragonHistory_Leave), S::kStep),
    C("DragonForm_Transform", 0x4514A0, 0x1C6, kCalls4514A0, BH_N(kCalls4514A0), BH_FN(DragonForm_Transform), S::kHelper),
    C("DragonForm_FindRecipe", 0x451670, 0x7D, kCalls451670, BH_N(kCalls451670), BH_FN(DragonForm_FindRecipe), S::kHelper, 0xFF),
    C("DragonForm_RecipeSlotHeld", 0x4516F0, 0x57, nullptr, 0, BH_FN(DragonForm_RecipeSlotHeld), S::kHelper, 0xFF),
    C("DragonForm_TryPartyRecipe", 0x451750, 0x47, kCalls451750, BH_N(kCalls451750), BH_FN(DragonForm_TryPartyRecipe), S::kHelper, 0xFF),
    C("DragonForm_Mix", 0x4517A0, 0x4AC, kCalls4517A0, BH_N(kCalls4517A0), BH_FN(DragonForm_Mix), S::kHelper),
    C("DragonForm_StatShift", 0x451C50, 0x2E, nullptr, 0, BH_FN(DragonForm_StatShift), S::kHelper, 0xFF),
    C("DragonForm_SetMixBytes", 0x451C80, 0x11D, nullptr, 0, BH_FN(DragonForm_SetMixBytes), S::kHelper, 0xFF),
    C("DragonForm_MixAbilities", 0x451DA0, 0x23B, kCalls451DA0, BH_N(kCalls451DA0), BH_FN(DragonForm_MixAbilities), S::kHelper),
    C("DragonForm_AddAbilityRow", 0x451FE0, 0x9F, nullptr, 0, BH_FN(DragonForm_AddAbilityRow), S::kHelper, 0xFF),
    C("DragonForm_ApplyRecipe", 0x452080, 0x332, kCalls452080, BH_N(kCalls452080), BH_FN(DragonForm_ApplyRecipe), S::kHelper),
    {"DragonForm_PartyRecipe", 0x4523C0, 0x1A4, kCalls4523C0, BH_N(kCalls4523C0), nullptr, 0, kTables4523C0, BH_N(kTables4523C0),
     BH_FN(DragonForm_PartyRecipe), 0xFF, true, S::kHelper},   // calm: see OutEffect
    C("DragonGenes_Held", 0x452570, 0x3C, nullptr, 0, BH_FN(DragonGenes_Held), S::kHelper, 0xFF),
    C("DragonGenes_SumCost", 0x4525B0, 0x38, nullptr, 0, BH_FN(DragonGenes_SumCost), S::kHelper),
};
// the stat rebuild, the roll and its tests, the AP pop-up
const bh::Clone kStats[] = {
    C("Battle_RecalcStats", 0x453300, 0x25F, kCalls453300, BH_N(kCalls453300), BH_FN(Battle_RecalcStats), S::kHelper),
    C("Battle_RecalcMemberStats", 0x453560, 0x23A, kCalls453560, BH_N(kCalls453560), BH_FN(Battle_RecalcMemberStats), S::kHelper),
    C("Battle_MemberRollByAction", 0x453910, 0xF2, kCalls453910, BH_N(kCalls453910), BH_FN(Battle_MemberRollByAction), S::kHelper, 0xFF),
    C("Battle_ActionBitSet", 0x453A90, 0x2D, nullptr, 0, BH_FN(Battle_ActionBitSet), S::kHelper, 0xFF),
    C("Battle_MemberListFull", 0x453AC0, 0x47, nullptr, 0, BH_FN(Battle_MemberListFull), S::kHelper, 0xFF),
    C("Battle_SetApPopup", 0x453EB0, 0xE6, kCalls453EB0, BH_N(kCalls453EB0), BH_FN(Battle_SetApPopup), S::kHelper),
};
// the three effect tasks (Sprite_Current a task slot)
const bh::Clone kTasks[] = {
    C("BattleFxDash_Dispatch", 0x452680, 0x23, kCalls452680, BH_N(kCalls452680), BH_FN(BattleFxDash_Dispatch), S::kTask, 0, 1, 7),
    C("BattleFxDash_Start", 0x4526B0, 0x38, kCalls4526B0, BH_N(kCalls4526B0), BH_FN(BattleFxDash_Start), S::kTask),
    C("BattleFxDash_WaitPose", 0x4526F0, 0x26, kCalls4526F0, BH_N(kCalls4526F0), BH_FN(BattleFxDash_WaitPose), S::kTask),
    C("BattleFxDash_Rise", 0x452720, 0xC2, kCalls452720, BH_N(kCalls452720), BH_FN(BattleFxDash_Rise), S::kTask, 0xFF),
    C("BattleFxDash_Advance", 0x4527F0, 0x15C, kCalls4527F0, BH_N(kCalls4527F0), BH_FN(BattleFxDash_Advance), S::kTask, 0xFF),
    C("BattleFxDash_Return", 0x452950, 0xC8, kCalls452950, BH_N(kCalls452950), BH_FN(BattleFxDash_Return), S::kTask, 0xFF),
    C("BattleFxDash_Arc", 0x452A20, 0x60, kCalls452A20, BH_N(kCalls452A20), BH_FN(BattleFxDash_Arc), S::kTask, 0xFF),
    C("BattleFxDash_Land", 0x452A80, 0x50, kCalls452A80, BH_N(kCalls452A80), BH_FN(BattleFxDash_Land), S::kTask),
    C("BattleFxPose_Dispatch", 0x452AD0, 0x23, kCalls452AD0, BH_N(kCalls452AD0), BH_FN(BattleFxPose_Dispatch), S::kTask, 0, 1, 2),
    C("BattleFxPose_Start", 0x452B00, 0x35, kCalls452B00, BH_N(kCalls452B00), BH_FN(BattleFxPose_Start), S::kTask),
    C("BattleFxPose_WaitOwner", 0x452B40, 0x16, kCalls452B40, BH_N(kCalls452B40), BH_FN(BattleFxPose_WaitOwner), S::kTask),
    C("BattleFxTrail_Dispatch", 0x452B60, 0x23, kCalls452B60, BH_N(kCalls452B60), BH_FN(BattleFxTrail_Dispatch), S::kTask, 0, 1, 2),
    C("BattleFxTrail_Start", 0x452B90, 0x57, kCalls452B90, BH_N(kCalls452B90), BH_FN(BattleFxTrail_Start), S::kTask, 0xFF),
};
// round eleven's two debts
const bh::Clone kSlots[] = {
    C("Field_SlotsReleaseOwner", 0x454A80, 0x2B, kCalls454A80, BH_N(kCalls454A80), BH_FN(Field_SlotsReleaseOwner), S::kHelper),
    C("Field_SlotStart", 0x455290, 0x66, nullptr, 0, BH_FN(Field_SlotStart), S::kHelper, 0xFF),
};
// BMAGIC's four map cells
const bh::Clone kCells[] = {
    C("MapCell_DrawTexQuads", 0x4CEB40, 0x21A, kCalls4CEB40, BH_N(kCalls4CEB40), BH_FN(MapCell_DrawTexQuads), S::kHelper),
    C("MapCell_DrawShadedQuads", 0x4CED60, 0x251, kCalls4CED60, BH_N(kCalls4CED60), BH_FN(MapCell_DrawShadedQuads), S::kHelper),
    C("MapCell_DrawSpinQuads", 0x4CEFC0, 0x2A9, kCalls4CEFC0, BH_N(kCalls4CEFC0), BH_FN(MapCell_DrawSpinQuads), S::kHelper),
    C("MapCell_DrawGroundSprite", 0x4CF270, 0x234, kCalls4CF270, BH_N(kCalls4CF270), BH_FN(MapCell_DrawGroundSprite), S::kHelper),
};

// ===========================================================================
// Stand-ins
// ===========================================================================

// The transformation's cells its callers read again after a call: the form
// code 0x675F56 (DragonForm_Transform after DragonForm_Mix) and the group code
// 0x675F57 (after DragonForm_FindRecipe / _ApplyRecipe / _Mix). A recorder of
// one of those moves both from Noise(), noting what they held (so a store
// before the call is compared, not wiped).
std::uint32_t FormEffect(const std::uint32_t*, std::uint32_t answer) {
    unsigned char* const code = Mem(at::kFormCode);
    unsigned char* const group = Mem(at::kFormGroupWork);
    bh::Note(code[0], group[0]);
    const std::uint32_t n = bh::Noise();
    if (n & 1) code[0] = static_cast<unsigned char>(n >> 8);
    if (n & 2) group[0] = static_cast<unsigned char>(n >> 16);
    return answer;
}

// DragonForm_FindRecipe's recorder answers none (0xFF) half the time, so
// DragonForm_Transform's mix path is as common as its recipe path.
std::uint32_t RecipeEffect(const std::uint32_t* a, std::uint32_t answer) {
    answer = FormEffect(a, answer);
    return bh::Noise() & 1 ? (answer | 0xFFu) : answer;
}
// The roll's refusals (out, the action's bit, a full list) answer 0 two
// times in three, so its Rand is reached in about a fifth of the rounds.
std::uint32_t MostlyZero(const std::uint32_t*, std::uint32_t answer) {
    return bh::Noise() % 3 != 0 ? (answer & 0xFFFFFF00u) : answer;
}

// Battle_ActorIsOut for DragonForm_PartyRecipe: its round's rule (Seed) - one
// member out at most, so two of the others are always found (fewer is the
// original's unwritten-stack read, which ours refuses); every other clone
// takes the standard flag. The clone runs calm: a disturbance moving the actor
// 0x904B34 between two of its calls would skip a second member (the
// original's read again of the actor is then not probed - docs/battle_e6.md
// section 6).
bool g_out_rule = false;
unsigned g_out = 0xFF;
std::uint32_t OutEffect(const std::uint32_t* a, std::uint32_t answer) {
    if (!g_out_rule) return answer;
    return (answer & 0xFFFFFF00u) | ((a[0] & 0xFF) == g_out ? 1u : 0u);
}

const bh::Callee kFormCallees[] = {
    {BE6_OURS(Battle_RecalcStats), 1, {kU8}, A::kGarbage, 0, 0},
    {BE6_OURS(DragonForm_FindRecipe), 0, {}, A::kByte, 0xFF, 0x14, {}, &RecipeEffect},
    {BE6_OURS(DragonForm_ApplyRecipe), 1, {kU8}, A::kGarbage, 0, 0, {}, &FormEffect},
    {BE6_OURS(DragonForm_Mix), 1, {kAll}, A::kGarbage, 0, 0, {}, &FormEffect},
    {BE6_OURS(DragonForm_RecipeSlotHeld), 2, {kU8, kU8}, A::kFlag, 0, 0},
    {BE6_OURS(DragonForm_TryPartyRecipe), 0, {}, A::kFlag, 0, 0},
    {BE6_OURS(DragonForm_PartyRecipe), 0, {}, A::kGarbage, 0, 0},
    {BE6_OURS(DragonForm_StatShift), 1, {kU8}, A::kGarbage, 0, 0},
    {BE6_OURS(DragonForm_SetMixBytes), 5, {kU8, kU8, kU8, kU8, kU8}, A::kGarbage, 0, 0},
    {BE6_OURS(DragonForm_MixAbilities), 0, {}, A::kGarbage, 0, 0},
    {BE6_OURS(DragonForm_AddAbilityRow), 2, {kU8, kU8}, A::kByte, 0, 9},
    {BE6_OURS(DragonGenes_Held), 1, {kU8}, A::kFlag, 0, 0},
    {BE6_OURS(Battle_ActorIsOut), 1, {kU8}, A::kFlag, 0, 0, {}, &OutEffect},
};
const bh::Callee kStatsCallees[] = {
    {BE6_OURS(Battle_RecalcMemberStats), 2, {kAll, kU8}, A::kGarbage, 0, 0},
    {BE6_OURS(Battle_ActionBitSet), 1, {kU8}, A::kFlag, 0, 0, {}, &MostlyZero},
    {BE6_OURS(Battle_MemberListFull), 1, {kU8}, A::kFlag, 0, 0, {}, &MostlyZero},
    {BE6_OURS(Battle_ActorIsOut), 1, {kU8}, A::kFlag, 0, 0, {}, &MostlyZero},
};
// the trail task's create is tested for 0xFF: answer it a third of the time
const bh::Callee kTaskCallees[] = {
    {BE6_OURS(BattleTask_Create), 2, {kU8, kU8}, A::kByte, 0, 47, {}, &bh::CreateMayFail},
};
// the cells: Area_TestCondition reads the word its callers load into ax (the
// rest of eax is theirs); the GTE's matrix family run for real - the push and
// pop keep the matrix both passes start from, and the rotation / translation
// the cells build go through pointers into their frames
// The heights the cells add to their vertices (0x4CF4B0, AreaMap_Elevation;
// only ax is read) answered small, so that a seeded matrix projects the
// vertices on screen and the draws past the screen tests run.
std::uint32_t SmallHeight(const std::uint32_t*, std::uint32_t answer) {
    return (answer & 0xFFFF0000u) | (static_cast<U>(static_cast<int>(bh::Noise() % 513) - 256) & 0xFFFFu);
}

const bh::Callee kCellCallees[] = {
    {BE6_OURS(Area_TestCondition), 1, {kU16}, A::kFlag, 0, 0},
    {"0x4CF4B0", at::kCellHeight, at::kCellHeight, 2, {kAll, kAll}, A::kGarbage, 0, 0, {}, &SmallHeight},
    {BE6_OURS(AreaMap_Elevation), 2, {kAll, kAll}, A::kGarbage, 0, 0, {}, &SmallHeight},
    {BE6_OURS(Gte_PushMatrix), 0, {}, A::kThrough, 0, 0},
    {BE6_OURS(Gte_PopMatrix), 0, {}, A::kThrough, 0, 0},
    {BE6_OURS(Gte_RotTrans), 2, {kAll, kAll}, A::kThrough, 0, 0},
    {BE6_OURS(Gte_SetRotMatrix), 1, {kAll}, A::kThrough, 0, 0},
    {BE6_OURS(Gte_SetTransMatrix), 1, {kAll}, A::kThrough, 0, 0},
};

const bh::DataTable kTaskTables[] = {{at::kDashSteps, 7}, {at::kPoseSteps, 2}, {at::kTrailSteps, 2}};

// A BMAGIC record of the fuzz's own (DrawLayer_Open hands a pointer into the
// loaded area's data): random each round, its length byte seeded.
unsigned char g_cell[0x200];

const bh::Region kRegions[] = {
    {at::kMixSums, 0x10},                 // the sums, the form and group codes
    {0x939B20, 0x3A0},                    // the party's backup past the engine region (0x939AE0 + 3 x 0x14C)
    {at::kActionBits, 0x20},              // the action flag set
    {at::kEffectState, 0x18},             // MoveScript_EffectState (the roll's table pick)
    {at::kFieldSlots, 0x80},      // the eight Field_Slots records
    {at::kKind2Z, 8},                     // Field_Kind2Z / _Kind2X (the dash's rise)
    {at::kVertexScratch, 8},  // slot 43's point
    {at::kScreenXY, 8},    // and its projection
    {at::kOtSlot, 1},
    {0, sizeof g_cell},                   // g_cell (filled in at SelfTest)
};
// the cells' run also holds the GTE's state and the camera's matrix: random,
// then (two times in three) a sane projection seeded
constexpr U kGte = 0x7DE428;              // Gte_RampFar .. Gte_Depth: the whole GTE state (magic_s23_fuzz.cpp's region)
constexpr U kGteSize = 0x380;
constexpr U kGteDepth = 0x7DE450;         // Gte_MatrixDepth: kept 0..15 (the pushes stay in the stack)
constexpr U kGteMatrix = 0x7DE4A0;        // Gte_Matrix: 3 x 3 s16, pad, 3 x s32
constexpr U kGteNearZ = 0x7DE498;
constexpr U kGteH = 0x7DE780;             // Gte_ProjDistance
constexpr U kGteOffsetY = 0x7DE78C;
constexpr U kGteOffsetX = 0x7DE790;
// Gte_Vertices (0x7DE468, six dwords) is left out: the loads copy each
// vertex's fourth short, which the originals leave as stale stack bytes (the
// GTE reads only the first three; section 7, L9)
constexpr U kGteVertices = 0x7DE468;
const bh::Region kCellRegions[] = {
    {kGte, kGteVertices - kGte},
    {kGteVertices + 0x18, kGte + kGteSize - (kGteVertices + 0x18)},
    {at::kCameraMatrix, 0x20},
};

// ===========================================================================
// Seeds, arguments, disturbance
// ===========================================================================

const bh::Clone* g_cur = nullptr;
U g_args[bh::kArgs];

unsigned char* Party(unsigned m) { return Mem(at::kParty + (m % 4) * at::kPartyStride); }
unsigned char Small(unsigned span) { return static_cast<unsigned char>(bh::Next() % span); }
// a byte of the kind the code compares: small signed values most of the time
unsigned char Signed(unsigned span) {
    return static_cast<unsigned char>(bh::Often() ? static_cast<int>(bh::Next() % (2 * span + 1)) - static_cast<int>(span)
                                                  : static_cast<int>(bh::Next()));
}
U Dirty(U v) { return (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (v & 0xFF); }

// The chosen genes: 0..3 of them mostly, from 0..0x10 mostly (0xA, 0xB, 0xC
// and 0x10 the code's own).
void SeedGenes() {
    Mem(at::kGeneCount)[0] = bh::Often() ? Small(4) : Small(8);
    for (unsigned i = 0; i < 3; ++i)
        Mem(at::kGenes + i)[0] = bh::Often() ? Small(0x11) : static_cast<unsigned char>(bh::Next());
    if (bh::Half()) Mem(at::kGenes + bh::Next() % 3)[0] = static_cast<unsigned char>(0xA + bh::Next() % 3);
}
// The actor's record: its character index small (record 0 is in the state)
void SeedMember() {
    unsigned char* const r = Party(Mem(at::kActor)[0]);
    r[0x148] = bh::Often() ? 0 : Small(9);
    Mem(at::kPartyCount)[0] = Small(4);
}
// An action id whose row's nibble has non-zero odds in both tables (the
// original divides by them; a zero is ours' abort, not a count).
unsigned SafeAction() {
    for (unsigned tries = 0; tries < 256; ++tries) {
        const unsigned id = bh::Often() ? bh::Next() % 0x200 : bh::Next() & 0xFFFF;
        const unsigned nibble = Mem(at::kActionRows + id * at::kActionRowStride)[0] >> 4;
        if (Mem(at::kRollOdds)[nibble] != 0 && Mem(at::kRollOddsB)[nibble] != 0) return id;
    }
    return 0;
}
unsigned char* Record(unsigned v) {
    switch (v % 4) {
    case 0: return Party(v >> 2);
    case 1: return bh::EnemyAt(v >> 2);
    default: return bh::SpriteRecord(v);
    }
}

// The GTE's matrix stack depth inside the stack; two times in three a
// projection that lands the cells' vertices near the screen: a rotation near
// identity (Gte_Matrix and Camera_Matrix), a translation 1000..4000 deep, a
// distance 300..800 and the offsets near the screen's middle.
void SeedGte() {
    SetLong(Mem(kGteDepth), static_cast<std::int32_t>(bh::Next() % 0x10));
    if (!bh::Often()) return;
    const U matrices[2] = {kGteMatrix, at::kCameraMatrix};
    for (U m : matrices)
        for (unsigned i = 0; i < 9; ++i)
            SetWord(Mem(m + 2 * i), static_cast<U>((i % 4 == 0 ? 0x1000 : 0) + (bh::Half() ? static_cast<int>(bh::Next() % 129) - 64 : 0)));
    SetLong(Mem(kGteMatrix + 0x14), static_cast<std::int32_t>(bh::Next() % 401) - 200);
    SetLong(Mem(kGteMatrix + 0x18), static_cast<std::int32_t>(bh::Next() % 401) - 200);
    SetLong(Mem(kGteMatrix + 0x1C), static_cast<std::int32_t>(1000 + bh::Next() % 3000));
    SetLong(Mem(kGteH), static_cast<std::int32_t>(300 + bh::Next() % 500));
    SetLong(Mem(kGteOffsetX), static_cast<std::int32_t>(140 + bh::Next() % 41));
    SetLong(Mem(kGteOffsetY), static_cast<std::int32_t>(100 + bh::Next() % 41));
    SetLong(Mem(kGteNearZ), static_cast<std::int32_t>(16 + bh::Next() % 200));
}

void Seed(unsigned k) {
    const bh::Clone& c = g_cur[k];
    unsigned char* const s = Sprite_Current;
    g_out_rule = false;
    std::memset(g_args, 0, sizeof g_args);
    for (U& a : g_args) a = bh::Next();
    switch (c.base) {
    // --- the transformation
    case 0x4514A0: case 0x451670: case 0x451750: case 0x4517A0: case 0x452570: case 0x4525B0:
        SeedGenes();
        SeedMember();
        if (c.base == 0x451750) {
            if (bh::Half()) Mem(at::kGenes + bh::Next() % 3)[0] = 0x10;
            Mem(at::kPartySize)[0] = bh::Often() ? 3 : static_cast<unsigned char>(bh::Next());
        }
        if (c.base == 0x452570) g_args[0] = Dirty(bh::Half() ? Mem(at::kGenes + bh::Next() % 3)[0] : bh::Next());
        break;
    case 0x4516F0: {
        SeedGenes();
        const unsigned recipe = bh::Often() ? bh::Next() % 11 : bh::Next() & 0xFF;
        const unsigned slot = bh::Often() ? bh::Next() % 3 : bh::Next() & 0xFF;
        if (bh::Half()) {
            Mem(at::kGeneCount)[0] = static_cast<unsigned char>(1 + bh::Next() % 3);
            Mem(at::kGenes + bh::Next() % 3)[0] = Mem(at::kRecipeGenes)[recipe * 3 + slot];
        }
        g_args[0] = Dirty(recipe);
        g_args[1] = Dirty(slot);
        break;
    }
    case 0x451C50:
        g_args[0] = Dirty(Signed(6));
        break;
    case 0x451C80:
        for (unsigned i = 0; i < 5; ++i) g_args[i] = Dirty(Signed(6));
        break;
    case 0x451DA0:
        for (unsigned i = 0; i < 14; ++i) Mem(at::kMixSums + i)[0] = Signed(2);
        Mem(at::kFormCode)[0] = bh::Often() ? Small(4) : static_cast<unsigned char>(bh::Next());
        break;
    case 0x451FE0: {
        const unsigned row = bh::Often() ? bh::Next() % 15 : bh::Next() & 0xFF;
        const unsigned count = bh::Often() ? bh::Next() % 10 : bh::Next() & 0xFF;
        unsigned char* const list = Party(Mem(at::kActor)[0]) + 0xF4;
        if (count && bh::Half()) list[bh::Next() % count] = Mem(at::kAbilityRows)[row * 3 + bh::Next() % 3];
        g_args[0] = Dirty(row);
        g_args[1] = Dirty(count);
        break;
    }
    case 0x452080:
        SeedMember();
        g_args[0] = Dirty(bh::Often() ? bh::Next() % 21 : bh::Next());
        break;
    case 0x4523C0: {
        // the actor a member or not; one member out at most (OutEffect)
        const unsigned actor = bh::Half() ? bh::Next() % 3 : 3 + bh::Next() % 3;
        Mem(at::kActor)[0] = static_cast<unsigned char>(actor);
        g_out_rule = true;
        g_out = actor >= 3 ? bh::Next() % 4 : actor;   // 3: none
        for (unsigned m = 0; m < 3; ++m) Party(m)[0x89] = bh::Often() ? static_cast<unsigned char>(1 + bh::Next() % 8) : Small(0x100);
        SeedGenes();
        break;
    }
    // --- the stats
    case 0x453300:
        g_args[0] = Dirty(bh::Next() % 11);
        break;
    case 0x453560: {
        const unsigned m = bh::Next() % 3;
        g_args[0] = Key(Party(bh::Often() ? m : bh::Next() % 3) + 0x80);
        g_args[1] = Dirty(m);
        Mem(at::kForm)[0] = bh::Half() ? 0x16 : static_cast<unsigned char>(bh::Next());
        break;
    }
    case 0x453910: {
        const unsigned m = bh::Often() ? bh::Next() % 3 : bh::Next() % 4;
        unsigned char* const r = Party(m);
        if (bh::Often()) r[0x130] |= 1;
        if (bh::Often()) r[0x124] = Mem(at::kActor)[0];
        r[0x89] = Small(0x18);
        if (bh::Half()) Mem(at::kEffectState + r[0x89])[0] = 6;
        SetWord(bh::Pointer(at::kActingSprite2) + 2, SafeAction());
        g_args[0] = Dirty(m);
        break;
    }
    case 0x453A90:
        g_args[0] = Dirty(bh::Next());
        break;
    case 0x453AC0: {
        const unsigned m = bh::Often() ? bh::Next() % 3 : bh::Next() % 4;
        unsigned char* const r = Party(m);
        if (bh::Often()) {
            for (unsigned i = 0; i < 10; ++i) r[0xFE + i] = static_cast<unsigned char>(1 + bh::Next() % 255);
            if (bh::Half()) r[0xFE + bh::Next() % 10] = 0;
        }
        g_args[0] = Dirty(m);
        break;
    }
    case 0x453EB0: {
        const U amount = bh::Often() ? static_cast<U>(static_cast<int>(bh::Next() % 41) - 20) : bh::Next();
        g_args[0] = bh::Half() ? amount : (amount & 0xFFFF);
        g_args[1] = Dirty(bh::Next() % 11);
        break;
    }
    // --- the tasks
    case 0x452720:
        if (bh::Half()) SetLong(s + 0x20, static_cast<std::int32_t>(static_cast<U>(Long(s + 0x3C)) + (bh::Next() & 0x3FFFFFF)));
        Mem(at::kFormTable)[0] = bh::Often() ? Small(8) : static_cast<unsigned char>(bh::Next());
        break;
    case 0x4527F0:
        s[0xB] = bh::Often() ? Small(3) : static_cast<unsigned char>(bh::Next());
        break;
    case 0x452A20:
        if (bh::Half()) {
            SetLong(s + 0x20, -8);
            SetLong(s + 0x14, -0x38);
        }
        break;
    case 0x452B40:
        if (bh::Half()) bh::Pointer(bh::at::kOwner)[1] = 5;
        break;
    // --- the field slots
    case 0x454A80: {
        unsigned char* const object = Record(bh::Next());
        for (unsigned i = 0; i < 8; ++i)
            if (bh::Half()) SetLong(Mem(at::kFieldSlots + 0xC + 0x10 * i), static_cast<std::int32_t>(Key(object)));
        g_args[0] = Key(object);
        break;
    }
    case 0x455290: {
        unsigned char* const object = Record(bh::Next());
        if (bh::Half())
            for (unsigned i = 0; i < 8; ++i) Mem(at::kFieldSlots + 0x10 * i)[0] |= 1;
        g_args[0] = Key(object);
        break;
    }
    // --- the cells: a record of whole entries, the cell's bytes
    case 0x4CEB40: case 0x4CED60: case 0x4CEFC0: case 0x4CF270: {
        const unsigned step = c.base == 0x4CEB40 ? 5 : c.base == 0x4CED60 ? 8 : 6;
        const unsigned first = c.base == 0x4CEFC0 ? 3 : 1;
        SeedGte();
        // the record's dwords: small signed x and z bytes, a low word to 0x40FF
        // (bit 14 a texture's), more bits half the time
        for (unsigned i = 4; i + 4 <= sizeof g_cell; i += 4) {
            U v = (static_cast<U>(static_cast<unsigned char>(static_cast<int>(bh::Next() % 65) - 32)) << 24) |
                  (static_cast<U>(static_cast<unsigned char>(static_cast<int>(bh::Next() % 65) - 32)) << 16) | (bh::Next() & 0x40FF);
            if (bh::Half()) v ^= bh::Next() & 0x3FFF3F00u;
            SetLong(g_cell + i, static_cast<std::int32_t>(v));
        }
        g_cell[2] = static_cast<unsigned char>(first + step * (bh::Next() % 5));
        if (c.base == 0x4CF270) {
            g_cell[8] = static_cast<unsigned char>(1 + bh::Next() % 255);
            g_cell[9] = Small(40);
            g_cell[0xA + 3] = 0xFF;   // the thresholds end by the fourth frame
        }
        g_args[0] = Key(g_cell);
        g_args[1] = bh::Often() ? 0x7E + bh::Next() % 5 : bh::Often() ? bh::Next() & 0xFF : bh::Next();
        g_args[2] = bh::Often() ? 0x7E + bh::Next() % 5 : bh::Often() ? bh::Next() & 0xFF : bh::Next();
        break;
    }
    default:
        break;
    }
}

void Args(unsigned, U* a) {
    for (unsigned i = 0; i < bh::kArgs; ++i) a[i] = g_args[i];
}

// What the group's functions read again after a call: the form and group
// codes, the gene count and genes, the party size byte, the form byte
// 0x904B89 (the member rebuild's test), the action bits.
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 6) {
    case 0: Mem(at::kFormCode)[0] = b; break;
    case 1: Mem(at::kFormGroupWork)[0] = b; break;
    case 2: Mem(at::kGeneCount)[0] = static_cast<unsigned char>(b % 6); break;
    case 3: Mem(at::kGenes + b % 3)[0] = static_cast<unsigned char>(h >> 24); break;
    case 4: Mem(at::kPartySize)[0] = b; break;
    default: Mem(at::kForm)[0] = h & 0x1000000 ? 0x16 : b; break;
    }
}

// BOF3X_BE6_RUN=<unit> runs that one alone (the controls script's shortcut);
// unset, all five run.
bool Wants(const char* run) {
    const char* const only = std::getenv("BOF3X_BE6_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, run) == 0;
}

void RunUnit(const char* run, const bh::Clone* clones, unsigned n, const bh::Callee* callees, unsigned n_callees,
             const bh::DataTable* tables, unsigned n_tables, unsigned phase_span, bool gte = false) {
    if (!Wants(run)) return;
    static bh::Region regions[BH_COUNT(kRegions) + 3];
    std::memcpy(regions, kRegions, sizeof kRegions);
    regions[BH_COUNT(kRegions) - 1].at = Key(g_cell);
    unsigned n_regions = BH_COUNT(kRegions);
    if (gte)
        for (const bh::Region& r : kCellRegions) regions[n_regions++] = r;
    g_cur = clones;
    bh::Group g{"battle_e6", clones, n, callees, n_callees, tables, n_tables, regions, n_regions, &Seed, &Disturb, 6000};
    g.args = &Args;
    g.engine = true;
    g.phase_span = phase_span;
    bh::Run(g);
    g_cur = nullptr;
    g_out_rule = false;
}

}  // namespace

void SelfTest() {
    RunUnit("form", kForm, BH_COUNT(kForm), kFormCallees, BH_COUNT(kFormCallees), nullptr, 0, 0);
    RunUnit("stats", kStats, BH_COUNT(kStats), kStatsCallees, BH_COUNT(kStatsCallees), nullptr, 0, 0);
    RunUnit("tasks", kTasks, BH_COUNT(kTasks), kTaskCallees, BH_COUNT(kTaskCallees), kTaskTables, BH_COUNT(kTaskTables), 7);
    RunUnit("slots", kSlots, BH_COUNT(kSlots), nullptr, 0, nullptr, 0, 0);
    RunUnit("cells", kCells, BH_COUNT(kCells), kCellCallees, BH_COUNT(kCellCallees), nullptr, 0, 0, true);
}

}  // namespace battle_e6
