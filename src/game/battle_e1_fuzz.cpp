// BOF3X_SHADOW=battle_e1: group BE1's 41 functions through the boss
// harness's engine frame (boss_harness.h, docs/boss_harness.md section 10),
// once at start-up: one boss_harness::Run with Group::engine.
// docs/battle_e1.md section 5.
//
// The clone rows are tools/band_rows.py's (--group BE1 --clones,
// 2026-09-29), each read against the disassembly. The shapes: BATE's and the
// engine's steps kStep (the dispatchers' entries), their dispatchers
// kDispatch with the absolute step byte (state_cell) drawn below their
// tables (BattleExtra_EquipRun, a `call` through its table, kStep with the
// same draw), the cdecl helpers and draws kHelper with their words from
// Args. The .data tables the dispatchers jump through are DataTables (their
// entries recorders). BOF3X_BE1_ONLY=<hex address> runs that one clone (the
// controls script's shortcut); unset, all 41 run.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_e1.h"
#include "game/battle_e1_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace battle_e1 {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetLong;
using move_script::SetWord;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)

bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret = 0,
            std::uint8_t states = 0, U state_cell = 0, const bh::JumpTable* tables = nullptr, int n_tables = 0) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, tables, n_tables, ours, ret, false, shape};
    c.states = states;
    c.state_cell = state_cell;
    return c;
}

// ===========================================================================
// The clone rows (tools/band_rows.py --group BE1 --clones)
// ===========================================================================

constexpr bh::CallSite kCalls42D7A0[] = {{0x6, 0x575690}, {0xF, 0x42D8C0}, {0x21, 0x42DA60}};
constexpr bh::CallSite kCalls42D7F0[] = {{0x7, 0x575690}, {0x10, 0x42D8C0}, {0x6E, 0x495040}};
constexpr bh::CallSite kCalls42D880[] = {{0x28, 0x575690}, {0x31, 0x42D8C0}};
constexpr bh::CallSite kCalls42D8C0[] = {{0x2A, 0x57CF60},  {0x3B, 0x5762D0},  {0x45, 0x497740},  {0x5A, 0x516B30},
                                         {0x89, 0x497740},  {0xA6, 0x516B30},  {0xB2, 0x497740},  {0xC8, 0x516B30},
                                         {0xDB, 0x5B9380},  {0xF7, 0x516F60},  {0x10C, 0x5B9380}, {0x11A, 0x516F60},
                                         {0x121, 0x42DB40}, {0x143, 0x497740}, {0x159, 0x516B30}, {0x163, 0x497740},
                                         {0x175, 0x516E70}, {0x17F, 0x497740}, {0x191, 0x516E70}};
constexpr bh::CallSite kCalls42DA60[] = {{0x70, 0x590E30}, {0x82, 0x590E30}, {0x94, 0x590DE0},
                                         {0xA5, 0x590DE0}, {0xB7, 0x590E30}, {0xC9, 0x590E30}};
constexpr bh::CallSite kCalls42DB40[] = {{0xC, 0x5A7650},  {0x14, 0x5A7780}, {0x60, 0x461E50},
                                         {0x6C, 0x5A7650}, {0x74, 0x5A7780}, {0xAC, 0x461E50}};
constexpr bh::CallSite kCalls42DC20[] = {{0x6, 0x575690}, {0x1D, 0x587740}, {0x22, 0x42E0E0}, {0x48, 0x5B9450}};
constexpr bh::CallSite kCalls42DC80[] = {{0x6, 0x575690}};
constexpr bh::CallSite kCalls42DCD0[] = {{0x15, 0x575690}};
constexpr bh::CallSite kCalls42DCF0[] = {{0x43, 0x461EB0}, {0x62, 0x587740},  {0xA1, 0x591C20},
                                         {0xCC, 0x587740}, {0x107, 0x587740}, {0x111, 0x587740}};
constexpr bh::CallSite kCalls42DE50[] = {{0x4C, 0x591C20},  {0x6C, 0x461EB0},  {0x76, 0x42E2F0},  {0x162, 0x587740},
                                         {0x191, 0x587740}, {0x199, 0x42E250}, {0x1B7, 0x587740}, {0x1CE, 0x587740}};
