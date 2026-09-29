// BOF3X_SHADOW=battle_e4: group BE4's 56 battle-engine functions through the
// boss harness as an engine group (boss_harness.h, docs/boss_harness.md
// section 10), once at start-up: one boss_harness::Run. docs/battle_e4.md
// section 5.
//
// The clone rows are tools/band_rows.py's (--group BE4 --clones, 2026-09-29),
// each read against the disassembly; the extents are the tool's (the code's,
// not the cut's padding). Shapes: the cdecl helpers kHelper with their words
// from Args; the command's and the escape's table entries kStep with their
// dispatched byte (0x904AA4, or 0x904AA3) drawn below a bound; the four
// dispatchers kDispatch by 0x904AA4 over their tables (DataTables, counted
// from the code: the entries set the sub-states 0..4, 0..3, 0..3, 0..2).
// Four entries are driven a second time through the dispatcher that reaches
// them in the game (Clone::via): 0x448B80 and 0x448C80 (Capcom's, nobody's),
// 0x44A000 (nobody's), and BattleItemCmd_Dispatch 0x448180 (ours).
//
// BOF3X_BE4_RUN=<hex base>[,<hex base>...] runs those clones alone (the
// controls script's shortcut); unset, all run.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_e4.h"
#include "game/battle_e4_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace battle_e4 {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)

bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret = 0,
            std::uint8_t states = 0, U state_cell = 0) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, shape};
    c.states = states;
    c.state_cell = state_cell;
    return c;
}
bh::Clone V(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, U dispatcher, U cell,
            std::uint8_t state, U state_cell) {
    bh::Clone c = C(name, base, size, calls, n, ours, S::kStep);
    c.via = {dispatcher, cell, 0, state, state_cell};
    return c;
}

// ===========================================================================
// The clone rows (tools/band_rows.py --group BE4 --clones)
// ===========================================================================

constexpr bh::CallSite kCalls444660[] = {{0xE, 0x5A79A0}, {0x26, 0x5A77C0}, {0x2F, 0x461E50}, {0x3B, 0x5A7740}, {0x6A, 0x5A7780}, {0x73, 0x461E50}};
constexpr bh::CallSite kCalls4446E0[] = {{0xE, 0x5A79A0}, {0x26, 0x5A77C0}, {0x2F, 0x461E50}, {0x3B, 0x5A7740}, {0xB1, 0x5A7780}, {0xBA, 0x461E50}};
constexpr bh::CallSite kCalls4457F0[] = {{0x17, 0x4456C0}, {0x42, 0x4456C0}, {0x6A, 0x4456C0}, {0x93, 0x4456C0}};
constexpr bh::CallSite kCalls446110[] = {{0x2D, 0x5B93D2}, {0x50, 0x5B93D2}, {0x59, 0x5B93D2}};
constexpr bh::CallSite kCalls4461B0[] = {{0x33, 0x5B93D2}, {0xB5, 0x5B93D2}, {0xC3, 0x5B93D2}};
constexpr bh::CallSite kCalls446810[] = {{0x77, 0x5B93D2}};
constexpr bh::CallSite kCalls446B00[] = {{0x1B, 0x4456C0}, {0x63, 0x4456C0}};
constexpr bh::CallSite kCalls446CB0[] = {{0x65, 0x5B93D2}, {0xA8, 0x5B93D2}};
constexpr bh::CallSite kCalls448BA0[] = {{0x21, 0x591810}};
constexpr bh::CallSite kCalls448C00[] = {{0x2E, 0x461EB0}, {0x5C, 0x591810}, {0x75, 0x587740}};
constexpr bh::CallSite kCalls448CA0[] = {{0x11, 0x497740}, {0x86, 0x587740}, {0xB6, 0x587740}, {0xCC, 0x449F60}, {0xDF, 0x449E90}, {0x114, 0x587740}};
constexpr bh::CallSite kCalls448DE0[] = {{0x59, 0x497740}, {0x7D, 0x461EB0}, {0x9B, 0x587740}, {0xFF, 0x449C70}, {0x10D, 0x587740},
                                         {0x11D, 0x587740}, {0x136, 0x587740}, {0x185, 0x587740}, {0x1AF, 0x587740}};
constexpr bh::CallSite kCalls448FC0[] = {{0x5E, 0x461EB0}, {0x71, 0x461EB0}, {0x161, 0x587740}, {0x178, 0x591C20}, {0x17E, 0x497740},
                                         {0x1AB, 0x587740}, {0x1D4, 0x449BB0}, {0x1F2, 0x587740}, {0x1FA, 0x449A00}, {0x206, 0x587740}};
constexpr bh::CallSite kCalls4491D0[] = {{0x59, 0x497740}, {0x7D, 0x461EB0}, {0x9B, 0x587740}, {0xF4, 0x587740}, {0x153, 0x591C20},
                                         {0x159, 0x497740}, {0x1A1, 0x587740}, {0x1B6, 0x587740}, {0x1E0, 0x587740}};
