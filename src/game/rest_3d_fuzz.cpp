// BOF3X_SHADOW=rest_3d: group R3D's 36 functions through the boss harness's
// engine frame (boss_harness.h, docs/boss_harness.md section 10), once at
// start-up: one boss_harness::Run, Group::engine set. docs/rest_3d.md
// section 5.
//
// The clone rows are tools/band_rows.py's (--group R3D --clones --harness
// boss, 2026-10-04, through the round's band14.py), each read against the
// disassembly: the eighteen Effect_Handlers slots are kSteps (void (void), as
// Effect_ApplyResult calls them); the helpers kHelpers (cdecl words, al or eax
// compared where their callers read it); DragonCmd_PartDispatch a kDispatch
// with state_cell 0x904AA3, the byte drawn below DragonCmd_Parts' seven, the
// table a DataTable (its entries, BE5's, recorders). BOF3X_R3D_ONLY=<substring>
// runs the clones whose name holds it (the controls script's shortcut); unset,
// all 36.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/rest_3d.h"
#include "game/rest_3d_callees.h"
#include "hook/log.h"

namespace rest_3d {
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

bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, S shape, U ret = 0) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, shape};
    return c;
}

// ===========================================================================
// The clone rows (tools/band_rows.py --group R3D --clones --harness boss)
// ===========================================================================

constexpr bh::CallSite kCalls44E4B0[] = {{0x0, 0x5B93D2}, {0x16, 0x44F6A0}};
constexpr bh::CallSite kCalls44E530[] = {{0x0, 0x44FB30}};
constexpr bh::CallSite kCalls44E580[] = {{0x2F, 0x44ED10}};
constexpr bh::CallSite kCalls44E5D0[] = {{0xD, 0x44F940}};
constexpr bh::CallSite kCalls44E640[] = {{0xB, 0x44FCE0}, {0x33, 0x44FCE0}};
constexpr bh::CallSite kCalls44E6C0[] = {{0x0, 0x44FB30}};
constexpr bh::CallSite kCalls44E720[] = {{0x6B, 0x590660}, {0xF4, 0x453C00}, {0x124, 0x44FDE0}, {0x13D, 0x453300}};
constexpr bh::CallSite kCalls44E8B0[] = {{0x0, 0x44FB30}, {0x34, 0x453300}, {0x61, 0x453300}};
constexpr bh::CallSite kCalls44E920[] = {{0xD, 0x44F6A0}, {0x46, 0x453300}, {0x70, 0x453300}, {0x8C, 0x453DA0}, {0x94, 0x44FB30}};
constexpr bh::CallSite kCalls44E9C0[] = {{0x0, 0x44FB30}, {0x3C, 0x453300}, {0x71, 0x453300}};
constexpr bh::CallSite kCalls44EA40[] = {{0x0, 0x44FB30}, {0x7, 0x44FCA0}, {0x11, 0x44FCA0}, {0x18, 0x44FCA0}};
constexpr bh::CallSite kCalls44EA70[] = {{0x10, 0x44D8B0}};
constexpr bh::CallSite kCalls44EA90[] = {{0xB, 0x44FCE0}, {0x33, 0x44FCE0}};
constexpr bh::CallSite kCalls44EB10[] = {{0x0, 0x44FB30}, {0x17, 0x44F880}, {0x2C, 0x44F1D0}};
constexpr bh::CallSite kCalls44EB50[] = {{0x10, 0x44C5C0}};
constexpr bh::CallSite kCalls44EB70[] = {{0x15, 0x445CF0}, {0x2F, 0x445CF0}};
constexpr bh::CallSite kCalls44EBE0[] = {{0x12, 0x445CF0}, {0x46, 0x44F6A0}, {0x54, 0x44FCA0}};
constexpr bh::CallSite kCalls44EC40[] = {{0x3, 0x44FB30}, {0x18, 0x4456C0}, {0x54, 0x4456C0}};
constexpr bh::CallSite kCalls44F1D0[] = {{0x80, 0x44F460},  {0x9C, 0x44F4B0},  {0xA9, 0x446EA0},  {0xAF, 0x446650},
                                         {0xC5, 0x44F460},  {0xEA, 0x446EA0},  {0x163, 0x44F460}, {0x172, 0x44F490},
                                         {0x1C0, 0x44F4B0}, {0x1CD, 0x446EA0}, {0x1D3, 0x446650}, {0x1F0, 0x44F4B0},
                                         {0x1FD, 0x446EA0}, {0x203, 0x446650}, {0x223, 0x454DC0}, {0x23E, 0x446BB0},
                                         {0x25B, 0x454DC0}, {0x276, 0x446BB0}};
