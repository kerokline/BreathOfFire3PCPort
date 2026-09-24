// Damage, effects and affinities: group BE of the seventh round, eighteen
// functions of the battle engine (PC 0x42E400..0x4551A0, the PSX's
// BATTLE.EMI overlays compiled into the exe) that the owner's combat route
// reaches. docs/battle_damage.md has the formulas in words.
//
//   - The damage chain: Battle_ApplyDamage 0x445A30 (PSX 0x801DBB40) takes
//     Battle_CalcDamage 0x445CF0 (0x801DC00C) of the attacker and target, which
//     takes Battle_BaseDamage 0x4462B0 (0x801DCAA0), which takes
//     Battle_ScaleDamage 0x446430 (0x801DCD18), which takes
//     Battle_ElementAffinity 0x44EE80 (0x8009FA78).
//   - The effects: Effect_ApplyResult 0x44B9F0 (0x8009A160) calls one of 130
//     handlers and applies the HP and AP deltas they leave; two amounts the
//     handlers compute, Effect_SkillDamage 0x44ED10 (0x8009F820) and
//     Effect_HealAmount 0x44F130 (0x800A0080).
//   - The turn order: Battle_BuildEntryOrder 0x444F40 (0x801DA7A4),
//     Battle_BuildTurnOrder 0x4450E0 (0x801DAAB4), Battle_LevelClass 0x445640
//     (0x801DB3FC), Battle_ClearCommands 0x445680 (0x801DB45C),
//     Battle_DefaultTarget 0x445730 (0x801DB594), Battle_ActorCanCommand
//     0x4458B0 (0x801DB80C), Battle_ActorCanAct 0x445980 (0x801DB9AC),
//     Battle_RemoveFromTurnOrder 0x446650 (0x801DD114).
//   - The enemies' AI conditions: EnemyAI_ChooseActions 0x44AE90 (0x80098F8C)
//     and EnemyAI_RowDone 0x44B2C0 (0x80099830).
//
// Every call goes through battle_damage::g (battle_damage_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. The effect handlers are called through the table
// in memory, as the original calls them; the fuzz swaps the table.
//
// Several originals write into their own argument slots (0x4458B0, 0x445980,
// 0x445730, 0x446650, 0x445CF0, 0x44F130) - as scratch, never read back by a
// caller (every caller pops its arguments or pushes fresh ones: the E8 scan
// in docs/battle_damage.md section 1). Not kept.
#include "game/battle_damage.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_damage_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_damage {

namespace {
template <class To> To Fn(std::uint32_t address) { return reinterpret_cast<To>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Battle_ActorCanAct,
    Battle_ActorCanCommand,
    Battle_LevelClass,
    Battle_CalcDamage,
    Battle_BaseDamage,
    Battle_ScaleDamage,
    Battle_ElementAffinity,
    EnemyAI_RowDone,
    Fn<unsigned char (__cdecl*)(unsigned)>(0x4456C0),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned)>(0x44A650),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(0x44FA70),
    Fn<void (__cdecl*)(unsigned, unsigned)>(0x44F1D0),
    Fn<void (__cdecl*)(unsigned, unsigned)>(0x44F4B0),
    Fn<int (__cdecl*)(int, unsigned, unsigned)>(0x446110),
    Fn<int (__cdecl*)(int, unsigned, unsigned)>(0x4461B0),
    Fn<int (__cdecl*)()>(0x4463E0),
    Fn<int (__cdecl*)(unsigned, unsigned)>(0x44F030),
    Rand,
    Fn<void (__cdecl*)(unsigned char*, const unsigned char*)>(0x44B3A0),
    Fn<void (__cdecl*)(unsigned char*, unsigned, unsigned)>(0x44B2E0),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(0x44B240),
    Fn<void (__cdecl*)()>(0x44B920),
};
Callees g = kOriginals;

}  // namespace battle_damage

using namespace battle_damage;

namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// An actor byte: 0..2 the party, 3 and up the enemies. Both address the
// working record by the byte alone, as the original does (`and eax, 0xFF`).
unsigned char* Party(unsigned actor) { return At(at::kParty + (actor & 0xFF) * at::kPartyStride); }
unsigned char* Enemy(unsigned actor) { return At(at::kEnemy + ((actor & 0xFF) - 3) * at::kEnemyStride); }
unsigned char Byte(std::uint32_t address) { return At(address)[0]; }
std::int32_t S32(std::uint32_t v) { return static_cast<std::int32_t>(v); }
std::uint32_t U32(std::int32_t v) { return static_cast<std::uint32_t>(v); }
short S16(unsigned v) { return static_cast<short>(static_cast<unsigned short>(v)); }
// 32-bit products wrap, as imul's low half does.
std::int32_t Mul(std::int32_t a, std::int32_t b) { return S32(U32(a) * U32(b)); }

// The two sorts: entries of (s16 value, u16 actor), for i, for j after i, the
// pair swapped whenever entry i's value is below entry j's (signed) - highest
// first, equal values in their first order only as far as the swaps leave
// them. `cmp ax, [ecx]; jge` in both (0x445048, 0x445332).
void SortDescending(unsigned char* entries, unsigned n) {
    if (n <= 1) return;
    for (unsigned i = 0; i + 1 < n; ++i)
        for (unsigned j = i + 1; j < n; ++j) {
            unsigned char* const a = entries + 4 * i;
            unsigned char* const b = entries + 4 * j;
            if (S16(Word(a)) < S16(Word(b))) {
                const std::int32_t t = Long(b);
                SetLong(b, Long(a));
                SetLong(a, t);
            }
        }
}

// The gate both order builders apply: with the byte 0x904B7A set, an actor
// takes part only with bit 15 of its second flag dword.
bool Gated(const unsigned char* flags2) { return Byte(at::kGate) != 0 && (Long(flags2) & 0x8000) == 0; }

}  // namespace

// ===========================================================================
// The turn order

// original 0x445640 (PSX Battle_LevelClass 0x801DB3FC): the first of six
// thresholds in row `row` of 0x64E374 that the level is below, else 6. Row 0
// the enemies' (16, 36, 64, 99, 99, 99), row 1 the party's (8, 16, 32, 48,
// 99, 99), so 4 and 5 never come back and a level of 99 is class 6. Both
// arguments' low bytes; the row is not bounded.
extern "C" unsigned char __cdecl Battle_LevelClass(unsigned level, unsigned row) {
    const unsigned char* const thresholds = At(at::kLevelRows + (row & 0xFF) * 6);
    for (unsigned i = 0; i < 6; ++i)
        if ((level & 0xFF) < thresholds[i]) return static_cast<unsigned char>(i);
    return 6;
}