constexpr bh::CallSite kCalls4493E0[] = {{0x22, 0x591810}, {0x46, 0x587740}};
constexpr bh::CallSite kCalls4494A0[] = {{0x22, 0x591810}, {0x30, 0x445730}, {0x56, 0x449E00}, {0x6B, 0x445730}};
constexpr bh::CallSite kCalls449540[] = {{0x50, 0x461EB0}, {0x68, 0x591810}, {0x74, 0x449E00}, {0x8A, 0x445730}, {0xAD, 0x587740}, {0xD8, 0x4469F0},
                                         {0xDE, 0x445730}, {0xF0, 0x587740}, {0x119, 0x4469F0}, {0x11F, 0x4457F0}, {0x131, 0x587740}};
constexpr bh::CallSite kCalls449680[] = {{0x50, 0x461EB0}, {0x68, 0x591810}, {0x76, 0x445730}, {0x96, 0x587740}, {0xA9, 0x449E00},
                                         {0xCD, 0x4469F0}, {0xF9, 0x4469F0}, {0xFF, 0x445730}, {0x114, 0x587740}, {0x128, 0x449E00},
                                         {0x14C, 0x4469F0}, {0x161, 0x587740}, {0x186, 0x4469F0}, {0x18C, 0x4457F0}, {0x1A1, 0x587740}};
constexpr bh::CallSite kCalls449830[] = {{0x6, 0x587740}, {0x49, 0x449FE0}};
constexpr bh::CallSite kCalls4498A0[] = {{0x5, 0x587740}, {0x10, 0x44A990}};
constexpr bh::CallSite kCalls449910[] = {{0x22, 0x591810}};
constexpr bh::CallSite kCalls449970[] = {{0x4D, 0x461EB0}, {0x60, 0x591810}, {0x79, 0x587740}};
constexpr bh::CallSite kCalls449A00[] = {{0x83, 0x591B60}, {0x97, 0x590BB0}, {0xB8, 0x590660}, {0x13F, 0x453C00}, {0x16B, 0x44FDE0}, {0x184, 0x453300}};
constexpr bh::CallSite kCalls449BB0[] = {{0x41, 0x5917A0}};
constexpr bh::CallSite kCalls449C70[] = {{0x83, 0x590BB0}, {0x97, 0x590660}, {0x120, 0x453C00}, {0x14C, 0x44FDE0}, {0x165, 0x453300}};
constexpr bh::CallSite kCalls44A010[] = {{0x4F, 0x4456C0}, {0x92, 0x4456C0}, {0xD5, 0x44A520}, {0xE4, 0x5B93D2}};
constexpr bh::CallSite kCalls44A150[] = {{0x16, 0x44A650}, {0x6E, 0x5720C0}, {0x95, 0x454590}, {0xB4, 0x446D90},
                                         {0xD4, 0x446D90}, {0xF3, 0x446D90}, {0x10E, 0x4456C0}};
constexpr bh::CallSite kCalls44A380[] = {{0xF, 0x4450E0}, {0x2D, 0x4456C0}, {0x61, 0x446EA0}, {0x67, 0x446650},
                                         {0x84, 0x444310}, {0x8B, 0x497740}, {0x98, 0x44A650}};
constexpr bh::CallSite kCalls44A470[] = {{0x18, 0x5720C0}, {0x1E, 0x5725F0}, {0x31, 0x454DC0}, {0x42, 0x494E70},
                                         {0x47, 0x444310}, {0x4E, 0x497740}, {0x5C, 0x44A650}};
constexpr bh::CallSite kCalls44A520[] = {{0x4A, 0x4456C0}};
constexpr bh::CallSite kCalls44A910[] = {{0x38, 0x5171A0}};
constexpr bh::CallSite kCalls44A960[] = {{0x21, 0x5171A0}};
constexpr bh::CallSite kCalls44AA90[] = {{0x22, 0x497740}, {0x30, 0x44A650}};

constexpr U kS3 = at::kStep3, kS4 = at::kStep4;

