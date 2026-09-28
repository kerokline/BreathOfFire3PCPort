// BOF3X_SHADOW=boss_sc: group BSC's 53 boss functions through the boss harness
// (boss_harness.h), once at start-up: one boss_harness::Run per set-up (fights
// 11, 12, 14, 15, 16, 46) and per kind (12..17, 53). docs/boss_sc.md section 5.
//
// The clone rows are tools/boss_rows.py's (--unit <UNIT> --clones,
// 2026-09-28), each read against the disassembly. Every function is called
// directly in its engine shape: the set-ups kSetup, the hooks kEnd / kExit /
// kEvent, the kinds' dispatchers kDispatch with their state byte drawn below
// their tables (the second-level ones by +2 and +3 likewise), the +0xF4 hook
// tables and their entries kEnemyHook, the states kState, Amalgam's two draws
// kCallee. The kinds' .data tables are DataTables (their entries recorders).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sc.h"
#include "game/boss_sc_callees.h"
#include "game/move_script_bytes.h"

namespace boss_sc {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetLong;
using move_script::SetWord;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)

bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret = 0,
            std::uint8_t state_at = 1, std::uint8_t states = 0) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, shape};
    c.state_at = state_at;
    c.states = states;
    return c;
}

// ===========================================================================
// The clone rows (tools/boss_rows.py --unit <UNIT> --clones)
// ===========================================================================

// B11
constexpr bh::CallSite kCalls439430[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};
constexpr bh::CallSite kCalls439450[] = {{0x2, 0x4949D0}, {0x9, 0x494920}, {0x18, 0x589590}, {0x1F, 0x5891F0}};
// K12
constexpr bh::CallSite kCalls4396D0[] = {{0x3A, 0x455290}, {0x4C, 0x5893A0}};
constexpr bh::CallSite kCalls439770[] = {{0x0, 0x5893A0}};
constexpr bh::CallSite kCalls439790[] = {{0x7, 0x589410}, {0x20, 0x461E10}, {0x2A, 0x587900}, {0x83, 0x439930}};
constexpr bh::CallSite kCalls439830[] = {{0xA, 0x439930}, {0x1A, 0x4399E0}};
constexpr bh::CallSite kCalls439870[] = {{0x6, 0x454A80}, {0xE, 0x437470}};
constexpr bh::CallSite kCalls439930[] = {{0x11, 0x5A79A0}, {0x29, 0x5A77C0}, {0x32, 0x461E50}, {0x91, 0x5A7710}, {0x9A, 0x461E50}};
constexpr bh::CallSite kCalls4399E0[] = {{0x13, 0x5A79A0}, {0x29, 0x5A77C0}, {0x32, 0x461E50},
                                         {0x4A, 0x5A79A0}, {0x112, 0x5A75D0}, {0x11B, 0x461E50}};
// B12
constexpr bh::CallSite kCalls439900[] = {{0x17, 0x446DE0}, {0x1C, 0x446E00}};
// K13, K14
constexpr bh::CallSite kCalls439B30[] = {{0x72, 0x589590}, {0x79, 0x5891F0}};
constexpr bh::CallSite kCalls439BD0[] = {{0x26, 0x4976D0}, {0x52, 0x4976D0}};
constexpr bh::CallSite kCalls439C80[] = {{0x72, 0x589590}, {0x79, 0x5891F0}};
constexpr bh::CallSite kCalls439D20[] = {{0x26, 0x4976D0}, {0x52, 0x4976D0}, {0x7A, 0x4976D0}};
// K17
constexpr bh::CallSite kCalls439E90[] = {{0x5, 0x589590}, {0x15, 0x5891F0}};
constexpr bh::CallSite kCalls439EC0[] = {{0x15, 0x5893A0}, {0x1A, 0x5893A0}};
constexpr bh::CallSite kCalls439F50[] = {{0x0, 0x444310}, {0x21, 0x5171A0}, {0x28, 0x497740},
                                         {0x36, 0x44A650}, {0x46, 0x533BA0}, {0x71, 0x5891F0}};