// original 0x445980 (PSX Battle_ActorCanAct 0x801DB9AC): 1 if the actor can
// take a turn. A party member: bit 0 of its object's first byte, none of the
// status bits 0x4944, and with 0x904B8E set bit 4 of its second flags; an
// enemy the same on its own fields with 0x4144. Then 0 when 0x904AE4 names the
// actor's side (2 the party, 1 the enemies) or is 3.
extern "C" unsigned char __cdecl Battle_ActorCanAct(unsigned actor) {
    if ((actor & 0xFF) < 3) {
        const unsigned char* const p = Party(actor);
        if ((p[at::kPPresent] & 1) == 0) return 0;
        if (Word(p + at::kPStatus) & 0x4944) return 0;
        if (Byte(at::kGate2) != 0 && (p[at::kPFlags2] & 0x10) == 0) return 0;
        const unsigned char side = Byte(at::kSideOut);
        return side == 2 || side == 3 ? 0 : 1;
    }
    const unsigned char* const e = Enemy(actor);
    if ((e[at::kEPresent] & 1) == 0) return 0;
    if (Word(e + at::kEStatus) & 0x4144) return 0;
    if (Byte(at::kGate2) != 0 && (e[at::kEFlags2] & 0x10) == 0) return 0;
    const unsigned char side = Byte(at::kSideOut);
    return side == 1 || side == 3 ? 0 : 1;
}

// original 0x4458B0 (PSX 0x801DB80C): Battle_ActorCanAct with more refused -
// the status mask 0x4964 for the party, and the second flags' bits 0x14001
// (party) or 0x4000 (enemy). What the entry order asks before it lists a
// member.
extern "C" unsigned char __cdecl Battle_ActorCanCommand(unsigned actor) {
    if ((actor & 0xFF) < 3) {
        const unsigned char* const p = Party(actor);
        if ((p[at::kPPresent] & 1) == 0) return 0;
        if (Word(p + at::kPStatus) & 0x4964) return 0;
        if (Byte(at::kGate2) != 0 && (p[at::kPFlags2] & 0x10) == 0) return 0;
        if (Long(p + at::kPFlags2) & 0x14001) return 0;
        const unsigned char side = Byte(at::kSideOut);
        return side == 2 || side == 3 ? 0 : 1;
    }
    const unsigned char* const e = Enemy(actor);
    if ((e[at::kEPresent] & 1) == 0) return 0;
    if (Word(e + at::kEStatus) & 0x4164) return 0;
    if (Byte(at::kGate2) != 0 && (e[at::kEFlags2] & 0x10) == 0) return 0;
    if (Long(e + at::kEFlags2) & 0x4000) return 0;
    const unsigned char side = Byte(at::kSideOut);
    return side == 1 || side == 3 ? 0 : 1;
}

// original 0x444F40 (PSX 0x801DA7A4): the order the party enters its
// commands in. Each member who passes the gate and Battle_ActorCanCommand is
// listed with its agility (+0x98), the list sorted highest first, then
// written to 0x904AB6 (three bytes, 0xFF-ended) - each member asked
// Battle_ActorCanCommand again - with the count at 0x904AC3 (read back as a
// signed byte for the index, as the original does).
// As the original has it: the enemies are walked first through
// Battle_ActorCanAct for a sum and a highest agility that nothing reads (the
// PSX computes and drops the same); the calls are kept, the arithmetic is not.
extern "C" void __cdecl Battle_BuildEntryOrder() {
    for (unsigned a = 3; a <= 10; ++a) {
        if (Gated(Enemy(a) + at::kEFlags2)) continue;
        g.can_act(a);
    }
    unsigned char list[3 * 4];
    unsigned n = 0;
    for (unsigned a = 0; a <= 2; ++a) {
        const unsigned char* const p = Party(a);
        if (Gated(p + at::kPFlags2)) continue;
        if (!g.can_command(a)) continue;
        SetWord(list + 4 * n, Word(p + at::kPAgi));
        SetWord(list + 4 * n + 2, a);
        ++n;
    }
    SortDescending(list, n);
    At(at::kEntryCount)[0] = 0;
    SetWord(At(at::kEntryOrder), 0xFFFF);
    At(at::kEntryOrder + 2)[0] = 0xFF;
    for (unsigned k = 0; k < n; ++k) {
        const unsigned char actor = list[4 * k + 2];
        if (!g.can_command(actor)) continue;
        const int index = static_cast<signed char>(Byte(at::kEntryCount));
        At(at::kEntryOrder + index)[0] = actor;
        At(at::kEntryCount)[0] = static_cast<unsigned char>(Byte(at::kEntryCount) + 1);
    }
}

// original 0x4450E0 (PSX Battle_BuildTurnOrder 0x801DAAB4): who acts in
// which order this round. Each actor who passes the gate and
// Battle_ActorCanAct gets a value in 0x939A80 (s16 value, u16 actor):
//   - a party member its agility (+0x98) plus a command bonus times the
//     percentage of its level class (0x64E328, row 1) / 100: the bonus 4 for
//     an ability of order kind 0 (NameTable_Abilities +1), 2 for kind 1 or 3,
//     1 for an item, else 0 - 16-bit arithmetic, the product truncated to a
//     word and divided signed;
//   - an enemy its agility (+0x38) plus a signed byte of 0x64E334, sixteen
//     per level class (row 0), picked by Rand & 15.
// The count to 0x904AE3, the pairs sorted highest first, the actors copied
// to 0x904ACC (eleven bytes, 0xFF where unused), the cursor 0x904AE2 to 0.
// As the original has it: the party is first walked through
// Battle_ActorCanAct for a sum and a highest agility nothing reads; and a
// level class of 6 reads the percentage past the five real ones (the first
// word of 0x64E334, 516 %) and the enemies' bytes past their four rows (the
// variance table's bytes) - the PSX has the same (docs/battle_damage.md,
// defects).
extern "C" void __cdecl Battle_BuildTurnOrder() {
    for (unsigned a = 0; a <= 2; ++a) {
        if (Gated(Party(a) + at::kPFlags2)) continue;
        g.can_act(a);
    }
    unsigned n = 0;
    for (unsigned a = 0; a <= 2; ++a) {
        unsigned char* const p = Party(a);
        if (Gated(p + at::kPFlags2)) continue;
        if (!g.can_act(a)) continue;
        unsigned char* const slot = At(at::kTurnWork + 4 * n);
        const unsigned char command = p[at::kPCommand];
        SetWord(slot + 2, a);
        SetWord(slot, 0);
        if (command == 4) {
            const unsigned kind = At(bof3::addr::NameTable_Abilities + Word(p + at::kPCommandId) * at::kAbilityStride)[1];
            if (kind == 0) SetWord(slot, 4);
            else if (kind == 1 || kind == 3) SetWord(slot, 2);
        }
        if (command == 5) SetWord(slot, 1);
        const unsigned level_class = g.level_class(p[at::kPLevel], 1);
        const unsigned percent = Word(At(at::kTurnPercent + 2 * level_class));
        const int product = S16(percent * Word(slot));
        SetWord(slot, static_cast<unsigned>(product / 100) + Word(p + at::kPAgi));
        ++n;
    }
    for (unsigned a = 3; a <= 10; ++a) {
        const unsigned char* const e = Enemy(a);
        if (Gated(e + at::kEFlags2)) continue;
        if (!g.can_act(a)) continue;
        unsigned char* const slot = At(at::kTurnWork + 4 * n);
        const unsigned level_class = g.level_class(e[at::kELevel], 0);
        const unsigned roll = static_cast<unsigned>(g.rand()) & 0xF;
        const int jitter = static_cast<signed char>(Byte(at::kTurnJitter + 16 * level_class + roll));
        SetWord(slot, static_cast<unsigned>(jitter) + Word(e + at::kEAgi));
        SetWord(slot + 2, a);
        ++n;
    }
    At(at::kTurnCount)[0] = static_cast<unsigned char>(n);
    SortDescending(At(at::kTurnWork), n);
    SetLong(At(at::kTurnOrder), -1);
    SetLong(At(at::kTurnOrder + 4), -1);
    SetWord(At(at::kTurnOrder + 8), 0xFFFF);
    At(at::kTurnOrder + 10)[0] = 0xFF;
    for (unsigned k = 0; k < Byte(at::kTurnCount); ++k) At(at::kTurnOrder + k)[0] = Byte(at::kTurnWork + 4 * k + 2);
    At(at::kTurnCursor)[0] = 0;
}