const bh::Clone kAll56[] = {
    // the screen tiles
    C("BattleWin_DimScreen", 0x444660, 0x7D, kCalls444660, BH_N(kCalls444660), BH_FN(BattleWin_DimScreen), S::kHelper),
    C("BattleWin_DrawTileTint", 0x4446E0, 0xC4, kCalls4446E0, BH_N(kCalls4446E0), BH_FN(BattleWin_DrawTileTint), S::kHelper),
    // turn and damage helpers
    C("Battle_EnemyOutpaces", 0x445600, 0x3F, nullptr, 0, BH_FN(Battle_EnemyOutpaces), S::kHelper, 0xFF),
    C("Battle_PrevTarget", 0x4457F0, 0xB1, kCalls4457F0, BH_N(kCalls4457F0), BH_FN(Battle_PrevTarget), S::kHelper, 0xFF),
    C("Battle_HitOrMissParty", 0x446110, 0x9E, kCalls446110, BH_N(kCalls446110), BH_FN(Battle_HitOrMissParty), S::kHelper, 0xFFFF),
    C("Battle_HitOrMissEnemy", 0x4461B0, 0xFA, kCalls4461B0, BH_N(kCalls4461B0), BH_FN(Battle_HitOrMissEnemy), S::kHelper, 0xFFFF),
    C("Battle_PartyDefenceMean", 0x4463E0, 0x4A, nullptr, 0, BH_FN(Battle_PartyDefenceMean), S::kHelper, 0xFFFFFFFF),
    C("Battle_SetHpChange", 0x446540, 0xB7, nullptr, 0, BH_FN(Battle_SetHpChange), S::kHelper),
    C("Battle_ReloadPartyRecords", 0x446600, 0x43, nullptr, 0, BH_FN(Battle_ReloadPartyRecords), S::kHelper),
    C("Battle_OrderPushFront", 0x446700, 0x1B, nullptr, 0, BH_FN(Battle_OrderPushFront), S::kHelper),
    C("AutoBattle_FillCommands", 0x446720, 0x50, nullptr, 0, BH_FN(AutoBattle_FillCommands), S::kHelper),
    C("Battle_TurnVectorC", 0x446770, 0x41, nullptr, 0, BH_FN(Battle_TurnVectorC), S::kHelper),
    C("Battle_TurnVector18", 0x4467C0, 0x41, nullptr, 0, BH_FN(Battle_TurnVector18), S::kHelper),
    C("Battle_MemberReactRoll", 0x446810, 0x98, kCalls446810, BH_N(kCalls446810), BH_FN(Battle_MemberReactRoll), S::kHelper, 0xFF),
    C("Battle_WriteBackMember", 0x446A80, 0x7B, nullptr, 0, BH_FN(Battle_WriteBackMember), S::kHelper),
    C("Battle_PickEnemyTarget", 0x446B00, 0xAB, kCalls446B00, BH_N(kCalls446B00), BH_FN(Battle_PickEnemyTarget), S::kHelper),
    C("Battle_WakeRoll", 0x446CB0, 0xD6, kCalls446CB0, BH_N(kCalls446CB0), BH_FN(Battle_WakeRoll), S::kHelper, 0xFF),
    C("Battle_ReturnItem", 0x446D90, 0x45, nullptr, 0, BH_FN(Battle_ReturnItem), S::kHelper, 0xFF),
    C("BattleEnd_EnterStep1", 0x446DE0, 0x16, nullptr, 0, BH_FN(BattleEnd_EnterStep1), S::kHelper),
    C("BattleEnd_EnterStep2", 0x446E00, 0x16, nullptr, 0, BH_FN(BattleEnd_EnterStep2), S::kHelper),
    C("BattleEnd_EnterStep3", 0x446E20, 0x16, nullptr, 0, BH_FN(BattleEnd_EnterStep3), S::kHelper),
    // the item command's states 5..9 and the equipment window
    C("ItemMenu_SetupForMember", 0x447F40, 0x8C, nullptr, 0, BH_FN(ItemMenu_SetupForMember), S::kHelper),
    C("BattleItemCmd_SideBegin", 0x448BA0, 0x51, kCalls448BA0, BH_N(kCalls448BA0), BH_FN(BattleItemCmd_SideBegin), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_SidePick", 0x448C00, 0x7C, kCalls448C00, BH_N(kCalls448C00), BH_FN(BattleItemCmd_SidePick), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipMenu", 0x448CA0, 0x138, kCalls448CA0, BH_N(kCalls448CA0), BH_FN(BattleItemCmd_EquipMenu), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipSlotPick", 0x448DE0, 0x1DD, kCalls448DE0, BH_N(kCalls448DE0), BH_FN(BattleItemCmd_EquipSlotPick), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipItemPick", 0x448FC0, 0x210, kCalls448FC0, BH_N(kCalls448FC0), BH_FN(BattleItemCmd_EquipItemPick), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipUsePick", 0x4491D0, 0x202, kCalls4491D0, BH_N(kCalls4491D0), BH_FN(BattleItemCmd_EquipUsePick), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipUseKind", 0x4493E0, 0x9B, kCalls4493E0, BH_N(kCalls4493E0), BH_FN(BattleItemCmd_EquipUseKind), S::kStep, 0, 10, kS3),
    C("BattleItemCmd_EquipTargetDispatch", 0x449480, 0x11, nullptr, 0, BH_FN(BattleItemCmd_EquipTargetDispatch), S::kDispatch, 0, 5, kS4),
    C("BattleItemCmd_EquipTargetBegin", 0x4494A0, 0x93, kCalls4494A0, BH_N(kCalls4494A0), BH_FN(BattleItemCmd_EquipTargetBegin), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipPickEnemy", 0x449540, 0x13C, kCalls449540, BH_N(kCalls449540), BH_FN(BattleItemCmd_EquipPickEnemy), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipPickMember", 0x449680, 0x1AC, kCalls449680, BH_N(kCalls449680), BH_FN(BattleItemCmd_EquipPickMember), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipCommit", 0x449830, 0x69, kCalls449830, BH_N(kCalls449830), BH_FN(BattleItemCmd_EquipCommit), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipCancel", 0x4498A0, 0x4A, kCalls4498A0, BH_N(kCalls4498A0), BH_FN(BattleItemCmd_EquipCancel), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipSideDispatch", 0x4498F0, 0x11, nullptr, 0, BH_FN(BattleItemCmd_EquipSideDispatch), S::kDispatch, 0, 4, kS4),
    C("BattleItemCmd_EquipSideBegin", 0x449910, 0x52, kCalls449910, BH_N(kCalls449910), BH_FN(BattleItemCmd_EquipSideBegin), S::kStep, 0, 8, kS4),
    C("BattleItemCmd_EquipSidePick", 0x449970, 0x83, kCalls449970, BH_N(kCalls449970), BH_FN(BattleItemCmd_EquipSidePick), S::kStep, 0, 8, kS4),
    C("BattleEquip_Apply", 0x449A00, 0x1A3, kCalls449A00, BH_N(kCalls449A00), BH_FN(BattleEquip_Apply), S::kHelper),
    C("BattleEquip_Preview", 0x449BB0, 0xBD, kCalls449BB0, BH_N(kCalls449BB0), BH_FN(BattleEquip_Preview), S::kHelper),
    C("BattleEquip_RemoveSlot", 0x449C70, 0x18C, kCalls449C70, BH_N(kCalls449C70), BH_FN(BattleEquip_RemoveSlot), S::kHelper, 0xFF),
    C("BattleEquip_OpenChange", 0x449E90, 0xC9, nullptr, 0, BH_FN(BattleEquip_OpenChange), S::kHelper),
    C("BattleEquip_OpenUse", 0x449F60, 0x73, nullptr, 0, BH_FN(BattleEquip_OpenUse), S::kHelper),
    // the escape
    C("Escape_Roll", 0x44A010, 0x113, kCalls44A010, BH_N(kCalls44A010), BH_FN(Escape_Roll), S::kStep, 0, 4, kS3),
    C("Escape_FailDispatch", 0x44A130, 0x11, nullptr, 0, BH_FN(Escape_FailDispatch), S::kDispatch, 0, 4, kS4),
    C("Escape_Begin", 0x44A150, 0x18F, kCalls44A150, BH_N(kCalls44A150), BH_FN(Escape_Begin), S::kStep, 0, 8, kS4),
    C("Escape_StepBack", 0x44A2E0, 0x49, nullptr, 0, BH_FN(Escape_StepBack), S::kStep, 0, 8, kS4),
    C("Escape_StepOn", 0x44A330, 0x49, nullptr, 0, BH_FN(Escape_StepOn), S::kStep, 0, 8, kS4),
    C("Escape_Failed", 0x44A380, 0xCD, kCalls44A380, BH_N(kCalls44A380), BH_FN(Escape_Failed), S::kStep, 0, 8, kS4),
    C("Escape_WinDispatch", 0x44A450, 0x11, nullptr, 0, BH_FN(Escape_WinDispatch), S::kDispatch, 0, 3, kS4),
    C("Escape_Leave", 0x44A470, 0x73, kCalls44A470, BH_N(kCalls44A470), BH_FN(Escape_Leave), S::kStep, 0, 8, kS4),
    C("Escape_End", 0x44A4F0, 0x30, nullptr, 0, BH_FN(Escape_End), S::kStep, 0, 8, kS4),
    C("Escape_Chance", 0x44A520, 0x96, kCalls44A520, BH_N(kCalls44A520), BH_FN(Escape_Chance), S::kHelper, 0xFF),
    // text
    C("Battle_MemberNameToText", 0x44A910, 0x41, kCalls44A910, BH_N(kCalls44A910), BH_FN(Battle_MemberNameToText), S::kHelper),
    C("Battle_EnemyNameToText", 0x44A960, 0x2A, kCalls44A960, BH_N(kCalls44A960), BH_FN(Battle_EnemyNameToText), S::kHelper),
    C("BattleBanner_AddLine", 0x44AA90, 0x39, kCalls44AA90, BH_N(kCalls44AA90), BH_FN(BattleBanner_AddLine), S::kHelper),
    // four entries again, through the dispatcher that reaches them in the game
    V("BattleItemCmd_SideBegin via 0x448B80", 0x448BA0, 0x51, kCalls448BA0, BH_N(kCalls448BA0), BH_FN(BattleItemCmd_SideBegin), 0x448B80,
      0x64E4A0, 0, kS4),
    V("BattleItemCmd_EquipSlotPick via 0x448C80", 0x448DE0, 0x1DD, kCalls448DE0, BH_N(kCalls448DE0), BH_FN(BattleItemCmd_EquipSlotPick),
      0x448C80, 0x64E4B4, 1, kS4),
    V("BattleItemCmd_EquipUseKind via BattleItemCmd_Dispatch", 0x4493E0, 0x9B, kCalls4493E0, BH_N(kCalls4493E0),
      BH_FN(BattleItemCmd_EquipUseKind), bof3::addr::BattleItemCmd_Dispatch, 0x64E45C + 4 * 7, 7, kS3),
    V("Escape_Roll via 0x44A000", 0x44A010, 0x113, kCalls44A010, BH_N(kCalls44A010), BH_FN(Escape_Roll), 0x44A000, 0x64E4FC, 0, kS3),
};

// The dispatchers' tables, counted from the code (docs/battle_e4.md section 5).
const bh::DataTable kTables[] = {{0x64E4C0, 5}, {0x64E4D4, 4}, {0x64E508, 4}, {0x64E518, 3}};

// Beyond the standard set: the group's own functions its others call
// directly, the two cross-group callees (BE5's, BE6's), and three standard
// callees whose words the originals push with the caller's upper bytes
// (Inventory_Add / _Remove read low bytes only - char_stats.cpp, scena_sx.cpp;
// Battle_ReturnQueuedItem the low byte - battle_setup.cpp).
const bh::Callee kCallees[] = {
    {"BattleEquip_OpenUse", 0x449F60, KeyOf(&::BattleEquip_OpenUse), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"BattleEquip_OpenChange", 0x449E90, KeyOf(&::BattleEquip_OpenChange), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"BattleEquip_RemoveSlot", 0x449C70, KeyOf(&::BattleEquip_RemoveSlot), 0, {}, bh::Answer::kFlag, 0, 0},
    {"BattleEquip_Preview", 0x449BB0, KeyOf(&::BattleEquip_Preview), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"BattleEquip_Apply", 0x449A00, KeyOf(&::BattleEquip_Apply), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"Battle_PrevTarget", 0x4457F0, KeyOf(&::Battle_PrevTarget), 1, {kAll}, bh::Answer::kByte, 0xFF, 10},
    {"Battle_ReturnItem", 0x446D90, KeyOf(&::Battle_ReturnItem), 2, {kU8, kU16}, bh::Answer::kFlag, 0, 0},
    {"Escape_Chance", 0x44A520, KeyOf(&::Escape_Chance), 1, {kAll}, bh::Answer::kByte, 0, 0x40},
    {"0x44FDE0", at::kAfterEquip, at::kAfterEquip, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x453300", at::kMemberRefresh, at::kMemberRefresh, 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    {"Inventory_Add", bof3::addr::Inventory_Add, KeyOf(&::Inventory_Add), 3, {kU8, kU8, kU8}, bh::Answer::kFlag, 0, 0},
    {"Inventory_Remove", bof3::addr::Inventory_Remove, KeyOf(&::Inventory_Remove), 3, {kU8, kU8, kU8}, bh::Answer::kFlag, 0, 0},
    {"Battle_ReturnQueuedItem", bof3::addr::Battle_ReturnQueuedItem, KeyOf(&::Battle_ReturnQueuedItem), 1, {kU8}, bh::Answer::kGarbage, 0, 0},
};

// Beyond the standard regions: the character records past the first, the
// members' character bytes and 0x66972C, the preview bytes, the tint, the
// escape's field cells and MoveScript_FAWord, the auto-repeat latch, the
// inventory lists, the message queue's tail and what Escape_Roll reads past
// the eight enemy objects.
const bh::Region kRegions[] = {
    {0x903B24, 0x903F90 - 0x903B24},   // CharacterRecords 1..7 (0x903A70 + 8 x 0xA4 = Cond_Flags)
    {0x904060, 0x10},                  // 0x904065..0x904067
    {at::kCharOf, 0x18},               // MoveScript_EffectState
    {at::kPreview, 8},
    {at::kTint, 4},
    {0x904EFC, 4},                     // MoveScript_FAWord
    {at::kKind2Z, 8},                  // Field_Kind2Z, Field_Kind2X
    {at::kRepeatLatch, 4},
    {0x904154, 0x9045FC - 0x904154},   // the inventory's id and count lists (0x656B00 / 0x656B14)
    {0x93C320, 0x3C0},                 // the queue's tail to 0x93C340, Escape_Roll's objects past the eight
};

const bh::Clone* g_run = kAll56;
unsigned g_run_n = BH_COUNT(kAll56);
bh::Clone g_only[BH_COUNT(kAll56)];

unsigned char* M(U address) { return Mem(address); }
unsigned char Byte(std::initializer_list<U> v) {
    return static_cast<unsigned char>(v.begin()[bh::Next() % v.size()]);
}
U Word16(std::initializer_list<U> v) { return v.begin()[bh::Next() % v.size()]; }
unsigned char* Enemy(unsigned actor) { return M(at::kEnemies + ((actor & 0xFF) - 3u) * at::kEnemyStride); }

// The cells every function's indices go through, kept inside what the
// regions hold (docs/battle_e4.md section 5): the members' +5 (their party
// index, the menu actor's) and +0x148 / +0x89 (character bytes), the three
// character bytes 0x904065 and 0x66972C's first eight, record 18's slot cursor
// and member, record 16's category.
void Indices() {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = bh::PartyOf(static_cast<unsigned char>(m));
        p[5] = static_cast<unsigned char>(bh::Often() ? m : bh::Next() % 3);
        p[0x148] = static_cast<unsigned char>(bh::Next() % 8);
        p[0x89] = static_cast<unsigned char>(bh::Next() % 8);
    }
    for (unsigned i = 0; i < 3; ++i) M(at::kMemberChar)[i] = static_cast<unsigned char>(bh::Next() % 8);
    for (unsigned i = 0; i < 8; ++i) M(at::kCharOf)[i] = static_cast<unsigned char>(bh::Next() % 8);
    M(at::kSlots + 0xA)[0] = static_cast<unsigned char>(bh::Next() % 6);
    M(at::kSlots + 0xC)[0] = static_cast<unsigned char>(bh::Next() % 3);
    M(at::kList + 0xA)[0] = static_cast<unsigned char>(bh::Next() % 5);
}

// After every disturbance: the same cells a function reads again after a call
// (the slot cursor in BattleEquip_RemoveSlot and _Preview, the category in
// BattleItemCmd_SidePick, a member's +5 through the menu actor), put back
// inside their tables - the engine disturbance moves bytes of the current
// window record and of Sprite_Current, which may be those.
void Settle() {
    unsigned char* const cursor = M(at::kSlots + 0xA);
    if (*cursor > 5) *cursor = static_cast<unsigned char>(*cursor % 6);
    unsigned char* const member = M(at::kSlots + 0xC);
    if (*member > 2) *member = static_cast<unsigned char>(*member % 3);
    unsigned char* const category = M(at::kList + 0xA);
    if (*category > 4) *category = static_cast<unsigned char>(*category % 5);
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = bh::PartyOf(static_cast<unsigned char>(m));
        if (p[5] > 2) p[5] = static_cast<unsigned char>(p[5] % 3);
    }
}