constexpr bh::CallSite kCalls44F6A0[] = {{0x84, 0x44F770}, {0x9A, 0x5B93D2}};
constexpr bh::CallSite kCalls44F880[] = {{0x74, 0x44F770}, {0x8A, 0x5B93D2}};
constexpr bh::CallSite kCalls44F940[] = {{0x84, 0x44FA10}, {0x9A, 0x5B93D2}};
constexpr bh::CallSite kCalls44FA70[] = {{0x6E, 0x44F770}, {0x84, 0x5B93D2}};
constexpr bh::CallSite kCalls44FBB0[] = {{0x0, 0x44FB30}, {0x12, 0x44F6A0}, {0x3D, 0x44F650}, {0x49, 0x453300}};
constexpr bh::CallSite kCalls44FC10[] = {{0xD, 0x44F6A0}, {0x38, 0x44F650}, {0x44, 0x453300}};
constexpr bh::CallSite kCalls44FC60[] = {{0x0, 0x44FB30}, {0x12, 0x44F6A0}, {0x2C, 0x44F1D0}};
constexpr bh::CallSite kCalls44FCA0[] = {{0xD, 0x44F6A0}, {0x27, 0x44F1D0}};
constexpr bh::CallSite kCalls44FCE0[] = {{0x6A, 0x44EE80}, {0x75, 0x5B93D2}};

bh::Clone Dispatch() {
    bh::Clone c{"DragonCmd_PartDispatch", 0x44FF00, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, BH_FN(DragonCmd_PartDispatch), 0, false,
                S::kDispatch};
    c.states = DragonCmd_Parts_count;
    c.state_cell = at::kDragonPart;
    return c;
}