// original 0x445680 (PSX Battle_ClearCommands 0x801DB45C): the command byte
// and the first flag dword of the three party records and the eight enemy
// records cleared.
extern "C" void __cdecl Battle_ClearCommands() {
    for (unsigned a = 0; a < 3; ++a) {
        unsigned char* const p = Party(a);
        p[at::kPCommand] = 0;
        SetLong(p + at::kPFlags, 0);
    }
    for (unsigned a = 3; a < 11; ++a) {
        unsigned char* const e = Enemy(a);
        e[at::kECommand] = 0;
        SetLong(e + at::kEFlags, 0);
    }
}

// original 0x445730 (PSX 0x801DB594): the first actor on the same side as
// `actor` (its low byte) that 0x4456C0 does not call out, looking from it
// upwards and then from the side's first: the party 0..2, the enemies 3..10;
// 0xFF if none. A byte above 10 skips the upward look and asks 3 up to it.
// Each probe is pushed as the argument slot with its low byte replaced - the
// callee reads the byte.
extern "C" unsigned char __cdecl Battle_DefaultTarget(unsigned actor) {
    const unsigned b = actor & 0xFF;
    const unsigned high = actor & ~0xFFu;
    if (b < 3) {
        for (unsigned i = b; i <= 2; ++i)
            if (!g.is_out(high | i)) return static_cast<unsigned char>(i);
        for (unsigned i = 0; i < b; ++i)
            if (!g.is_out(high | i)) return static_cast<unsigned char>(i);
        return 0xFF;
    }
    if (b <= 10)
        for (unsigned i = b; i <= 10; ++i)
            if (!g.is_out(high | i)) return static_cast<unsigned char>(i);
    for (unsigned i = 3; i < b; ++i)
        if (!g.is_out(high | i)) return static_cast<unsigned char>(i);
    return 0xFF;
}

// original 0x446650 (PSX Battle_RemoveFromTurnOrder 0x801DD114): the actor's
// command byte cleared and bit 1 of its flags (a party member also bit 2 of
// its second flags), then every place in the turn order (0x904ACC, up to the
// count 0x904AE3) holding the actor's byte set to 0xFF.
extern "C" void __cdecl Battle_RemoveFromTurnOrder(unsigned actor) {
    const unsigned char b = static_cast<unsigned char>(actor);
    if (b <= 2) {
        unsigned char* const p = Party(actor);
        p[at::kPCommand] = 0;
        SetLong(p + at::kPFlags, S32(U32(Long(p + at::kPFlags)) & ~2u));
        SetLong(p + at::kPFlags2, S32(U32(Long(p + at::kPFlags2)) & ~4u));
    } else {
        unsigned char* const e = Enemy(actor);
        e[at::kECommand] = 0;
        SetLong(e + at::kEFlags, S32(U32(Long(e + at::kEFlags)) & ~2u));
    }
    for (unsigned k = 0; k < Byte(at::kTurnCount); ++k)
        if (Byte(at::kTurnOrder + k) == b) At(at::kTurnOrder + k)[0] = 0xFF;
}

// ===========================================================================
// The damage chain

// original 0x446430 (PSX Battle_ScaleDamage 0x801DCD18): the formula's tail.
// In 8.8 fixed point: s = base << 8; the multiplier 0x100 - (s / 256000) <<
// 17, >> 8, at least 0xCD (205/256 - for any base under 256,000 it is 0xCD);
// times s >> 8; times the variance Battle_DamageVarianceTable[Rand & 7]
// (218..307 / 256) >> 8. Then, when DamageScratch's low byte has any of bits
// 0..4 (the weapon's elements), times Battle_ElementAffinity(target, the
// scratch word) / 100; when it has bit 5 (holy), times
// Battle_HolyAffinityTable[the target's holy class] / 100 (the scratch read
// again after the affinity call, as the original does). Rounded half up out
// of 8.8. Every product 32-bit and wrapping, every division signed and
// truncating. `attacker` is not read.
extern "C" int __cdecl Battle_ScaleDamage(unsigned attacker, unsigned target, int base) {
    (void)attacker;
    const std::int32_t s = S32(U32(base) << 8);
    std::int32_t m = S32(0x100u - (U32(s / 256000) << 17)) >> 8;
    if (m < 0xCD) m = 0xCD;
    const int roll = g.rand();
    m = Mul(m, s) >> 8;
    std::int32_t v = Mul(Long(At(bof3::addr::Battle_DamageVarianceTable + 4 * (U32(roll) & 7))), m) >> 8;
    unsigned scratch = Word(At(bof3::addr::DamageScratch));
    if (scratch & 0x1F) {
        const short affinity = static_cast<short>(g.element_affinity(target, scratch));
        v = Mul(affinity, v) / 100;
        scratch = Word(At(bof3::addr::DamageScratch));
    }
    if (scratch & 0x20) {
        const unsigned holy = (target & 0xFF) <= 2 ? Party(target)[at::kPHoly] : Enemy(target)[at::kEHoly];
        v = Mul(S16(Word(At(bof3::addr::Battle_HolyAffinityTable + 2 * holy))), v) / 100;
    }
    if ((v & 0xFF) >= 0x80) v = S32(U32(v) + 0x100);
    return v >> 8;
}