constexpr bh::CallSite kCalls42E050[] = {{0x6, 0x575690}, {0x1E, 0x495040}};
constexpr bh::CallSite kCalls42E090[] = {{0x2D, 0x5B9450}, {0x3C, 0x575690}};
constexpr bh::CallSite kCalls42EDA0[] = {{0x2D, 0x4439A0}, {0x41, 0x442FA0}, {0x56, 0x4432F0}};
constexpr bh::CallSite kCalls42EE00[] = {{0x3A, 0x442FA0}, {0x42, 0x444660}, {0x57, 0x4439A0}, {0x79, 0x5903F0}};
constexpr bh::CallSite kCalls42EF50[] = {{0x2C, 0x4456C0}, {0x64, 0x446D90}, {0x9A, 0x4456C0}, {0xFD, 0x454590}};
constexpr bh::CallSite kCalls42F5F0[] = {{0x0, 0x4301B0}, {0x13, 0x44A650}, {0x26, 0x446FB0}};
constexpr bh::CallSite kCalls42F9D0[] = {{0x13, 0x5B93D2}, {0x37, 0x5B93D2}, {0xBA, 0x452EB0}, {0xC5, 0x452EB0}, {0xCC, 0x452F10}};
constexpr bh::CallSite kCalls42FE20[] = {{0xA0, 0x44A910}, {0x105, 0x497740}, {0x10F, 0x44A880}, {0x134, 0x497740}, {0x13E, 0x44A880}};
constexpr bh::CallSite kCalls4315C0[] = {{0x5C, 0x4DF820}, {0x99, 0x44F4B0}, {0xA5, 0x454DC0}, {0x108, 0x5891C0}, {0x12A, 0x536AC0}};
constexpr bh::CallSite kCalls4319B0[] = {{0x17, 0x4456C0}};
constexpr bh::CallSite kCalls431C10[] = {{0x19, 0x44A910}, {0x40, 0x432170}, {0x7C, 0x5B9380},
                                         {0x83, 0x497740}, {0x9D, 0x432170}, {0xF3, 0x59E2D0}};
constexpr bh::CallSite kCalls431FE0[] = {{0x18, 0x4456C0}};
constexpr bh::JumpTable kTables432170[] = {{0x85, 0x2A4, 7}};
constexpr bh::CallSite kCalls432440[] = {{0x2, 0x495040}, {0x9, 0x4549F0}};
constexpr bh::CallSite kCalls432460[] = {{0x37, 0x589330}, {0x7F, 0x454DC0},  {0xA6, 0x589330},
                                         {0xEE, 0x454DC0}, {0x111, 0x589330}, {0x15C, 0x454DC0},
                                         {0x164, 0x494E70}, {0x169, 0x435260}, {0x170, 0x495040}};
constexpr bh::CallSite kCalls4325F0[] = {{0x0, 0x432930}, {0xA, 0x4327F0}};
constexpr bh::CallSite kCalls432630[] = {{0x0, 0x432930}, {0xA, 0x4327F0}, {0x14, 0x4329A0}, {0x33, 0x432A30}, {0x105, 0x495040}};
constexpr bh::CallSite kCalls432750[] = {{0x1, 0x432930},  {0xB, 0x4327F0},  {0x15, 0x4329A0}, {0x2D, 0x454DC0},
                                         {0x37, 0x454DC0}, {0x41, 0x454DC0}, {0x46, 0x59E330}, {0x7B, 0x494E70},
                                         {0x80, 0x435260}, {0x85, 0x587860}, {0x8F, 0x5A9976}};
constexpr bh::CallSite kCalls4327F0[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50},  {0x45, 0x5A79E0},
                                         {0x7E, 0x5A7710}, {0x87, 0x461E50}, {0x99, 0x5A79E0},  {0xCE, 0x5A7710},
                                         {0xD7, 0x461E50}, {0xEC, 0x5A79E0}, {0x121, 0x5A7710}, {0x12A, 0x461E50}};
constexpr bh::CallSite kCalls432930[] = {{0xD, 0x5A79A0}, {0x23, 0x5A77C0}, {0x2C, 0x461E50}, {0x38, 0x5A7740}, {0x5E, 0x461E50}};
constexpr bh::CallSite kCalls4329A0[] = {{0x11, 0x5A79A0}, {0x29, 0x5A77C0}, {0x32, 0x461E50},
                                         {0x44, 0x5A79E0}, {0x7D, 0x5A7710}, {0x86, 0x461E50}};
constexpr bh::CallSite kCalls432A30[] = {{0xF, 0x5A79A0},  {0x25, 0x5A77C0}, {0x2E, 0x461E50},  {0x3A, 0x5A7740},
                                         {0x7D, 0x461E50}, {0x8F, 0x5A79A0}, {0xA6, 0x5A77C0},  {0xAF, 0x461E50},
                                         {0xBB, 0x5A7610}, {0x125, 0x5A7780}, {0x12E, 0x461E50}};

