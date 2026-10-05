// BOF3X_SHADOW=rest_3b: group R3B's 64 functions through the boss harness's
// engine frame (boss_harness.h, docs/boss_harness.md section 10), once at
// start-up: one boss_harness::Run, Group::engine set. docs/rest_3b.md
// section 3.
//
// The clone rows are tools/band_rows.py's (--group R3B --clones --harness
// boss, 2026-10-04), each read against the disassembly: the EXP helpers and
// the percent clamps are kHelpers (cdecl words, the answer compared: eax
// whole where the callers read it, al for BattleResult_MemberTakesExp); the
// menus' steps and the 45 Effect_Handlers slots are kSteps (void (void), as
// their dispatchers and Effect_ApplyResult call them); the four dispatchers
// are kDispatch with their state cell and their table's own length, the
// tables DataTables (their entries recorders). BOF3X_R3B_ONLY=<substring>
// runs the clones whose name holds it (the controls script's shortcut);
// unset, all 64.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/rest_3b.h"
#include "game/rest_3b_callees.h"
#include "hook/log.h"

namespace rest_3b {
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
constexpr U kRandAt = 0x5B93D2;   // Rand, Capcom's CRT (its name is a macro in symbols.gen.h)
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)

bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret = 0) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, shape};
    return c;
}
// A dispatcher: jmp [table + 4 * byte cell], the byte drawn below `states`.
bh::Clone Disp(const char* name, U base, U size, const void* ours, U cell, std::uint8_t states) {
    bh::Clone c{name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, ours, 0, false, S::kDispatch};
    c.states = states;
    c.state_cell = cell;
    return c;
}

// ===========================================================================
// The clone rows (tools/band_rows.py --group R3B --clones --harness boss)
// ===========================================================================

constexpr bh::CallSite kCalls4468B0[] = {{0x20, 0x446990}, {0x49, 0x4469D0}, {0x74, 0x4469D0}, {0x98, 0x4469D0}};
constexpr bh::CallSite kCalls446990[] = {{0x6, 0x4456C0}};
constexpr bh::CallSite kCalls447290[] = {{0x31, 0x461EB0}, {0x45, 0x445730}, {0x65, 0x587740}, {0x92, 0x4469F0}, {0x98, 0x445730},
                                         {0xAA, 0x587740}, {0xD4, 0x4469F0}, {0xDA, 0x4457F0}, {0xEC, 0x587740}};
