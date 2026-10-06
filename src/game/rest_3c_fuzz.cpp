// BOF3X_SHADOW=rest_3c: group R3C's 61 Effect_Handlers slots through the boss
// harness's engine frame (boss_harness.h, docs/boss_harness.md section 10),
// once at start-up: one boss_harness::Run, Group::engine set. docs/rest_3c.md
// section 3.
//
// The clone rows are tools/band_rows.py's (--group R3C --clones --harness
// boss, 2026-10-04), each read against the disassembly: every slot is a kStep
// (void (void), as Effect_ApplyResult calls it through Effect_Handlers), its
// answer compared where it forwards a callee's (ret_mask 0xFF for the status
// helpers' al, the whole eax for a tail jump or the stat-damage helper's).
// BOF3X_R3C_ONLY=<substring> runs the clones whose name holds it (the controls
// script's shortcut); unset, all 61.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/rest_3c.h"
#include "game/rest_3c_callees.h"
#include "hook/log.h"

namespace rest_3c {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetWord;
using move_script::Word;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)

bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const void* ours, U ret) {
    bh::Clone c{name, base, size, calls, n, nullptr, 0, nullptr, 0, ours, ret, false, S::kStep};
    return c;
}

// ===========================================================================
// The clone rows (tools/band_rows.py --group R3C --clones --harness boss)
// ===========================================================================

