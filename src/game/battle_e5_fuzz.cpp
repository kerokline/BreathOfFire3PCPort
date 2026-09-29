// BOF3X_SHADOW=battle_e5: group BE5's 52 functions through the boss harness's
// engine frame (boss_harness.h, docs/boss_harness.md section 10), once at
// start-up: one boss_harness::Run, Group::engine set. docs/battle_e5.md
// section 5.
//
// The clone rows are tools/band_rows.py's (--group BE5 --clones, 2026-09-29),
// each read against the disassembly: the AI helpers, BattleForm_ApplyStats
// and the two slot pricers are kHelpers (cdecl words, al compared where they
// answer); the three Effect_Handlers slots and the Dragon run's 33 steps are
// kSteps (void (void), as Effect_ApplyResult and the dispatchers call them);
// the six Dragon dispatchers are kDispatch with state_cell 0x904AA4, the byte
// drawn below each table's length, their six .data tables DataTables (their
// entries recorders). BOF3X_BE5_ONLY=<substring> runs the clones whose name
// holds it (the controls script's shortcut); unset, all 52.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_e5.h"
#include "game/battle_e5_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace battle_e5 {
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
            const bh::JumpTable* tables = nullptr, int n_tables = 0) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, tables, n_tables, ours, ret, false, shape};
    return c;
}
// A Dragon dispatcher: jmp [table + 4 * byte 0x904AA4], the byte drawn below `steps`.
bh::Clone Disp(const char* name, U base, const void* ours, std::uint8_t steps) {
    bh::Clone c{name, base, 0x11, nullptr, 0, nullptr, 0, nullptr, 0, ours, 0, false, S::kDispatch};
    c.states = steps;
    c.state_cell = at::kStep4;
    return c;
}

// ===========================================================================
// The clone rows (tools/band_rows.py --group BE5 --clones)
// ===========================================================================

constexpr bh::CallSite kCalls44B240[] = {{0x58, 0x44B2C0}};
constexpr bh::CallSite kCalls44B3A0[] = {{0x48, 0x44F4B0}, {0x6B, 0x44F1D0}, {0x95, 0x44B5E0}, {0xC9, 0x44B870}, {0x1D2, 0x453300}};
constexpr bh::JumpTable kTables44B3A0[] = {{0x1E, 0x224, 7}};
constexpr bh::JumpTable kTables44B5E0[] = {{0x16, 0x274, 7}};
constexpr bh::JumpTable kTables44B870[] = {{0x11, 0x90, 8}};
constexpr bh::CallSite kCalls44C3D0[] = {{0xD, 0x44F6A0}, {0x1BE, 0x590E80}, {0x1D0, 0x453DA0}};
constexpr bh::CallSite kCalls44C5C0[] = {{0xD, 0x44F6A0}, {0x1C8, 0x590E80}, {0x1DA, 0x453EB0}};
constexpr bh::CallSite kCalls44CCA0[] = {{0x0, 0x5B93D2}, {0x25, 0x445CF0}, {0x4A, 0x453DA0}, {0x52, 0x44FB30}};
constexpr bh::CallSite kCalls44FF30[] = {{0xE, 0x454590}};
constexpr bh::CallSite kCalls44FF60[] = {{0x0, 0x454810}, {0xB, 0x4549F0}, {0x12, 0x4549F0}};
constexpr bh::CallSite kCalls44FFA0[] = {{0x7, 0x59E2D0}, {0x36, 0x59E2D0}, {0x4D, 0x587740}, {0xAF, 0x4525B0}};
constexpr bh::CallSite kCalls450090[] = {{0xD, 0x497740},  {0x31, 0x461EB0}, {0x4F, 0x587740}, {0x73, 0x587740},
                                         {0x96, 0x587740}, {0xB7, 0x587740}, {0xD8, 0x587740}};
constexpr bh::CallSite kCalls450200[] = {{0x9, 0x447F40}, {0x20, 0x44A990}, {0x31, 0x454590}};
constexpr bh::CallSite kCalls450250[] = {{0x0, 0x454810}, {0xB, 0x4549F0}};
constexpr bh::CallSite kCalls4502C0[] = {{0x16, 0x59E2D0}, {0x4A, 0x497740}};
constexpr bh::CallSite kCalls450340[] = {{0x1B, 0x461EB0}, {0x39, 0x587740}, {0x58, 0x450510}, {0x66, 0x587740},
                                         {0x7C, 0x587740}, {0xAD, 0x587740}, {0xCA, 0x587740}, {0x1BC, 0x587740}};