// B16
constexpr bh::CallSite kCalls43A030[] = {{0x1E, 0x5B93D2}};
constexpr bh::CallSite kCalls43A060[] = {{0x3A, 0x444310}, {0x5B, 0x5171A0}, {0x62, 0x497740}, {0x70, 0x44A650},
                                         {0x8F, 0x533BA0}, {0xBA, 0x5891F0}, {0xFC, 0x4976D0}};
constexpr bh::CallSite kCalls43A190[] = {{0x28, 0x446DE0}, {0x56, 0x589590}, {0x75, 0x5891F0}, {0x84, 0x446E20}};
constexpr bh::CallSite kCalls43A230[] = {{0x18, 0x4949D0}, {0x1F, 0x494920}, {0x2E, 0x589590}, {0x4F, 0x5891F0}, {0x5D, 0x589200},
                                         {0x70, 0x4949D0}, {0x77, 0x494920}, {0x86, 0x589590}, {0x8D, 0x5891F0}, {0xB5, 0x4949D0},
                                         {0xBC, 0x494920}, {0xCB, 0x589590}, {0xD2, 0x5891F0}, {0xFD, 0x5341A0}};
// K15, K53, K16
constexpr bh::CallSite kCalls43A380[] = {{0x38, 0x5893A0}};
constexpr bh::CallSite kCalls43A3F0[] = {{0x4C, 0x5893A0}};
constexpr bh::CallSite kCalls43A4F0[] = {{0x38, 0x5893A0}};
// B14, B15
constexpr bh::CallSite kCalls43A480[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};
constexpr bh::CallSite kCalls43A560[] = {{0x10, 0x446DE0}, {0x15, 0x446E00}};
constexpr bh::CallSite kCalls43A580[] = {{0x2, 0x494A60}};