// original 0x4462B0 (PSX Battle_BaseDamage 0x801DCAA0): the attack less the
// defence, then Battle_ScaleDamage. The attack is the scratch word 0x939FE4.
// A party attacker, or an enemy on an enemy: less the target's defence
// 0x939F86 (mode 0) or nothing (mode 1), not below 0. An enemy on the party:
// less 0x4463E0's defence (mode 0), not below 0, then in 8.8 plus the enemy's
// word +0x18 x (2 + Rand % 2) << 16 / (0x64E380[i] << 8), i.e. x 0.2 or 0.3;
// i is (x / -5), 0 above -5, 8 below -0x33 - and x is 0 or more by then, so
// i is always 0 and the divisor always 10 (the PSX has the same dead
// arithmetic). Then + (Rand & 1) and Battle_ScaleDamage(attacker, target,
// that), whose answer is returned whole. Only the arguments' low bytes are
// read; both are passed on whole.
extern "C" int __cdecl Battle_BaseDamage(unsigned attacker, unsigned target, unsigned mode) {
    const unsigned a = attacker & 0xFF;
    const bool plain = (mode & 0xFF) != 0;
    std::int32_t v;
    if (a <= 2 || (target & 0xFF) > 2) {
        const std::int32_t def = plain ? 0 : Word(At(at::kTargetDef));
        v = static_cast<std::int32_t>(U32(Long(At(at::kAttackerAtk))) & 0xFFFF) - def;
        if (v < 0) v = 0;
    } else {
        const std::int32_t def = plain ? 0 : static_cast<std::int32_t>(U32(g.party_def()) & 0xFFFF);
        std::int32_t x = static_cast<std::int32_t>(U32(Long(At(at::kAttackerAtk))) & 0xFFFF) - def;
        if (x < 0) x = 0;
        unsigned char index = static_cast<unsigned char>(x / -5);
        if (x > -5) index = 0;
        if (x < -0x33) index = 8;
        const std::int32_t factor = g.rand() % 2 + 2;
        const std::int32_t numerator = S32(U32(Mul(factor, Word(Enemy(attacker) + at::kELevel))) << 16);
        const std::int32_t divisor = static_cast<std::int32_t>(Byte(at::kBonusDivisor + index)) << 8;
        if (divisor == 0) bof3::Fatal("Battle_BaseDamage: a zero divisor, where the original divides by zero");
        v = S32(U32(numerator / divisor) + (U32(x) << 8)) >> 8;
    }
    v = S32(U32(v) + (U32(g.rand()) & 1));
    return g.scale_damage(attacker, target, v);
}

namespace {
// Battle_CalcDamage's roll for flag bit 1: eight percentages on its stack.
constexpr unsigned char kFlagRoll[8] = {50, 50, 50, 50, 60, 60, 60, 70};
}  // namespace

// original 0x445CF0 (PSX Battle_CalcDamage 0x801DC00C): an attack's damage,
// before it is applied.
//   1. DamageScratch = `element`'s low word; 0xFFFF means the weapon's: a
//      party attacker's weapon element byte (NameTable_Weapons +0x13), an
//      enemy 0.
//   2. A charged attacker (second flags bit 6): the attack 0x939FE4 plus
//      (charge x attack) >> 1 for the party - but set to that for an enemy,
//      as on the PSX; the charge byte cleared, the bit cleared.
//   3. Battle_BaseDamage(attacker, target, 0). A party attacker on an enemy:
//      doubled for weapons 0x13 / 0x17 / 0x33 on family 4, and again for
//      0x16 / 0x35 / 0x40 on family 1 (the enemy record's +0x0D).
//   4. The target's status byte: without any of 0x64, the to-hit roll
//      (0x446110 on the party, 0x4461B0 on an enemy) decides the amount;
//      with 0x40 or 0x20 the status is cured through 0x44F4B0 (its bit
//      or'ed into 0x904B9A) and the amount is at least 1; with 4, at least 1.
//   5. Flag bit 1: the amount x {50,50,50,50,60,60,60,70}[Rand % 8] / 100.
//   6. 0x904AA8 bit 7: amount x 2 + Battle_BaseDamage(.., 1) / 4.
//   7. A quarter off: on a party target when 0x904060 is 2; on a status 0x80.
//   8. At least 1 with status 0x60, or with 0x904AA8 bit 7.
//   9. 0 if the target's flags have 0x10000; else clamped to +-9999.
// From step 7 on the amount is a word (the original's `sar ax, 2` with the
// rest of eax whatever it held); before it, 32 bits, as the to-hit roll and
// step 6 see it.
// As the original has it: step 5 reads the table at Rand % 8 signed - the
// CRT's rand is never negative, and ours refuses loudly if it were.
extern "C" short __cdecl Battle_CalcDamage(unsigned attacker, unsigned target, unsigned element) {
    const unsigned a = attacker & 0xFF;
    const unsigned t = target & 0xFF;
    unsigned char* const scratch = At(bof3::addr::DamageScratch);
    if ((element & 0xFFFF) == 0xFFFF) {
        if (a <= 2) {
            const unsigned weapon = Party(a)[at::kPWeapon];
            SetWord(scratch, Byte(bof3::addr::NameTable_Weapons + weapon * at::kWeaponStride + at::kWeaponElement));
        } else {
            SetWord(scratch, 0);
        }
    } else {
        SetWord(scratch, element);
    }
    unsigned char* const atk = At(at::kAttackerAtk);
    if (a <= 2) {
        unsigned char* const p = Party(a);
        if (p[at::kPFlags2] & 0x40) {
            const std::int32_t add = static_cast<std::int32_t>(p[at::kPCharge] * (U32(Long(atk)) & 0xFFFF)) >> 1;
            SetWord(atk, Word(atk) + U32(add));
            p[at::kPCharge] = 0;
            SetLong(p + at::kPFlags2, S32(U32(Long(p + at::kPFlags2)) & ~0x40u));
        }
    } else {
        unsigned char* const e = Enemy(a);
        if (e[at::kEFlags2] & 0x40) {
            const std::int32_t set = static_cast<std::int32_t>(e[at::kECharge] * (U32(Long(atk)) & 0xFFFF)) >> 1;
            SetWord(atk, U32(set));
            e[at::kECharge] = 0;
            SetLong(e + at::kEFlags2, S32(U32(Long(e + at::kEFlags2)) & ~0x40u));
        }
    }
    std::int32_t v = g.base_damage(attacker, target, 0);
    if (a <= 2 && t >= 3) {
        const unsigned char weapon = Party(a)[at::kPWeapon];
        if ((weapon == 0x13 || weapon == 0x17 || weapon == 0x33) && Enemy(t)[at::kEFamily] == 4) v = S32(U32(v) * 2);
        if ((weapon == 0x16 || weapon == 0x35 || weapon == 0x40) && Enemy(t)[at::kEFamily] == 1) v = S32(U32(v) * 2);
    }
    unsigned char status, flags;
    if (t <= 2) {
        status = Party(t)[at::kPStatus];
        flags = Party(t)[at::kPFlags];
    } else {
        status = Enemy(t)[at::kEStatus];
        flags = Enemy(t)[at::kEFlags];
    }
    if ((status & 0x64) == 0) {
        v = t <= 2 ? g.hit_party(v, attacker, target) : g.hit_enemy(v, attacker, target);
    } else {
        if (status & 0x40) {
            At(at::kCuredBits)[0] = static_cast<unsigned char>(Byte(at::kCuredBits) | 0x40);
            if (t <= 2) {
                g.cure(Party(t)[at::kPCureId], 0x40);
                Party(t)[at::kPShown2] = 0;
            } else {
                g.cure(Enemy(t)[at::kECureId], 0x40);
                Enemy(t)[at::kEShown2] = 0;
            }
            if ((U32(v) & 0xFFFF) == 0) v = 1;
        }
        if (status & 0x20) {
            SetWord(At(at::kCuredBits), Word(At(at::kCuredBits)) | 0x20);
            if (t <= 2) {
                g.cure(Party(t)[at::kPCureId], 0x20);
                Party(t)[at::kPShown2] = 0;
            } else {
                g.cure(Enemy(t)[at::kECureId], 0x20);
                Enemy(t)[at::kEShown2] = 0;
            }
            if ((U32(v) & 0xFFFF) == 0) v = 1;
        }
        if ((status & 4) && (U32(v) & 0xFFFF) == 0) v = 1;
    }
    if (flags & 2) {
        const int roll = g.rand();
        if (roll < 0) bof3::Fatal("Battle_CalcDamage: Rand %d, where the original reads its saved registers", roll);
        v = Mul(kFlagRoll[roll % 8], S16(U32(v))) / 100;
    }
    if (Byte(at::kBattleFlags) & 0x80) {
        const std::int32_t plain = S16(U32(g.base_damage(attacker, target, 1)));
        v = S32(U32((plain + (plain < 0 ? 3 : 0)) >> 2) + U32(v) * 2);
    }
    unsigned short d = static_cast<unsigned short>(v);
    if (t <= 2 && Byte(at::kPartyCut) == 2) d = static_cast<unsigned short>(d - static_cast<unsigned short>(S16(d) >> 2));
    if (status & 0x80) d = static_cast<unsigned short>(d - static_cast<unsigned short>(S16(d) >> 2));
    if ((status & 0x60) && d == 0) d = 1;
    if ((Byte(at::kBattleFlags) & 0x80) && d == 0) d = 1;
    if (t <= 2 ? (Long(Party(t) + at::kPFlags) & 0x10000) : (Long(Enemy(t) + at::kEFlags) & 0x10000)) return 0;
    if (S16(d) > 9999) return 9999;
    if (S16(d) < -9999) return -9999;
    return S16(d);
}

