// BOF3X_SHADOW=rest_2d: group R2D's 51 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up.
// docs/rest_2d.md section 4. BOF3X_R2D_ONLY=<text> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone rows are tools/band_rows.py --group R2D --clones --harness scenario
// (2026-10-04), each extent read again to its last instruction (capstone); the
// cut's sizes are padding past them. Shapes: the masters' state handlers kState
// (void, no arguments, reading no sprite), the field menu's kMenu (the menu
// block's state / step seeded per dispatcher below its own table's count), the
// helpers kCall with their arguments set by Args. The five .data state tables
// the dispatchers read are DataTables (recorders while the fuzz runs);
// FieldAbility_Effects' ten handlers take (caster, target, battle), so the
// table holds typed stand-ins of this file's while the fuzz runs (a handler
// recorder logs no arguments) and is put back after. Every callee is re-listed
// here with the width the callee reads (the originals push whole registers
// whose upper bytes ours cannot hold) and an answer around what the group's
// code tests.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2d.h"
#include "game/rest_2d_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_2d {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char& B(U a) { return sh::Mem(a)[0]; }

// --- the clone table (band_rows.py --group R2D --clones --harness scenario, 2026-10-04) ---
constexpr sh::CallSite kCalls5869A0[] = {{0xC, 0x461EB0}, {0x75, 0x587740}, {0xAA, 0x587740}, {0xB4, 0x587740}, {0x11F, 0x587740},
                                         {0x14C, 0x585DC0}, {0x15B, 0x585BE0}, {0x197, 0x586B90}, {0x1AD, 0x586160}, {0x1D8, 0x516B30}};
constexpr sh::CallSite kCalls586B90[] = {{0x62, 0x5A7670}, {0xF1, 0x461E50}, {0xFD, 0x5A7690}, {0x156, 0x461E50}};
constexpr sh::CallSite kCalls586D00[] = {{0x14, 0x586D20}};
constexpr sh::CallSite kCalls586D20[] = {{0xC, 0x461EB0}, {0x3B, 0x587740}, {0x54, 0x587740}, {0x81, 0x587740}, {0x95, 0x587740},
                                         {0xCD, 0x585DC0}, {0xDB, 0x585BE0}, {0x117, 0x586B90}, {0x12D, 0x586160}, {0x158, 0x516B30},
                                         {0x177, 0x5905D0}};
constexpr sh::CallSite kCalls586EB0[] = {{0x1D, 0x4976D0}, {0x13D, 0x4976D0}};
constexpr sh::CallSite kCalls587010[] = {{0x1D, 0x4976D0}};
constexpr sh::CallSite kCalls587050[] = {{0x64, 0x4976D0}};
constexpr sh::CallSite kCalls5870E0[] = {{0x1D, 0x4976D0}};
constexpr sh::CallSite kCalls587130[] = {{0x16, 0x5869A0}};
constexpr sh::CallSite kCalls587150[] = {{0x14, 0x586D20}};
constexpr sh::CallSite kCalls587170[] = {{0x20, 0x4976D0}, {0xC9, 0x4976D0}};
constexpr sh::CallSite kCalls587260[] = {{0x1D, 0x4976D0}};
constexpr sh::CallSite kCalls5872A0[] = {{0x64, 0x4976D0}};
constexpr sh::CallSite kCalls587330[] = {{0x1D, 0x4976D0}};
constexpr sh::CallSite kCalls587380[] = {{0x14, 0x4976D0}};
constexpr sh::CallSite kCalls5873C0[] = {{0x90, 0x587680}, {0xAD, 0x57C140}, {0xC5, 0x590BB0}, {0xDC, 0x57C0F0}, {0xF9, 0x587680},
                                         {0x12A, 0x587680}, {0x15B, 0x587680}, {0x1C8, 0x590C90}, {0x202, 0x587680}, {0x284, 0x4976D0}};
constexpr sh::JumpTable kTables5873C0[] = {{0x68, 0x2A4, 4}};
constexpr sh::CallSite kCalls589E00[] = {{0x6, 0x575690}};
constexpr sh::CallSite kCalls589FB0[] = {{0xF, 0x536700}};
constexpr sh::CallSite kCalls58A0F0[] = {{0x3E, 0x590CE0}};
constexpr sh::CallSite kCalls58A140[] = {{0x3E, 0x590CE0}};
constexpr sh::CallSite kCalls58A190[] = {{0xC, 0x590CE0}};
constexpr sh::CallSite kCalls58A1B0[] = {{0x11, 0x531BB0}, {0x72, 0x590CE0}, {0x89, 0x531BB0}};
constexpr sh::CallSite kCalls58A260[] = {{0x11, 0x531BB0}, {0x75, 0x590CE0}, {0x8C, 0x531BB0}};
constexpr sh::CallSite kCalls58A310[] = {{0xF, 0x590F60}};
constexpr sh::CallSite kCalls58A350[] = {{0xF, 0x590F60}};
constexpr sh::CallSite kCalls58A380[] = {{0xF, 0x590CE0}, {0x1D, 0x590F60}};
constexpr sh::CallSite kCalls58A3C0[] = {{0x42, 0x591DB0}, {0xDE, 0x591DB0}};
constexpr sh::CallSite kCalls58A4D0[] = {{0x6, 0x575690}, {0xB, 0x58A850}, {0x31, 0x587740}};
constexpr sh::CallSite kCalls58A510[] = {{0x29, 0x591940}, {0x3E, 0x575690}};
constexpr sh::CallSite kCalls58A570[] = {{0xA, 0x575690}, {0x60, 0x591940}, {0x7C, 0x461EB0}, {0x85, 0x531BB0}, {0xF1, 0x587740},
                                         {0x10D, 0x587740}, {0x117, 0x587740}, {0x11F, 0x58A920}, {0x153, 0x587740}, {0x15D, 0x587740},
                                         {0x165, 0x531BB0}, {0x180, 0x531BB0}};