const bh::Clone kClones[] = {
    // BATE state 3, the tally
    C("BattleExtra_TallyOpen", 0x42D7A0, 0x41, kCalls42D7A0, BH_N(kCalls42D7A0), BH_FN(BattleExtra_TallyOpen), S::kStep),
    C("BattleExtra_TallyCount", 0x42D7F0, 0x84, kCalls42D7F0, BH_N(kCalls42D7F0), BH_FN(BattleExtra_TallyCount), S::kStep),
    C("BattleExtra_TallyClose", 0x42D880, 0x3A, kCalls42D880, BH_N(kCalls42D880), BH_FN(BattleExtra_TallyClose), S::kStep),
    C("BattleExtra_DrawTally", 0x42D8C0, 0x19F, kCalls42D8C0, BH_N(kCalls42D8C0), BH_FN(BattleExtra_DrawTally), S::kHelper),
    C("BattleExtra_ApplyTally", 0x42DA60, 0xD4, kCalls42DA60, BH_N(kCalls42DA60), BH_FN(BattleExtra_ApplyTally), S::kHelper),
    C("BattleExtra_DrawPlus", 0x42DB40, 0xBA, kCalls42DB40, BH_N(kCalls42DB40), BH_FN(BattleExtra_DrawPlus), S::kHelper),
    // BATE state 2, the equipment screen
    C("BattleExtra_EquipDispatch", 0x42DC00, 0xE, nullptr, 0, BH_FN(BattleExtra_EquipDispatch), S::kDispatch, 0, 3, at::kModeStep),
    C("BattleExtra_EquipOpenDispatch", 0x42DC10, 0xE, nullptr, 0, BH_FN(BattleExtra_EquipOpenDispatch), S::kDispatch, 0, 3,
      at::kModeSub),
    C("BattleExtra_EquipOpen", 0x42DC20, 0x51, kCalls42DC20, BH_N(kCalls42DC20), BH_FN(BattleExtra_EquipOpen), S::kStep),
    C("BattleExtra_EquipFadeIn", 0x42DC80, 0x48, kCalls42DC80, BH_N(kCalls42DC80), BH_FN(BattleExtra_EquipFadeIn), S::kStep),
    C("BattleExtra_EquipRun", 0x42DCD0, 0x1C, kCalls42DCD0, BH_N(kCalls42DCD0), BH_FN(BattleExtra_EquipRun), S::kStep, 0, 2,
      at::kModeSub),
    C("BattleExtra_EquipSlotInput", 0x42DCF0, 0x15A, kCalls42DCF0, BH_N(kCalls42DCF0), BH_FN(BattleExtra_EquipSlotInput), S::kStep),
    C("BattleExtra_EquipListInput", 0x42DE50, 0x1EA, kCalls42DE50, BH_N(kCalls42DE50), BH_FN(BattleExtra_EquipListInput), S::kStep),
    C("BattleExtra_EquipLeaveDispatch", 0x42E040, 0xE, nullptr, 0, BH_FN(BattleExtra_EquipLeaveDispatch), S::kDispatch, 0, 2,
      at::kModeSub),
    C("BattleExtra_EquipFadeOut", 0x42E050, 0x33, kCalls42E050, BH_N(kCalls42E050), BH_FN(BattleExtra_EquipFadeOut), S::kStep),
    C("BattleExtra_EquipLeave", 0x42E090, 0x43, kCalls42E090, BH_N(kCalls42E090), BH_FN(BattleExtra_EquipLeave), S::kStep),
    // the command menu
    C("BattleHold_Dispatch", 0x42ED90, 0xE, nullptr, 0, BH_FN(BattleHold_Dispatch), S::kDispatch, 0, 3, at::kSubStep),
    C("BattleHold_Shrink", 0x42EDA0, 0x5F, kCalls42EDA0, BH_N(kCalls42EDA0), BH_FN(BattleHold_Shrink), S::kStep),
    C("BattleHold_ShowAll", 0x42EE00, 0x94, kCalls42EE00, BH_N(kCalls42EE00), BH_FN(BattleHold_ShowAll), S::kStep),
    C("BattleHold_Regrow", 0x42EEA0, 0x2E, nullptr, 0, BH_FN(BattleHold_Regrow), S::kStep),
    C("Cmd_AutoBattle", 0x42EF50, 0x119, kCalls42EF50, BH_N(kCalls42EF50), BH_FN(Cmd_AutoBattle), S::kStep),
    // action kind 3, the random pick, the notice
    C("BattleAction_Kind3Dispatch", 0x42F5E0, 0xE, nullptr, 0, BH_FN(BattleAction_Kind3Dispatch), S::kDispatch, 0, 5, at::kSubStep2),
    C("BattleAction_Kind3Banner", 0x42F5F0, 0x45, kCalls42F5F0, BH_N(kCalls42F5F0), BH_FN(BattleAction_Kind3Banner), S::kStep),
    C("BattleAction_Kind3Wait", 0x42F640, 0x24, nullptr, 0, BH_FN(BattleAction_Kind3Wait), S::kStep),
    C("BattleAction_PickRandomAbility", 0x42F9D0, 0xD3, kCalls42F9D0, BH_N(kCalls42F9D0), BH_FN(BattleAction_PickRandomAbility),
      S::kHelper, 0xFF),
    C("BattleAction_AbilityNotice", 0x42FE20, 0x14A, kCalls42FE20, BH_N(kCalls42FE20), BH_FN(BattleAction_AbilityNotice), S::kStep),
    // the end and the result
    C("BattleEnd_AwaitRestore", 0x4315C0, 0x143, kCalls4315C0, BH_N(kCalls4315C0), BH_FN(BattleEnd_AwaitRestore), S::kStep),
    C("BattleResult_CountExpShares", 0x4319B0, 0x61, kCalls4319B0, BH_N(kCalls4319B0), BH_FN(BattleResult_CountExpShares), S::kHelper,
      0xFF),
    C("BattleResult_LevelUpNotice", 0x431C10, 0x136, kCalls431C10, BH_N(kCalls431C10), BH_FN(BattleResult_LevelUpNotice), S::kStep),
    C("BattleResult_ZennyBonus", 0x431FE0, 0x68, kCalls431FE0, BH_N(kCalls431FE0), BH_FN(BattleResult_ZennyBonus), S::kHelper, 0xFF),
    C("Char_LevelUpGain", 0x432170, 0x2C0, nullptr, 0, BH_FN(Char_LevelUpGain), S::kHelper, 0xFFFF, 0, 0, kTables432170,
      BH_N(kTables432170)),
    // the loss screen
    C("BattleLoss_Dispatch", 0x432430, 0xE, nullptr, 0, BH_FN(BattleLoss_Dispatch), S::kDispatch, 0, 5, at::kSubStep2),
    C("BattleLoss_FadeOut", 0x432440, 0x1E, kCalls432440, BH_N(kCalls432440), BH_FN(BattleLoss_FadeOut), S::kStep),
    C("BattleLoss_ResetParty", 0x432460, 0x186, kCalls432460, BH_N(kCalls432460), BH_FN(BattleLoss_ResetParty), S::kStep),
    C("BattleLoss_Show", 0x4325F0, 0x32, kCalls4325F0, BH_N(kCalls4325F0), BH_FN(BattleLoss_Show), S::kStep),
    C("BattleLoss_BarGrow", 0x432630, 0x11D, kCalls432630, BH_N(kCalls432630), BH_FN(BattleLoss_BarGrow), S::kStep),
    C("BattleLoss_Restart", 0x432750, 0x99, kCalls432750, BH_N(kCalls432750), BH_FN(BattleLoss_Restart), S::kStep),
    C("BattleLoss_DrawPanels", 0x4327F0, 0x135, kCalls4327F0, BH_N(kCalls4327F0), BH_FN(BattleLoss_DrawPanels), S::kHelper),
    C("BattleLoss_DrawBlack", 0x432930, 0x69, kCalls432930, BH_N(kCalls432930), BH_FN(BattleLoss_DrawBlack), S::kHelper),
    C("BattleLoss_DrawCaption", 0x4329A0, 0x90, kCalls4329A0, BH_N(kCalls4329A0), BH_FN(BattleLoss_DrawCaption), S::kHelper),
    C("BattleLoss_DrawBar", 0x432A30, 0x13B, kCalls432A30, BH_N(kCalls432A30), BH_FN(BattleLoss_DrawBar), S::kHelper),
};
static_assert(sizeof kClones / sizeof kClones[0] == 41, "the 39 of the cut and the two it lacks");
const bh::Clone* g_base = kClones;   // the Run's clones (Seed and Args index them by k)