// original 0x445A30 (PSX Battle_ApplyDamage 0x801DBB40): an attack landed.
// The target's shown byte (+0x11C / +0x8C) = 0x11; the amount
// Battle_CalcDamage(attacker, target, 0xFFFF). The flag 0x100 in the target's
// flags (an enemy's only while its HP is at most 0x7FFF) makes the amount the
// whole HP, shows the message 0x44A650(1, 0, 0, 0x1E, *0x669E04), clears the
// flag and sets 0x8031F3. Then, a party member: damage below its HP is taken
// off; otherwise HP 0, and the flag bit 2 (a kill) set when the overkill is
// below its byte +0x8C and neither of its second flags' bits 0..1 is set; a
// heal (0 or less) is added up to max HP, reaching it sets shown bit 2. An
// enemy the same without the overkill test, and an HP of 0xFFFF takes
// nothing. Last, a party attacker whose weapon kind (+0x138) is 5, for a
// non-zero amount: weapon 'J' / 'L' / 'O' inflicts 0x40 / 8 / 0x20 through
// 0x44F1D0 unless 0x44FA70 says it was resisted. Returns the amount.
extern "C" short __cdecl Battle_ApplyDamage(unsigned attacker, unsigned target) {
    const unsigned t = target & 0xFF;
    if (t <= 2) Party(t)[at::kPShown] = 0x11;
    else Enemy(t)[at::kEShown] = 0x11;
    std::int32_t v = g.calc_damage(attacker, target, 0xFFFF);
    if (t <= 2) {
        unsigned char* const p = Party(t);
        if (Long(p + at::kPFlags) & 0x100) {
            v = Word(p + at::kPHp);
            g.show_message(1, 0, 0, 0x1E, U32(Long(At(at::kKillMessage))));
            const std::uint32_t f = U32(Long(p + at::kPFlags)) & ~0x100u;
            At(at::kInstantKill)[0] = 1;
            SetLong(p + at::kPFlags, S32(f));
        }
        const std::int32_t hp = Word(p + at::kPHp);
        if (v > 0) {
            if (hp > v) {
                SetWord(p + at::kPHp, U32(hp - v));
            } else {
                if (v - hp < p[at::kPSurvive] && (p[at::kPFlags2] & 3) == 0)
                    SetLong(p + at::kPFlags, S32(U32(Long(p + at::kPFlags)) | 4));
                SetWord(p + at::kPHp, 0);
            }
        } else {
            const std::int32_t max = Word(p + at::kPMaxHp);
            if (hp - v < max) {
                SetWord(p + at::kPHp, U32(hp - v));
            } else {
                SetWord(p + at::kPHp, U32(max));
                p[at::kPShown] = static_cast<unsigned char>(p[at::kPShown] | 4);
            }
        }
    } else {
        unsigned char* const e = Enemy(t);
        if (Word(e + at::kEHp) != 0xFFFF) {
            if ((Long(e + at::kEFlags) & 0x100) && Word(e + at::kEHp) <= 0x7FFF) {
                v = Word(e + at::kEHp);
                g.show_message(1, 0, 0, 0x1E, U32(Long(At(at::kKillMessage))));
                const std::uint32_t f = U32(Long(e + at::kEFlags)) & ~0x100u;
                At(at::kInstantKill)[0] = 1;
                SetLong(e + at::kEFlags, S32(f));
            }
            const std::int32_t hp = Word(e + at::kEHp);
            if (v > 0) {
                SetWord(e + at::kEHp, hp > v ? U32(hp - v) : 0);
            } else {
                const std::int32_t max = Word(e + at::kEMaxHp);
                if (hp - v < max) {
                    SetWord(e + at::kEHp, U32(hp - v));
                } else {
                    SetWord(e + at::kEHp, U32(max));
                    e[at::kEShown] = static_cast<unsigned char>(e[at::kEShown] | 4);
                }
            }
        }
    }
    if ((attacker & 0xFF) <= 2) {
        const unsigned char* const p = Party(attacker);
        if (p[at::kPWeaponKind] == 5 && v != 0) {
            if (p[at::kPWeapon] == 'J' && !g.element_resisted(attacker, target)) g.inflict(target, 0x40);
            if (p[at::kPWeapon] == 'L' && !g.element_resisted(attacker, target)) g.inflict(target, 8);
            if (p[at::kPWeapon] == 'O' && !g.element_resisted(attacker, target)) g.inflict(target, 0x20);
        }
    }
    return static_cast<short>(v);
}