constexpr sh::CallSite kCalls58A730[] = {{0x6, 0x575690}, {0x34, 0x591940}, {0x63, 0x587740}, {0x6D, 0x587740}, {0x75, 0x58AA30}};
constexpr sh::CallSite kCalls58A7D0[] = {{0x6, 0x575690}, {0x1D, 0x58AAB0}, {0x25, 0x531BB0}, {0x40, 0x531BB0}};
constexpr sh::CallSite kCalls58A850[] = {{0xA, 0x531BB0}, {0x66, 0x531BB0}};
constexpr sh::CallSite kCalls58A920[] = {{0x9, 0x531BB0}, {0x5C, 0x531BB0}};
constexpr sh::CallSite kCalls58AA30[] = {{0x8, 0x531BB0}, {0x5C, 0x531BB0}};
constexpr sh::CallSite kCalls58AAF0[] = {{0x6, 0x575690}, {0xB, 0x58BC30}, {0x15, 0x587740}};
constexpr sh::CallSite kCalls58AB30[] = {{0x6, 0x575690}};
constexpr sh::CallSite kCalls58AB60[] = {{0x7, 0x575690}, {0x6D, 0x461EB0}, {0xB7, 0x587740}, {0xE3, 0x587740}, {0x180, 0x587740},
                                         {0x18A, 0x587740}};
constexpr sh::CallSite kCalls58AD30[] = {{0x9, 0x575690}, {0x44, 0x591C20}, {0x9C, 0x461EB0}, {0xAE, 0x587740}, {0x127, 0x587740},
                                         {0x25E, 0x587740}, {0x2B7, 0x587740}, {0x2DF, 0x57D9A0}, {0x309, 0x497680}, {0x31E, 0x587740},
                                         {0x32F, 0x587740}, {0x39E, 0x57D9A0}, {0x3DD, 0x587740}};
constexpr sh::CallSite kCalls58B130[] = {{0x7, 0x575690}, {0x1E, 0x58C2A0}, {0x2B, 0x531BB0}, {0x53, 0x531BB0}};