constexpr bh::CallSite kCalls4505A0[] = {{0x4, 0x59E2D0}};
constexpr bh::CallSite kCalls450610[] = {{0x42, 0x4525B0}};
constexpr bh::CallSite kCalls450680[] = {{0x9, 0x447F40}, {0x29, 0x454590}};
constexpr bh::CallSite kCalls450700[] = {{0x40, 0x497740},  {0x53, 0x44A6E0},  {0xAF, 0x4525B0},  {0xB9, 0x587740},
                                         {0xF2, 0x587740},  {0x139, 0x587740}, {0x170, 0x4525B0}, {0x17A, 0x587740},
                                         {0x1D4, 0x587740}, {0x1F6, 0x497740}, {0x244, 0x497740}, {0x269, 0x461EB0},
                                         {0x289, 0x587740}, {0x2B3, 0x587740}, {0x2E6, 0x587740}, {0x310, 0x587740}};
constexpr bh::CallSite kCalls450A30[] = {{0x5, 0x587740}};
constexpr bh::CallSite kCalls450A70[] = {{0x9, 0x447F40}, {0x29, 0x454590}};
constexpr bh::CallSite kCalls450AB0[] = {{0x0, 0x454810}, {0xB, 0x4549F0}};
constexpr bh::CallSite kCalls450B60[] = {{0x16, 0x59E2D0}, {0x4A, 0x497740}};
constexpr bh::CallSite kCalls450BE0[] = {{0x1B, 0x461EB0}, {0x39, 0x587740}, {0x58, 0x450D60},
                                         {0x66, 0x587740}, {0x7C, 0x587740}, {0x16E, 0x587740}};
constexpr bh::CallSite kCalls450DF0[] = {{0x4, 0x59E2D0}};
constexpr bh::CallSite kCalls450E70[] = {{0x42, 0x4525B0}, {0x4C, 0x587740}};
constexpr bh::CallSite kCalls450EF0[] = {{0x9, 0x447F40}, {0x24, 0x454590}};
constexpr bh::CallSite kCalls450F50[] = {{0xB, 0x59E2D0}};
constexpr bh::CallSite kCalls450FC0[] = {{0x3, 0x497740}, {0x27, 0x461EB0}, {0x45, 0x587740}, {0x97, 0x587740}, {0x189, 0x587740}};
constexpr bh::CallSite kCalls451160[] = {{0x2, 0x497740}, {0x26, 0x461EB0}, {0x43, 0x587740}, {0x70, 0x587740}, {0xAA, 0x5905D0}};
constexpr bh::CallSite kCalls451290[] = {{0x3, 0x497740},  {0x27, 0x461EB0}, {0x45, 0x587740}, {0x89, 0x587740},
                                         {0x9F, 0x587740}, {0xD0, 0x587740}, {0xE6, 0x587740}, {0x1D8, 0x587740}};