const bh::Clone kB11[] = {
    C("Boss11_Setup", 0x439410, 0x1F, nullptr, 0, BH_FN(Boss11_Setup), S::kSetup),
    C("Boss11_End", 0x439430, 0x1A, kCalls439430, BH_N(kCalls439430), BH_FN(Boss11_End), S::kEnd),
    C("Boss11_Exit", 0x439450, 0x48, kCalls439450, BH_N(kCalls439450), BH_FN(Boss11_Exit), S::kExit),
};
const bh::Clone kK12[] = {
    C("BossAmalgam_Dispatch", 0x4396B0, 0x12, nullptr, 0, BH_FN(BossAmalgam_Dispatch), S::kDispatch, 0, 1, 12),
    C("BossAmalgam_Enter", 0x4396D0, 0x51, kCalls4396D0, BH_N(kCalls4396D0), BH_FN(BossAmalgam_Enter), S::kState, 0xFF),
    C("BossAmalgam_ActDispatch", 0x439730, 0x12, nullptr, 0, BH_FN(BossAmalgam_ActDispatch), S::kDispatch, 0, 2, 6),
    C("BossAmalgam_DeathDispatch", 0x439750, 0x12, nullptr, 0, BH_FN(BossAmalgam_DeathDispatch), S::kDispatch, 0, 3, 4),
    C("BossAmalgam_DeathTick", 0x439770, 0x13, kCalls439770, BH_N(kCalls439770), BH_FN(BossAmalgam_DeathTick), S::kState, 0xFF),
    C("BossAmalgam_DeathStart", 0x439790, 0x96, kCalls439790, BH_N(kCalls439790), BH_FN(BossAmalgam_DeathStart), S::kState),
    C("BossAmalgam_DeathMelt", 0x439830, 0x40, kCalls439830, BH_N(kCalls439830), BH_FN(BossAmalgam_DeathMelt), S::kState),
    C("BossAmalgam_DeathEnd", 0x439870, 0x52, kCalls439870, BH_N(kCalls439870), BH_FN(BossAmalgam_DeathEnd), S::kState),
    C("BossAmalgam_Hook", 0x4398D0, 0x10, nullptr, 0, BH_FN(BossAmalgam_Hook), S::kEnemyHook),
    C("BossAmalgam_DrawSprite", 0x439930, 0xA3, kCalls439930, BH_N(kCalls439930), BH_FN(BossAmalgam_DrawSprite), S::kCallee),
    C("BossAmalgam_DrawStreak", 0x4399E0, 0x127, kCalls4399E0, BH_N(kCalls4399E0), BH_FN(BossAmalgam_DrawStreak), S::kCallee),
};
const bh::Clone kB12[] = {
    C("Boss12_Setup", 0x4398E0, 0x1F, nullptr, 0, BH_FN(Boss12_Setup), S::kSetup),
    C("Boss12_End", 0x439900, 0x21, kCalls439900, BH_N(kCalls439900), BH_FN(Boss12_End), S::kEnd),
};
const bh::Clone kK13[] = {
    C("BossBalio_Dispatch", 0x439B10, 0x12, nullptr, 0, BH_FN(BossBalio_Dispatch), S::kDispatch, 0, 1, 12),
    C("BossBalio_Enter", 0x439B30, 0x8C, kCalls439B30, BH_N(kCalls439B30), BH_FN(BossBalio_Enter), S::kState),
    C("BossBalio_Hook", 0x439BC0, 0x10, nullptr, 0, BH_FN(BossBalio_Hook), S::kEnemyHook),
    C("BossBalio_HookAct", 0x439BD0, 0x67, kCalls439BD0, BH_N(kCalls439BD0), BH_FN(BossBalio_HookAct), S::kEnemyHook),
    C("BossBalio_HookHit", 0x439C40, 0x19, nullptr, 0, BH_FN(BossBalio_HookHit), S::kEnemyHook),
};
const bh::Clone kK14[] = {
    C("BossSunder_Dispatch", 0x439C60, 0x12, nullptr, 0, BH_FN(BossSunder_Dispatch), S::kDispatch, 0, 1, 12),
    C("BossSunder_Enter", 0x439C80, 0x8C, kCalls439C80, BH_N(kCalls439C80), BH_FN(BossSunder_Enter), S::kState),
    C("BossSunder_Hook", 0x439D10, 0x10, nullptr, 0, BH_FN(BossSunder_Hook), S::kEnemyHook),
    C("BossSunder_HookAct", 0x439D20, 0x81, kCalls439D20, BH_N(kCalls439D20), BH_FN(BossSunder_HookAct), S::kEnemyHook),
    C("BossSunder_HookHit", 0x439DB0, 0x44, nullptr, 0, BH_FN(BossSunder_HookHit), S::kEnemyHook),
};
const bh::Clone kK17[] = {
    C("BossNina_Dispatch", 0x439E00, 0x12, nullptr, 0, BH_FN(BossNina_Dispatch), S::kDispatch, 0, 1, 12),
    C("BossNina_Enter", 0x439E20, 0x43, nullptr, 0, BH_FN(BossNina_Enter), S::kState),
    C("BossNina_WalkDispatch", 0x439E70, 0x12, nullptr, 0, BH_FN(BossNina_WalkDispatch), S::kDispatch, 0, 2, 3),
    C("BossNina_WalkStart", 0x439E90, 0x26, kCalls439E90, BH_N(kCalls439E90), BH_FN(BossNina_WalkStart), S::kState),
    C("BossNina_WalkStep", 0x439EC0, 0x1F, kCalls439EC0, BH_N(kCalls439EC0), BH_FN(BossNina_WalkStep), S::kState, 0xFF),
    C("BossNina_Hook", 0x439EE0, 0x10, nullptr, 0, BH_FN(BossNina_Hook), S::kEnemyHook),
    C("BossNina_HookAct", 0x439EF0, 0x58, nullptr, 0, BH_FN(BossNina_HookAct), S::kEnemyHook),
    C("BossNina_HookHit", 0x439F50, 0x86, kCalls439F50, BH_N(kCalls439F50), BH_FN(BossNina_HookHit), S::kEnemyHook),
};
const bh::Clone kB16[] = {
    C("Boss16_Setup", 0x43A030, 0x2D, kCalls43A030, BH_N(kCalls43A030), BH_FN(Boss16_Setup), S::kSetup),
    C("Boss16_Event", 0x43A060, 0x128, kCalls43A060, BH_N(kCalls43A060), BH_FN(Boss16_Event), S::kEvent, 0xFF),
    C("Boss16_End", 0x43A190, 0x9A, kCalls43A190, BH_N(kCalls43A190), BH_FN(Boss16_End), S::kEnd),
    C("Boss16_Exit", 0x43A230, 0x126, kCalls43A230, BH_N(kCalls43A230), BH_FN(Boss16_Exit), S::kExit),
};
const bh::Clone kK15[] = {
    C("BossRocky_Dispatch", 0x43A360, 0x12, nullptr, 0, BH_FN(BossRocky_Dispatch), S::kDispatch, 0, 1, 12),
    C("BossRocky_Enter", 0x43A380, 0x3D, kCalls43A380, BH_N(kCalls43A380), BH_FN(BossRocky_Enter), S::kState, 0xFF),
    C("BossRocky_Hook", 0x43A3C0, 0x10, nullptr, 0, BH_FN(BossRocky_Hook), S::kEnemyHook),
};
const bh::Clone kK53[] = {
    C("BossSample8_Dispatch", 0x43A3D0, 0x12, nullptr, 0, BH_FN(BossSample8_Dispatch), S::kDispatch, 0, 1, 12),
    C("BossSample8_Enter", 0x43A3F0, 0x51, kCalls43A3F0, BH_N(kCalls43A3F0), BH_FN(BossSample8_Enter), S::kState, 0xFF),
    C("BossSample8_Hook", 0x43A450, 0x10, nullptr, 0, BH_FN(BossSample8_Hook), S::kEnemyHook),
};
const bh::Clone kB14[] = {
    C("Boss14_Setup", 0x43A460, 0x1F, nullptr, 0, BH_FN(Boss14_Setup), S::kSetup),
    C("Boss14_End", 0x43A480, 0x1A, kCalls43A480, BH_N(kCalls43A480), BH_FN(Boss14_End), S::kEnd),
};
const bh::Clone kB46[] = {
    C("Boss46_Setup", 0x43A4B0, 0x1F, nullptr, 0, BH_FN(Boss46_Setup), S::kSetup),
};
const bh::Clone kK16[] = {
    C("BossPooch_Dispatch", 0x43A4D0, 0x12, nullptr, 0, BH_FN(BossPooch_Dispatch), S::kDispatch, 0, 1, 12),
    C("BossPooch_Enter", 0x43A4F0, 0x3D, kCalls43A4F0, BH_N(kCalls43A4F0), BH_FN(BossPooch_Enter), S::kState, 0xFF),
    C("BossPooch_Hook", 0x43A530, 0x10, nullptr, 0, BH_FN(BossPooch_Hook), S::kEnemyHook),
};
const bh::Clone kB15[] = {
    C("Boss15_Setup", 0x43A540, 0x1F, nullptr, 0, BH_FN(Boss15_Setup), S::kSetup),
    C("Boss15_End", 0x43A560, 0x1A, kCalls43A560, BH_N(kCalls43A560), BH_FN(Boss15_End), S::kEnd),
    C("Boss15_Exit", 0x43A580, 0x9, kCalls43A580, BH_N(kCalls43A580), BH_FN(Boss15_Exit), S::kExit),
};