const bh::Clone kAll36[] = {
    // Effect_Handlers slots 112..129
    C("Effect112_HpToOne", 0x44E4B0, 0x71, kCalls44E4B0, BH_N(kCalls44E4B0), BH_FN(Effect112_HpToOne), S::kStep),
    C("Effect113_ActorNullDamage", 0x44E530, 0x42, kCalls44E530, BH_N(kCalls44E530), BH_FN(Effect113_ActorNullDamage), S::kStep),
    C("Effect114_SkillApDamage", 0x44E580, 0x42, kCalls44E580, BH_N(kCalls44E580), BH_FN(Effect114_SkillApDamage), S::kStep),
    C("Effect115_HpToZero", 0x44E5D0, 0x64, kCalls44E5D0, BH_N(kCalls44E5D0), BH_FN(Effect115_HpToZero), S::kStep),
    C("Effect116_HpLessDefence", 0x44E640, 0x76, kCalls44E640, BH_N(kCalls44E640), BH_FN(Effect116_HpLessDefence), S::kStep),
    C("Effect117_PartyFlag400Others", 0x44E6C0, 0x60, kCalls44E6C0, BH_N(kCalls44E6C0), BH_FN(Effect117_PartyFlag400Others),
      S::kStep),
    C("Effect118_RaiseCharByte1E", 0x44E720, 0x181, kCalls44E720, BH_N(kCalls44E720), BH_FN(Effect118_RaiseCharByte1E), S::kStep),
    C("Effect119_ActorFlag1000", 0x44E8B0, 0x68, kCalls44E8B0, BH_N(kCalls44E8B0), BH_FN(Effect119_ActorFlag1000), S::kStep),
    C("Effect120_TargetFlag800", 0x44E920, 0x99, kCalls44E920, BH_N(kCalls44E920), BH_FN(Effect120_TargetFlag800), S::kStep),
    C("Effect121_ActorFlag4000", 0x44E9C0, 0x78, kCalls44E9C0, BH_N(kCalls44E9C0), BH_FN(Effect121_ActorFlag4000), S::kStep),
    C("Effect122_InflictThree", 0x44EA40, 0x21, kCalls44EA40, BH_N(kCalls44EA40), BH_FN(Effect122_InflictThree), S::kStep),
    C("Effect123_Ability6A", 0x44EA70, 0x15, kCalls44EA70, BH_N(kCalls44EA70), BH_FN(Effect123_Ability6A), S::kStep),
    C("Effect124_HalfHpLessDefence", 0x44EA90, 0x76, kCalls44EA90, BH_N(kCalls44EA90), BH_FN(Effect124_HalfHpLessDefence), S::kStep),
    C("Effect125_Inflict8Roll80", 0x44EB10, 0x35, kCalls44EB10, BH_N(kCalls44EB10), BH_FN(Effect125_Inflict8Roll80), S::kStep),
    C("Effect126_Ability4EDrainAp", 0x44EB50, 0x15, kCalls44EB50, BH_N(kCalls44EB50), BH_FN(Effect126_Ability4EDrainAp), S::kStep),
    C("Effect127_AttackDoubledKind4", 0x44EB70, 0x6F, kCalls44EB70, BH_N(kCalls44EB70), BH_FN(Effect127_AttackDoubledKind4),
      S::kStep),
    C("Effect128_HalfAttackInflict4", 0x44EBE0, 0x5B, kCalls44EBE0, BH_N(kCalls44EBE0), BH_FN(Effect128_HalfAttackInflict4),
      S::kStep),
    C("Effect129_TargetSoleFlag40000", 0x44EC40, 0xC7, kCalls44EC40, BH_N(kCalls44EC40), BH_FN(Effect129_TargetSoleFlag40000),
      S::kStep),
    // the helpers
    C("Battle_PsiStatusDeathAffinity", 0x44F030, 0xFD, nullptr, 0, BH_FN(Battle_PsiStatusDeathAffinity), S::kHelper, kAll),
    C("Battle_InflictStatus", 0x44F1D0, 0x281, kCalls44F1D0, BH_N(kCalls44F1D0), BH_FN(Battle_InflictStatus), S::kHelper),
    C("Battle_LacksAccessory", 0x44F460, 0x2A, nullptr, 0, BH_FN(Battle_LacksAccessory), S::kHelper, kU8),
    C("Battle_LacksArmour", 0x44F490, 0x20, nullptr, 0, BH_FN(Battle_LacksArmour), S::kHelper, kU8),
    C("Effect_StepStatByte", 0x44F650, 0x41, nullptr, 0, BH_FN(Effect_StepStatByte), S::kHelper),
    C("Battle_StatusResisted", 0x44F6A0, 0xC9, kCalls44F6A0, BH_N(kCalls44F6A0), BH_FN(Battle_StatusResisted), S::kHelper, kU8),
    C("Battle_StatusResistRate", 0x44F770, 0x101, nullptr, 0, BH_FN(Battle_StatusResistRate), S::kHelper, kAll),
    C("Battle_StatusResistedMask", 0x44F880, 0xB9, kCalls44F880, BH_N(kCalls44F880), BH_FN(Battle_StatusResistedMask), S::kHelper,
      kU8),
    C("Battle_StatusResisted20", 0x44F940, 0xC9, kCalls44F940, BH_N(kCalls44F940), BH_FN(Battle_StatusResisted20), S::kHelper, kU8),
    C("Battle_StatusResistRate20", 0x44FA10, 0x5B, nullptr, 0, BH_FN(Battle_StatusResistRate20), S::kHelper, kAll),
    C("Battle_StatusResisted80", 0x44FA70, 0xB3, kCalls44FA70, BH_N(kCalls44FA70), BH_FN(Battle_StatusResisted80), S::kHelper, kU8),
    C("Effect_NoHitReaction", 0x44FB30, 0x75, nullptr, 0, BH_FN(Effect_NoHitReaction), S::kHelper),
    C("Effect_RollStatStepQuiet", 0x44FBB0, 0x54, kCalls44FBB0, BH_N(kCalls44FBB0), BH_FN(Effect_RollStatStepQuiet), S::kHelper, kU8),
    C("Effect_RollStatStep", 0x44FC10, 0x4F, kCalls44FC10, BH_N(kCalls44FC10), BH_FN(Effect_RollStatStep), S::kHelper, kU8),
    C("Effect_RollInflictQuiet", 0x44FC60, 0x37, kCalls44FC60, BH_N(kCalls44FC60), BH_FN(Effect_RollInflictQuiet), S::kHelper, kU8),
    C("Effect_RollInflict", 0x44FCA0, 0x32, kCalls44FCA0, BH_N(kCalls44FCA0), BH_FN(Effect_RollInflict), S::kHelper, kU8),
    C("Effect_HpBasedDamage", 0x44FCE0, 0xF2, kCalls44FCE0, BH_N(kCalls44FCE0), BH_FN(Effect_HpBasedDamage), S::kHelper, kAll),
    // the Dragon command's part dispatcher
    Dispatch(),
};
static_assert(sizeof kAll36 / sizeof kAll36[0] == 36, "36 functions");