// original 0x44EE80 (PSX Battle_ElementAffinity 0x8009FA78): for each of bits
// 0..4 of the mask's low byte (fire, ice, lightning, earth, wind), the s16
// 0x64E95C[the target's class byte for it] added up - the party's five at
// +0x9F, an enemy's at +0x3F. Summed in 32 bits (the PSX in 16; every caller
// takes the low word).
extern "C" int __cdecl Battle_ElementAffinity(unsigned target, unsigned mask) {
    const unsigned char* const classes =
        (target & 0xFF) <= 2 ? Party(target) + at::kPElements : Enemy(target) + at::kEElements;
    std::int32_t sum = 0;
    for (unsigned k = 0; k < 5; ++k)
        if (mask & (1u << k)) sum += S16(Word(At(at::kElementTable + 2 * classes[k])));
    return sum;
}

// ===========================================================================
// The effects

namespace {
unsigned char* Result() { return At(U32(Long(At(at::kResult)))); }
}  // namespace

// original 0x44B9F0 (PSX Effect_ApplyResult 0x8009A160): an ability or item
// resolved on one target. No argument on the PC: the target is the byte
// 0x904B54 (the PSX passes it).
//   1. The target's shown byte |= 0x31.
//   2. Unless the acting kind (0x904B35) is 4 and the ability 0x904B80 is
//      0xA3: a charged actor (0x904B34; second flags bit 7) multiplies the
//      power word 0x939FEA by its charge (+0x135 / +0xA5) + 1, 16-bit; the
//      charge and the bit cleared.
//   3. The handler 0x64E73C[i], i = 0x64E540[ability] for kind 4, else
//      0x64E72C[category][item] from the word at *0x904B40 + 2 (category the
//      high byte, unbounded). It fills the result record *0x904B60: +4 the HP
//      delta, +6 the AP delta, positive is damage.
//   4. A target (0x904B54 read again) with flag 0x10000 takes nothing: both
//      deltas 0.
//   5. Each delta clamped to +-9999, then applied: damage below HP / AP
//      taken off, else 0 - a party member's HP kill sets flag bit 2 under
//      Battle_ApplyDamage's overkill test; a heal up to the maximum, reaching
//      it sets shown bits 0 and 2 (HP) or 1 and 3 (AP) and clears bit 4 or 5.
//      An enemy's HP or AP of 0xFFFF takes nothing.
// As the original has it (the PSX too): an enemy's AP heal is compared with
// its max HP, then clamped to its max AP (docs/battle_damage.md, defects).
extern "C" void __cdecl Effect_ApplyResult() {
    const unsigned target0 = Byte(at::kTarget);
    if (target0 <= 2) Party(target0)[at::kPShown] = static_cast<unsigned char>(Party(target0)[at::kPShown] | 0x31);
    else Enemy(target0)[at::kEShown] = static_cast<unsigned char>(Enemy(target0)[at::kEShown] | 0x31);
    const std::uint32_t acting = U32(Long(At(at::kActing)));
    const std::uint32_t ability = U32(Long(At(at::kAbility)));
    const unsigned actor = acting & 0xFF;
    const unsigned kind = (acting >> 8) & 0xFF;
    if (!(kind == 4 && (ability & 0xFFFF) == 0xA3)) {
        unsigned char* const power = At(at::kAttackerInt);
        if (actor <= 2) {
            unsigned char* const p = Party(actor);
            if (p[at::kPFlags2] & 0x80) {
                const unsigned times = p[at::kPCharge2] + 1u;
                p[at::kPCharge2] = 0;
                SetWord(power, times * Word(power));
                SetLong(p + at::kPFlags2, S32(U32(Long(p + at::kPFlags2)) & ~0x80u));
            }
        } else {
            unsigned char* const e = Enemy(actor);
            if (e[at::kEFlags2] & 0x80) {
                const unsigned times = e[at::kECharge2] + 1u;
                e[at::kECharge2] = 0;
                SetWord(power, times * Word(power));
                SetLong(e + at::kEFlags2, S32(U32(Long(e + at::kEFlags2)) & ~0x80u));
            }
        }
    }
    unsigned handler;
    if (kind == 4) {
        handler = Byte(at::kSkillHandler + (ability & 0xFFFF));
    } else {
        const unsigned item = Word(At(U32(Long(At(at::kItemCommand)))) + 2);
        handler = At(U32(Long(At(at::kItemHandlers + 4 * (item >> 8)))))[item & 0xFF];
    }
    reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(U32(Long(At(at::kHandlers + 4 * handler)))))();

    const unsigned t = Byte(at::kTarget);
    if (t <= 2 ? (Long(Party(t) + at::kPFlags) & 0x10000) : (Long(Enemy(t) + at::kEFlags) & 0x10000)) {
        SetWord(Result() + 4, 0);
        SetWord(Result() + 6, 0);
    }
    unsigned char* r = Result();
    if (S16(Word(r + 4)) > 9999) {
        SetWord(r + 4, 9999);
        r = Result();
    }
    if (S16(Word(r + 4)) < -9999) {
        SetWord(r + 4, static_cast<unsigned>(-9999));
        r = Result();
    }
    const std::int32_t hp_delta = S16(Word(r + 4));
    if (hp_delta != 0) {
        if (t <= 2) {
            unsigned char* const p = Party(t);
            const std::int32_t hp = Word(p + at::kPHp);
            if (hp_delta > 0) {
                if (hp > hp_delta) {
                    SetWord(p + at::kPHp, U32(hp - hp_delta));
                } else {
                    if (hp_delta - hp < p[at::kPSurvive] && (p[at::kPFlags2] & 3) == 0)
                        SetLong(p + at::kPFlags, S32(U32(Long(p + at::kPFlags)) | 4));
                    SetWord(p + at::kPHp, 0);
                }
            } else {
                const std::int32_t max = Word(p + at::kPMaxHp);
                if (hp - hp_delta < max) {
                    SetWord(p + at::kPHp, U32(hp - hp_delta));
                } else {
                    SetWord(p + at::kPHp, U32(max));
                    p[at::kPShown] = static_cast<unsigned char>((p[at::kPShown] & 0xEF) | 5);
                }
                r = Result();
            }
        } else {
            unsigned char* const e = Enemy(t);
            const std::int32_t hp = Word(e + at::kEHp);
            if (hp != 0xFFFF) {
                if (hp_delta > 0) {
                    SetWord(e + at::kEHp, hp > hp_delta ? U32(hp - hp_delta) : 0);
                } else {
                    const std::int32_t max = Word(e + at::kEMaxHp);
                    if (hp - hp_delta < max) {
                        SetWord(e + at::kEHp, U32(hp - hp_delta));
                    } else {
                        SetWord(e + at::kEHp, U32(max));
                        e[at::kEShown] = static_cast<unsigned char>((e[at::kEShown] & 0xEF) | 5);
                    }
                    r = Result();
                }
            }
        }
    }
    if (S16(Word(r + 6)) > 9999) {
        SetWord(r + 6, 9999);
        r = Result();
    }
    if (S16(Word(r + 6)) < -9999) {
        SetWord(r + 6, static_cast<unsigned>(-9999));
        r = Result();
    }
    const std::int32_t ap_delta = S16(Word(r + 6));
    if (ap_delta == 0) return;
    if (t <= 2) {
        unsigned char* const p = Party(t);
        const std::int32_t ap = Word(p + at::kPAp);
        if (ap_delta > 0) {
            SetWord(p + at::kPAp, ap > ap_delta ? U32(ap - ap_delta) : 0);
        } else {
            const std::int32_t max = Word(p + at::kPMaxAp);
            if (ap - ap_delta < max) {
                SetWord(p + at::kPAp, U32(ap - ap_delta));
            } else {
                SetWord(p + at::kPAp, U32(max));
                p[at::kPShown] = static_cast<unsigned char>((p[at::kPShown] & 0xDF) | 0xA);
            }
        }
        return;
    }
    unsigned char* const e = Enemy(t);
    const std::int32_t ap = Word(e + at::kEAp);
    if (ap == 0xFFFF) return;
    if (ap_delta > 0) {
        SetWord(e + at::kEAp, ap > ap_delta ? U32(ap - ap_delta) : 0);
    } else if (ap - ap_delta < Word(e + at::kEMaxHp)) {
        SetWord(e + at::kEAp, U32(ap - ap_delta));
    } else {
        SetWord(e + at::kEAp, Word(e + at::kEMaxAp));
        e[at::kEShown] = static_cast<unsigned char>((e[at::kEShown] & 0xDF) | 0xA);
    }
}