// The buttons: cancel and confirm two distinct single bits (the save's words
// are single buttons), Input_Pressed one of the branches' values.
void Buttons() {
    static const U kBits[] = {0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800};
    const U cancel = kBits[bh::Next() % 7];
    U confirm = kBits[bh::Next() % 7];
    if (confirm == cancel) confirm = cancel == 0x20 ? 0x40 : 0x20;
    SetWord(M(at::kCancelButtons), cancel);
    SetWord(M(at::kConfirmButtons), confirm);
    const U pressed = Word16({0, cancel, confirm, 0x10, 0x1000, 0x2000, 0x4000, 0x8000, 0xA000, 4, 8, bh::Next() & 0xFFFF,
                              cancel | 0x1000});
    SetWord(M(at::kInputPressed), pressed);
}

// The command states' windows: record 21's option and +3, record 18's +3 and
// word +0x10, record 17's +3, top, cursor and item at their ends.
void Windows() {
    M(at::kAbove + 0xA)[0] = Byte({0, 1, 0, 1, 2, 0xFF});
    M(at::kAbove + 3)[0] = Byte({0, 0, 1});
    M(at::kSlots + 3)[0] = Byte({0, 0, 1, 2});
    SetWord(M(at::kSlots + 0x10), bh::Half() ? 0 : bh::Next());
    M(at::kCands + 3)[0] = Byte({0, 0, 1});
    M(at::kCands + 0xA)[0] = Byte({0, 1, 6, 7, 8, 0x72, 0x73, 0x78, 0x79, bh::Next()});
    M(at::kCands + 0xB)[0] = Byte({0, 1, 6, 7, 8, 0x72, 0x7E, 0x7F, 0x80, bh::Next()});
    M(at::kCands + 0xD)[0] = Byte({0, 0, bh::Next()});
}