// ===========================================================================
// The callees the standard set lacks or records too coarsely, the table, the
// regions
// ===========================================================================

// The rate lookups' stand-ins: the rolls read the answer's ax (movsx) and
// stop at -1, so a third of the answers are 0xFFFF; the rest small rates of
// either sign, or anything.
U RateEffect(const U*, U answer) {
    const U n = bh::Noise();
    switch (n % 6) {
    case 0:
    case 1: return (answer & 0xFFFF0000u) | 0xFFFF;
    case 2: return (n >> 8) % 201;
    case 3: return static_cast<U>(-static_cast<std::int32_t>((n >> 8) % 301));
    default: return answer;
    }
}

// The target moved half the time (the old byte noted first, so a store ours
// makes before the call is compared, not wiped): the slots read 0x904B54 again
// after Battle_CalcDamage and Effect_HpBasedDamage, some to pick the record,
// some not - the side of slots 116 / 124 / 127 is the read before the call.
void MoveTarget(U n) {
    if ((n & 1) == 0) return;
    bh::Note(Mem(at::kTarget)[0]);
    Mem(at::kTarget)[0] = static_cast<unsigned char>((n >> 8) % 11);
}
U CalcDamageEffect(const U*, U answer) {
    MoveTarget(bh::Noise());
    return answer;
}
// Effect_HpBasedDamage's: the target moved, then half the time an answer
// within 2 of the (new) target's DEF, so slots 116 / 124's clamp at 0 sees
// -2..2 (the garbage answer reaches -1 once in 65,536).
U HpDamageEffect(const U*, U answer) {
    const U n = bh::Noise();
    MoveTarget(n);
    if ((n & 2) == 0) return answer;
    const unsigned t = Mem(at::kTarget)[0];
    const U def = t <= 2 ? move_script::Word(Mem(at::kParty + t * at::kPartyStride + 0xA6))
                         : move_script::Word(Mem(at::kEnemies + (t - 3) * at::kEnemyStride + 0xB6));
    return (answer & 0xFFFF0000u) | ((def + (n >> 16) % 5 - 2) & 0xFFFF);
}