// ===========================================================================
// The callees the standard set lacks or records too coarsely, the tables,
// the regions
// ===========================================================================

// The group's own functions called directly, BE4's three by address, and
// the standard callees whose words the originals push in whole registers
// with a left-over upper half (the callee reads the byte or the word: its
// own disassembly, or ours, says which) - listed at what the callee reads.
const bh::Callee kCallees[] = {
    // the group's own
    {"BattleExtra_DrawTally", 0x42D8C0, KeyOf(&::BattleExtra_DrawTally), 2, {kAll, kAll}, bh::Answer::kGarbage, 0, 0},
    {"BattleExtra_ApplyTally", 0x42DA60, KeyOf(&::BattleExtra_ApplyTally), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"BattleExtra_DrawPlus", 0x42DB40, KeyOf(&::BattleExtra_DrawPlus), 2, {kU16, kU16}, bh::Answer::kGarbage, 0, 0},
    {"Char_LevelUpGain", 0x432170, KeyOf(&::Char_LevelUpGain), 2, {kU8, kU8}, bh::Answer::kFlag, 0, 0},
    {"BattleLoss_DrawPanels", 0x4327F0, KeyOf(&::BattleLoss_DrawPanels), 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    {"BattleLoss_DrawBlack", 0x432930, KeyOf(&::BattleLoss_DrawBlack), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"BattleLoss_DrawCaption", 0x4329A0, KeyOf(&::BattleLoss_DrawCaption), 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    {"BattleLoss_DrawBar", 0x432A30, KeyOf(&::BattleLoss_DrawBar), 1, {kU16}, bh::Answer::kGarbage, 0, 0},
    // BE4's (analysis/round12_cut.tsv), called raw until it merges
    {"0x444660", at::kBe4Draw444660, at::kBe4Draw444660, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x446D90", at::kBe4ReturnItem, at::kBe4ReturnItem, 2, {kU8, kU16}, bh::Answer::kFlag, 0, 0},
    {"0x44A910", at::kBe4MemberName, at::kBe4MemberName, 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    // what the callee reads of a word pushed with a left-over upper half
    {"Menu_DrawBackdrop", bof3::addr::Menu_DrawBackdrop, KeyOf(&::Menu_DrawBackdrop), 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    {"Stat_AddClamped", bof3::addr::Stat_AddClamped, KeyOf(&::Stat_AddClamped), 2, {kAll, kU16}, bh::Answer::kGarbage, 0, 0},
    {"Stat_AddCap999", bof3::addr::Stat_AddCap999, KeyOf(&::Stat_AddCap999), 2, {kAll, kU16}, bh::Answer::kFlag, 0, 0},
    {"BattleWin_DrawPartyStatus", bof3::addr::BattleWin_DrawPartyStatus, KeyOf(&::BattleWin_DrawPartyStatus), 2, {kU16, kU16},
     bh::Answer::kGarbage, 0, 0},
    {"BattleWin_DrawCommandCross", bof3::addr::BattleWin_DrawCommandCross, KeyOf(&::BattleWin_DrawCommandCross), 2, {kU16, kU16},
     bh::Answer::kGarbage, 0, 0},
    {"BattleWin_DrawCommandLabel", bof3::addr::BattleWin_DrawCommandLabel, KeyOf(&::BattleWin_DrawCommandLabel), 1, {kU8},
     bh::Answer::kGarbage, 0, 0},
    {"Menu_DrawIcon", bof3::addr::Menu_DrawIcon, KeyOf(&::Menu_DrawIcon), 6, {kU8, kU16, kU16, kU8, kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"Battle_ClearStatus", bof3::addr::Battle_ClearStatus, KeyOf(&::Battle_ClearStatus), 2, {kU8, kAll}, bh::Answer::kGarbage, 0, 0},
    {"Sprite_AnimFromSet", bof3::addr::Sprite_AnimFromSet, KeyOf(&::Sprite_AnimFromSet), 4, {kU8, kU16, kAll, kAll},
     bh::Answer::kGarbage, 0, 0},
    {"PartySet_Select", bof3::addr::PartySet_Select, KeyOf(&::PartySet_Select), 2, {kU8, kU8}, bh::Answer::kGarbage, 0, 0},
};

// The dispatchers' tables, their counts from the code (the steps' writes of
// the step bytes), not the tool's.
const bh::DataTable kTables[] = {
    {at::kEquipSteps, 3}, {at::kEquipOpenSteps, 3}, {at::kEquipRunSteps, 2}, {at::kEquipLeaveSteps, 2},
    {at::kHoldSteps, 3},  {at::kKind3Steps, 5},     {at::kLossSteps, 5},
};

// The cells beyond the engine frame that the group reads or writes.
const bh::Region kRegions[] = {
    {at::kTallyText, 0x20},       // BATE's sprintf buffer; also where 0x904ABC + 0x904AB4 lands past the battle bytes
    {at::kTallyShown, 0x3C},      // the tally's counters, targets and 0x675EBE..C0
    {at::kWaitWord, 2},           // MoveScript_WaitWordDA
    {0x903B24, 0x46C},            // character records 1..7 past the engine frame's 0x903A50..0x903B24
    {at::kSumA, 0xC},             // 0x939A04..0x939A0F
    {bof3::addr::Text_Records, 0x40},
    {at::kClutA, 0x200},
    {at::kClutB, 0x200},
    {at::kPartySetByte, 1},
    {at::kRandomIds24, 0x40},     // the random picks' id tables (.data): drawn, so every flags branch is reached
};

// ===========================================================================
// Seeds, arguments, disturbance
// ===========================================================================

unsigned char Byte(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<unsigned char>(bh::Next());
    const U* v = often.begin();
    return static_cast<unsigned char>(v[bh::Next() % often.size()]);
}
std::uint16_t Word16(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<std::uint16_t>(bh::Next());
    const U* v = often.begin();
    return static_cast<std::uint16_t>(v[bh::Next() % often.size()]);
}
unsigned char* MemberAt(U i) { return Mem(at::kMembers + i * at::kMemberSize); }

// Input_Pressed against the confirm and cancel words: none, one, the other,
// both, or any.
void SeedButtons() {
    SetWord(Mem(at::kConfirmButtons), Word16({0x20, 0x40, 0x4000, 0x60}));
    SetWord(Mem(at::kCancelButtons), Word16({0x40, 0x10, 0x80, 0x8000}));
    const unsigned confirm = move_script::Word(Mem(at::kConfirmButtons));
    const unsigned cancel = move_script::Word(Mem(at::kCancelButtons));
    switch (bh::Next() % 5) {
    case 0: SetWord(Mem(at::kInputPressed), 0); break;
    case 1: SetWord(Mem(at::kInputPressed), confirm); break;
    case 2: SetWord(Mem(at::kInputPressed), cancel & ~confirm); break;
    case 3: SetWord(Mem(at::kInputPressed), confirm | cancel); break;
    default: break;
    }
}

// The character records' levels and EXP around the table's steps.
void SeedLevels() {
    for (U r = 0; r < 8; ++r) {
        unsigned char* const rec = Mem(at::kCharRecords + r * at::kCharSize);
        rec[0xA] = Byte({0, 1, 2, 10, 40, 97, 98, 99, 100});
        const U exp = bh::Often() ? bh::Next() % (bh::Half() ? 0x400u : 0x100000u) : bh::Next();
        SetLong(rec + 0xC, static_cast<std::int32_t>(exp));
    }
}

void Seed(unsigned k) {
    // every round: the party count inside the three records (the loops index
    // ObjTrio by it; the originals' past 3 is described, not drawn)
    Mem(at::kPartyCount)[0] = static_cast<unsigned char>(bh::Often() ? 1 + bh::Next() % 3 : bh::Next() % 4);
    Mem(at::kTallyRow)[0] = static_cast<unsigned char>(bh::Next() & 0x7F);   // never negative (section 7)
    if (bh::Half()) SetWord(Mem(at::kWaitWord), 0);
    switch (g_base[k].base) {
    case 0x42D7F0: {   // the tally's counter: each row and 6, the step reaching the target or not, a press
        const unsigned row = bh::Often() ? bh::Next() % 7 : bh::Next() & 0x7F;
        Mem(at::kTallyRow)[0] = static_cast<unsigned char>(row);
        if (row < 6 && bh::Often()) {
            const U shown = bh::Half() ? bh::Next() % 1000 : bh::Next();
            SetLong(Mem(at::kTallyShown + 4 * row), static_cast<std::int32_t>(shown));
            const U step = Mem(at::kTallyStep + row)[0];
            SetLong(Mem(at::kTallyTarget + 4 * row), static_cast<std::int32_t>(shown + step + (bh::Next() % 5) - 2));
        }
        Mem(at::kInputPressed)[0] = bh::Half() ? 0 : static_cast<unsigned char>(bh::Next());
        break;
    }
    case 0x42DA60:   // the tally's thresholds
        SetLong(Mem(at::kSumB), static_cast<std::int32_t>(bh::Often() ? BH_PICK(0, 0x31, 0x32, 0x33, 0x4F, 0x50, 0x51) : bh::Next()));
        break;
    case 0x42DC80: case 0x42E050:   // the timers at 1, and around
        Mem(at::kModeTimer)[0] = Byte({1, 1, 2, 0, 0xFF});
        break;
    case 0x42DCF0:
        Mem(at::kWin1 + 0xA)[0] = Byte({0, 3, 1, 2});
        SeedButtons();
        break;
    case 0x42DE50:
        Mem(at::kWin0 + 0xA)[0] = Byte({0, 1, 8, 9, 10, 0x6E, 0x76, 0x77});
        Mem(at::kWin0 + 0xB)[0] = bh::Half() ? static_cast<unsigned char>(Mem(at::kWin0 + 0xA)[0] + BH_PICK(0, 8, 9, 0xFF))
                                             : Byte({0, 1, 0x6E, 0x6F, 0x7E, 0x7F, 0x80});
        Mem(at::kWin0 + 0xD)[0] = bh::Half() ? 0 : static_cast<unsigned char>(bh::Next());
        if (bh::Half()) SetWord(Mem(at::kWin0 + 0x10), 0);
        SeedButtons();
        break;
    case 0x42EDA0: case 0x42EEA0:   // the menu member and its cross size at the ends
        Mem(at::kMenuMember)[0] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 3 : bh::Next());
        Mem(at::kCrossGrow + Mem(at::kMenuMember)[0])[0] = Byte({0, 1, 2, 3, 4, 7, 8, 9, 0xFF});
        break;
    case 0x42EE00: {
        const unsigned held = move_script::Word(Mem(at::kInputHeld));
        SetWord(Mem(at::kInputHeld), bh::Half() ? held | 0x100 : held & ~0x100u);
        break;
    }
    case 0x42EF50: {   // the entries, the first entry's item command, the auto test
        Mem(at::kEntryCount)[0] = Byte({0, 1, 2, 3, 0xFF, 0x80, 5});
        const unsigned char first = Byte({0, 1, 2, 0xFF, 0, 1, 2, 4});
        Mem(at::kEntryFirst)[0] = first;
        if (first <= 2) MemberAt(first)[0x125] = Byte({5, 5, 4});
        Mem(at::kAutoTest)[0] = bh::Half() ? 0 : static_cast<unsigned char>(bh::Next());
        break;
    }
    case 0x42F640:
        if (bh::Half()) SetWord(Mem(at::kEffectBits), 0);
        break;
    case 0x42F9D0:
        SetWord(Mem(at::kActionId), Word16({0x24, 0x25, 0x8C, 0x24, 0x25, 0x8C, 0x23, 0}));
        Mem(at::kActor)[0] = Byte({0, 1, 2, 3, 4, 10});
        break;
    case 0x42FE20: {
        Mem(at::kMessageUp)[0] = Byte({0, 0, 1});
        Mem(at::kMemberAt)[0] = Byte({0, 1, 2, 3, 0xFF});
        for (U i = 0; i < 3; ++i) {
            unsigned char* const m = MemberAt(i) + 0x130;
            m[0] = static_cast<unsigned char>(bh::Half() ? m[0] | 8 : m[0] & ~8);
        }
        Mem(at::kActor)[0] = Byte({3, 4, 5, 10});
        break;
    }
    case 0x4315C0:   // the members' busy bit, the cursor at the count
        for (U i = 0; i < 3; ++i)
            if (bh::Often()) MemberAt(i)[0x131] = static_cast<unsigned char>(MemberAt(i)[0x131] & ~0x20);
        if (bh::Often()) Mem(at::kMemberAt)[0] = Mem(at::kPartyCount)[0];
        break;
    case 0x4319B0: case 0x431FE0:
        for (U i = 0; i < 3; ++i) {
            if (bh::Half()) MemberAt(i)[bh::Half() ? 0x96 : 0x97] = 7;
            if (bh::Half()) MemberAt(i)[0x135] = static_cast<unsigned char>(MemberAt(i)[0x135] & ~4);
        }
        break;
    case 0x431C10:
        if (bh::Often() && move_script::Word(Mem(at::kInputPressed)) == 0) SetWord(Mem(at::kInputPressed), 0x20);
        if (!bh::Often()) SetWord(Mem(at::kInputPressed), 0);
        Mem(at::kRoster)[0] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 8 : bh::Next());
        SeedLevels();
        break;
    case 0x432170:
        SeedLevels();
        break;
    case 0x432630:
        SetWord(Mem(at::kLossBar), Word16({0xF0, 0xF3, 0xF4, 0xF5, 0x2C, 0, 0xFFFF}));
        if (bh::Half()) SetWord(Mem(at::kInputHeld), 0);
        break;
    default:
        break;
    }
}

// The words: the tally window's (x, y) as its callers push them or any; the
// plus sign's any; Char_LevelUpGain's roster and stat, inside the eight
// records and the seven cases most of the time, garbage above the byte half
// the time; the draws' shade and width.
void Args(unsigned k, U* a) {
    const auto garbage = [] { return bh::Half() ? bh::Next() & 0xFFFFFF00u : 0u; };
    switch (g_base[k].base) {
    case 0x42D8C0:
        a[0] = bh::Often() ? 0x37 : bh::Next();
        a[1] = bh::Often() ? 0x37 : bh::Next();
        break;
    case 0x432170:
        a[0] = garbage() | (bh::Often() ? bh::Next() % 8 : bh::Next() & 0xFF);
        a[1] = garbage() | (bh::Often() ? bh::Next() % 7 : bh::Next() & 0xFF);
        break;
    case 0x4327F0: case 0x4329A0:
        a[0] = garbage() | (bh::Often() ? 0x80 : bh::Next() & 0xFF);
        break;
    case 0x432A30:
        a[0] = (bh::Half() ? bh::Next() & 0xFFFF0000u : 0) | (bh::Often() ? 0x2C + bh::Next() % 0xCC : bh::Next() & 0xFFFF);
        break;
    default:
        break;
    }
}

// What the group's functions read again after a call: BATE's step bytes and
// timer, the tally's targets, the backdrop's kind, window 0 and 1's cursor
// bytes and scroll word, Input_Pressed / Input_Held, the result's slot,
// roster and old level, the loss bar, the action word, the party count
// (kept inside the three records).
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 14) {
    case 0: Mem(at::kModeStep)[0] = b; break;
    case 1: Mem(at::kModeSub)[0] = b; break;
    case 2: Mem(at::kTallyTarget + 0xC + (h >> 24) % 3 * 4)[0] = b; break;
    case 3: Mem(at::kMenuShade)[0] = b; break;
    case 4: Mem(at::kWin0 + 0xA + (h >> 24) % 2)[0] = b; break;
    case 5: Mem(at::kWin1 + 0xA)[0] = b; break;
    case 6: SetWord(Mem(at::kWin0 + 0x10), h & 0x1000000 ? 0 : b); break;
    case 7: Mem(at::kInputPressed + (h >> 24) % 2)[0] = b; break;
    case 8: Mem(at::kInputHeld + (h >> 24) % 2)[0] = b; break;
    case 9: Mem(at::kRoster)[0] = static_cast<unsigned char>(b % 8); break;
    case 10: SetWord(Mem(at::kOldLevel), b); break;
    case 11: SetWord(Mem(at::kLossBar), b); break;
    case 12: SetWord(Mem(at::kActionId), h & 0x1000000 ? 0x25 : b); break;
    default: Mem(at::kPartyCount)[0] = static_cast<unsigned char>(b % 4); break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_BE1_ONLY=<hex address>: that clone alone (the controls' shortcut)
    const char* const only = std::getenv("BOF3X_BE1_ONLY");
    const U want = only && *only ? static_cast<U>(std::strtoul(only, nullptr, 16)) : 0;
    const bh::Clone* clones = kClones;
    unsigned n = BH_COUNT(kClones);
    if (want != 0) {
        for (const bh::Clone& c : kClones)
            if (c.base == want) {
                clones = &c;
                n = 1;
            }
        if (n != 1) bof3::Fatal("battle_e1: BOF3X_BE1_ONLY=%s names no clone", only);
    }
    // Seed and Args index kClones by k: with one clone, k is 0 of `clones`
    g_base = clones;
    bh::Group g{"battle_e1", clones, n, kCallees, BH_COUNT(kCallees), kTables, BH_COUNT(kTables), kRegions, BH_COUNT(kRegions),
                &Seed, &Disturb, 6000};
    g.args = &Args;
    g.engine = true;
    bh::Run(g);
}

}  // namespace battle_e1