#define R2D_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R2D_ROW(name, base, size, calls) #name, base, size, calls, R2D_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
#define R2D_LEAF(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kSt = sh::Shape::kState, kMe = sh::Shape::kMenu, kCa = sh::Shape::kCall;
constexpr U kAl = 0xFFu, kEax = 0xFFFFFFFFu;
const sh::Clone kAll[] = {
    {R2D_ROW(MasterScreen_PickMember, 0x5869A0, 0x1E2, kCalls5869A0), 0, false, kCa},
    {R2D_ROW(MasterScreen_DrawCursorFrame, 0x586B90, 0x163, kCalls586B90), 0, false, kCa},
    {R2D_ROW(MasterJoin_Step3Ask, 0x586D00, 0x1B, kCalls586D00), 0, false, kSt},
    {R2D_ROW(MasterScreen_AskYesNo, 0x586D20, 0x181, kCalls586D20), 0, false, kCa},
    {R2D_ROW(MasterJoin_Step5Apply, 0x586EB0, 0x159, kCalls586EB0), 0, false, kSt},
    {R2D_ROW(MasterJoin_Step6Told, 0x587010, 0x39, kCalls587010), 0, false, kSt},
    {R2D_ROW(MasterJoin_Step7AllCheck, 0x587050, 0x81, kCalls587050), 0, false, kSt},
    {R2D_ROW(MasterJoin_Step8Close, 0x5870E0, 0x34, kCalls5870E0), 0, false, kSt},
    {R2D_LEAF(MasterQuit_ByStep, 0x587120, 0xE), 0, false, kSt},
    {R2D_ROW(MasterQuit_Step2Pick, 0x587130, 0x1F, kCalls587130), 0, false, kSt},
    {R2D_ROW(MasterQuit_Step3Ask, 0x587150, 0x1B, kCalls587150), 0, false, kSt},
    {R2D_ROW(MasterQuit_Step5Apply, 0x587170, 0xE6, kCalls587170), 0, false, kSt},
    {R2D_ROW(MasterQuit_Step6Told, 0x587260, 0x39, kCalls587260), 0, false, kSt},
    {R2D_ROW(MasterQuit_Step7NoneLeftCheck, 0x5872A0, 0x81, kCalls5872A0), 0, false, kSt},
    {R2D_ROW(MasterQuit_Step8Close, 0x587330, 0x34, kCalls587330), 0, false, kSt},
    {R2D_LEAF(MasterGrant_ByStep, 0x587370, 0xE), 0, false, kSt},
    {R2D_ROW(MasterGrant_Step0Open, 0x587380, 0x37, kCalls587380), 0, false, kSt},
    {"MasterGrant_Step1Member", 0x5873C0, 0x2B4, kCalls5873C0, R2D_N(kCalls5873C0), nullptr, 0, kTables5873C0, R2D_N(kTables5873C0),
     reinterpret_cast<const void*>(&::MasterGrant_Step1Member), 0, false, kSt},
    {R2D_LEAF(MasterScreen_NameToText, 0x587680, 0x35), 0, false, kCa},
    {R2D_LEAF(MasterGrant_Step2Next, 0x5876C0, 0x2F), 0, false, kSt},
    {R2D_LEAF(MasterScreen_State6Leave, 0x5876F0, 0x43), 0, false, kSt},
    {R2D_ROW(FieldMenu_TopBarCountdown, 0x589E00, 0x44, kCalls589E00), 0, false, kMe},
    {R2D_ROW(FieldMenu_CampAllowedCell, 0x589FB0, 0x2F, kCalls589FB0), kAl, false, kCa},
    {R2D_LEAF(FieldAbility_NotHere, 0x58A0E0, 0x3), kAl, false, kCa},
    {R2D_ROW(FieldAbility_HealOne20, 0x58A0F0, 0x50, kCalls58A0F0), kEax, false, kCa},
    {R2D_ROW(FieldAbility_HealOne40, 0x58A140, 0x50, kCalls58A140), kEax, false, kCa},
    {R2D_ROW(FieldAbility_HealOneFull, 0x58A190, 0x1E, kCalls58A190), kEax, false, kCa},
    {R2D_ROW(FieldAbility_HealAll40, 0x58A1B0, 0xA9, kCalls58A1B0), kEax, false, kCa},
    {R2D_ROW(FieldAbility_HealAll120, 0x58A260, 0xAC, kCalls58A260), kEax, false, kCa},
    {R2D_ROW(FieldAbility_Clear80, 0x58A310, 0x21, kCalls58A310), kEax, false, kCa},
    {R2D_LEAF(FieldAbility_NoEffect, 0x58A340, 0x3), kAl, false, kCa},
    {R2D_ROW(FieldAbility_ClearA0, 0x58A350, 0x21, kCalls58A350), kEax, false, kCa},
    {R2D_ROW(FieldAbility_HealFullClearA0, 0x58A380, 0x34, kCalls58A380), kEax, false, kCa},
    {R2D_ROW(FieldAbility_Use, 0x58A3C0, 0xF7, kCalls58A3C0), kAl, false, kCa},
    {R2D_LEAF(FieldMenuStatus_ByState, 0x58A4C0, 0xE), 0, false, kMe},
    {R2D_ROW(FieldMenuStatus_Open, 0x58A4D0, 0x3A, kCalls58A4D0), 0, false, kMe},
    {R2D_ROW(FieldMenuStatus_SlideIn, 0x58A510, 0x5F, kCalls58A510), 0, false, kMe},
    {R2D_ROW(FieldMenuStatus_Choose, 0x58A570, 0x1B8, kCalls58A570), 0, false, kMe},
    {R2D_ROW(FieldMenuStatus_Detail, 0x58A730, 0x95, kCalls58A730), 0, false, kMe},
    {R2D_ROW(FieldMenuStatus_Close, 0x58A7D0, 0x7F, kCalls58A7D0), 0, false, kMe},
    {R2D_ROW(FieldMenuStatus_PlaceWindows, 0x58A850, 0xCF, kCalls58A850), 0, false, kCa},
    {R2D_ROW(FieldMenuStatus_DetailWindows, 0x58A920, 0x109, kCalls58A920), 0, false, kCa},
    {R2D_ROW(FieldMenuStatus_ListWindows, 0x58AA30, 0x80, kCalls58AA30), 0, false, kCa},
    {R2D_LEAF(FieldMenuStatus_ClearWindows, 0x58AAB0, 0x21), 0, false, kCa},
    {R2D_LEAF(FieldMenuItems_ByState, 0x58AAE0, 0xE), 0, false, kMe},
    {R2D_ROW(FieldMenuItems_Open, 0x58AAF0, 0x38, kCalls58AAF0), 0, false, kMe},
    {R2D_ROW(FieldMenuItems_SlideIn, 0x58AB30, 0x30, kCalls58AB30), 0, false, kMe},
    {R2D_ROW(FieldMenuItems_Category, 0x58AB60, 0x1C8, kCalls58AB60), 0, false, kMe},
    {R2D_ROW(FieldMenuItems_List, 0x58AD30, 0x3F5, kCalls58AD30), 0, false, kMe},
    {R2D_ROW(FieldMenuItems_Close, 0x58B130, 0x8D, kCalls58B130), 0, false, kMe},
    {R2D_LEAF(FieldMenuItems_State5ByStep, 0x58B1C0, 0xE), 0, false, kMe},
};
#undef R2D_ROW
#undef R2D_LEAF
#undef R2D_N
constexpr unsigned kCount = sizeof kAll / sizeof kAll[0];
static_assert(kCount == 51, "the cut's 51 rows for R2D");