// Masks narrowed to what each callee reads, where the caller pushes a whole
// register whose upper bytes are its own leftovers (a different value in the
// copy and in ours): each read cited (capstone, 2026-10-04).
const bh::Callee kCallees[] = {
    // the group's own, called directly
    {"Effect_NoHitReaction", 0x44FB30, KeyOf(&::Effect_NoHitReaction), 0, {}, bh::Answer::kGarbage, 0, 0},
    // 0x44F6A0 / 0x44F940 read only the second word, and hand it to the rate, which reads its byte (cmp cl, 2)
    {"Battle_StatusResisted", 0x44F6A0, KeyOf(&::Battle_StatusResisted), 2, {0, kU8}, bh::Answer::kFlag, 0, 0},
    {"Battle_StatusResisted20", 0x44F940, KeyOf(&::Battle_StatusResisted20), 2, {0, kU8}, bh::Answer::kFlag, 0, 0},
    // 0x44F880: the target's byte and the mask's nine bits (and edx, 0x1FF)
    {"Battle_StatusResistedMask", 0x44F880, KeyOf(&::Battle_StatusResistedMask), 3, {0, kU8, 0x1FF}, bh::Answer::kFlag, 0, 0},
    // the rates: the actor's byte (cmp cl, 2; and ecx, 0xFF), the mask's 0x1C0 (test dl / dh; test edx, 0x1C0) and 0x20 (dl)
    {"Battle_StatusResistRate", 0x44F770, KeyOf(&::Battle_StatusResistRate), 2, {kU8, 0x1C0}, bh::Answer::kGarbage, 0, 0, {},
     &RateEffect},
    {"Battle_StatusResistRate20", 0x44FA10, KeyOf(&::Battle_StatusResistRate20), 2, {kU8, 0x20}, bh::Answer::kGarbage, 0, 0, {},
     &RateEffect},
    // the inflict: the target's byte (cmp bl, 2; and esi, 0xFF; every callee it hands the word on reads the byte) and
    // the status's low word (its byte; test ah, 8)
    {"Battle_InflictStatus", 0x44F1D0, KeyOf(&::Battle_InflictStatus), 2, {kU8, kU16}, bh::Answer::kGarbage, 0, 0},
    {"Battle_LacksAccessory", 0x44F460, KeyOf(&::Battle_LacksAccessory), 2, {kU8, kU8}, bh::Answer::kFlag, 0, 0},
    {"Battle_LacksArmour", 0x44F490, KeyOf(&::Battle_LacksArmour), 2, {kU8, kU8}, bh::Answer::kFlag, 0, 0},
    // the step: the word's low half (mov bx, [esp+8]; movsx esi, bx) and the stat's byte (and eax, 0xFF)
    {"Effect_StepStatByte", 0x44F650, KeyOf(&::Effect_StepStatByte), 2, {kU16, kU8}, bh::Answer::kGarbage, 0, 0},
    {"Effect_RollInflict", 0x44FCA0, KeyOf(&::Effect_RollInflict), 1, {kAll}, bh::Answer::kFlag, 0, 0},
    {"Effect_HpBasedDamage", 0x44FCE0, KeyOf(&::Effect_HpBasedDamage), 1, {kAll}, bh::Answer::kGarbage, 0, 0, {},
     &HpDamageEffect},
    // the engine set's row (both actors' bytes, the element word), louder: the target moved (CalcDamageEffect)
    {"Battle_CalcDamage", bof3::addr::Battle_CalcDamage, KeyOf(&::Battle_CalcDamage), 3, {kU8, kU8, kU16}, bh::Answer::kGarbage,
     0, 0, {}, &CalcDamageEffect},
    // ours already, not in the engine set
    // Effect_SkillDamage: the caster unread, the target's byte, the power's low word, the psi word's byte
    {"Effect_SkillDamage", bof3::addr::Effect_SkillDamage, KeyOf(&::Effect_SkillDamage), 4, {0, kU8, kU16, kU8},
     bh::Answer::kGarbage, 0, 0},
    // Battle_ElementAffinity: the target's byte, the mask's five element bits
    {"Battle_ElementAffinity", bof3::addr::Battle_ElementAffinity, KeyOf(&::Battle_ElementAffinity), 2, {kU8, 0x1F},
     bh::Answer::kGarbage, 0, 0},
    // Battle_RecalcStats reads the actor's byte (BE6's own listing; battle_e6.cpp)
    {"Battle_RecalcStats", bof3::addr::Battle_RecalcStats, KeyOf(&::Battle_RecalcStats), 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    {"BattleForm_ApplyStats", bof3::addr::BattleForm_ApplyStats, KeyOf(&::BattleForm_ApplyStats), 0, {}, bh::Answer::kGarbage, 0, 0},
    {"Effect_DrainAp", bof3::addr::Effect_DrainAp, KeyOf(&::Effect_DrainAp), 0, {}, bh::Answer::kGarbage, 0, 0},
    // this wave's R3C (ours; the row keyed by address)
    {"0x44D8B0", at::kAbility6AHelper, at::kAbility6AHelper, 0, {}, bh::Answer::kGarbage, 0, 0},
};

const bh::DataTable kTables[] = {
    // DragonCmd_Parts (BE5's seven parts), one word: the jmp leaves the caller's word for the part, so its
    // recorder logs it (the parts forward it, their steps do not read it)
    {0x64ECCC, 7, 4, 1},
};

// The cells beyond the engine frame the group reads or writes.
const bh::Region kRegions[] = {
    {0x903B24, 0x46C},   // CharacterRecords past the engine frame's head, to 0x903F90 (eight records): slot 118
    {0x803478, 0x6F8},   // past WindowRecords to party index 10's +0x134 dword (0x803B6C): slot 129's party-indexed
                         // writes for an enemy target 5..10 (3 and 4 land in WindowRecords)
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
// An actor 0..10: past 10 the enemy objects run off the image's end 0x93F000
// (a fault on both sides, not a count).
U ActorByte() { return bh::Next() % 11; }
// A status / element mask: the bits the rates test, alone and together, or anything.
U Mask() { return bh::Often() ? BH_PICK(0, 0x20, 0x40, 0x80, 0x100, 0xC0, 0x140, 0x180, 0x1C0, 0x1E0, 0x1F, 0x3F) : bh::Next(); }
// A status word for the inflict: each bit of its byte, 0x800, combinations, or anything.
U Status() {
    return bh::Often() ? BH_PICK(1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x800, 0x848, 0x6C, 0xFF, 0x8FF, 0x14) : bh::Next();
}

// The battle frame every function of the group reads some of: the actor and
// the target inside the actors, the ability (its mask and step from
// NameTable_Abilities), the roll's two terms at their clamps, each side's
// class bytes inside the rate tables (sometimes past them: the .data after
// is read on both sides), the flag words with 0x10000 half the time, the
// status words, Field_State's equipment bytes at the items the inflict
// tests, the party size 0..4, the members' characters 0..7.
void Frame() {
    Mem(at::kActor)[0] = static_cast<unsigned char>(ActorByte());
    Mem(at::kTarget)[0] = static_cast<unsigned char>(ActorByte());
    SetWord(Mem(at::kAbility), bh::Often() ? bh::Next() % 0x200 : BH_PICK(0x4E, 0x6A, 0, 1));
    SetWord(Mem(at::kAttackerInt), Word16({0, 1, 249, 250, 254, 255, 499, 500, 1000}));
    SetWord(Mem(at::kTargetInt), Word16({0, 1, 374, 375, 379, 380, 500, 1000}));
    SetWord(Mem(at::kNewStatus), Word16({0, 0, 0x80, 0x840}));
    Mem(at::kPartySize)[0] = static_cast<unsigned char>(bh::Often() ? BH_PICK(1, 2, 3, 3) : bh::Next() % 5);
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = Member(m);
        for (unsigned i = 0; i < 4; ++i) p[0xB4 + i] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 8 : bh::Next());
        SetLong(p + 0x130, static_cast<std::int32_t>(bh::Half() ? bh::Next() | 0x10000 : bh::Next() & ~0x10000u));
        SetWord(p + 0x90, Word16({0, 0x40, 0x800, 4, 0x64, 0xC, 0x14, 0x80}));
        p[0x148] = static_cast<unsigned char>(bh::Next() % 8);
        Mem(bof3::addr::CharacterRecords + p[0x148] * at::kCharacterStride + 0x1E)[0] = Byte({0, 7, 8, 9, 10});
    }
    for (unsigned n = 0; n < 8; ++n) {
        unsigned char* const e = EnemyN(n);
        for (unsigned i = 0; i < 4; ++i) e[0xC4 + i] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 8 : bh::Next());
        SetLong(e + 0x110, static_cast<std::int32_t>(bh::Half() ? bh::Next() | 0x10000 : bh::Next() & ~0x10000u));
        SetWord(e + 0x92, Word16({0, 0x40, 0x800, 4, 0x64, 0xC, 0x14, 0x80}));
        e[0x8D] = Byte({4, 4, 3, 5});
    }
    unsigned char* const fs = bh::Pointer(bh::at::kMemberCurrent);
    fs[0x94] = Byte({0x2B, 0x2B, 0x2A, 0});
    fs[0x96] = Byte({6, 8, 9, 0});
    fs[0x97] = Byte({6, 8, 9, 0});
}