const bh::Clone kAll52[] = {
    // the enemy AI's row helpers
    C("EnemyAI_OtherRowsDone", 0x44B240, 0x7E, kCalls44B240, BH_N(kCalls44B240), BH_FN(EnemyAI_OtherRowsDone), S::kHelper, 0xFF),
    C("EnemyAI_SetRowDone", 0x44B2E0, 0x32, nullptr, 0, BH_FN(EnemyAI_SetRowDone), S::kHelper),
    C("EnemyAI_CondElement", 0x44B320, 0x7E, nullptr, 0, BH_FN(EnemyAI_CondElement), S::kHelper, 0xFF),
    C("EnemyAI_ApplyAction", 0x44B3A0, 0x240, kCalls44B3A0, BH_N(kCalls44B3A0), BH_FN(EnemyAI_ApplyAction), S::kHelper, 0,
      kTables44B3A0, 1),
    C("EnemyAI_ScaleStat", 0x44B5E0, 0x290, nullptr, 0, BH_FN(EnemyAI_ScaleStat), S::kHelper, 0, kTables44B5E0, 1),
    C("EnemyAI_SetAttrByte", 0x44B870, 0xB0, nullptr, 0, BH_FN(EnemyAI_SetAttrByte), S::kHelper, 0, kTables44B870, 1),
    C("EnemyAI_DedupMessages", 0x44B920, 0xC5, nullptr, 0, BH_FN(EnemyAI_DedupMessages), S::kHelper),
    // Effect_Handlers slots 22, 23, 38
    C("Effect_DrainHp", 0x44C3D0, 0x1EE, kCalls44C3D0, BH_N(kCalls44C3D0), BH_FN(Effect_DrainHp), S::kStep),
    C("Effect_DrainAp", 0x44C5C0, 0x1F8, kCalls44C5C0, BH_N(kCalls44C5C0), BH_FN(Effect_DrainAp), S::kStep),
    C("Effect_QuarterAttack", 0x44CCA0, 0x57, kCalls44CCA0, BH_N(kCalls44CCA0), BH_FN(Effect_QuarterAttack), S::kStep),
    // the transformation's stats
    C("BattleForm_ApplyStats", 0x44FDE0, 0x112, nullptr, 0, BH_FN(BattleForm_ApplyStats), S::kHelper),
    // the Dragon run: part 0
    Disp("DragonCmd_LoadDispatch", 0x44FF10, BH_FN(DragonCmd_LoadDispatch), 2),
    C("DragonCmd_LoadStart", 0x44FF30, 0x23, kCalls44FF30, BH_N(kCalls44FF30), BH_FN(DragonCmd_LoadStart), S::kStep),
    C("DragonCmd_LoadWait", 0x44FF60, 0x3C, kCalls44FF60, BH_N(kCalls44FF60), BH_FN(DragonCmd_LoadWait), S::kStep),
    // part 1
    C("DragonCmd_Open", 0x44FFA0, 0xCA, kCalls44FFA0, BH_N(kCalls44FFA0), BH_FN(DragonCmd_Open), S::kStep),
    // part 2
    Disp("DragonCmd_MenuDispatch", 0x450070, BH_FN(DragonCmd_MenuDispatch), 5),
    C("DragonCmd_MenuInput", 0x450090, 0x114, kCalls450090, BH_N(kCalls450090), BH_FN(DragonCmd_MenuInput), S::kStep),
    C("DragonCmd_MenuPick", 0x4501B0, 0x26, nullptr, 0, BH_FN(DragonCmd_MenuPick), S::kStep),
    C("DragonCmd_MenuClose", 0x4501E0, 0x14, nullptr, 0, BH_FN(DragonCmd_MenuClose), S::kStep),
    C("DragonCmd_MenuCancel", 0x450200, 0x46, kCalls450200, BH_N(kCalls450200), BH_FN(DragonCmd_MenuCancel), S::kStep),
    C("DragonCmd_MenuCancelLoad", 0x450250, 0x2C, kCalls450250, BH_N(kCalls450250), BH_FN(DragonCmd_MenuCancelLoad), S::kStep),
    // part 3
    Disp("DragonCmd_SlotsDispatch", 0x450280, BH_FN(DragonCmd_SlotsDispatch), 7),
    C("DragonCmd_SlotsCloseMenu", 0x4502A0, 0x1B, nullptr, 0, BH_FN(DragonCmd_SlotsCloseMenu), S::kStep),
    C("DragonCmd_SlotsOpen", 0x4502C0, 0x80, kCalls4502C0, BH_N(kCalls4502C0), BH_FN(DragonCmd_SlotsOpen), S::kStep),
    C("DragonCmd_SlotsCursor", 0x450340, 0x1C6, kCalls450340, BH_N(kCalls450340), BH_FN(DragonCmd_SlotsCursor), S::kStep),
    C("DragonCmd_SlotAffordable", 0x450510, 0x82, nullptr, 0, BH_FN(DragonCmd_SlotAffordable), S::kHelper, 0xFF),
    C("DragonCmd_SlotsCancel", 0x4505A0, 0x3E, kCalls4505A0, BH_N(kCalls4505A0), BH_FN(DragonCmd_SlotsCancel), S::kStep),
    C("DragonCmd_SlotsBack", 0x4505E0, 0x24, nullptr, 0, BH_FN(DragonCmd_SlotsBack), S::kStep),
    C("DragonCmd_SlotsConfirm", 0x450610, 0x70, kCalls450610, BH_N(kCalls450610), BH_FN(DragonCmd_SlotsConfirm), S::kStep),
    C("DragonCmd_SlotsClose", 0x450680, 0x40, kCalls450680, BH_N(kCalls450680), BH_FN(DragonCmd_SlotsClose), S::kStep),
    // part 4
    Disp("DragonCmd_GenesDispatch", 0x4506C0, BH_FN(DragonCmd_GenesDispatch), 5),
    C("DragonCmd_GenesOpen", 0x4506E0, 0x1B, nullptr, 0, BH_FN(DragonCmd_GenesOpen), S::kStep),
    C("DragonCmd_GenesPick", 0x450700, 0x327, kCalls450700, BH_N(kCalls450700), BH_FN(DragonCmd_GenesPick), S::kStep),
    C("DragonCmd_GenesConfirm", 0x450A30, 0x35, kCalls450A30, BH_N(kCalls450A30), BH_FN(DragonCmd_GenesConfirm), S::kStep),
    C("DragonCmd_GenesClose", 0x450A70, 0x3E, kCalls450A70, BH_N(kCalls450A70), BH_FN(DragonCmd_GenesClose), S::kStep),
    C("DragonCmd_Commit", 0x450AB0, 0x6B, kCalls450AB0, BH_N(kCalls450AB0), BH_FN(DragonCmd_Commit), S::kStep),
    // part 5
    Disp("DragonCmd_Slots2Dispatch", 0x450B20, BH_FN(DragonCmd_Slots2Dispatch), 7),
    C("DragonCmd_Slots2CloseMenu", 0x450B40, 0x1B, nullptr, 0, BH_FN(DragonCmd_Slots2CloseMenu), S::kStep),
    C("DragonCmd_Slots2Open", 0x450B60, 0x80, kCalls450B60, BH_N(kCalls450B60), BH_FN(DragonCmd_Slots2Open), S::kStep),
    C("DragonCmd_Slots2Cursor", 0x450BE0, 0x178, kCalls450BE0, BH_N(kCalls450BE0), BH_FN(DragonCmd_Slots2Cursor), S::kStep),
    C("DragonCmd_Slot2Affordable", 0x450D60, 0x82, nullptr, 0, BH_FN(DragonCmd_Slot2Affordable), S::kHelper, 0xFF),
    C("DragonCmd_Slots2Cancel", 0x450DF0, 0x45, kCalls450DF0, BH_N(kCalls450DF0), BH_FN(DragonCmd_Slots2Cancel), S::kStep),
    C("DragonCmd_Slots2Back", 0x450E40, 0x24, nullptr, 0, BH_FN(DragonCmd_Slots2Back), S::kStep),
    C("DragonCmd_Slots2Confirm", 0x450E70, 0x7D, kCalls450E70, BH_N(kCalls450E70), BH_FN(DragonCmd_Slots2Confirm), S::kStep),
    C("DragonCmd_Slots2Close", 0x450EF0, 0x3B, kCalls450EF0, BH_N(kCalls450EF0), BH_FN(DragonCmd_Slots2Close), S::kStep),
    // part 6
    Disp("DragonCmd_StoreDispatch", 0x450F30, BH_FN(DragonCmd_StoreDispatch), 7),
    C("DragonCmd_StoreOpen", 0x450F50, 0x4C, kCalls450F50, BH_N(kCalls450F50), BH_FN(DragonCmd_StoreOpen), S::kStep),
    C("DragonCmd_StoreWait", 0x450FA0, 0x12, nullptr, 0, BH_FN(DragonCmd_StoreWait), S::kStep),
    C("DragonCmd_StoreCursor", 0x450FC0, 0x193, kCalls450FC0, BH_N(kCalls450FC0), BH_FN(DragonCmd_StoreCursor), S::kStep),
    C("DragonCmd_StoreAsk", 0x451160, 0xB3, kCalls451160, BH_N(kCalls451160), BH_FN(DragonCmd_StoreAsk), S::kStep),
    C("DragonCmd_StoreCopy", 0x451220, 0x63, nullptr, 0, BH_FN(DragonCmd_StoreCopy), S::kStep),
    C("DragonCmd_StoreSource", 0x451290, 0x1E2, kCalls451290, BH_N(kCalls451290), BH_FN(DragonCmd_StoreSource), S::kStep),
};
static_assert(sizeof kAll52 / sizeof kAll52[0] == 52, "52 functions");