// ===========================================================================
// The callees the standard set lacks, the tables, the regions
// ===========================================================================

// Stand-ins louder than the real callees where ours reads a cell again after
// the call: the fight's bits after Msg_OpenScript (the action hooks), the
// scene byte after Scenario_CallA and 0x446DE0 (the exit and end hooks), the
// banner's character after Battle_OpenMsgWindow. Each moves its cell from
// Noise(), so a read moved across the call is refused.
// (bh::ScriptBitsEffect, bh::MoveCounterEffect and bh::BannerCharEffect: the
// harness's since round eleven's cleanup folded this group's BitsEffect,
// SceneEffect and CharEffect - the same draws; the two group cells are this
// group's regions, which the harness's forms require)

const bh::Callee kCallees[] = {
    {"Msg_OpenScript", bof3::addr::Msg_OpenScript, KeyOf(&::Msg_OpenScript), 1, {0xFFFF}, bh::Answer::kGarbage, 0, 0, {}, &bh::ScriptBitsEffect},
    {"Scenario_CallA", bof3::addr::Scenario_CallA, KeyOf(&::Scenario_CallA), 1, {kAll}, bh::Answer::kGarbage, 0, 0, {},
     &bh::MoveCounterEffect},
    {"Battle_OpenMsgWindow", bof3::addr::Battle_OpenMsgWindow, KeyOf(&::Battle_OpenMsgWindow), 0, {}, bh::Answer::kGarbage, 0, 0, {},
     &bh::BannerCharEffect},
    {"0x446DE0", at::kEndWin, at::kEndWin, 0, {}, bh::Answer::kGarbage, 0, 0, {}, &bh::MoveCounterEffect},
    // engine code nobody owns (boss_sc_callees.h)
    {"0x455290", at::kSlotStart, at::kSlotStart, 2, {kAll, kAll}, bh::Answer::kGarbage, 0, 0},
    {"0x454A80", at::kSlotsReleaseFor, at::kSlotsReleaseFor, 1, {kAll}, bh::Answer::kGarbage, 0, 0},
    // the group's own, called directly (they read the argument's low byte)
    {"BossAmalgam_DrawSprite", 0x439930, KeyOf(&::BossAmalgam_DrawSprite), 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    {"BossAmalgam_DrawStreak", 0x4399E0, KeyOf(&::BossAmalgam_DrawStreak), 1, {kU8}, bh::Answer::kGarbage, 0, 0},
};

const bh::DataTable kTablesK12[] = {{0x64CC24, 12}, {0x64CC54, 6}, {0x64CC6C, 4}, {0x64CC7C, 3, 4, 1}};
const bh::DataTable kTablesK13[] = {{0x64CCD0, 12}, {0x64CD00, 3, 4, 1}};
const bh::DataTable kTablesK14[] = {{0x64CD0C, 12}, {0x64CD3C, 3, 4, 1}};
const bh::DataTable kTablesK17[] = {{0x64CD48, 12}, {0x64CD78, 3}, {0x64CD84, 3, 4, 1}};
const bh::DataTable kTablesK15[] = {{0x64CDAC, 12}, {0x64CDDC, 3, 4, 1}};
const bh::DataTable kTablesK53[] = {{0x64CDE8, 12}, {0x64CE18, 3, 4, 1}};
const bh::DataTable kTablesK16[] = {{0x64CE38, 12}, {0x64CE68, 3, 4, 1}};

// The cells beyond the battle frame that the group reads or writes.
const bh::Region kRegions[] = {
    {at::kMoveCounter, 4},     // the move-script counters (the end hooks' scene byte)
    {at::kSaveWord, 2},
    {at::kFieldByte131, 1},
    {at::kLeaderPick, 1},
    {at::kWaitTurns, 1},
    {at::kMsgPass, 1},
    {at::kBannerChar, 1},
};

// ===========================================================================
// Seeds, arguments, disturbance
// ===========================================================================

const bh::Clone* g_cur = nullptr;   // the Run's clones (for Seed and Args)

unsigned char Byte(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<unsigned char>(bh::Next());
    const U* v = often.begin();
    return static_cast<unsigned char>(v[bh::Next() % often.size()]);
}
std::uint16_t Hp() {
    return static_cast<std::uint16_t>(bh::Often() ? BH_PICK(0, 0, 1, 2, 0xFFFF, 0x100) : bh::Next());
}
unsigned char* E() { return bh::Pointer(bh::at::kEnemyCurrent); }

// A dispatcher's other state bytes inside their tables, so a
// dispatcher reading the wrong byte lands on another entry (a count) rather
// than past its table (a Fatal); the byte the harness drew is left alone.
using bh::OtherStates;   // the harness's since round eleven's cleanup folded this group's copy (the same draws)

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    switch (g_cur[k].base) {
    // --- the end and exit hooks: the win bit, the fight's bits, the scene byte
    case 0x439430: case 0x439900: case 0x43A480: case 0x43A560:
        Mem(at::kBattleEnd)[0] = Byte({0, 1, 2, 3, 0xFD, 0x82, 8, 0xA});
        break;
    case 0x43A190:
        Mem(at::kBattleEnd)[0] = Byte({0, 1, 2, 3, 0xFD, 0x82});
        Mem(at::kFightFlags)[0] = Byte({0, 0x40, 0xBF, 0x41});
        break;
    case 0x43A230:
        Mem(at::kBattleEnd)[0] = Byte({0, 1, 2, 3, 0xFD, 0x82});
        Mem(at::kFightFlags)[0] = Byte({0, 1, 0x40, 0x41, 0xBE, 0xFF});
        Mem(at::kMoveCounter)[0] = Byte({0x14, 0x14, 0x15, 0x13, 0x50, 0x94});
        break;
    // --- fight 16's event hook
    case 0x43A060:
        Mem(at::kFightFlags)[0] = Byte({0, 1, 0x40, 0x41, 8, 0xBE, 0xBF});
        Mem(at::kWaitTurns)[0] = bh::Half() ? 0 : Byte({1, 8, 0xA, 0x80});
        Mem(at::kActor)[0] = Byte({0, 0, 0, 1, 3, 0x80});
        Mem(at::kTarget)[0] = Byte({0x40, 0x44, 0x4, 0, 0xBF, 0xC0});
        Mem(at::kEnemy0 + at::kEnemyStatus + 1)[0] = Byte({0, 0x20, 0xDF, 0x60});
        Mem(at::kEnemy1 + at::kEnemyStatus + 1)[0] = Byte({0, 0x20, 0xDF, 0x60});
        break;
    // --- the entrances that read the fight byte (fight 13's HP)
    case 0x439B30: case 0x439C80:
        Mem(at::kFight)[0] = Byte({0xD, 0xD, 0x10, 0x8D, 0xC, 0xE});
        break;
    // --- the Balio / Sunder action hooks: fight 16 and the fight's bits
    case 0x439BD0: case 0x439D20:
        Mem(at::kFight)[0] = Byte({0x10, 0x10, 0x10, 0xD, 0x11, 0x90});
        Mem(at::kFightFlags)[0] = static_cast<unsigned char>(bh::Often() ? bh::Next() & 0x3F : bh::Next());
        break;
    // --- the hit hooks: HP 0 and neighbours; Sunder's +0x108 and actor 8
    case 0x439C40:
        SetWord(E() + 0xA4, Hp());
        break;
    case 0x439DB0:
        Mem(at::kFight)[0] = Byte({0x10, 0x10, 0xD, 0x11, 0x90});
        Mem(at::kActor)[0] = Byte({8, 8, 7, 9, 0x88});
        SetWord(E() + 0xA4, Hp());
        SetWord(E() + 0x108, Hp());
        break;
    // --- Nina's hooks: the fight's bit 0, the wait count at its ends
    case 0x439EF0:
        Mem(at::kFightFlags)[0] = Byte({0, 1, 0xFE, 0xFF, 0x40});
        Mem(at::kWaitTurns)[0] = Byte({0, 0, 1, 2, 8, 0x80, 0xFF});
        Mem(at::kCount4AE2)[0] = Byte({0, 1, 2, 0x80});
        break;
    case 0x439F50:
        Mem(at::kBannerChar)[0] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 8 : bh::Next());
        break;
    // --- Amalgam's melt: the row counter at and around its end
    case 0x439830:
        if (bh::Often())
            SetLong(s + 0x20, static_cast<std::int32_t>(BH_PICK(0x54, 0x55, 0x56, 0x52, 0, 0xFFFFFFFEu, 0x10054, 0x57, 0x10056)));
        break;
    case 0x439EC0:   // BossNina_WalkStep: bit 7 of +0 at both values
        s[0] = static_cast<unsigned char>(bh::Half() ? s[0] | 0x80 : s[0] & 0x7F);
        break;
    // --- the dispatchers: the other state bytes inside their tables
    case 0x4396B0: OtherStates(1, 12, 6, 4); break;
    case 0x439730: OtherStates(2, 12, 6, 4); break;
    case 0x439750: OtherStates(3, 12, 4, 4); break;   // +2 below 4: a plant reading it lands in the table
    case 0x439E00: OtherStates(1, 12, 3, 3); break;
    case 0x439E70: OtherStates(2, 12, 3, 3); break;
    case 0x439B10: case 0x439C60: case 0x43A360: case 0x43A3D0: case 0x43A4D0: OtherStates(1, 12, 0, 0); break;
    default:
        break;
    }
}