constexpr bh::CallSite kCalls4473E0[] = {{0x5, 0x587740}, {0x1C, 0x44A990}};
constexpr bh::CallSite kCalls447880[] = {{0x1D, 0x449FE0}};
constexpr bh::CallSite kCalls447D30[] = {{0x5, 0x587740}, {0x10, 0x44A990}};
constexpr bh::CallSite kCalls447DD0[] = {{0x2E, 0x461EB0}, {0x4A, 0x591E50}, {0x7F, 0x587740}};
constexpr bh::CallSite kCalls448140[] = {{0x5, 0x587740}, {0x1C, 0x44A990}};
constexpr bh::CallSite kCalls448600[] = {{0x1D, 0x449FE0}};
constexpr bh::CallSite kCalls448B40[] = {{0x5, 0x587740}, {0x10, 0x44A990}};
constexpr bh::CallSite kCalls44BED0[] = {{0x0, 0x44FB30}};
constexpr bh::CallSite kCalls44BEE0[] = {{0x3E, 0x5B93D2}, {0x7D, 0x445CF0}};
constexpr bh::CallSite kCalls44BF70[] = {{0xF, 0x445CF0}, {0x75, 0x44FCA0}};
constexpr bh::CallSite kCalls44BFF0[] = {{0x12, 0x445CF0}, {0x3A, 0x44FCA0}};
constexpr bh::CallSite kCalls44C040[] = {{0x26, 0x44ED10}};
constexpr bh::CallSite kCalls44C080[] = {{0xB, 0x44F4B0}, {0x13, 0x44FB30}};
constexpr bh::CallSite kCalls44C0A0[] = {{0x1D, 0x44FB30}, {0x32, 0x44FCE0}};
constexpr bh::CallSite kCalls44C120[] = {{0xD, 0x44F130}};
constexpr bh::CallSite kCalls44C1F0[] = {{0xB, 0x5B93D2}, {0x20, 0x44F4B0}};
constexpr bh::CallSite kCalls44C220[] = {{0xB, 0x44F4B0}, {0x13, 0x44FB30}};
constexpr bh::CallSite kCalls44C240[] = {{0x8, 0x44F4B0}, {0x10, 0x44FB30}};
constexpr bh::CallSite kCalls44C260[] = {{0xB, 0x44F4B0}, {0x13, 0x44FB30}};
constexpr bh::CallSite kCalls44C280[] = {{0xB, 0x44F4B0}, {0x13, 0x44FB30}};
constexpr bh::CallSite kCalls44C2A0[] = {{0x58, 0x44F4B0}};
constexpr bh::CallSite kCalls44C330[] = {{0x12, 0x445CF0}, {0x3D, 0x44FCA0}};
constexpr bh::CallSite kCalls44C380[] = {{0x2, 0x44FBB0}};
constexpr bh::CallSite kCalls44C7C0[] = {{0x2, 0x44FBB0}};
constexpr bh::CallSite kCalls44C7D0[] = {{0x2, 0x44FBB0}};
constexpr bh::CallSite kCalls44C7E0[] = {{0x2, 0x44FC60}};
constexpr bh::CallSite kCalls44C7F0[] = {{0x2, 0x44FC60}};
constexpr bh::CallSite kCalls44C800[] = {{0x29, 0x5B93D2}, {0xC4, 0x44F4B0}};
constexpr bh::CallSite kCalls44C8E0[] = {{0x4A, 0x44F4B0}};
constexpr bh::CallSite kCalls44C940[] = {{0x12, 0x445CF0}, {0x3A, 0x44FCA0}};
constexpr bh::CallSite kCalls44C9C0[] = {{0x19, 0x445CF0}};
constexpr bh::CallSite kCalls44C9F0[] = {{0x61, 0x445CF0}, {0x8D, 0x44F6A0}, {0xA0, 0x446EA0}, {0xAB, 0x446650}};
constexpr bh::CallSite kCalls44CAB0[] = {{0x82, 0x445CF0}};
constexpr bh::CallSite kCalls44CB90[] = {{0x9B, 0x590E30}, {0xB3, 0x445CF0}};
constexpr bh::CallSite kCalls44CC60[] = {{0x11, 0x44ED10}};
constexpr bh::CallSite kCalls44CC90[] = {{0x2, 0x44FBB0}};
constexpr bh::CallSite kCalls44CD00[] = {{0x29, 0x445CF0}};
constexpr bh::CallSite kCalls44CD40[] = {{0x4E, 0x445CF0}};
constexpr bh::CallSite kCalls44CDB0[] = {{0x98, 0x590E30}, {0xAF, 0x445CF0}};
constexpr bh::CallSite kCalls44CE80[] = {{0x4E, 0x445CF0}};
constexpr bh::CallSite kCalls44CEF0[] = {{0x2, 0x44FC60}};
constexpr bh::CallSite kCalls44CF00[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44CF20[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44CF40[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44CF60[] = {{0x0, 0x44FB30}};
constexpr bh::CallSite kCalls44CFC0[] = {{0x12, 0x44FBB0}};
constexpr bh::CallSite kCalls44CFE0[] = {{0x10, 0x44C040}};

#define ROW(name, base, size, calls) C(#name, base, size, calls, BH_N(calls), BH_FN(name), S::kStep)
#define ROW0(name, base, size) C(#name, base, size, nullptr, 0, BH_FN(name), S::kStep)

const bh::Clone kAll64[] = {
    // the result screen's EXP, the percent clamps
    C("BattleResult_AddExp", 0x4468B0, 0xD7, kCalls4468B0, BH_N(kCalls4468B0), BH_FN(BattleResult_AddExp), S::kHelper, kAll),
    C("BattleResult_MemberTakesExp", 0x446990, 0x37, kCalls446990, BH_N(kCalls446990), BH_FN(BattleResult_MemberTakesExp), S::kHelper, 0xFF),
    C("CharId_ToRosterIndex", 0x4469D0, 0x16, nullptr, 0, BH_FN(CharId_ToRosterIndex), S::kHelper, kAll),
    C("Stat_PercentCap999", 0x446F20, 0x30, nullptr, 0, BH_FN(Stat_PercentCap999), S::kHelper, kAll),
    C("Stat_PercentCap9999", 0x446F50, 0x30, nullptr, 0, BH_FN(Stat_PercentCap9999), S::kHelper, kAll),
    C("Stat_PercentCap100", 0x446F80, 0x2E, nullptr, 0, BH_FN(Stat_PercentCap100), S::kHelper, kAll),
    // the command menus' steps and dispatchers
    ROW(BattleTarget_PickParty, 0x447290, 0xF6, kCalls447290),
    ROW(BattleTarget_Cancel, 0x4473E0, 0x42, kCalls4473E0),
    ROW(BattleItem_CloseWait, 0x447880, 0x23, kCalls447880),
    ROW(BattleItem_TargetCancel, 0x447D30, 0x3A, kCalls447D30),
    Disp("BattleItem_SideDispatch", 0x447D70, 0x11, BH_FN(BattleItem_SideDispatch), at::kStep4, 4),
    ROW0(BattleItem_SideBegin, 0x447D90, 0x3B),
    ROW(BattleItem_SidePick, 0x447DD0, 0x86, kCalls447DD0),
    ROW(BattleAttackCmd_Cancel, 0x448140, 0x3D, kCalls448140),
    ROW(BattleItemCmd_CloseWait, 0x448600, 0x23, kCalls448600),
    ROW(BattleItemCmd_TargetCancel, 0x448B40, 0x3A, kCalls448B40),
    Disp("BattleItemCmd_SideDispatch", 0x448B80, 0x11, BH_FN(BattleItemCmd_SideDispatch), at::kStep4, 4),
    Disp("BattleItemCmd_EquipDispatch", 0x448C80, 0x11, BH_FN(BattleItemCmd_EquipDispatch), at::kStep4, 4),
    Disp("Escape_Dispatch", 0x44A000, 0xE, BH_FN(Escape_Dispatch), at::kStep3, 3),
    // Effect_Handlers slots
    ROW(EffectSlot00_Miss, 0x44BED0, 0x5, kCalls44BED0),
    ROW(EffectSlot01_VariedHit, 0x44BEE0, 0x90, kCalls44BEE0),
    ROW(EffectSlot02_HitInflict4, 0x44BF70, 0x7C, kCalls44BF70),
    ROW(EffectSlot03_HalfHitInflict20, 0x44BFF0, 0x41, kCalls44BFF0),
    ROW(EffectSlot04_SkillPower, 0x44C040, 0x39, kCalls44C040),
    ROW(EffectSlot05_Clear80, 0x44C080, 0x18, kCalls44C080),
    ROW(EffectSlot06_HpThirdHit, 0x44C0A0, 0x75, kCalls44C0A0),
    ROW(EffectSlot07_Heal, 0x44C120, 0x20, kCalls44C120),
    ROW0(EffectSlot09_Heal40, 0x44C150, 0xC),
    ROW0(EffectSlot10_Heal100, 0x44C160, 0xC),
    ROW0(EffectSlot11_HealFull, 0x44C170, 0x7D),
    ROW(EffectSlot12_Heal5Clear68, 0x44C1F0, 0x29, kCalls44C1F0),
    ROW(EffectSlot13_Clear80, 0x44C220, 0x18, kCalls44C220),
    ROW(EffectSlot14_Clear8, 0x44C240, 0x15, kCalls44C240),
    ROW(EffectSlot15_Clear100, 0x44C260, 0x18, kCalls44C260),
    ROW(EffectSlot16_ClearBFC, 0x44C280, 0x18, kCalls44C280),
    ROW(EffectSlot17_RaiseDown, 0x44C2A0, 0x84, kCalls44C2A0),
    ROW(EffectSlot18_HalfHitInflict80, 0x44C330, 0x44, kCalls44C330),
    ROW(EffectSlot19_StatMod0, 0x44C380, 0x9, kCalls44C380),
    ROW0(EffectSlot20_ApHeal20, 0x44C390, 0x16),
    ROW0(EffectSlot21_ApHeal100, 0x44C3B0, 0x16),
    ROW(EffectSlot24_StatMod1, 0x44C7C0, 0x9, kCalls44C7C0),
    ROW(EffectSlot25_StatMod2, 0x44C7D0, 0x9, kCalls44C7D0),
    ROW(EffectSlot26_Inflict40, 0x44C7E0, 0x9, kCalls44C7E0),
    ROW(EffectSlot27_Inflict20, 0x44C7F0, 0x9, kCalls44C7F0),
    ROW(EffectSlot28_RaiseQuarter, 0x44C800, 0xD9, kCalls44C800),
    ROW(EffectSlot29_RaiseFull, 0x44C8E0, 0x5F, kCalls44C8E0),
    ROW(EffectSlot30_HalfHitInflict8, 0x44C940, 0x41, kCalls44C940),
    ROW(EffectSlot32_HalfHitPlus1, 0x44C9C0, 0x30, kCalls44C9C0),
    ROW(EffectSlot33_HitDropTurn, 0x44C9F0, 0xB4, kCalls44C9F0),
    ROW(EffectSlot34_HitIgnore8, 0x44CAB0, 0xD4, kCalls44CAB0),
    ROW(EffectSlot35_StatSumHit, 0x44CB90, 0xC6, kCalls44CB90),
    ROW(EffectSlot36_Skill20, 0x44CC60, 0x24, kCalls44CC60),
    ROW(EffectSlot37_StatMod3, 0x44CC90, 0x9, kCalls44CC90),
    ROW(EffectSlot39_ElementHit, 0x44CD00, 0x3C, kCalls44CD00),
    ROW(EffectSlot40_StatAAHit, 0x44CD40, 0x61, kCalls44CD40),
    ROW(EffectSlot41_StatSumHit2, 0x44CDB0, 0xC2, kCalls44CDB0),
    ROW(EffectSlot42_DoubleHit20, 0x44CE80, 0x61, kCalls44CE80),
    ROW(EffectSlot43_Inflict10, 0x44CEF0, 0x9, kCalls44CEF0),
    ROW(EffectSlot44_Skill66, 0x44CF00, 0x15, kCalls44CF00),
    ROW(EffectSlot45_Skill65, 0x44CF20, 0x15, kCalls44CF20),
    ROW(EffectSlot46_Skill62, 0x44CF40, 0x15, kCalls44CF40),
    ROW(EffectSlot47_MissMark200, 0x44CF60, 0x53, kCalls44CF60),
    ROW(EffectSlot48_StatMod0Skill55, 0x44CFC0, 0x19, kCalls44CFC0),
    ROW(EffectSlot49_Skill5D, 0x44CFE0, 0x15, kCalls44CFE0),
};
static_assert(sizeof kAll64 / sizeof kAll64[0] == 64, "64 functions");
#undef ROW
#undef ROW0

// ===========================================================================
// The callees the standard sets lack or record too coarsely, the tables, the
// regions
// ===========================================================================

// BattleResult_AddExp reads the slot's character byte +0x89 again for each
// CharId_ToRosterIndex call: its stand-in moves one member's byte half the
// time (inside the ids the roster table answers 0..7 for), the old byte noted.
U RosterMoves(const U*, U answer) {
    const U n = bh::Noise();
    if (n & 1) {
        unsigned char* const id = Mem(at::kParty + (n >> 8) % 3 * at::kPartyStride + 0x89);
        bh::Note(id[0]);
        id[0] = static_cast<unsigned char>((n >> 12) % 24);
    }
    return answer;
}
// ... and the count 0x904AB0 again after the member test: its stand-in moves
// it half the time (0..3), the old byte noted.
U CountMoves(const U*, U answer) {
    const U n = bh::Noise();
    if (n & 1) {
        bh::Note(Mem(at::kPartyCount)[0]);
        Mem(at::kPartyCount)[0] = static_cast<unsigned char>((n >> 8) % 4);
    }
    return answer;
}
// Char_AbilityList answers a list the caller indexes by a byte: a pointer
// into the harness's text buffer (0x200 bytes, a compared region), so a row
// of 0..255 stays inside it.
U AbilityListEffect(const U*, U) { return Key(bh::TextBuffer() + bh::Noise() % 0x100); }

// A record as the originals index it by an actor / target byte (a member
// below 3, else the enemy object byte - 3, the difference in 32 bits).
unsigned char* Rec(unsigned n, U party_at, U enemy_at) {
    return n < 3 ? Mem(at::kParty + n * at::kPartyStride + party_at)
                 : Mem(at::kEnemies + static_cast<U>(static_cast<std::int32_t>(n) - 3) * at::kEnemyStride + enemy_at);
}
// Battle_CalcDamage answers, a third of the time, the target's HP less 1, 0
// or plus 1 (slot 2 compares the delta with it: its boundary).
U HitAtHpEffect(const U*, U answer) {
    const U n = bh::Noise();
    if (n % 3 != 0) return answer;
    const unsigned t = Mem(at::kTarget)[0];
    const U hp = move_script::Word(Rec(t, 0x98, 0xA4));
    return (answer & 0xFFFF0000u) | ((hp + (n >> 8) % 3 - 1) & 0xFFFF);
}
// 0x44FCE0 answers, a third of the time, the target enemy object's +0xB6 less
// 1, 0 or plus 1 (slot 6 subtracts it and clamps a negative delta).
U ShareAtStatEffect(const U*, U answer) {
    const U n = bh::Noise();
    if (n % 3 != 0) return answer;
    const unsigned t = Mem(at::kTarget)[0];
    const U v = move_script::Word(Mem(at::kEnemies + static_cast<U>(static_cast<std::int32_t>(t) - 3) * at::kEnemyStride + 0xB6));
    return (answer & 0xFFFF0000u) | ((v + (n >> 8) % 3 - 1) & 0xFFFF);
}
// Rand: 15 bits as the CRT's (the engine set's form), and the target moved
// half the time (0..10, noted first): slots 1, 12 and 28 read it again after.
U RandMovesTarget(const U*, U answer) {
    const U n = bh::Noise();
    if (n & 1) {
        bh::Note(Mem(at::kTarget)[0]);
        Mem(at::kTarget)[0] = static_cast<unsigned char>((n >> 8) % 11);
    }
    return answer & 0x7FFF;
}
// Battle_ClearStatus notes the two raise counters as it is called (slots 17,
// 28 and 29 count one up around the call): a store moved across the call is
// compared, not hidden.
U ClearNotesCounts(const U*, U answer) {
    bh::Note(Mem(at::kPartyUp)[0], Mem(at::kEnemiesLeft)[0]);
    return answer;
}

const bh::Callee kCallees[] = {
    // the group's own, called directly (the member test and the roster index by
    // BattleResult_AddExp, slot 4 by slots 44..46 and 49's tail jump). The
    // words carry the caller's leftovers above the byte each reads (capstone:
    // and esi, 0xFF / and eax, 0xFF at their entries).
    {"BattleResult_MemberTakesExp", 0x446990, KeyOf(&::BattleResult_MemberTakesExp), 1, {kU8}, bh::Answer::kFlag, 0, 0, {},
     &CountMoves},
    {"CharId_ToRosterIndex", 0x4469D0, KeyOf(&::CharId_ToRosterIndex), 1, {kU8}, bh::Answer::kByte, 0, 7, {}, &RosterMoves},
    {"EffectSlot04_SkillPower", 0x44C040, KeyOf(&::EffectSlot04_SkillPower), 0, {}, bh::Answer::kGarbage, 0, 0},
    // ours, outside the engine set: Effect_SkillDamage reads target & 0xFF,
    // power & 0xFFFF, psi & 0xFF and not the caster (battle_damage.cpp);
    // Effect_HealAmount target & 0xFF, not the caster
    {"Effect_SkillDamage", bof3::addr::Effect_SkillDamage, KeyOf(&::Effect_SkillDamage), 4, {0, kU8, kU16, kU8},
     bh::Answer::kGarbage, 0, 0},
    {"Effect_HealAmount", bof3::addr::Effect_HealAmount, KeyOf(&::Effect_HealAmount), 2, {0, kU8}, bh::Answer::kGarbage, 0, 0},
    // BE4's: its word whole (BattleTarget_PickParty hands it Battle_WrapIndex's eax); an actor 0..10 or 0xFF
    {"Battle_PrevTarget", bof3::addr::Battle_PrevTarget, KeyOf(&::Battle_PrevTarget), 1, {kAll}, bh::Answer::kByte, 0xFF, 10},
    // the low byte of the member and the type (al / cl loaded over leftovers), the 1 whole
    {"Char_AbilityList", bof3::addr::Char_AbilityList, KeyOf(&::Char_AbilityList), 3, {kU8, kU8, kAll}, bh::Answer::kGarbage, 0, 0,
     {}, &AbilityListEffect},
    // R3D's (this wave, raw until it merges): each takes one pushed immediate
    {"0x44FBB0", at::kStatMod, at::kStatMod, 1, {kAll}, bh::Answer::kFlag, 0, 0},
    {"0x44FC60", at::kInflictMiss, at::kInflictMiss, 1, {kAll}, bh::Answer::kFlag, 0, 0},
    {"0x44FCA0", at::kInflict, at::kInflict, 1, {kAll}, bh::Answer::kFlag, 0, 0},
    {"0x44FCE0", at::kHpShare, at::kHpShare, 1, {kAll}, bh::Answer::kGarbage, 0, 0, {}, &ShareAtStatEffect},
    // the engine set's rows, louder (the same masks): the answers at the
    // slots' boundaries, the target moved by Rand, the counters noted
    {"Battle_CalcDamage", bof3::addr::Battle_CalcDamage, KeyOf(&::Battle_CalcDamage), 3, {kU8, kU8, kU16}, bh::Answer::kGarbage, 0, 0,
     {}, &HitAtHpEffect},
    {"Rand", kRandAt, kRandAt, 0, {}, bh::Answer::kRand, 0, 0, {}, &RandMovesTarget},
    // Battle_ActorIsOut answers al 0 or 1 (symbols.toml, its disassembly): the standard kFlag's "not 0" always
    // has bit 4, so a test of al & 0xFE could not be told from al & 0xFF (control 7)
    {"Battle_ActorIsOut", bof3::addr::Battle_ActorIsOut, KeyOf(&::Battle_ActorIsOut), 1, {kU8}, bh::Answer::kBool, 0, 0},
    {"Battle_ClearStatus", bof3::addr::Battle_ClearStatus, KeyOf(&::Battle_ClearStatus), 2, {kU8, kAll}, bh::Answer::kGarbage, 0, 0,
     {}, &ClearNotesCounts},
};

const bh::DataTable kTables[] = {
    {at::kItemSideSteps, 4}, {at::kItemCmdSideSteps, 4}, {at::kItemCmdEquipSteps, 4}, {at::kEscapeStates, 3},
};

// The cells beyond the engine frame the group reads or writes.
const bh::Region kRegions[] = {
    {0x903B24, 0x903F90 - 0x903B24},   // CharacterRecords past the engine frame's 0x903A50..0x903B24 (records 0..7)
    {at::kRepeatLatch, 4},             // Input_AutoRepeat's latch (BattleItem_SideBegin zeroes it)
    // past the window records to the chapter bytes, and past those: a party
    // record by an actor or a target byte 3..10, as slots 6 and 47 index it
    {0x803478, 0x8034E0 - 0x803478},
    {0x8034F0, 0x803B70 - 0x8034F0},
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
std::uint16_t Word16(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<std::uint16_t>(bh::Next());
    const U* v = often.begin();
    return static_cast<std::uint16_t>(v[bh::Next() % often.size()]);
}
unsigned char* Member(unsigned n) { return Mem(at::kParty + (n % 3) * at::kPartyStride); }
unsigned char* EnemyN(unsigned n) { return Mem(at::kEnemies + (n % 8) * at::kEnemyStride); }
U Upper() { return bh::Half() ? bh::Next() & 0xFFFFFF00u : 0; }

// The keys: the confirm and cancel masks single bits, the pressed word one of
// them, 0x10, a direction, both, none or anything.
void Keys() {
    const U confirm = BH_PICK(0x20, 0x40, 0x2000, 0x60);
    const U cancel = BH_PICK(0x40, 0x80, 0x100, 0x4000);
    SetWord(Mem(at::kConfirmButtons), confirm);
    SetWord(Mem(at::kCancelButtons), cancel);
    const U picks[] = {0, 0x1000, 0x2000, 0x4000, 0x8000, 0x5000, 0xA000, confirm, cancel, cancel | confirm, 0x10};
    SetWord(Mem(at::kInputPressed), bh::Often() ? bh::Pick(picks, BH_COUNT(picks)) : bh::Next());
}

// Every round: the actor and the target 0..10 (past 10 an enemy object runs
// off the image's end 0x93F000, a fault on both sides, not a count), the
// members' characters (0..23 index the roster table's 24; 0x0A slot 6's test),
// their and the enemies' status words, HP and maxima at their boundaries.
void BattleFrame() {
    Mem(at::kActor)[0] = static_cast<unsigned char>(bh::Next() % 11);
    Mem(at::kTarget)[0] = static_cast<unsigned char>(bh::Next() % 11);
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = Member(m);
        p[0x89] = bh::Half() ? 0x0A : static_cast<unsigned char>(bh::Next() % 24);
        const U max = Word16({1, 2, 4, 5, 99, 999, 9999});
        SetWord(p + 0xA0, max);
        SetWord(p + 0x98, bh::Half() ? max - bh::Next() % 3 : Word16({0, 1, 0xFFFF}));
        SetWord(p + 0x90, static_cast<std::uint16_t>(bh::Half() ? bh::Next() | 0x4000 : bh::Next() & ~0x4000u));
        if (bh::Half()) p[0x135] = static_cast<unsigned char>(p[0x135] ^ 0x04);   // the dword +0x134's bit 10
    }
    for (unsigned n = 0; n < 8; ++n) {
        unsigned char* const e = EnemyN(n);
        const U max = Word16({1, 2, 4, 5, 99, 999, 0xFFFF, 0xFFFE});
        SetWord(e + 0xB0, max);
        SetWord(e + 0xA4, bh::Half() ? max - bh::Next() % 3 : Word16({0, 1, 0xFFFF}));
        SetWord(e + 0x92, static_cast<std::uint16_t>(bh::Half() ? bh::Next() | 0x4000 : bh::Next() & ~0x4000u));
    }
    SetWord(Mem(at::kAbility), Word16({0, 1, 0x55, 0x5D, 0x62, 0x65, 0x66, 0xE2}) % 0xE4);
    SetWord(Mem(at::kPowerAdd2), Word16({0, 1, 139, 140, 141, 142, 0xFFFF}));
    Mem(at::kPartyCount)[0] = static_cast<unsigned char>(bh::Next() % 4);
}

// The menus' frame: the keys, the command's target byte at the party's ends,
// the window closing byte, the list's page and row, the ability actor, the
// command word +2 inside the ability rows.
void MenuFrame() {
    Keys();
    unsigned char* const c = bh::Pointer(at::kCommand);
    c[0] = Byte({0, 1, 2, 3, 0x40, 0x80, 0xC0, 0xFF, 0x7F});
    SetWord(c + 2, Word16({0, 1, 0x20, 0x97, 0xE3}) % 0xE4);
    Mem(at::kWin16Closing)[0] = Byte({0, 0, 1});
    Mem(at::kWin16Page)[0] = Byte({0, 1, 2, 3});
    SetLong(Mem(at::kWin16Cursor), static_cast<std::int32_t>(bh::Next()));
    Mem(at::kAbilityActor)[0] = Byte({0, 1, 2});
}

void Seed(unsigned k) {
    const bh::Clone& c = g_cur[k];
    BattleFrame();
    if (c.base >= 0x447290 && c.base < 0x44BED0) MenuFrame();
    switch (c.base) {
    case 0x4468B0:   // BattleResult_AddExp: the EXP of every record near the cap
        for (unsigned r = 0; r < 8; ++r) {
            const U exp = bh::Often() ? 9999999u - BH_PICK(0, 1, 2, 100, 1000) : bh::Next() % 10000000u;
            SetLong(Mem(at::kCharRecords + r * at::kCharStride + 0xC), static_cast<std::int32_t>(exp));
        }
        break;
    default:
        break;
    }
}

// The helpers' words: bytes inside what each indexes with garbage above half
// the time (the originals read the low byte); the products at their bounds.
void Args(unsigned k, U* a) {
    const bh::Clone& c = g_cur[k];
    switch (c.base) {
    case 0x4468B0:
        a[0] = bh::Often() ? BH_PICK(0, 1, 2, 3, 100, 1000, 9999999, 10000000, 0xFFFFFFFF) : bh::Next();
        break;
    case 0x446990:
        a[0] = Upper() | (bh::Often() ? bh::Next() % 3 : bh::Next() % 11);
        break;
    case 0x4469D0:
        a[0] = Upper() | (bh::Often() ? bh::Next() % 24 : bh::Next() & 0xFF);
        break;
    case 0x446F20:
    case 0x446F50:
    case 0x446F80: {
        const U v[] = {0, 1, 50, 99, 100, 101, 999, 1000, 9999, 10000, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFF, 0xFFFFFF9C};
        const U p[] = {0, 1, 50, 99, 100, 101, 150, 200, 0xFFFFFFFF, 0x10000, 0x7FFFFFFF};
        a[0] = bh::Often() ? bh::Pick(v, BH_COUNT(v)) : bh::Next();
        a[1] = bh::Often() ? bh::Pick(p, BH_COUNT(p)) : bh::Next();
        break;
    }
    default:
        break;
    }
}

// What the group's functions read again after a call: the actor, the target
// (0..10), the count, a member's character byte, the status words' 0x4000,
// the command's target byte, 0x939FE8. From the hash only.
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 8) {
    case 0: Mem(at::kTarget)[0] = static_cast<unsigned char>(b % 11); break;
    case 1: Mem(at::kActor)[0] = static_cast<unsigned char>(b % 11); break;
    case 2: Mem(at::kPartyCount)[0] = static_cast<unsigned char>(b % 4); break;
    case 3: Member(b)[0x89] = static_cast<unsigned char>((b >> 2) % 24); break;
    case 4: Member(b)[0x91] = static_cast<unsigned char>(Member(b)[0x91] ^ 0x40); break;
    case 5: EnemyN(b)[0x93] = static_cast<unsigned char>(EnemyN(b)[0x93] ^ 0x40); break;
    case 6: bh::Pointer(at::kCommand)[0] = b; break;
    default: SetWord(Mem(at::kPowerAdd2), h >> 20); break;
    }
}

}  // namespace

void SelfTest() {
    static bh::Clone clones[64];
    unsigned n = 0;
    const char* const only = std::getenv("BOF3X_R3B_ONLY");
    for (const bh::Clone& c : kAll64)
        if (only == nullptr || *only == 0 || std::strstr(c.name, only) != nullptr) clones[n++] = c;
    if (n == 0) bof3::Fatal("rest_3b: BOF3X_R3B_ONLY=%s names no clone", only);
    g_cur = clones;
    bh::Group g{"rest_3b", clones, n, kCallees, BH_COUNT(kCallees), kTables, BH_COUNT(kTables), kRegions, BH_COUNT(kRegions),
                &Seed, &Disturb, 6000};
    g.args = &Args;
    g.engine = true;
    bh::Run(g);
    g_cur = nullptr;
}

}  // namespace rest_3b