void Seed(unsigned) { Frame(); }

// The words of the helpers: bytes inside what each indexes, garbage above
// them half the time (the originals read the low byte).
void Args(unsigned k, U* a) {
    switch (g_cur[k].base) {
    case 0x44F030:   // (target, mask)
    case 0x44F770:
    case 0x44FA10:
        a[0] = Upper() | ActorByte();
        a[1] = Mask();
        break;
    case 0x44F1D0:   // (target, status)
        a[0] = Upper() | ActorByte();
        a[1] = Status();
        break;
    case 0x44F460:   // (actor, item)
    case 0x44F490:
        a[0] = Upper() | (bh::Often() ? bh::Next() % 5 : bh::Next() & 0xFF);
        a[1] = Upper() | BH_PICK(6, 8, 9, 0x2B, 0x2A, 0);
        break;
    case 0x44F650:   // (step, stat): the step's signed word at the clamps' edges, the stat's byte 0..7
        a[0] = Upper() | Word16({0, 1, 0xFFFF, 25, 26, 50, 51, 0xFFE7, 0xFFE6, 0x7F, 0xFF80, 0x8000});
        a[1] = Upper() | (bh::Often() ? bh::Next() % 8 : bh::Next() % 0x20);
        break;
    case 0x44F6A0:   // (actor, target)
    case 0x44F940:
    case 0x44FA70:
        a[0] = bh::Next();
        a[1] = Upper() | ActorByte();
        break;
    case 0x44F880:   // (actor, target, mask)
        a[0] = bh::Next();
        a[1] = Upper() | ActorByte();
        a[2] = Mask();
        break;
    case 0x44FBB0:   // (stat)
    case 0x44FC10:
        a[0] = Upper() | (bh::Often() ? bh::Next() % 8 : bh::Next() % 0x20);
        break;
    case 0x44FC60:   // (status)
    case 0x44FCA0:
        a[0] = Status();
        break;
    case 0x44FCE0: {   // (divisor): its callers' 1 and 2, signs and sizes; never 0 (the original's idiv faults, ours aborts)
        const U d = bh::Often() ? BH_PICK(1, 2, 1, 2, 3, 0xFFFFFFFF, 0xFFFFFFFE, 100, 0x10000) : bh::Next();
        a[0] = d == 0 ? 1 : d;
        break;
    }
    default:
        break;
    }
}