// The words: a hook's (the index 0..2 the harness drew) and the event hook's
// code (0..2, the codes it acts on, two times in three; else the harness's
// 0..6) with garbage above the byte half the time (the originals read the low
// byte); the draws' row byte likewise, from the rows the melt reaches (0..0x56
// in twos) or any.
void Args(unsigned k, U* a) {
    const bh::Clone& c = g_cur[k];
    // fight 16's event hook acts on codes 0..2 only: those most of the time
    if (c.shape == S::kEvent && bh::Often()) a[0] = bh::Next() % 3;
    if (c.shape == S::kEnemyHook || c.shape == S::kEvent) {
        if (bh::Half()) a[0] |= bh::Next() & 0xFFFFFF00u;
    } else if (c.shape == S::kCallee) {
        a[0] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (bh::Often() ? 2 * (bh::Next() % 0x2C) : bh::Next() & 0xFF);
    }
}

// What the group's functions read again after a call: the fight's bits (the
// action hooks re-read them after Msg_OpenScript), the scene byte (fight 16's
// exit hook reads it after Scenario_CallA), the wait count, the banner's
// character, member 1's flags.
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 5) {
    case 0: Mem(at::kFightFlags)[0] = b; break;
    case 1: Mem(at::kMoveCounter)[0] = h & 0x1000000 ? 0x14 : b; break;
    case 2: Mem(at::kWaitTurns)[0] = b; break;
    case 3: Mem(at::kBannerChar)[0] = b; break;
    default: Mem(at::kMember1Flags)[0] = b; break;
    }
}