// ===========================================================================
// The callees the standard set lacks or records too coarsely, the tables,
// the regions
// ===========================================================================

// 0x4525B0 (BE6's) is called before cells its callers read again (the gene
// count 0x904B87 in DragonCmd_GenesPick): its stand-in moves the count half
// the time, the old value noted first (round 11 doc section 5.3's form).
U DragonTaskEffect(const U*, U) {
    const U n = bh::Noise();
    if (n & 1) {
        bh::Note(Mem(at::kGeneCount)[0]);
        Mem(at::kGeneCount)[0] = static_cast<unsigned char>((n >> 8) % 5);
    }
    return bh::Noise();
}

// The drains mark the acting sprite's +8 (| 4 or | 8 when the stat runs past
// its maximum) and clear it after the pop-up: the pop-ups' stand-ins note the
// byte as they are called, so the mark is compared, not wiped.
U PopupEffect(const U*, U answer) {
    bh::Note(bh::Pointer(0x904B40)[8]);
    return answer;
}

// The Dragon run's cursors read Input_Pressed again after a refusal's sound
// (0x107): the sound's stand-in moves it half the time, the old word noted.
U SoundEffect(const U*, U answer) {
    const U n = bh::Noise();
    if (n & 1) {
        bh::Note(move_script::Word(Mem(at::kInputPressed)));
        SetWord(Mem(at::kInputPressed), n >> 8);
    }
    return answer;
}