// original 0x44ED10 (PSX 0x8009F820): an ability's damage, called by the
// effect handlers with (caster, target, power, psi). In integers: (INT
// 0x939FEA + 100) x power (its low word) x 100 / 100 (the product wraps, the
// division unsigned); x (100 - the target's 0x939F8A / 5, at least 50) / 100
// (unsigned); when the ability's element mask (NameTable_Abilities +4, low
// nine bits) is non-zero, x the affinity / 100 (signed) - Battle_ElementAffinity,
// or 0x44F030 when `psi`'s low byte is set; x {85..120}[Rand & 7] / 10000
// (signed). Then a quarter off (when positive) when the result's target
// 0x904B54 is in the party and 0x904060 is 2; halved (signed) when the target
// has second flags bit 9; 0 when it has flag 0x10000. `caster` is not read.
extern "C" int __cdecl Effect_SkillDamage(unsigned caster, unsigned target, unsigned power, unsigned psi) {
    (void)caster;
    const std::uint32_t scaled = (Word(At(at::kAttackerInt)) + 100u) * (power & 0xFFFF);
    std::uint32_t v = scaled * 100u / 100u;
    std::int32_t resist = 100 - static_cast<std::int32_t>(Word(At(at::kTargetInt)) / 5u);
    if (resist < 50) resist = 50;
    v = U32(resist) * v / 100u;
    const unsigned mask =
        Word(At(bof3::addr::NameTable_Abilities + (U32(Long(At(at::kAbility))) & 0xFFFF) * at::kAbilityStride + 4)) & 0x1FF;
    if (mask != 0) {
        const short affinity = static_cast<short>((psi & 0xFF) ? g.psi_affinity(target, mask) : g.element_affinity(target, mask));
        v = U32(Mul(affinity, S32(v)) / 100);
    }
    const unsigned roll = U32(g.rand()) & 7;
    std::int32_t amount = Mul(Word(At(at::kPowerVariance + 2 * roll)), S32(v)) / 10000;
    if (Byte(at::kTarget) <= 2 && Byte(at::kPartyCut) == 2 && amount > 0) amount -= amount >> 2;
    if ((target & 0xFF) <= 2) {
        const unsigned char* const p = Party(target);
        if (Long(p + at::kPFlags2) & 0x200) amount /= 2;
        if (Long(p + at::kPFlags) & 0x10000) return 0;
    } else {
        const unsigned char* const e = Enemy(target);
        if (Long(e + at::kEFlags2) & 0x200) amount /= 2;
        if (Long(e + at::kEFlags) & 0x10000) return 0;
    }
    return amount;
}

// original 0x44F130 (PSX 0x800A0080): a healing ability's HP delta, called by
// two effect handlers with (caster, target). The ability's power byte
// (NameTable_Abilities +3) x (INT 0x939FEA + 100) x 100 / 100 (wrapping,
// unsigned), x the s16 0x64E99C[the target's holy class] / -10000 (signed,
// truncating) - so a heal (negative) for classes 4..7 (50..400 %) and damage
// for 0..2 (-300..-100 %). The PSX divides by 10000 and negates; the same
// numbers. `caster` is not read.
extern "C" int __cdecl Effect_HealAmount(unsigned caster, unsigned target) {
    (void)caster;
    const std::uint32_t id = U32(Long(At(at::kAbility))) & 0xFFFF;
    const std::uint32_t power = Byte(bof3::addr::NameTable_Abilities + id * at::kAbilityStride + 3);
    const std::uint32_t base = power * (Word(At(at::kAttackerInt)) + 100u) * 100u / 100u;
    const unsigned holy = (target & 0xFF) <= 2 ? Party(target)[at::kPHoly] : Enemy(target)[at::kEHoly];
    return Mul(S16(Word(At(at::kHealTable + 2 * holy))), S32(base)) / -10000;
}