// BOF3X_BSC_RUN=<unit, lower case> runs that one alone (the controls
// script's shortcut); unset, all thirteen run.
bool Wants(const char* run) {
    const char* const only = std::getenv("BOF3X_BSC_RUN");
    return only == nullptr || *only == 0 || std::strcmp(only, run) == 0;
}

void RunUnit(const char* run, const bh::Clone* clones, unsigned n, const bh::DataTable* tables, unsigned n_tables, int fight,
             int kind) {
    if (!Wants(run)) return;
    g_cur = clones;
    bh::Group g{"boss_sc", clones, n, kCallees, BH_COUNT(kCallees), tables, n_tables, kRegions, BH_COUNT(kRegions),
                &Seed, &Disturb, 6000};
    g.args = &Args;
    g.fight = fight;
    g.kind = kind;
    bh::Run(g);
    g_cur = nullptr;
}

}  // namespace

void SelfTest() {
    RunUnit("b11", kB11, BH_COUNT(kB11), nullptr, 0, 11, -1);
    RunUnit("k12", kK12, BH_COUNT(kK12), kTablesK12, BH_COUNT(kTablesK12), 12, 12);
    RunUnit("b12", kB12, BH_COUNT(kB12), nullptr, 0, 12, -1);
    RunUnit("k13", kK13, BH_COUNT(kK13), kTablesK13, BH_COUNT(kTablesK13), 16, 13);
    RunUnit("k14", kK14, BH_COUNT(kK14), kTablesK14, BH_COUNT(kTablesK14), 16, 14);
    RunUnit("k17", kK17, BH_COUNT(kK17), kTablesK17, BH_COUNT(kTablesK17), 16, 17);
    RunUnit("b16", kB16, BH_COUNT(kB16), nullptr, 0, 16, -1);
    RunUnit("k15", kK15, BH_COUNT(kK15), kTablesK15, BH_COUNT(kTablesK15), 14, 15);
    RunUnit("k53", kK53, BH_COUNT(kK53), kTablesK53, BH_COUNT(kTablesK53), 14, 53);
    RunUnit("b14", kB14, BH_COUNT(kB14), nullptr, 0, 14, -1);
    RunUnit("b46", kB46, BH_COUNT(kB46), nullptr, 0, 46, -1);
    RunUnit("k16", kK16, BH_COUNT(kK16), kTablesK16, BH_COUNT(kTablesK16), 15, 16);
    RunUnit("b15", kB15, BH_COUNT(kB15), nullptr, 0, 15, -1);
}

}  // namespace boss_sc