// The five state tables, each to its reader's count (docs/rest_2d.md section 3).
const sh::DataTable kTables[] = {
    {at::kQuitSteps, at::kQuitStepCount},     {at::kGrantSteps, at::kGrantStepCount},
    {at::kStatusStates, at::kStatusStateCount}, {at::kItemsStates, at::kItemsStateCount},
    {at::kItemsState5Steps, at::kItemsState5StepCount},
};

// --- FieldAbility_Effects' typed stand-ins ------------------------------------------------
//
// FieldAbility_Use calls the table's entry with (caster, target, battle), all
// three whole as the caller holds them; the stand-in logs them under the
// handler's own address and answers an al FieldAbility_Use tests (1 and 5 take
// the cost, the rest do not) under garbage.
constexpr U kEffectAddress[at::kAbilityEffectCount] = {0x58A0E0, 0x58A0F0, 0x58A140, 0x58A190, 0x58A1B0,
                                                       0x58A260, 0x58A310, 0x58A340, 0x58A350, 0x58A380};
template <int I> unsigned __cdecl EffectEntry(unsigned caster, unsigned target, unsigned battle) {
    sh::Record(kEffectAddress[I], caster, target, battle);
    sh::Stir();
    const U n = sh::Noise();
    static const U kAnswers[] = {1, 5, 4, 3, 0, 2, 0x81, 0x41};
    return (n & 0xFFFFFF00u) | (n % 5 == 0 ? (n >> 8) & 0xFF : kAnswers[(n >> 3) % 8]);
}
using EffectFn = unsigned (__cdecl*)(unsigned, unsigned, unsigned);
const EffectFn kEffectEntries[at::kAbilityEffectCount] = {&EffectEntry<0>, &EffectEntry<1>, &EffectEntry<2>, &EffectEntry<3>,
                                                          &EffectEntry<4>, &EffectEntry<5>, &EffectEntry<6>, &EffectEntry<7>,
                                                          &EffectEntry<8>, &EffectEntry<9>};

// --- the stand-ins' answers (Noise() and the state only: both passes the same) ----------

U WithAl(U answer, U al) { return (answer & 0xFFFFFF00u) | (al & 0xFF); }

// AreaMap_ByteAt: the leader's cell's codes the camp check compares (each high
// nibble's neighbours too), else any byte.
U FxCell(const U*, U answer) {
    static const U kCodes[] = {0xA0, 0xA1, 0xAF, 0x91, 0xA5, 0x90, 0x9F, 0xB0, 0x00, 0xFF};
    const U n = sh::Noise();
    return WithAl(answer, n % 6 == 0 ? n >> 8 : kCodes[(n >> 3) % 10]);
}
// Input_AutoRepeat: none of the bits a third of the time, else one or two of
// those the screens test (0x8000, 0x4000, 0x2000, 0x1000, 8, 4), else any.
U FxRepeat(const U*, U answer) {
    static const U kBits[] = {0x8000, 0x4000, 0x2000, 0x1000, 8, 4};
    const U n = sh::Noise();
    if (n % 3 == 0) return answer & 0xFFFF0000u;
    if (n % 3 == 1) return (answer & 0xFFFF0000u) | kBits[(n >> 4) % 6] | (n & 0x100 ? kBits[(n >> 9) % 6] : 0);
    return answer;
}
// Skill_ApCost: a cost around the AP words the seed puts in (0..0x30).
U FxCost(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 6 == 0 ? n >> 8 : (n >> 4) % 0x34);
}
// ItemUse_Dispatch: 0 and 4 (the item used) against the others.
U FxItemUse(const U*, U answer) {
    const U n = sh::Noise();
    static const U kAnswers[] = {0, 4, 1, 2, 3, 5};
    return WithAl(answer, n % 5 == 0 ? n >> 8 : kAnswers[(n >> 3) % 6]);
}