constexpr bh::CallSite kCalls44D000[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44D020[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44D040[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44D060[] = {{0x12, 0x44FBB0}};
constexpr bh::CallSite kCalls44D080[] = {{0x12, 0x44FBB0}};
constexpr bh::CallSite kCalls44D0A0[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44D0C0[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44D0E0[] = {{0x12, 0x44FC60}};
constexpr bh::CallSite kCalls44D100[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44D120[] = {{0x10, 0x44C120}};
constexpr bh::CallSite kCalls44D140[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44D160[] = {{0x10, 0x44CF60}};
constexpr bh::CallSite kCalls44D180[] = {{0x22, 0x445CF0}};
constexpr bh::CallSite kCalls44D1C0[] = {{0x36, 0x445CF0}};
constexpr bh::CallSite kCalls44D220[] = {{0x2, 0x44FCE0}};
constexpr bh::CallSite kCalls44D240[] = {{0x2, 0x44FCE0}};
constexpr bh::CallSite kCalls44D260[] = {{0x2, 0x44FCE0}};
constexpr bh::CallSite kCalls44D280[] = {{0x2, 0x44FC60}};
constexpr bh::CallSite kCalls44D290[] = {{0x58, 0x445CF0}, {0x8D, 0x44FCA0}};
constexpr bh::CallSite kCalls44D330[] = {{0x2, 0x44FC60}};
constexpr bh::CallSite kCalls44D340[] = {{0x5, 0x44FC60}};
constexpr bh::CallSite kCalls44D350[] = {{0x0, 0x44FB30}, {0x7, 0x44FCA0}};
constexpr bh::CallSite kCalls44D390[] = {{0x78, 0x445CF0}};
constexpr bh::CallSite kCalls44D420[] = {{0x1D, 0x44FB30}};
constexpr bh::CallSite kCalls44D450[] = {{0x2, 0x44FB30}, {0x3D, 0x590660}, {0xC6, 0x453C00}, {0xF6, 0x44FDE0}, {0x111, 0x453300}};
constexpr bh::CallSite kCalls44D580[] = {{0x8, 0x44F1D0}};
constexpr bh::CallSite kCalls44D630[] = {{0x29, 0x44ED10}};
constexpr bh::CallSite kCalls44D680[] = {{0x10, 0x44C040}};
constexpr bh::CallSite kCalls44D6A0[] = {{0xB, 0x44F4B0}, {0x13, 0x44C170}};
constexpr bh::CallSite kCalls44D6D0[] = {{0x1, 0x44FB30}};
constexpr bh::CallSite kCalls44D770[] = {{0x12, 0x445CF0}};
constexpr bh::CallSite kCalls44D7E0[] = {{0xD, 0x44F6A0}};
constexpr bh::CallSite kCalls44D850[] = {{0x33, 0x44FCA0}, {0x49, 0x44FB30}};
constexpr bh::CallSite kCalls44D8B0[] = {{0xD, 0x44F6A0}};
constexpr bh::CallSite kCalls44D920[] = {{0x3E, 0x5B93D2}, {0x92, 0x445CF0}};
constexpr bh::CallSite kCalls44D9D0[] = {{0xD, 0x44F6A0}, {0x46, 0x453300}, {0x70, 0x453300}, {0x8C, 0x453DA0}, {0x94, 0x44FB30}};
constexpr bh::CallSite kCalls44DA70[] = {{0x78, 0x445CF0}};
constexpr bh::CallSite kCalls44DB40[] = {{0x1, 0x44FB30}};
constexpr bh::CallSite kCalls44DBC0[] = {{0x19, 0x44F6A0}, {0x25, 0x44FB30}, {0x30, 0x446EA0}, {0x3C, 0x446650}};
constexpr bh::CallSite kCalls44DC10[] = {{0x2, 0x44FB30}, {0x66, 0x446EA0}};
constexpr bh::CallSite kCalls44DCA0[] = {{0x0, 0x44FB30}, {0x9, 0x44F650}, {0x14, 0x453300}};
constexpr bh::CallSite kCalls44DD60[] = {{0xD, 0x44F6A0}};
constexpr bh::CallSite kCalls44DDD0[] = {{0xD, 0x44F130}, {0x27, 0x44F4B0}};
constexpr bh::CallSite kCalls44DE20[] = {{0x48, 0x445CF0}};
constexpr bh::CallSite kCalls44DEF0[] = {{0x26, 0x44ED10}};
constexpr bh::CallSite kCalls44DF80[] = {{0x12, 0x445CF0}, {0x3A, 0x44FCA0}};
constexpr bh::CallSite kCalls44DFD0[] = {{0x70, 0x590660}, {0xF9, 0x453C00}, {0x129, 0x44FDE0}, {0x143, 0x453300}, {0x168, 0x44F6A0}};
constexpr bh::CallSite kCalls44E260[] = {{0x0, 0x44FB30}};
constexpr bh::CallSite kCalls44E2B0[] = {{0x0, 0x44FB30}};
constexpr bh::CallSite kCalls44E330[] = {{0x0, 0x44FB30}};
constexpr bh::CallSite kCalls44E3B0[] = {{0x0, 0x44FB30}};
constexpr bh::CallSite kCalls44E400[] = {{0x0, 0x44FB30}, {0x56, 0x453300}, {0xA3, 0x453300}};

const bh::Clone kAll61[] = {
    C("Effect_AsAbility60", 0x44D000, 0x15, kCalls44D000, BH_N(kCalls44D000), BH_FN(Effect_AsAbility60), 0xFFFFFFFFu),   // slot 50
    C("Effect_AsAbility68", 0x44D020, 0x15, kCalls44D020, BH_N(kCalls44D020), BH_FN(Effect_AsAbility68), 0xFFFFFFFFu),   // slot 51
    C("Effect_AsAbility5B", 0x44D040, 0x15, kCalls44D040, BH_N(kCalls44D040), BH_FN(Effect_AsAbility5B), 0xFFFFFFFFu),   // slot 52
    C("Effect_AsAbility52RaiseStat1", 0x44D060, 0x19, kCalls44D060, BH_N(kCalls44D060), BH_FN(Effect_AsAbility52RaiseStat1), 0xFF),   // slot 53
    C("Effect_AsAbility5ARaiseStat1", 0x44D080, 0x19, kCalls44D080, BH_N(kCalls44D080), BH_FN(Effect_AsAbility5ARaiseStat1), 0xFF),   // slot 54
    C("Effect_AsAbility5C", 0x44D0A0, 0x15, kCalls44D0A0, BH_N(kCalls44D0A0), BH_FN(Effect_AsAbility5C), 0xFFFFFFFFu),   // slot 55
    C("Effect_AsAbility64", 0x44D0C0, 0x15, kCalls44D0C0, BH_N(kCalls44D0C0), BH_FN(Effect_AsAbility64), 0xFFFFFFFFu),   // slot 56
    C("Effect_AsAbility57Inflict10", 0x44D0E0, 0x19, kCalls44D0E0, BH_N(kCalls44D0E0), BH_FN(Effect_AsAbility57Inflict10), 0xFF),   // slot 57
    C("Effect_AsAbility61", 0x44D100, 0x15, kCalls44D100, BH_N(kCalls44D100), BH_FN(Effect_AsAbility61), 0xFFFFFFFFu),   // slot 58
    C("Effect_AsAbility46", 0x44D120, 0x15, kCalls44D120, BH_N(kCalls44D120), BH_FN(Effect_AsAbility46), 0xFFFFFFFFu),   // slot 59
    C("Effect_AsAbility67", 0x44D140, 0x15, kCalls44D140, BH_N(kCalls44D140), BH_FN(Effect_AsAbility67), 0xFFFFFFFFu),   // slot 60
    C("Effect_AsAbility56", 0x44D160, 0x15, kCalls44D160, BH_N(kCalls44D160), BH_FN(Effect_AsAbility56), 0xFFFFFFFFu),   // slot 61
    C("Effect_DoubleDamage", 0x44D180, 0x3E, kCalls44D180, BH_N(kCalls44D180), BH_FN(Effect_DoubleDamage), 0),   // slot 62
    C("Effect_QuarterDamage", 0x44D1C0, 0x55, kCalls44D1C0, BH_N(kCalls44D1C0), BH_FN(Effect_QuarterDamage), 0),   // slot 63
    C("Effect_ActorHpDamage3", 0x44D220, 0x15, kCalls44D220, BH_N(kCalls44D220), BH_FN(Effect_ActorHpDamage3), 0xFFFFFFFFu),   // slot 64
    C("Effect_ActorHpDamage1", 0x44D240, 0x15, kCalls44D240, BH_N(kCalls44D240), BH_FN(Effect_ActorHpDamage1), 0xFFFFFFFFu),   // slot 65
    C("Effect_ActorHpDamage2", 0x44D260, 0x15, kCalls44D260, BH_N(kCalls44D260), BH_FN(Effect_ActorHpDamage2), 0xFFFFFFFFu),   // slot 66
    C("Effect_InflictStatus4", 0x44D280, 0x9, kCalls44D280, BH_N(kCalls44D280), BH_FN(Effect_InflictStatus4), 0xFF),   // slot 67
    C("Effect_DamageInflict40", 0x44D290, 0x98, kCalls44D290, BH_N(kCalls44D290), BH_FN(Effect_DamageInflict40), 0),   // slot 68
    C("Effect_InflictStatus8", 0x44D330, 0x9, kCalls44D330, BH_N(kCalls44D330), BH_FN(Effect_InflictStatus8), 0xFF),   // slot 69
    C("Effect_InflictStatus80", 0x44D340, 0xC, kCalls44D340, BH_N(kCalls44D340), BH_FN(Effect_InflictStatus80), 0xFF),   // slot 70
    C("Effect_InflictStatus20", 0x44D350, 0xE, kCalls44D350, BH_N(kCalls44D350), BH_FN(Effect_InflictStatus20), 0xFF),   // slot 71
    C("Effect_Heal5Ap1", 0x44D360, 0x26, nullptr, 0, BH_FN(Effect_Heal5Ap1), 0),   // slot 72
    C("Effect_Attack80", 0x44D390, 0x8C, kCalls44D390, BH_N(kCalls44D390), BH_FN(Effect_Attack80), 0),   // slot 73
    C("Effect_FlagActorPair", 0x44D420, 0x22, kCalls44D420, BH_N(kCalls44D420), BH_FN(Effect_FlagActorPair), 0xFFFFFFFFu),   // slot 74
    C("Effect_TargetRecalcParty", 0x44D450, 0x12B, kCalls44D450, BH_N(kCalls44D450), BH_FN(Effect_TargetRecalcParty), 0),   // slot 75
    C("Effect_Inflict40Heal20Ap4", 0x44D580, 0x37, kCalls44D580, BH_N(kCalls44D580), BH_FN(Effect_Inflict40Heal20Ap4), 0),   // slot 76
    C("Effect_Heal1", 0x44D5C0, 0xC, nullptr, 0, BH_FN(Effect_Heal1), 0),   // slot 77
    C("Effect_Heal80", 0x44D5D0, 0xC, nullptr, 0, BH_FN(Effect_Heal80), 0),   // slot 78
    C("Effect_Heal240", 0x44D5E0, 0xC, nullptr, 0, BH_FN(Effect_Heal240), 0),   // slot 79
    C("Effect_RestoreAp5", 0x44D5F0, 0x16, nullptr, 0, BH_FN(Effect_RestoreAp5), 0),   // slot 80
    C("Effect_RestoreAp40", 0x44D610, 0x16, nullptr, 0, BH_FN(Effect_RestoreAp40), 0),   // slot 81
    C("Effect_AsAbility66Half", 0x44D630, 0x4F, kCalls44D630, BH_N(kCalls44D630), BH_FN(Effect_AsAbility66Half), 0),   // slot 82
    C("Effect_AsAbility63", 0x44D680, 0x15, kCalls44D680, BH_N(kCalls44D680), BH_FN(Effect_AsAbility63), 0xFFFFFFFFu),   // slot 83
    C("Effect_CureBFCHealMax", 0x44D6A0, 0x18, kCalls44D6A0, BH_N(kCalls44D6A0), BH_FN(Effect_CureBFCHealMax), 0xFFFFFFFFu),   // slot 84
    C("Effect_Heal5", 0x44D6C0, 0xC, nullptr, 0, BH_FN(Effect_Heal5), 0),   // slot 85
    C("Effect_EnemyRaiseAAAE", 0x44D6D0, 0x9E, kCalls44D6D0, BH_N(kCalls44D6D0), BH_FN(Effect_EnemyRaiseAAAE), 0),   // slot 87
    C("Effect_AttackNoKill", 0x44D770, 0x6D, kCalls44D770, BH_N(kCalls44D770), BH_FN(Effect_AttackNoKill), 0),   // slot 88
    C("Effect_HalveTargetHp", 0x44D7E0, 0x6A, kCalls44D7E0, BH_N(kCalls44D7E0), BH_FN(Effect_HalveTargetHp), 0),   // slot 89
    C("Effect_Inflict800Unguarded", 0x44D850, 0x59, kCalls44D850, BH_N(kCalls44D850), BH_FN(Effect_Inflict800Unguarded), 0),   // slot 90
    C("Effect_DamageAllHp", 0x44D8B0, 0x6F, kCalls44D8B0, BH_N(kCalls44D8B0), BH_FN(Effect_DamageAllHp), 0),   // slot 91
    C("Effect_RandomPowerAttack", 0x44D920, 0xA5, kCalls44D920, BH_N(kCalls44D920), BH_FN(Effect_RandomPowerAttack), 0),   // slot 92
    C("Effect_TargetFlag2000", 0x44D9D0, 0x99, kCalls44D9D0, BH_N(kCalls44D9D0), BH_FN(Effect_TargetFlag2000), 0xFFFFFFFFu),   // slot 93
    C("Effect_SureHitHalfDamage", 0x44DA70, 0xCD, kCalls44DA70, BH_N(kCalls44DA70), BH_FN(Effect_SureHitHalfDamage), 0),   // slot 94
    C("Effect_EnemyDouble94And96", 0x44DB40, 0x7D, kCalls44DB40, BH_N(kCalls44DB40), BH_FN(Effect_EnemyDouble94And96), 0),   // slot 95
    C("Effect_DropFromTurnOrder", 0x44DBC0, 0x50, kCalls44DBC0, BH_N(kCalls44DBC0), BH_FN(Effect_DropFromTurnOrder), 0),   // slot 96
    C("Effect_FlushTurnOrder", 0x44DC10, 0x84, kCalls44DC10, BH_N(kCalls44DC10), BH_FN(Effect_FlushTurnOrder), 0),   // slot 97
    C("Effect_RaiseStat4By10", 0x44DCA0, 0x1D, kCalls44DCA0, BH_N(kCalls44DCA0), BH_FN(Effect_RaiseStat4By10), 0),   // slot 98
    C("Effect_ActorHpToDamage", 0x44DCC0, 0x95, nullptr, 0, BH_FN(Effect_ActorHpToDamage), 0),   // slot 99
    C("Effect_DamageToOneHp", 0x44DD60, 0x6E, kCalls44DD60, BH_N(kCalls44DD60), BH_FN(Effect_DamageToOneHp), 0),   // slot 100
    C("Effect_HealAndCureBFC", 0x44DDD0, 0x30, kCalls44DDD0, BH_N(kCalls44DDD0), BH_FN(Effect_HealAndCureBFC), 0),   // slot 101
    C("Effect_RestoreAp10", 0x44DE00, 0x16, nullptr, 0, BH_FN(Effect_RestoreAp10), 0),   // slot 102
    C("Effect_MultiHit", 0x44DE20, 0xCA, kCalls44DE20, BH_N(kCalls44DE20), BH_FN(Effect_MultiHit), 0),   // slot 103
    C("Effect_SkillDamageLessDef", 0x44DEF0, 0x8C, kCalls44DEF0, BH_N(kCalls44DEF0), BH_FN(Effect_SkillDamageLessDef), 0),   // slot 104
    C("Effect_HalfDamageInflict20", 0x44DF80, 0x41, kCalls44DF80, BH_N(kCalls44DF80), BH_FN(Effect_HalfDamageInflict20), 0),   // slot 105
    C("Effect_ActorStepThenHpToOne", 0x44DFD0, 0x28D, kCalls44DFD0, BH_N(kCalls44DFD0), BH_FN(Effect_ActorStepThenHpToOne), 0),   // slot 106
    C("Effect_ActorFlag8000", 0x44E260, 0x4E, kCalls44E260, BH_N(kCalls44E260), BH_FN(Effect_ActorFlag8000), 0),   // slot 107
    C("Effect_ActorFlag40Count", 0x44E2B0, 0x76, kCalls44E2B0, BH_N(kCalls44E2B0), BH_FN(Effect_ActorFlag40Count), 0),   // slot 108
    C("Effect_ActorFlag80Count", 0x44E330, 0x76, kCalls44E330, BH_N(kCalls44E330), BH_FN(Effect_ActorFlag80Count), 0),   // slot 109
    C("Effect_ActorFlag100", 0x44E3B0, 0x4E, kCalls44E3B0, BH_N(kCalls44E3B0), BH_FN(Effect_ActorFlag100), 0),   // slot 110
    C("Effect_TargetClearBuffs", 0x44E400, 0xAA, kCalls44E400, BH_N(kCalls44E400), BH_FN(Effect_TargetClearBuffs), 0),   // slot 111
};
static_assert(sizeof kAll61 / sizeof kAll61[0] == 61, "61 functions");

// ===========================================================================
// The callees the standard set lacks or records too coarsely, the regions
// ===========================================================================

bool InCharRecords(U p) { return p >= at::kCharRecords && p + at::kCharStride <= at::kCharRecords + 10 * at::kCharStride; }

// Char_RecalcStats rewrites the record the party loop then copies from (its
// +0x12..+0x17 and +0x20..+0x3F): the stand-in notes what the caller left
// and moves both, so a copy taken before the call is refused.
U RecordEffect(const U* a, U answer) {
    if (InCharRecords(a[0])) {
        unsigned char* const r = Mem(a[0]);
        bh::NoteBytes(r + 0x12, 6);
        bh::NoteBytes(r + 0x20, 32);
        bh::FillBytes(r + 0x12, 6);
        bh::FillBytes(r + 0x20, 32);
    }
    return answer;
}

// Formation_ApplyStatMods rewrites the members' +0xC0 block, which the second
// loop copies to +0xA0 after it: the stand-in notes one member's block (the
// first loop's copy, compared) and moves it.
U StatModsEffect(const U*, U answer) {
    unsigned char* const m = Mem(at::kParty + (bh::Noise() % 3) * at::kPartyStride + 0xC0);
    bh::NoteBytes(m, 32);
    bh::FillBytes(m, 32);
    return answer;
}

// Battle_CalcDamage's answer is compared against the target's HP (slots 68,
// 88: < and > on the words), against 0 (68, 105) and summed into a clamp (103):
// garbage meets an HP or 0 one time in 65,536. Two times in three the stand-in
// answers 0, 1, or the target's HP less one, equal or one more (its words
// read after the disturbance, the same on both passes), the upper half kept.
U CalcEffect(const U* a, U answer) {
    const U n = bh::Noise();
    if (n % 3 == 0) return answer;
    const unsigned t = a[1] & 0xFF;
    if (t > 10) return answer;
    const unsigned hp = t < 3 ? Word(Mem(at::kParty + t * at::kPartyStride + 0x98)) : Word(Mem(at::kEnemies + (t - 3) * at::kEnemyStride + 0xA4));
    static const unsigned kPick[] = {0, 1, 0xFFFFFFFFu, 0, 1};
    const unsigned k = (n >> 4) % 5;
    const unsigned v = k < 2 ? kPick[k] : hp + kPick[k];   // k 2..4: HP - 1, HP, HP + 1
    return (answer & 0xFFFF0000u) | (v & 0xFFFF);
}

// Masks narrowed where the callee reads less than the word and the caller's
// register above it is its own garbage (a different value in the copy and in
// ours): each read of the callee's (capstone, 2026-10-04).
const bh::Callee kCallees[] = {
    // ours, not in the standard sets.
    // Effect_SkillDamage 0x44ED10: `caster` unread; the target handed on (push ebx) to the affinity helpers and
    // compared as a byte (cmp bl, cl; and ebx, 0xFF); power `and ecx, 0xFFFF`; psi `mov cl, byte [esp + 0x18]`
    {"Effect_SkillDamage", bof3::addr::Effect_SkillDamage, KeyOf(&::Effect_SkillDamage), 4, {0, kU8, kU16, kU8},
     bh::Answer::kGarbage, 0, 0},
    // Effect_HealAmount 0x44F130: `caster` unread; the target `cmp al, 2` then `and eax, 0xFF`
    {"Effect_HealAmount", bof3::addr::Effect_HealAmount, KeyOf(&::Effect_HealAmount), 2, {0, kU8}, bh::Answer::kGarbage, 0, 0},
    // Battle_RecalcStats 0x453300: `mov al, [esp + 4]; cmp al, 2`, `and edi, 0xFF` (BE6's own listing, kU8)
    {"Battle_RecalcStats", bof3::addr::Battle_RecalcStats, KeyOf(&::Battle_RecalcStats), 1, {kU8}, bh::Answer::kGarbage, 0, 0},
    {"BattleForm_ApplyStats", bof3::addr::BattleForm_ApplyStats, KeyOf(&::BattleForm_ApplyStats), 0, {}, bh::Answer::kGarbage, 0, 0},
    // the standard three, louder (CalcEffect, RecordEffect, StatModsEffect); Battle_CalcDamage's masks the
    // standard row's (both actors' bytes: the slots push eax / ecx / edx with stale upper bytes)
    {"Battle_CalcDamage", bof3::addr::Battle_CalcDamage, KeyOf(&::Battle_CalcDamage), 3, {kU8, kU8, kU16}, bh::Answer::kGarbage,
     0, 0, {}, &CalcEffect},
    {"Char_RecalcStats", bof3::addr::Char_RecalcStats, KeyOf(&::Char_RecalcStats), 1, {kAll}, bh::Answer::kGarbage, 0, 0, {},
     &RecordEffect},
    {"Formation_ApplyStatMods", bof3::addr::Formation_ApplyStatMods, KeyOf(&::Formation_ApplyStatMods), 0, {}, bh::Answer::kGarbage,
     0, 0, {}, &StatModsEffect},
    // R3B's (ours; rows keyed by address): tail jumps, nothing pushed; their eax forwarded
    {"0x44C040", at::kSkillByAbility, at::kSkillByAbility, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x44C120", at::kHealByAbility, at::kHealByAbility, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x44C170", at::kHealMaxHp, at::kHealMaxHp, 0, {}, bh::Answer::kGarbage, 0, 0},
    {"0x44CF60", at::kFlag200, at::kFlag200, 0, {}, bh::Answer::kGarbage, 0, 0},
    // R3D's (ours; rows keyed by address)
    // 0x44F1D0 (target, status): cmp bl, 2 and and esi, 0xFF on the target, handed on whole to callees that read
    // its byte; Effect_Inflict40Heal20Ap4 pushes eax with the caller's upper bytes
    {"0x44F1D0", at::kInflictStatus, at::kInflictStatus, 2, {kU8, kAll}, bh::Answer::kGarbage, 0, 0},
    {"0x44F650", at::kRaiseStat, at::kRaiseStat, 2, {kAll, kAll}, bh::Answer::kGarbage, 0, 0},
    {"0x44FBB0", at::kRaiseByAbility, at::kRaiseByAbility, 1, {kAll}, bh::Answer::kFlag, 0, 0},
    {"0x44FC60", at::kMissInflict, at::kMissInflict, 1, {kAll}, bh::Answer::kFlag, 0, 0},
    {"0x44FCA0", at::kInflictUnlessResisted, at::kInflictUnlessResisted, 1, {kAll}, bh::Answer::kFlag, 0, 0},
    {"0x44FCE0", at::kHpDamage, at::kHpDamage, 1, {kAll}, bh::Answer::kGarbage, 0, 0},
};

// The cells beyond the engine frame the group reads or writes.
const bh::Region kRegions[] = {
    {0x904654, 0x10},                                                             // 0x904660 (slot 62's store)
    {0x903B24, at::kCharRecords + 10 * at::kCharStride - 0x903B24},              // CharacterRecords past the engine region, to ten
};

// ===========================================================================
// Seeds and disturbance
// ===========================================================================

const bh::Clone* g_cur = nullptr;   // the Run's clones (for Seed and Disturb)
U g_base = 0;                       // the clone the round runs

unsigned char* Member(unsigned n) { return Mem(at::kParty + (n % 3) * at::kPartyStride); }
unsigned char* EnemyObj(unsigned n) { return Mem(at::kEnemies + (n % 8) * at::kEnemyStride); }
unsigned char* CharRecord(unsigned c) { return Mem(at::kCharRecords + c * at::kCharStride); }
std::uint16_t Word16(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<std::uint16_t>(bh::Next());
    const U* v = often.begin();
    return static_cast<std::uint16_t>(v[bh::Next() % often.size()]);
}
unsigned char Byte8(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<unsigned char>(bh::Next());
    const U* v = often.begin();
    return static_cast<unsigned char>(v[bh::Next() % often.size()]);
}

// The target each slot can take: 0..10, but a member only for the turn-order
// flush (it writes +0x134 by ObjTrio's stride) and an enemy only for the two
// that write the enemy (target - 3) with no member branch - past those the
// original writes outside its records and ours aborts.
unsigned char TargetFor(U base, U v) {
    switch (base) {
    case 0x44DC10: return static_cast<unsigned char>(v % 3);
    case 0x44D6D0:
    case 0x44DB40: return static_cast<unsigned char>(3 + v % 8);
    default: return static_cast<unsigned char>(v % 11);
    }
}

void Seed(unsigned k) {
    g_base = g_cur[k].base;
    // the actor and the target (their dwords' upper bytes the fill's), the kind, the ability
    Mem(at::kActor)[0] = static_cast<unsigned char>(bh::Next() % 11);
    Mem(at::kTarget)[0] = TargetFor(g_base, bh::Next());
    Mem(at::kActingKind)[0] = Byte8({4, 4, 1, 0});
    SetWord(Mem(at::kAbility), bh::Often() ? (bh::Half() ? 0x9A : bh::Next() % 256) : bh::Next());
    // the party: its count, the characters (CharacterRecords' first ten), HP, the flag and status bits,
    // the stat words and the counts the slots test
    Mem(at::kPartyCount)[0] = static_cast<unsigned char>(bh::Next() % 4);
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = Member(m);
        p[0x148] = static_cast<unsigned char>(bh::Next() % 10);
        SetWord(p + 0x98, Word16({0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 100}));
        p[0x134] = static_cast<unsigned char>(bh::Half() ? p[0x134] & 0xFC : p[0x134]);
        p[0x144] = static_cast<unsigned char>(bh::Next() % 4);
        p[0x145] = static_cast<unsigned char>(bh::Next() % 4);
        p[0x90] = static_cast<unsigned char>(bh::Half() ? p[0x90] & ~0x48 : p[0x90] | 0x48);
        CharRecord(p[0x148])[0x1E] = static_cast<unsigned char>(bh::Next() % 7);
    }
    // the enemies: HP, the maximum HP 0xFFFF a quarter of the time, the data record by +0xF0, the bytes
    // +0xAA / +0xAE and their markers, +0x94 / +0x96 about their doubling's cap, the counts +0x124 / +0x125
    for (unsigned n = 0; n < 8; ++n) {
        unsigned char* const e = EnemyObj(n);
        e[0xF0] = static_cast<unsigned char>(bh::Next() % 8);
        SetWord(e + 0xA4, Word16({0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 100}));
        SetWord(e + 0xB0, bh::Next() % 4 == 0 ? 0xFFFF : Word16({0, 1, 9, 10, 11, 100, 1000}));
        SetWord(Mem(at::kEnemyData + e[0xF0] * at::kEnemyDataStride + 0x24), Word16({0, 9, 10, 99, 100, 1000, 0xFFFF}));
        e[0xAA] = static_cast<unsigned char>(bh::Next() % 10);
        e[0xAE] = static_cast<unsigned char>(bh::Next() % 10);
        e[0xAB] = static_cast<unsigned char>(bh::Half() ? e[0xAB] | 1 : e[0xAB] & 0xFE);
        e[0xAF] = static_cast<unsigned char>(bh::Half() ? e[0xAF] | 1 : e[0xAF] & 0xFE);
        SetWord(e + 0x94, Word16({0, 1, 0x7FFF, 0x8000, 0xFFFF}));
        SetWord(e + 0x96, Word16({0, 1, 0x7FFF, 0x8000, 0xFFFF}));
        e[0x124] = static_cast<unsigned char>(bh::Next() % 4);
        e[0x125] = static_cast<unsigned char>(bh::Next() % 4);
        e[0x92] = static_cast<unsigned char>(bh::Half() ? e[0x92] & ~0x48 : e[0x92] | 0x48);
    }
    // the multi-hit count, the turn order's cursor, end and slots
    Mem(at::kHits)[0] = static_cast<unsigned char>(bh::Often() ? bh::Next() % 10 : bh::Next() % 21);
    Mem(at::kTurnCursor)[0] = static_cast<unsigned char>(bh::Next() % 13);
    Mem(at::kTurnCount)[0] = static_cast<unsigned char>(bh::Next() % 12);
    for (unsigned i = 0; i < 12; ++i)
        Mem(at::kTurnOrder + i)[0] = static_cast<unsigned char>(bh::Half() ? 0xFF : bh::Next() % 11);
}

// What the slots read again after a call: the target and the actor (each
// inside what its slot may index), the party count, the turn order's end and
// a slot, the hit count, an HP word, a status word's bit 3, the attacker's
// power word. From the hash only.
void Disturb(U h) {
    const U v = h >> 16;
    switch ((h >> 8) % 10) {
    case 0: Mem(at::kTarget)[0] = TargetFor(g_base, v); break;
    case 1: Mem(at::kActor)[0] = static_cast<unsigned char>(v % 11); break;
    case 2: Mem(at::kPartyCount)[0] = static_cast<unsigned char>(v % 4); break;
    case 3: Mem(at::kTurnCount)[0] = static_cast<unsigned char>(v % 12); break;
    case 4: Mem(at::kTurnOrder + v % 12)[0] = static_cast<unsigned char>(v & 0x100 ? 0xFF : (v >> 4) % 11); break;
    case 5: Mem(at::kHits)[0] = static_cast<unsigned char>(v % 10); break;
    case 6: SetWord(Member(v) + 0x98, v >> 2); break;
    case 7: SetWord(EnemyObj(v) + 0xA4, v >> 3); break;
    case 8: {
        unsigned char* const s = v & 1 ? Member(v >> 1) + 0x90 : EnemyObj(v >> 1) + 0x92;
        s[0] = static_cast<unsigned char>(s[0] ^ 8);
        break;
    }
    default: SetWord(Mem(at::kAttackerAtk), v); break;
    }
}

}  // namespace

void SelfTest() {
    static bh::Clone clones[61];
    unsigned n = 0;
    const char* const only = std::getenv("BOF3X_R3C_ONLY");
    for (const bh::Clone& c : kAll61)
        if (only == nullptr || *only == 0 || std::strstr(c.name, only) != nullptr) clones[n++] = c;
    if (n == 0) bof3::Fatal("rest_3c: BOF3X_R3C_ONLY=%s names no clone", only);
    g_cur = clones;
    bh::Group g{"rest_3c", clones, n, kCallees, BH_COUNT(kCallees), nullptr, 0, kRegions, BH_COUNT(kRegions), &Seed, &Disturb, 6000};
    g.engine = true;
    bh::Run(g);
    g_cur = nullptr;
}

}  // namespace rest_3c