// Masks narrowed where the callee reads less than the word and the caller's
// register above it is its own garbage (a different value in the copy and in
// ours): each read of the callee's (capstone, 2026-09-29).
const bh::Callee kCallees[] = {
    // the group's own, called directly
    {"EnemyAI_ScaleStat", 0x44B5E0, KeyOf(&::EnemyAI_ScaleStat), 3, {kAll, kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"EnemyAI_SetAttrByte", 0x44B870, KeyOf(&::EnemyAI_SetAttrByte), 3, {kAll, kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"DragonCmd_SlotAffordable", 0x450510, KeyOf(&::DragonCmd_SlotAffordable), 0, {}, bh::Answer::kFlag, 0, 0},
    {"DragonCmd_Slot2Affordable", 0x450D60, KeyOf(&::DragonCmd_Slot2Affordable), 0, {}, bh::Answer::kFlag, 0, 0},
    // Battle_ClearStatus reads its actor's byte only (cmp dl, 2; and eax, 0xFF): EnemyAI_ApplyAction pushes it in ecx
    {"Battle_ClearStatus", bof3::addr::Battle_ClearStatus, KeyOf(&::Battle_ClearStatus), 2, {kU8, kAll}, bh::Answer::kGarbage, 0, 0},
    // Battle_SetDamagePopup and 0x453EB0 read the actor's byte (cmp al, 2; and eax, 0xFF): the drains push it in ecx
    {"Battle_SetDamagePopup", bof3::addr::Battle_SetDamagePopup, KeyOf(&::Battle_SetDamagePopup), 2, {kU16, kU8},
     bh::Answer::kGarbage, 0, 0, {}, &PopupEffect},
    // Battle_CalcDamage reads both actors' bytes (attacker & 0xFF, target & 0xFF): Effect_QuarterAttack pushes ecx
    {"Battle_CalcDamage", bof3::addr::Battle_CalcDamage, KeyOf(&::Battle_CalcDamage), 3, {kU8, kU8, kU16}, bh::Answer::kGarbage, 0, 0},
    // 0x44F6A0 hands its second word to 0x44F770, which reads its byte (cmp cl, 2; and eax, 0xFF)
    {"0x44F6A0", at::kResisted, at::kResisted, 2, {0, kU8}, bh::Answer::kFlag, 0, 0},
    // this wave's other groups' (raw until they merge)
    {"0x447F40", at::kTargetPrompt, at::kTargetPrompt, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x4525B0", at::kDragonTask, at::kDragonTask, 0, {}, bh::Answer::kGarbage, 0, 0, {}, &DragonTaskEffect},
    {"0x453300", at::kStatusPick, at::kStatusPick, 1, {kAll}, bh::Answer::kGarbage, 0, 0},
    {"0x453EB0", at::kApPopup, at::kApPopup, 2, {kU16, kU8}, bh::Answer::kGarbage, 0, 0, {}, &PopupEffect},
    // the standard Sound_PlayEffect (a short), louder (SoundEffect)
    {"Sound_PlayEffect", bof3::addr::Sound_PlayEffect, KeyOf(&::Sound_PlayEffect), 1, {kU16}, bh::Answer::kGarbage, 0, 0, {},
     &SoundEffect},
};

const bh::DataTable kTables[] = {
    {at::kLoadSteps, 2}, {at::kMenuSteps, 5}, {at::kSlotsSteps, 7}, {at::kGenesSteps, 5}, {at::kSlots2Steps, 7}, {at::kStoreSteps, 7},
};

// The cells beyond the engine frame the group reads or writes.
const bh::Region kRegions[] = {
    {at::kGeneFlags, 0x14},     // the 18 gene flags
    {at::kRepeatLatch, 4},      // Input_AutoRepeat's latch (zeroed by DragonCmd_Open)
    {0x93C320, 0x20},           // the message window's line list past the enemies' tail (lines 12..15)
};

// ===========================================================================
// Seeds, arguments, disturbance
// ===========================================================================

const bh::Clone* g_cur = nullptr;   // the Run's clones (for Seed and Args)
unsigned char* g_obj = nullptr;     // EnemyAI_ApplyAction's enemy, planted in Seed (an args hook cannot write)
U g_row = 0;                        // ... its row
unsigned g_enemy = 0;               // EnemyAI_OtherRowsDone's enemy

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
unsigned char* Enemy(unsigned n) { return Mem(at::kEnemies + (n % 8) * at::kEnemyStride); }
unsigned char* Member(unsigned n) { return Mem(at::kParty + (n % 3) * at::kPartyStride); }
unsigned char* Win(U window) { return Mem(window); }
U Upper() { return bh::Half() ? bh::Next() & 0xFFFFFF00u : 0; }

// The keys: the confirm and cancel masks single bits, the pressed word one of
// them, 0x10, both, none or anything (so every branch of a step is reached).
void Keys() {
    const U confirm = BH_PICK(0x20, 0x40, 0x2000, 0x60);
    const U cancel = BH_PICK(0x40, 0x80, 0x100, 0x4000);
    SetWord(Mem(at::kConfirmButtons), confirm);
    SetWord(Mem(at::kCancelButtons), cancel);
    const U picks[] = {0, 0, confirm, cancel, 0x10, confirm | 0x10, cancel | confirm, 0x8};
    const U pressed = bh::Often() ? bh::Pick(picks, BH_COUNT(picks)) : bh::Next();
    SetWord(Mem(at::kInputPressed), pressed);
}

// A list's cursor (window +0x12 a, +0x10 b) inside the slots it indexes, with
// the ends and one past them (a signed -1 among them).
void Cursor(U window, unsigned a_max, unsigned b_max) {
    SetWord(Win(window) + 0x12, bh::Often() ? bh::Next() % (a_max + 2) : 0xFFFF);
    SetWord(Win(window) + 0x10, bh::Often() ? bh::Next() % (b_max + 2) : 0xFFFF);
}

// The eighteen slots: a gene byte 0..17 or 0xFF, the fourth byte 0xFF (empty) a third of the time.
void Slots() {
    for (unsigned s = 0; s < 18; ++s) {
        unsigned char* const p = Mem(at::kSlots + 4 * s);
        for (unsigned j = 0; j < 3; ++j) p[j] = static_cast<unsigned char>(bh::Next() % 4 == 0 ? 0xFF : bh::Next() % 18);
        p[3] = static_cast<unsigned char>(bh::Next() % 3 == 0 ? 0xFF : bh::Next() % 18);
    }
}

// The Dragon run's frame: every step reads some of it.
void DragonFrame() {
    Keys();
    Mem(at::kPick)[0] = Byte({0, 1, 2, 3, 0x80, 0x81, 0x82, 0xFF, 0x7F});
    Mem(at::kGeneRow)[0] = static_cast<unsigned char>(BH_PICK(0, 1, 2, 0xFF));   // a gene's flag is written: never past the grid
    Mem(at::kGeneCol)[0] = static_cast<unsigned char>(bh::Next() % 7);
    Mem(at::kGeneCount)[0] = static_cast<unsigned char>(bh::Next() % 5);
    for (unsigned j = 0; j < 3; ++j) Mem(at::kGenes + j)[0] = static_cast<unsigned char>(bh::Next() % 20);   // written through
    for (unsigned i = 0; i < 18; ++i) Mem(at::kGeneFlags + i)[0] = static_cast<unsigned char>(bh::Half() ? 0 : 1 + bh::Next() % 3);
    Mem(at::kGeneCost)[0] = Byte({0, 1, 8, 0x20, 0xFF});
    SetWord(Mem(at::kAsk), Word16({0, 1, 2, 0x100}));
    Slots();
    Cursor(at::kWin18, 4, 3);
    Cursor(at::kWin19, 10, 3);
    for (U w : {at::kWin4, at::kWin16, at::kWin17, at::kWin18, at::kWin19, at::kWin21}) {
        Win(w)[0] = static_cast<unsigned char>(bh::Half() ? Win(w)[0] & 0xFE : Win(w)[0] | 1);
        Win(w)[3] = Byte({0, 0, 1, 2});
    }
    SetWord(Win(at::kWin18) + 4, Word16({0x5B, 0x5B, 0x5C, 0xFF5B}));
    SetWord(Win(at::kWin19) + 4, Word16({0x5B, 0x5B, 0xA3, 0xA3, 0x5A}));
    unsigned char* const actor = bh::Pointer(at::kMenuActor);
    actor[5] = Byte({0, 1, 2});
    SetWord(Member(actor[5]) + 0x9A, Word16({0, 1, 8, 0x20, 0x40, 0xFFFF}));
    SetWord(actor + 0x9A, Word16({0, 1, 8, 0x20, 0x40, 0xFFFF}));
}

void Seed(unsigned k) {
    const bh::Clone& c = g_cur[k];
    switch (c.base) {
    case 0x44B240: {   // EnemyAI_OtherRowsDone: the enemy's script in the area block, rows 0x63 or not
        g_enemy = bh::Next() % 8;   // the callers' loop 0..7; past the eight the objects run off the image's end 0x93F000
        unsigned char* const e = Enemy(g_enemy);
        e[0xF0] = static_cast<unsigned char>(bh::Next() % 7);
        for (unsigned i = 0; i < 4; ++i)
            Mem(at::kAiScripts + e[0xF0] * 0x8C + i * 16)[0] = static_cast<unsigned char>(bh::Next() % 4 == 0 ? 0x63 : bh::Next());
        break;
    }
    case 0x44B320: {   // EnemyAI_CondElement: +0x108, the acting kind, the actor, the ability, the weapons
        SetWord(bh::Pointer(at::kCurrentEnemy) + 0x108, Word16({0, 0, 1, 0x100}));
        Mem(at::kActingKind)[0] = Byte({4, 4, 1, 1, 0, 2, 5, 0x81});
        Mem(at::kActor)[0] = Byte({0, 1, 2, 3, 0x80});
        SetWord(Mem(at::kAbility), Word16({0, 1, 0x10, 0x4E, 0xA3, 0xC0}));
        for (unsigned m = 0; m < 3; ++m) Member(m)[0x92] = static_cast<unsigned char>(bh::Next() % 0x80);
        break;
    }
    case 0x44B3A0: {   // EnemyAI_ApplyAction: the row (kind 0..8 and past), HP, the message count at its end
        g_obj = Enemy(bh::Next());
        g_row = at::kAiScripts + (bh::Next() % 0x40) * 16;
        unsigned char* const row = Mem(g_row);
        row[1] = Byte({0, 1, 2, 3, 4, 5, 6, 7, 8});
        row[2] = Byte({0, 1, 2, 3, 0x80, 0x81, 0xFF, 0x40});
        row[3] = Byte({0, 1, 10, 20, 0x80, 0xFF});
        SetWord(row + 6, Word16({0, 0, 1, 0x1234}));
        SetWord(g_obj + 0xA4, Word16({0, 0, 1, 0xFFFF}));
        SetWord(g_obj + 0x94, Word16({0, 1, 0x7FFF, 0xFFFF, 6554, 6553}));
        SetWord(g_obj + 0x96, Word16({0, 1, 0x7FFF, 0xFFFF, 6554, 6553}));
        g_obj[5] = Byte({3, 4, 10, 0, 2});
        Mem(at::kEnemyMessageCount)[0] = static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 1, 6, 7, 8, 8) : bh::Next() % 9);
        break;
    }
    case 0x44B5E0: {   // EnemyAI_ScaleStat: the stats against their caps
        for (unsigned n = 0; n < 8; ++n) {
            unsigned char* const e = Enemy(n);
            for (U at_ : {0xA4u, 0xA6u, 0xD4u, 0xD6u, 0xD8u, 0xDAu, 0x98u})
                SetWord(e + at_, Word16({0, 1, 99, 100, 999, 1000, 9999, 0xFFFF}));
            SetWord(e + 0xD0, Word16({0, 99, 100, 999, 0xFFFF}));
            SetWord(e + 0xD2, Word16({0, 99, 100, 999, 0xFFFF}));
        }
        break;
    }
    case 0x44B920: {   // EnemyAI_DedupMessages: up to eight entries with repeats
        Mem(at::kEnemyMessageCount)[0] = static_cast<unsigned char>(bh::Next() % 9);
        for (unsigned i = 0; i < 8; ++i) {
            unsigned char* const q = Mem(at::kEnemyMessages + 4 * i);
            q[0] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 8 : bh::Next() % 11);
            SetWord(q + 2, Word16({1, 2, 3, 2}));
        }
        for (unsigned n = 0; n < 11; ++n) Mem(at::kEnemies + n * at::kEnemyStride + 0x8C)[0] = Byte({0, 1, 1, 2});
        break;
    }
    case 0x44C3D0:
    case 0x44C5C0:
    case 0x44CCA0: {   // the effects: the target and the actor on both sides, the flag, the stats near their maxima
        Mem(at::kTarget)[0] = static_cast<unsigned char>(bh::Next() % 11);   // 0..10: past 10 the objects run off the image
        Mem(at::kActor)[0] = static_cast<unsigned char>(bh::Next() % 11);
        for (unsigned m = 0; m < 3; ++m) {
            unsigned char* const p = Member(m);
            SetLong(p + 0x130, static_cast<std::int32_t>(bh::Half() ? bh::Next() | 0x10000 : bh::Next() & ~0x10000u));
            const U max_hp = Word16({1, 100, 999, 9999}), max_ap = Word16({1, 50, 999});
            SetWord(p + 0xA0, max_hp);
            SetWord(p + 0x98, bh::Half() ? max_hp - bh::Next() % 4 : bh::Next());
            SetWord(p + 0xA2, max_ap);
            SetWord(p + 0x9A, bh::Half() ? max_ap - bh::Next() % 4 : bh::Next());
        }
        for (unsigned n = 0; n < 8; ++n) {
            unsigned char* const e = Enemy(n);
            SetLong(e + 0x110, static_cast<std::int32_t>(bh::Half() ? bh::Next() | 0x10000 : bh::Next() & ~0x10000u));
            const U max_hp = Word16({1, 100, 999, 9999}), max_ap = Word16({1, 50, 999});
            SetWord(e + 0xB0, max_hp);
            SetWord(e + 0xA4, bh::Half() ? max_hp - bh::Next() % 4 : bh::Next());
            SetWord(e + 0xB2, max_ap);
            SetWord(e + 0xA6, bh::Half() ? max_ap - bh::Next() % 4 : bh::Next());
        }
        break;
    }
    case 0x44FDE0: {   // BattleForm_ApplyStats: the members' flag bit 1
        for (unsigned m = 0; m < 3; ++m) {
            unsigned char* const p = Member(m);
            p[0x134] = static_cast<unsigned char>(bh::Half() ? p[0x134] | 2 : p[0x134] & ~2);
        }
        SetWord(Mem(0x939EE0), Word16({0, 1, 0xFFFF, 0x8000, 0x7FFF, 100}));
        break;
    }
    default:
        if (c.base >= 0x44FF10) DragonFrame();
        break;
    }
}