#define R2D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
const sh::Callee kCallees[] = {
    // the engine's (ours), with the width each reads
    {R2D_OURS(Input_AutoRepeat), 1, {kW}, kG, 0, 0, {}, &FxRepeat},        // the pressed bits, masked by the caller
    {R2D_OURS(Sound_PlayEffect), 1, {kU16}, kG, 0, 0},
    {R2D_OURS(Msg_OpenScript), 1, {kU16}, kG, 0, 0},                       // `mov cx, word`: the upper half the caller's
    {R2D_OURS(Text_DrawAt), 5, {kU16, kU16, kU8, kU8, kW}, kG, 0, 0},
    {R2D_OURS(Menu_DrawHand), 3, {kU16, kU16, kW}, kG, 0, 0},             // x's upper half Text_DrawAt's answer's (0x590601 keeps 16)
    {R2D_OURS(Menu_DrawBackdrop), 1, {kU8}, kG, 0, 0},                     // `mov al, byte`
    {R2D_OURS(Gpu_SetLineF3), 1, {kW}, kG, 0, 0},
    {R2D_OURS(Gpu_SetLineF4), 1, {kW}, kG, 0, 0},
    {R2D_OURS(Inventory_Add), 3, {kU8, kU8, kU8}, kF, 0, 0},
    {R2D_OURS(AbilityList_Add), 4, {kU8, kU8, kU8, kU8}, kF, 0, 0},        // id and member bytes; the originals' upper bytes are 0 in play
    {R2D_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, kG, 0, 0, {}, &FxCell},    // (short) x, z
    {R2D_OURS(Char_HealHp), 3, {kU8, kW, kU8}, kF, 0, 0},                  // id & 0xFF, battle's low byte
    {R2D_OURS(Char_ClearStatus), 3, {kU8, kW, kU8}, kF, 0, 0},
    {R2D_OURS(Party_Count), 1, {kU8}, sh::Answer::kByte, 0, 3},
    {R2D_OURS(Skill_ApCost), 3, {kU8, kU8, kU8}, kG, 0, 0, {}, &FxCost},
    {R2D_OURS(Item_HelpMessage), 2, {kU8, kU8}, kG, 0, 0},
    {R2D_OURS(Item_CanUse), 4, {kU8, kW, kU8, kU8}, kF, 0, 0},
    {R2D_OURS(ItemUse_Dispatch), 3, {kW, kU8, kW}, kG, 0, 0, {}, &FxItemUse},
    // the group's own, called by E8
    {R2D_OURS(MasterScreen_PickMember), 2, {kU16, kU8}, kG, 0, 0},
    {R2D_OURS(MasterScreen_DrawCursorFrame), 6, {kU16, kU16, kU16, kU16, kU8, kU8}, kG, 0, 0},
    {R2D_OURS(MasterScreen_AskYesNo), 1, {kU16}, kG, 0, 0},
    {R2D_OURS(MasterScreen_NameToText), 1, {kU8}, kG, 0, 0},
    {R2D_OURS(FieldMenuStatus_PlaceWindows), 0, {}, kG, 0, 0},
    {R2D_OURS(FieldMenuStatus_DetailWindows), 0, {}, kG, 0, 0},
    {R2D_OURS(FieldMenuStatus_ListWindows), 0, {}, kG, 0, 0},
    {R2D_OURS(FieldMenuStatus_ClearWindows), 0, {}, kG, 0, 0},
    // R2C's and R2E's, ours, keyed by address (docs/rest_2d.md section 6)
    {"0x585DC0", at::kMemberPanel, at::kMemberPanel, 4, {kW, kW, kU8, kW}, kG, 0, 0},   // `and eax, 0xFF` on the member
    {"0x585BE0", at::kMemberLabel, at::kMemberLabel, 3, {kW, kW, kU8}, kG, 0, 0},
    {"0x586160", at::kPromptBox, at::kPromptBox, 5, {kW, kW, kW, kW, kU8}, kG, 0, 0},     // the style byte under garbage
    {"0x58BC30", at::kItemsWindows, at::kItemsWindows, 0, {}, kG, 0, 0},
    {"0x58C2A0", at::kItemsReset, at::kItemsReset, 0, {}, kG, 0, 0},
    // FieldAbility_Effects' entries: the typed stand-ins' log slots, keyed on each
    // handler's address (no clone calls one by E8; the table holds the stand-in)
#define R2D_EFFECT(i, name) {"FieldAbility_Effects[" #i "] " #name, kEffectAddress[i], kEffectAddress[i], 3, {kW, kW, kW}, kG, 0, 0, \
                              {}, nullptr, reinterpret_cast<const void*>(kEffectEntries[i])}
    R2D_EFFECT(0, NotHere), R2D_EFFECT(1, HealOne20), R2D_EFFECT(2, HealOne40), R2D_EFFECT(3, HealOneFull),
    R2D_EFFECT(4, HealAll40), R2D_EFFECT(5, HealAll120), R2D_EFFECT(6, Clear80), R2D_EFFECT(7, NoEffect),
    R2D_EFFECT(8, ClearA0), R2D_EFFECT(9, HealFullClearA0),
#undef R2D_EFFECT
};
#undef R2D_OURS