// What the group's functions read again after a call: the target (every
// slot after the mark, the roll, the damage, the step), the actor, the party
// size (slot 118 between its calls), the inflict's new bits and the target's
// status word (after Sprite_ReleaseTint), an enemy's +0x8D (slot 127 after
// Battle_CalcDamage), the members' max HP (slot 118 at its end). Inside what
// they index; drawn from the hash only.
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 8) {
    case 0: Mem(at::kTarget)[0] = static_cast<unsigned char>(b % 11); break;
    case 1: Mem(at::kActor)[0] = static_cast<unsigned char>(b % 11); break;
    case 2: Mem(at::kPartySize)[0] = static_cast<unsigned char>(b % 5); break;
    case 3: SetWord(Mem(at::kNewStatus), h >> 12); break;
    case 4: SetWord(Member(b) + 0x90, h >> 20); break;
    case 5: SetWord(EnemyN(b) + 0x92, h >> 20); break;
    case 6: EnemyN(b)[0x8D] = static_cast<unsigned char>(h & 0x100 ? 4 : h >> 24); break;
    default: SetWord(Member(b) + 0xA0, h >> 18); break;
    }
}

}  // namespace

void SelfTest() {
    static bh::Clone clones[36];
    unsigned n = 0;
    const char* const only = std::getenv("BOF3X_R3D_ONLY");
    for (const bh::Clone& c : kAll36)
        if (only == nullptr || *only == 0 || std::strstr(c.name, only) != nullptr) clones[n++] = c;
    if (n == 0) bof3::Fatal("rest_3d: BOF3X_R3D_ONLY=%s names no clone", only);
    g_cur = clones;
    bh::Group g{"rest_3d", clones, n, kCallees, BH_COUNT(kCallees), kTables, BH_COUNT(kTables), kRegions, BH_COUNT(kRegions),
                &Seed, &Disturb, 6000};
    g.args = &Args;
    g.engine = true;
    bh::Run(g);
    g_cur = nullptr;
}

}  // namespace rest_3d