// The words of the helpers: bytes inside what each indexes, garbage above them
// half the time (the originals read the low byte), the pointers the Seed chose.
void Args(unsigned k, U* a) {
    const bh::Clone& c = g_cur[k];
    switch (c.base) {
    case 0x44B240:
        a[0] = Upper() | (bh::Often() ? bh::Next() % 5 : bh::Next() & 0xFF);
        a[1] = Upper() | g_enemy;
        break;
    case 0x44B2E0:
        a[0] = Key(Enemy(bh::Next()));
        a[1] = Upper() | (bh::Often() ? bh::Next() % 10 : bh::Next() & 0xFF);
        a[2] = bh::Half() ? Upper() : Upper() | (1 + bh::Next() % 0xFF);
        break;
    case 0x44B320:
        a[0] = bh::Often() ? BH_PICK(1, 2, 4, 8, 0x10, 0x20, 0x40, 0x100, 0x80, 0x101, 0xFFFF) : bh::Next();
        break;
    case 0x44B3A0:
        a[0] = Key(g_obj);
        a[1] = g_row;
        break;
    case 0x44B5E0:
    case 0x44B870:
        a[0] = Key(Enemy(bh::Next()));
        a[1] = Upper() | (bh::Often() ? bh::Next() % 9 : bh::Next() & 0xFF);
        a[2] = Upper() | (bh::Often() ? BH_PICK(0, 1, 10, 20, 100, 0xFF) : bh::Next() & 0xFF);
        break;
    default:
        break;
    }
}