// Beyond field mode's standard regions (which hold the menu block, the style
// cells and CharacterRecords' first 0x24 bytes, Cond_Flags with the party list,
// the master bits and the ability bits, the save block's head and bytes,
// ObjTrio with Field_Members, Sprite_Objects, the pad, Field_Request,
// Frame_Counter, the counter byte 0x90384B and the packet cursor).
const sh::Region kRegions[] = {
    {at::kWindows, at::kWindowCount * at::kWindowStride},   // WindowRecords 0x803160..0x803477
    {0x903A94, 0x903F90 - 0x903A94},                        // CharacterRecords past the style region
    {0x939880, 0x60},                                       // the per-category picks / tops, the masters' cells
    {bof3::addr::Text_Records, 0x30},                       // the name and the ability's record
    {0x9039F0, 8},                                          // the field mode tail's step and argument (the master)
    {at::kItemsCategory, 4},
    {0x904160, 0x400},                                      // the inventory's id and count lists, to 0x904560
    {0x903584, 0x10},                                       // Field_ConfirmButtons / Field_CancelButtons
};

// --- the seed ----------------------------------------------------------------------------

unsigned g_clone;   // the round's clone's index in kAll (set by Seed, read by Disturb)

U BaseOf(unsigned k) { return kAll[k].base; }

void SeedButtons() {
    const U confirm = 1u << (sh::Next() % 16);
    U cancel = 1u << (sh::Next() % 16);
    if (cancel == confirm) cancel = confirm == 0x8000 ? 1 : confirm << 1;
    SetWord(sh::Mem(0x90358E), confirm | (sh::Half() ? 0 : 1u << (sh::Next() % 16)));
    SetWord(sh::Mem(0x903590), cancel);
    const U other = sh::Next() & 0xFFFF & ~(confirm | cancel);
    SetWord(sh::Mem(0x7E1BEC), PickOf(confirm, cancel, confirm | cancel, 0, other, confirm | other, cancel | other));
}

// The masters' records: the master byte +0x1F this master's or not, the level
// +0xA a few above or below the joining level +0x88.
void SeedRecords(unsigned char master) {
    for (unsigned r = 0; r < 8; ++r) {
        unsigned char* const rec = sh::Mem(at::kRecords + r * at::kRecordStride);
        rec[0x1F] = static_cast<unsigned char>(PickOf(master, master, 0xFF, master ^ 1, sh::Next()));
        rec[0x88] = static_cast<unsigned char>(PickOf(1, 10, 30, 99, sh::Next()));
        rec[0xA] = static_cast<unsigned char>(rec[0x88] + PickOf(0, 1, 2, 3, 4, 9, 0xFF, sh::Next()));
        SetWord(rec + 0x1A, PickOf(0, 1, 0x10, 0x20, 0x30, sh::Next() % 0x40, sh::Next()));
        SetWord(rec + 0x2A, PickOf(0, 1, 50, 99, 255, 999, 0xFFFF, sh::Next()));
    }
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const obj = ObjTrio + i * at::kObjStride;
        obj[0x148] = static_cast<unsigned char>(sh::Next() % 8);   // Field_Members: a CharacterRecords index
        SetWord(obj + 0x80 + 0x1A, PickOf(0, 1, 0x10, 0x20, 0x30, sh::Next()));
    }
}

void SeedMaster() {
    const auto master = static_cast<unsigned char>(PickOf(0xB, 0xC, 0xD, 0xE, 3, 5, 9, sh::Next() % 0x14, sh::Next()));
    B(at::kMaster) = master;
    SeedRecords(master);
    B(at::kCursor) = static_cast<unsigned char>(sh::Next() % 3);
    B(at::kAnswer) = static_cast<unsigned char>(PickOf(0, 1, 0, 1, sh::Next()));
    B(at::kStep) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 8, sh::Next()));
    B(at::kState) = static_cast<unsigned char>(PickOf(0, 1, 2, 5, 6, sh::Next()));
    Field_MemberCount = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 3, 3));
    Field_Request = static_cast<unsigned char>(PickOf(2, 0, 0, 1, 5, sh::Next()));
    B(at::kMasterBits) = static_cast<unsigned char>(PickOf(0, 8, 0x10, 0x20, 0x38, sh::Next()));
    B(at::kScriptObject) = static_cast<unsigned char>(sh::Next() % 30);
    // the ability bits: none, all, or random words
    for (unsigned i = 0; i < 8; ++i) SetLong(sh::Mem(at::kAbilityBits + 4 * i), static_cast<std::int32_t>(PickOf(0, 0, kW, sh::Next())));
    Frame_Counter = PickOf(0, 6, 8, 0xE, 0xF0, sh::Next());
}