const bh::Clone* g_cur = nullptr;

void Seed(unsigned k) {
    const U base = g_cur[k].base;
    Indices();
    Buttons();
    Windows();
    // the counts every loop runs to: the party 0..3 (1..3 where it divides), the divisors at least 1
    M(at::kPartyCount)[0] = Byte({0, 1, 2, 3, 3, 3});
    M(at::kPartyDivisor)[0] = Byte({1, 1, 2, 3, 8});
    M(at::kEnemyDivisor)[0] = Byte({1, 2, 3, 8});
    M(at::kFaceStep)[0] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 4 : bh::Next());
    switch (base) {
    case 0x4463E0:   // divides by the count
        M(at::kPartyCount)[0] = static_cast<unsigned char>(1 + bh::Next() % 3);
        break;
    case 0x446110:
    case 0x4461B0:
        M(at::kFlags)[0] = static_cast<unsigned char>(bh::Next() % 4 == 0 ? M(at::kFlags)[0] | 0x80 : M(at::kFlags)[0] & 0x7F);
        M(at::kEvadeParty)[0] = Byte({0, 1, 49, 50, 98, 99, 100, 0xFF});
        M(at::kHitEnemy)[0] = Byte({0, 1, 49, 50, 98, 99, 100, 0xFF});
        break;
    case 0x446810: {
        unsigned char* const fs = Field_State;
        M(at::kFlags)[0] = static_cast<unsigned char>(bh::Next() % 3 == 0 ? M(at::kFlags)[0] | 0x40 : M(at::kFlags)[0] & 0xBF);
        M(at::kActor)[0] = static_cast<unsigned char>(bh::Next() % 11);
        if (bh::Often()) fs[0x130] &= 0xFE;
        if (bh::Often()) SetWord(fs + 0x90, Word(fs + 0x90) & ~0x4864u);
        M(at::kActionKind)[0] = Byte({4, 4, 1, bh::Next()});
        SetWord(M(at::kAbility), Word16({0xA1, 0xA1, 0xA0, bh::Next() & 0xFFFF}));
        M(at::kCharged)[0] = Byte({0, 0, 1, bh::Next()});
        fs[0xB9] = Byte({0, 1, 50, 99, 100, bh::Next()});
        break;
    }
    case 0x446720:   // the entry order: members or the 0xFF end, the count 0..3
        M(at::kCommandsChosen)[0] = static_cast<unsigned char>(bh::Next() % 4);
        for (unsigned i = 0; i < 3; ++i) M(at::kEntryOrder)[i] = Byte({0, 1, 2, 0xFF});
        break;
    case 0x446700:   // the front stays inside the battle bytes (0x904ACC + 0xD3 = 0x904B9F)
        M(at::kOrderCursor)[0] = static_cast<unsigned char>(1 + bh::Next() % 0x20);
        break;
    case 0x446770:
    case 0x4467C0:
        bh::SpriteRecord(0)[8] = Byte({0, 1, 2, 3, 4, bh::Next()});
        bh::SpriteRecord(1)[8] = Byte({0, 1, 2, 3, 4, bh::Next()});
        break;
    case 0x446B00:   // equal +0x98 words, so the second pass has ties to break
        for (unsigned a = 3; a <= 10; ++a) {
            unsigned char* const e = Enemy(a);
            SetWord(e + 0x98, bh::Often() ? bh::Next() % 4 : bh::Next());
            if (bh::Half()) SetWord(e + 0xA4, bh::Next() % 4);
        }
        break;
    case 0x446CB0:
        for (unsigned m = 0; m < 3; ++m) bh::PartyOf(static_cast<unsigned char>(m))[0x12D] = Byte({0, 1, 2, 3, 4, 0xFF});
        for (unsigned a = 3; a <= 10; ++a) Enemy(a)[0x10D] = Byte({0, 1, 2, 3, 4, 0xFF});
        break;
    case 0x446D90:   // counts at 99 here and there
        for (unsigned c = 0; c < 4; ++c) {
            unsigned char* const counts = M(static_cast<U>(Long(M(at::kInventoryCounts + 4 * c))));
            for (unsigned i = 0; i < 0x80; ++i)
                if (bh::Next() % 3 == 0) counts[i] = 0x63;
        }
        break;
    case 0x449A00: {   // preview bytes: none, the same as worn, or another
        const unsigned member = M(at::kSlots + 0xC)[0];
        const unsigned char c = M(at::kCharOf + M(at::kMemberChar + member)[0])[0];
        const unsigned char* const r = M(at::kCharRecords + c * at::kCharStride);
        for (unsigned i = 0; i < 6; ++i) M(at::kPreview + i)[0] = Byte({0, r[0x12 + i], r[0x12 + i], bh::Next()});
        break;
    }
    case 0x449C70: {   // the slot's item: none a third of the time
        const unsigned member = M(at::kSlots + 0xC)[0];
        const unsigned char c = M(at::kCharOf + M(at::kMemberChar + member)[0])[0];
        unsigned char* const r = M(at::kCharRecords + c * at::kCharStride);
        if (bh::Next() % 3 == 0) r[0x12 + M(at::kSlots + 0xA)[0]] = 0;
        if (bh::Next() % 4 == 0) M(at::kSlots + 0xA)[0] = 0;
        break;
    }
    case 0x44A010:
        if (bh::Half()) M(at::kFight)[0] = 0;
        M(at::kEscapeTries)[0] = Byte({0, 1, 2, 3, 0xFF});
        M(at::kEscapeRun)[0] = Byte({1, 0, bh::Next()});
        break;
    case 0x44A520:
        M(at::kEscapeTries)[0] = Byte({2, 2, 1, bh::Next()});
        break;
    case 0x44A150:
        M(at::kStep3)[0] = Byte({2, 1, bh::Next()});
        for (unsigned m = 0; m < 3; ++m) bh::PartyOf(static_cast<unsigned char>(m))[0x125] = Byte({5, 5, bh::Next()});
        break;
    case 0x44A2E0:
    case 0x44A330:
    case 0x44A380:
    case 0x44A470:
        M(at::kKind2Hold)[0] = Byte({0, 0, bh::Next()});
        break;
    case 0x44A4F0:
        M(at::kEndHold)[0] = Byte({0, 0, bh::Next()});
        break;
    default:
        break;
    }
}