// What the group's functions read again after a call: the keys and their
// masks, the menu line, the grid, the gene count, the message count, the
// cursors, the prompt's answer, the drains' target and actor, the applied
// enemy's status, actor and HP (EnemyAI_ApplyAction reads them after
// Battle_ClearStatus and 0x453300). Kept inside what they index.
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 12) {
    case 0: SetWord(Mem(at::kInputPressed), h >> 12); break;
    case 1: SetWord(Mem(at::kConfirmButtons), 1u << (b % 16)); break;
    case 2: Mem(at::kPick)[0] = b; break;
    case 3: Mem(at::kGeneRow)[0] = static_cast<unsigned char>(b % 4 == 3 ? 0xFF : b % 3); break;
    case 4: Mem(at::kGeneCol)[0] = static_cast<unsigned char>(b % 7); break;
    case 5: Mem(at::kGeneCount)[0] = static_cast<unsigned char>(b % 5); break;
    case 6: Mem(at::kMsgCount)[0] = b; break;
    case 7: SetWord(Mem(at::kAsk), b & 3); break;
    case 8: SetWord(Mem(b & 1 ? at::kWin18 + 0x10 : at::kWin19 + 0x10), (b >> 1) % 4); break;
    case 9: Mem(at::kTarget)[0] = static_cast<unsigned char>(b % 11); break;
    case 10: Mem(at::kActor)[0] = static_cast<unsigned char>(b % 11); break;
    default:
        if (g_obj) {
            g_obj[0x92] = b;
            g_obj[5] = static_cast<unsigned char>(3 + (b >> 5));
            SetWord(g_obj + 0xA4, h & 0x100000 ? 0 : h >> 20);
        }
        break;
    }
}

}  // namespace

void SelfTest() {
    static bh::Clone clones[52];
    unsigned n = 0;
    const char* const only = std::getenv("BOF3X_BE5_ONLY");
    for (const bh::Clone& c : kAll52)
        if (only == nullptr || *only == 0 || std::strstr(c.name, only) != nullptr) clones[n++] = c;
    if (n == 0) bof3::Fatal("battle_e5: BOF3X_BE5_ONLY=%s names no clone", only);
    g_cur = clones;
    g_obj = nullptr;
    bh::Group g{"battle_e5", clones, n, kCallees, BH_COUNT(kCallees), kTables, BH_COUNT(kTables), kRegions, BH_COUNT(kRegions),
                &Seed, &Disturb, 6000};
    g.args = &Args;
    g.engine = true;
    bh::Run(g);
    g_cur = nullptr;
    g_obj = nullptr;
}

}  // namespace battle_e5