void SeedMenu() {
    B(at::kMenuTimer) = static_cast<unsigned char>(PickOf(0, 1, 1, 2, 5, 8, sh::Next()));
    B(at::kTopCursor) = static_cast<unsigned char>(PickOf(0, 1, 4, 6, sh::Next()));
    B(at::kStatusCursor) = static_cast<unsigned char>(PickOf(0, 1, 2, 0xFF, 3, 0x80, sh::Next() % 3));
    for (unsigned i = 0; i < 3; ++i) B(at::kPartyList + i) = static_cast<unsigned char>(PickOf(0, 1, 2, 5, 0xFF, sh::Next() % 24));
    B(at::kItemsCategory) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0xFF, 4, sh::Next()));
    // window 13: its list (category) 0..3, pick, top, the scroll words
    B(0x80333E) = static_cast<unsigned char>(sh::Next() % 4);
    B(0x803340) = static_cast<unsigned char>(PickOf(0, 1, 8, 9, 0x10, 0x7E, 0x7F, sh::Next() & 0x7F, sh::Next()));
    B(0x80333F) = static_cast<unsigned char>(PickOf(0, 1, 8, 9, 0x6E, 0x6F, 0x70, 0x77, B(0x803340), B(0x803340) + 1u, sh::Next() & 0x7F));
    SetWord(sh::Mem(0x803346), PickOf(0, 0, 0, 0x10, sh::Next()));
    B(0x80333C) = static_cast<unsigned char>(PickOf(1, 3, sh::Next()));
    for (unsigned c = 0; c < 5; ++c) {
        B(at::kItemsPicks + c) = static_cast<unsigned char>(sh::Next() & 0x7F);
        B(at::kItemsTops + c) = static_cast<unsigned char>(sh::Next() & 0x7F);
    }
    // the inventory: empty slots, counts 1 and 2
    for (unsigned i = 0; i < 0x400; ++i) {
        const U a = 0x904160 + i;
        if (a < 0x904354 || a >= 0x904554) {
            if (sh::Next() % 4 == 0) B(a) = 0;
        } else {
            B(a) = static_cast<unsigned char>(PickOf(1, 1, 2, 0, sh::Next()));
        }
    }
}

void Seed(unsigned k) {
    g_clone = k;
    SeedButtons();
    SeedMaster();
    SeedMenu();
    const U base = BaseOf(k);
    switch (base) {
    case 0x587120: B(at::kStep) = static_cast<unsigned char>(sh::Next() % at::kQuitStepCount); break;
    case 0x587370: B(at::kStep) = static_cast<unsigned char>(sh::Next() % at::kGrantStepCount); break;
    case 0x58A4C0: B(at::kMenuState) = static_cast<unsigned char>(sh::Next() % at::kStatusStateCount); break;
    case 0x58AAE0: B(at::kMenuState) = static_cast<unsigned char>(sh::Next() % at::kItemsStateCount); break;
    case 0x58B1C0: B(at::kMenuStep) = static_cast<unsigned char>(sh::Next() % at::kItemsState5StepCount); break;
    case 0x5873C0:   // a member gaining: the cursor's record this master's half the time, around g = 3
        if (sh::Half()) {
            unsigned char* const rec = sh::Mem(at::kRecords + ObjTrio[B(at::kCursor) * at::kObjStride + 0x148] * at::kRecordStride);
            rec[0x1F] = B(at::kMaster);
            rec[0xA] = static_cast<unsigned char>(rec[0x88] + PickOf(2, 3, 4, 6, 9, 0x80));
            if (sh::Half()) Field_Request = 0;
        }
        break;
    case 0x58AD30:   // the list: half the time a confirm with the scroll still, on a consumable of the first list
        if (sh::Half()) {
            SetWord(sh::Mem(0x803346), 0);
            SetWord(sh::Mem(0x7E1BEC), Word(sh::Mem(0x90358E)));
            if (sh::Half()) B(0x80333E) = 0;
            const U list = static_cast<U>(Long(sh::Mem(at::kItemIdLists + 4u * B(0x80333E))));
            B(list + B(0x803340)) = static_cast<unsigned char>(1 + sh::Next() % 92);
            B(at::kItemsCategory) = static_cast<unsigned char>(PickOf(0, 0, 1, 3));
        }
        break;
    case 0x589FB0: {   // the leader's position words
        SetWord(ObjTrio + 0x36, PickOf(0, 1, 0x7F, 0x8000, sh::Next()));
        SetWord(ObjTrio + 0x3A, PickOf(0, 1, 0x7F, 0x8000, sh::Next()));
        break;
    }
    default: break;
    }
}