// The words of the helpers: actors inside their sides (the originals index by
// them), a record of the harness's for the vector turns, an item word whose
// category is 0..3, boundaries for the pace and the escape's difference;
// garbage above the byte half the time where the originals read a byte.
U Garbage(U low) { return (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | low; }
void Args(unsigned k, U* a) {
    switch (g_cur[k].base) {
    case 0x445600: {
        a[0] = Garbage(3 + bh::Next() % 8);
        const U pace = Word(Enemy(a[0]) + 0xB8);
        a[1] = (bh::Half() ? bh::Next() & 0xFFFF0000u : 0) | (Word16({pace / 2, pace / 2 + 1, (pace + 1) / 2, bh::Next()}) & 0xFFFF);
        a[2] = (bh::Half() ? bh::Next() & 0xFFFF0000u : 0) | (Word16({pace, pace + 1, pace - 1, bh::Next()}) & 0xFFFF);
        break;
    }
    case 0x4457F0: a[0] = Garbage(bh::Often() ? bh::Next() % 13 : bh::Next() & 0xFF); break;
    case 0x446110: a[1] = Garbage(bh::Next() % 11); a[2] = Garbage(bh::Next() % 3); break;
    case 0x4461B0: a[1] = Garbage(bh::Next() % 11); a[2] = Garbage(3 + bh::Next() % 8); break;
    case 0x446540:
    case 0x446CB0: a[0] = Garbage(bh::Next() % 11); break;
    case 0x446700: a[0] = Garbage(bh::Next() & 0xFF); break;
    case 0x446770:
    case 0x4467C0: a[0] = Key(bh::SpriteRecord(bh::Next() & 1)); break;
    case 0x446A80:
    case 0x44A910: a[0] = Garbage(bh::Next() % 3); break;
    case 0x446D90:
        a[0] = Garbage(bh::Next() & 0xFF);
        a[1] = (bh::Half() ? bh::Next() & 0xFFFF0000u : 0) | (bh::Next() % 4) << 8 | (bh::Next() & 0xFF);
        break;
    case 0x44A520:
        a[0] = Word16({0xFFFFFFDFu, 0xFFFFFFE0u, 0xFFFFFFEFu, 0xFFFFFFF0u, 0xFFFFFFFFu, 0, 0xF, 0x10, 0x3F, 0x40, 0x50, 0x5F, 0x60,
                       0x70, 0x7FFFFFFFu, bh::Next()});
        break;
    case 0x44A960: a[0] = Garbage(3 + bh::Next() % 8); break;
    case 0x44AA90:
        a[0] = Garbage(bh::Often() ? bh::Next() % 8 : bh::Next() & 0xFF);
        a[1] = Garbage(bh::Often() ? bh::Next() % 10 : bh::Next() & 0xFF);
        break;
    default: break;
    }
}

// What the group's functions read again after a call that the engine
// disturbance does not move: the members' +5 and item words, record 18's word
// +0x10, record 17's item, record 21's option, the party count, the item
// category's id and the escape's field hold.
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 7) {
    case 0: bh::PartyOf(static_cast<unsigned char>(b % 3))[5] = static_cast<unsigned char>((h >> 24) % 3); break;
    case 1: SetWord(bh::PartyOf(static_cast<unsigned char>(b % 3)) + 0x126, h >> 12); break;
    case 2: SetWord(M(at::kSlots + 0x10), b & 1 ? 0 : h >> 12); break;
    case 3: M(at::kCands + 0xD)[0] = b; break;
    case 4: M(at::kAbove + 0xA)[0] = b & 1; break;
    case 5: M(at::kPartyCount)[0] = static_cast<unsigned char>(b % 4); break;
    default: M(at::kKind2Hold)[0] = b & 1; break;
    }
}

// BOF3X_BE4_RUN: the clones whose bases are listed, alone.
void Select() {
    const char* const only = std::getenv("BOF3X_BE4_RUN");
    if (only == nullptr || *only == 0) return;
    unsigned n = 0;
    for (const bh::Clone& c : kAll56) {
        for (const char* p = only; *p;) {
            char* end = nullptr;
            const unsigned long v = std::strtoul(p, &end, 16);
            if (end == p) break;
            if (v == c.base) {
                g_only[n++] = c;
                break;
            }
            p = *end == ',' ? end + 1 : end;
        }
    }
    if (n == 0) bof3::Fatal("battle_e4: BOF3X_BE4_RUN=%s names no clone", only);
    g_run = g_only;
    g_run_n = n;
}

}  // namespace

void SelfTest() {
    Select();
    g_cur = g_run;
    bh::Group g{"battle_e4", g_run, g_run_n, kCallees, BH_COUNT(kCallees), kTables, BH_COUNT(kTables), kRegions, BH_COUNT(kRegions),
                &Seed, &Disturb, 6000};
    g.args = &Args;
    g.settle = &Settle;
    g.engine = true;
    bh::Run(g);
    g_cur = nullptr;
}

}  // namespace battle_e4