// ===========================================================================
// The enemies' AI conditions

// original 0x44B2C0 (PSX EnemyAI_RowDone 0x80099830): bit `row` (its low
// byte, as a shift count of 0..31 - 8 and up answer 0) of the enemy object's
// byte +0xF1, the rows that have fired.
extern "C" unsigned char __cdecl EnemyAI_RowDone(const unsigned char* enemy, unsigned row) {
    return static_cast<unsigned char>((static_cast<unsigned>(enemy[0xF1]) >> (row & 0x1F)) & 1);
}

// original 0x44AE90 (PSX EnemyAI_ChooseActions 0x80098F8C): for each enemy
// 0..7 that 0x4456C0 does not call out, its script (0x8C5600 + 0x8C x the
// object's byte +0xF0), rows 0..3 of 16 bytes. Byte 0 of a row is a
// condition; when it holds, a row that has not fired (EnemyAI_RowDone) is
// applied (0x44B3A0(enemy, row)) and marked fired (0x44B2E0(enemy, row, 1));
// when it fails, a fired row is unmarked. The conditions (the object's HP
// +0xA4, AP +0xA6, max HP +0xB0, max AP +0xB2, all u16):
//   0x0B HP <= max / 4        0x0C HP <= max / 2        0x0D AP <= max AP / 5
//   0x0E another enemy is not out and has status bit 0x2000
//   0x0F 0x904AB2 > 0x904AB3  0x10 0x904AB3 == 1
//   0x11 HP > max / 2         0x12 AP > max AP / 2
//   0x13 turn counter 0x904B90 == 2                    0x14 == 10
//   0x15 party member 0's level less the object's word +0x98 is above 5
//   0x19 the counter's low byte even                   0x1A odd
//   0x1B the counter a multiple of 3                   0x1C not
//   0x1D 0x904AB1 == 1        0x1E != 1
//   0x1F 0x904AB3 == 0x904AB2                          0x20 0x904AB3 != 1
//   0x26 0x44B240(row, enemy)                          0x27 0x904B97 == 1
// Any other byte (0x16..0x18 and 0x21..0x25 among them) leaves the row as it
// is. The script byte and the condition are read again for every row. Then
// 0x44B920(). The PSX's script is 0x88 bytes a script; the PC's 0x8C.
extern "C" void __cdecl EnemyAI_ChooseActions() {
    for (unsigned e = 0; e < 8; ++e) {
        if (g.is_out(e + 3)) continue;
        unsigned char* const obj = Enemy(e + 3) + at::kEObject;
        for (unsigned r = 0; r < 4; ++r) {
            const unsigned char* const row = At(at::kAiScripts + obj[0xF0] * at::kAiScriptStride + 16 * r);
            const unsigned hp = Word(obj + 0xA4), ap = Word(obj + 0xA6), max_hp = Word(obj + 0xB0), max_ap = Word(obj + 0xB2);
            const std::uint32_t counter = U32(Long(At(at::kTurnCounter)));
            bool holds;
            switch (row[0]) {
            case 0x0B: holds = hp <= (max_hp >> 2); break;
            case 0x0C: holds = hp <= (max_hp >> 1); break;
            case 0x0D: holds = static_cast<std::int32_t>(ap) <= static_cast<std::int32_t>(max_ap / 5); break;
            case 0x0E: {
                unsigned char count = 0;
                for (unsigned k = 0; k < 8; ++k)
                    if (!g.is_out(k + 3) && (Enemy(k + 3)[at::kEStatus + 1] & 0x20)) ++count;
                holds = count != 0;
                break;
            }
            case 0x0F: holds = Byte(at::kAiByteB2) > Byte(at::kAiByteB3); break;
            case 0x10: holds = Byte(at::kAiByteB3) == 1; break;
            case 0x11: holds = hp > (max_hp >> 1); break;
            case 0x12: holds = ap > (max_ap >> 1); break;
            case 0x13: holds = counter == 2; break;
            case 0x14: holds = counter == 10; break;
            case 0x15: holds = static_cast<std::int32_t>(Party(0)[at::kPLevel]) - static_cast<std::int32_t>(Word(obj + 0x98)) > 5; break;
            case 0x19: holds = (counter & 1) == 0; break;
            case 0x1A: holds = (counter & 1) != 0; break;
            case 0x1B: holds = counter % 3 == 0; break;
            case 0x1C: holds = counter % 3 != 0; break;
            case 0x1D: holds = Byte(at::kAiByteB1) == 1; break;
            case 0x1E: holds = Byte(at::kAiByteB1) != 1; break;
            case 0x1F: holds = Byte(at::kAiByteB3) == Byte(at::kAiByteB2); break;
            case 0x20: holds = Byte(at::kAiByteB3) != 1; break;
            case 0x26: holds = g.ai_rows_left(r, e) != 0; break;
            case 0x27: holds = Byte(at::kAiByteB97) == 1; break;
            default: continue;
            }
            if (holds) {
                if (!g.row_done(obj, r)) {
                    g.ai_apply(obj, row);
                    g.ai_set_done(obj, r, 1);
                }
            } else if (g.row_done(obj, r)) {
                g.ai_set_done(obj, r, 0);
            }
        }
    }
    g.ai_finish();
}

// ===========================================================================

void BattleDamage_Inject() {
    if (bof3::WantsShadow("battle_damage")) battle_damage::SelfTest();
    BOF3_INJECT(Battle_BuildEntryOrder);
    BOF3_INJECT(Battle_BuildTurnOrder);
    BOF3_INJECT(Battle_LevelClass);
    BOF3_INJECT(Battle_ClearCommands);
    BOF3_INJECT(Battle_DefaultTarget);
    BOF3_INJECT(Battle_ActorCanCommand);
    BOF3_INJECT(Battle_ActorCanAct);
    BOF3_INJECT(Battle_ApplyDamage);
    BOF3_INJECT(Battle_CalcDamage);
    BOF3_INJECT(Battle_BaseDamage);
    BOF3_INJECT(Battle_ScaleDamage);
    BOF3_INJECT(Battle_RemoveFromTurnOrder);
    BOF3_INJECT(EnemyAI_ChooseActions);
    BOF3_INJECT(EnemyAI_RowDone);
    BOF3_INJECT(Effect_ApplyResult);
    BOF3_INJECT(Effect_SkillDamage);
    BOF3_INJECT(Battle_ElementAffinity);
    BOF3_INJECT(Effect_HealAmount);
}