// The helpers' arguments.
void Args(unsigned k, U* a) {
    const U hi = a[9] & 0xFFFFFF00u;
    switch (BaseOf(k)) {
    case 0x5869A0: a[1] = PickOf(0, 1, hi, hi | 1, a[1]); break;                         // the flag byte
    case 0x586B90:
        for (unsigned i = 0; i < 4; ++i)
            if (sh::Half()) a[i] = (a[i] & 0xFFFF0000u) | PickOf(0, 1, 4, 0x11, 0x110, 0x34, 0xFFFF, 0x7FFF, 0x8000);
        a[4] = PickOf(0, 1, hi, hi | 1, a[4]);
        break;
    case 0x587680: a[0] = (a[0] & 0xFFFFFF00u) | (sh::Next() % 8); break;               // a record index
    case 0x58A3C0: {
        const U battle = PickOf(0, 0, hi, 1, hi | 1, a[3]);
        a[3] = battle;
        const unsigned limit = (battle & 0xFF) ? 3 : 8;
        a[0] = PickOf(0, 0, hi, a[0] & 0xFFFFFF00u) | (sh::Next() % limit);
        // each byte the mapping tests, its neighbours, and the two ranges whole
        a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0x45, 0x4C, 0xAD, 0xB4, 0x50, 0x73, 0xD7, 0xA8, 0x4F, 0xA9, 0, sh::Next() % 0x100,
                                             0x46 + sh::Next() % 6, 0x46 + sh::Next() % 6, 0xAE + sh::Next() % 6,
                                             0xAE + sh::Next() % 6);
        break;
    }
    case 0x58A0F0: case 0x58A140: case 0x58A190: case 0x58A1B0: case 0x58A260:
    case 0x58A310: case 0x58A350: case 0x58A380:
        a[0] = (a[0] & 0xFFFFFF00u) | (sh::Next() % 8);
        a[2] = PickOf(0, 1, hi, a[2]);
        break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only), kept inside what they index with
// them: the masters' step, answer, cursor (0..2), state, the master, the
// records' master bytes; Field_Request; the member count (0..3); the menu's
// timer, Status cursor, the Items category row (0..3) and window 13's list
// (0..3), pick, top and scroll word.
void Disturb(U h) {
    const U v = h >> 8;
    switch (h % 16) {
    case 0: B(at::kStep) = static_cast<unsigned char>(v); break;
    case 1: B(at::kAnswer) = static_cast<unsigned char>(v & 1 ? v >> 1 : (v >> 1) & 1); break;
    case 2: B(at::kCursor) = static_cast<unsigned char>(v % 3); break;
    case 3: B(at::kMaster) = static_cast<unsigned char>(v & 1 ? 0xB + (v >> 1) % 4 : v >> 1); break;
    case 4: sh::Mem(at::kRecords + ((v >> 2) % 8) * at::kRecordStride)[0x1F] = static_cast<unsigned char>(v & 1 ? B(at::kMaster) : v >> 2); break;
    case 5: Field_Request = static_cast<unsigned char>(v & 1 ? 2 : v >> 1); break;
    case 6: Field_MemberCount = static_cast<unsigned char>(v % 4); break;
    case 7: B(at::kMenuTimer) = static_cast<unsigned char>(v % 3 == 0 ? 0 : v); break;
    case 8: B(at::kStatusCursor) = static_cast<unsigned char>(v & 1 ? (v >> 1) % 3 : v >> 1); break;
    case 9: B(at::kItemsCategory) = static_cast<unsigned char>(v & 1 ? (v >> 1) % 4 : v >> 1); break;
    case 10: B(0x80333E) = static_cast<unsigned char>(v % 4); break;
    case 11: B(0x803340) = static_cast<unsigned char>(v); break;
    case 12: B(0x80333F) = static_cast<unsigned char>(v & 0x7F); break;
    case 13: SetWord(sh::Mem(0x803346), v & 1 ? 0 : v >> 1); break;
    case 14: B(at::kState) = static_cast<unsigned char>(v); break;
    case 15: B(at::kMasterBits) = static_cast<unsigned char>(B(at::kMasterBits) ^ (8u << (v % 3))); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // DIV-0027's two sites inside 0x586D20 are re-aimed by YesNoLayout_Inject
    // under a Latin overlay before this runs: the copy would carry them, so the
    // prompt's own clone runs only with them as Capcom left them (the headless
    // self-test's default); its callers' clones reach a recorder either way.
    auto reaches = [](U site) {
        const unsigned char* const p = sh::Mem(site);
        return site + 5 + static_cast<U>(Long(p + 1));
    };
    const bool patched = reaches(0x586E78) != bof3::addr::Text_DrawAt || reaches(0x586E97) != bof3::addr::Menu_DrawHand;
    if (patched) bof3::Log("shadow      rest_2d: DIV-0027 re-aims 0x586E78 / 0x586E97 - MasterScreen_AskYesNo's clone left out");

    // BOF3X_R2D_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R2D_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k) {
        if (patched && kAll[k].base == 0x586D20) continue;
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    }
    if (n == 0) bof3::Fatal("rest_2d: BOF3X_R2D_ONLY=%s names no clone", only);
    static unsigned* s_index = index;

    // FieldAbility_Effects holds the typed stand-ins while the fuzz runs.
    unsigned char saved[4 * at::kAbilityEffectCount];
    unsigned char* const table = move_script::At(at::kAbilityEffects);
    std::memcpy(saved, table, sizeof saved);
    for (unsigned i = 0; i < at::kAbilityEffectCount; ++i) SetLong(table + 4 * i, static_cast<std::int32_t>(KeyOf(kEffectEntries[i])));

    sh::Group g = {"rest_2d", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    g.menu_span = at::kItemsStateCount;
    sh::Run(g);
    std::memcpy(table, saved, sizeof saved);
}

}  // namespace rest_2d
