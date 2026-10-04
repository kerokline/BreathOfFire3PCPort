// Round fourteen group R3C (docs/takeover-queue-round14.md section 2, the
// cut analysis/round14_cut.tsv's group column): 61 slots of Effect_Handlers
// (0x64E73C) in the band 0x44D000..0x44E4AA, each read to its last
// instruction with capstone (2026-10-04) and taken through the boss harness's
// engine frame (boss_harness.h, docs/boss_harness.md section 10).
// docs/rest_3c.md has every function one row each.
//
// Effect_ApplyResult (ours, battle_damage.cpp) calls a slot with no argument
// and drops its eax. A slot reads the actor 0x904B34 and the target 0x904B54
// (0..2 a member of ObjTrio, 3..10 an enemy object) and fills the result
// record *0x904B60 (+4 the HP delta, positive damage; +6 the AP delta; +8
// flags), or changes a record directly. Eleven set the acting kind 4 and an
// ability and tail-jump to an ability's damage or heal (R3B's), others call
// R3D's helpers (the resist test 0x44F6A0, the common tail 0x44FB30, the
// status and stat helpers): those are raw addresses (rest_3c_callees.h) until
// their groups merge. A slot that ends in a call or a jump to one of them
// answers that callee's eax (al for the status helpers); the rest are void.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original writes through an index past its records - a member past the
// three, an enemy past the eight, the party count past 3, the turn order past
// the byte after its eleven - ours aborts with a Fatal naming the function
// (the owner's rule for an unchecked index, round9 doc section 6); reads by an
// actor or target byte stay unchecked, as in the rest of our battle code.
// Every call goes through the harness (BH_CALL / BH_AT), so the start-up fuzz
// can stand recorders in for the callees.
#include "game/rest_3c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/rest_3c_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3c::at;
using U = std::uint32_t;
using S32 = std::int32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
std::uint16_t W(U address) { return Word(At(address)); }
void SetW(U address, unsigned v) { SetWord(At(address), v); }
U L(U address) { return static_cast<U>(Long(At(address))); }
S32 S16(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
unsigned char* Ptr(U cell) { return At(L(cell)); }
// The result record, read from its cell each time the original reads it.
unsigned char* Result() { return Ptr(at::kResult); }

// A record by an actor byte, as the original computes the address: unchecked
// (a read), the enemy's index the byte less 3 in 32 bits.
unsigned char* Party(U member) { return At(at::kParty + member * at::kPartyStride); }
unsigned char* Enemy(U index) { return At(at::kEnemies + index * at::kEnemyStride); }
unsigned char* CharRecord(U character) { return At(at::kCharRecords + character * at::kCharStride); }
// ... and for a write: a Fatal past the three members or the eight enemies,
// where the original writes on into what follows them.
unsigned char* PartyWrite(U member, const char* who) {
    if (member >= at::kPartyMax)
        bof3::Fatal("%s: member %u past ObjTrio's three - the original writes on into the window records (docs/rest_3c.md section 7)",
                    who, member);
    return Party(member);
}
unsigned char* EnemyWrite(U index, const char* who) {
    if (index >= at::kEnemyMax)
        bof3::Fatal("%s: enemy %u (an actor byte less 3) past the eight objects - the original writes outside them "
                    "(docs/rest_3c.md section 7)",
                    who, index);
    return Enemy(index);
}

// The actor's or target's word at a member's or an enemy's offset (the HP
// +0x98 / +0xA4 pair and the like), by the byte's side: 0..2 a member.
std::uint16_t SideWord(unsigned who, U party_at, U enemy_at) {
    return who < at::kPartyMax ? Word(Party(who) + party_at) : Word(Enemy(who - 3) + enemy_at);
}

// The party count 0x904AB0 as the loops over ObjTrio read it.
unsigned PartyCount(const char* who) {
    const unsigned n = B(at::kPartyCount);
    if (n > at::kPartyMax)
        bof3::Fatal("%s: the party count 0x904AB0 is %u, past ObjTrio's three members - the original walks on into "
                    "WindowRecords (docs/rest_3c.md section 7)",
                    who, n);
    return n;
}

using A0 = U (__cdecl*)();
using A1 = U (__cdecl*)(U);
using A2 = U (__cdecl*)(U, U);
using V0 = void (__cdecl*)();

U MissTail() { return BH_AT(A0, at::kMissTail)(); }
U Resisted() { return BH_AT(A2, at::kResisted)(B(at::kActor), B(at::kTarget)); }

// Slots 50..61, 83: the acting kind 4 and an ability, then a tail jump to
// R3B's kind-4 damage (0x44C040), heal (0x44C120) or flag (0x44CF60).
U AsAbility(unsigned ability, U tail) {
    B(at::kActingKind) = 4;
    SetW(at::kAbility, ability);
    return BH_AT(A0, tail)();
}

// The party's stats derived again, as BattleParty_RecalcStats (BE3's 0x442310)
// does after its Char_RecalcStats - the same instructions in slots 75 and 106:
// for each member below the count 0x904AB0, its +0x92..+0x97 from its
// character record's +0x12..+0x17 and 32 bytes of +0xC0 from the record's
// +0x20; Formation_ApplyStatMods; for each member, +0xC0 copied to +0xA0;
// BattleForm_ApplyStats; Battle_RecalcStats(member) for each member, the
// count read again after each call.
void RecalcParty(const char* who) {
    unsigned n = PartyCount(who);
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const m = Party(i);
        const unsigned char* const r = CharRecord(m[0x148]);
        for (unsigned k = 0; k < 6; ++k) m[0x92 + k] = r[0x12 + k];
        std::memcpy(m + 0xC0, r + 0x20, 32);
    }
    BH_CALL(Formation_ApplyStatMods)();
    n = PartyCount(who);
    for (unsigned i = 0; i < n; ++i) std::memmove(Party(i) + 0xA0, Party(i) + 0xC0, 32);
    BH_CALL(BattleForm_ApplyStats)();
    if (B(at::kPartyCount) == 0) return;
    unsigned member = 0;
    do {
        BH_CALL(Battle_RecalcStats)(member);
        member = static_cast<unsigned char>(member + 1);
    } while (member < B(at::kPartyCount));
}

}  // namespace

// ===========================================================================
// Slots 50..61, 83: an ability's effect by its number
// ===========================================================================

// original 0x44D000 (Effect_Handlers slot 50): kind 4, ability 0x60, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility60(void) { return AsAbility(0x60, at::kSkillByAbility); }
// original 0x44D020 (slot 51): kind 4, ability 0x68, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility68(void) { return AsAbility(0x68, at::kSkillByAbility); }
// original 0x44D040 (slot 52): kind 4, ability 0x5B, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility5B(void) { return AsAbility(0x5B, at::kSkillByAbility); }

// original 0x44D060 (slot 53): kind 4, ability 0x52, then 0x44FBB0(1) - the
// target's stat byte 1 raised by the ability's power unless it resists; al.
extern "C" unsigned __cdecl Effect_AsAbility52RaiseStat1(void) {
    B(at::kActingKind) = 4;
    SetW(at::kAbility, 0x52);
    return BH_AT(A1, at::kRaiseByAbility)(1);
}
// original 0x44D080 (slot 54): kind 4, ability 0x5A, then 0x44FBB0(1); al.
extern "C" unsigned __cdecl Effect_AsAbility5ARaiseStat1(void) {
    B(at::kActingKind) = 4;
    SetW(at::kAbility, 0x5A);
    return BH_AT(A1, at::kRaiseByAbility)(1);
}

// original 0x44D0A0 (slot 55): kind 4, ability 0x5C, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility5C(void) { return AsAbility(0x5C, at::kSkillByAbility); }
// original 0x44D0C0 (slot 56): kind 4, ability 0x64, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility64(void) { return AsAbility(0x64, at::kSkillByAbility); }

// original 0x44D0E0 (slot 57): kind 4, ability 0x57, then 0x44FC60(0x10) -
// status 0x10 on the target unless it resists; al.
extern "C" unsigned __cdecl Effect_AsAbility57Inflict10(void) {
    B(at::kActingKind) = 4;
    SetW(at::kAbility, 0x57);
    return BH_AT(A1, at::kMissInflict)(0x10);
}

// original 0x44D100 (slot 58): kind 4, ability 0x61, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility61(void) { return AsAbility(0x61, at::kSkillByAbility); }
// original 0x44D120 (slot 59): kind 4, ability 0x46, jmp 0x44C120 (the heal).
extern "C" unsigned __cdecl Effect_AsAbility46(void) { return AsAbility(0x46, at::kHealByAbility); }
// original 0x44D140 (slot 60): kind 4, ability 0x67, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility67(void) { return AsAbility(0x67, at::kSkillByAbility); }
// original 0x44D160 (slot 61): kind 4, ability 0x56, jmp 0x44CF60 (flag 0x200).
extern "C" unsigned __cdecl Effect_AsAbility56(void) { return AsAbility(0x56, at::kFlag200); }

// ===========================================================================
// Slots 62..76: damage forms, the status helpers, small heals
// ===========================================================================

// original 0x44D180 (slot 62): 0x904AA9 |= 0x20, 0x939F86 = 0, the HP delta
// Battle_CalcDamage(actor, target, 0xFFFF) doubled (16 bits), then 0x904660 = 5.
extern "C" void __cdecl Effect_DoubleDamage(void) {
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    B(at::kFlags2) = static_cast<unsigned char>(B(at::kFlags2) | 0x20);
    SetW(at::kTargetDef, 0);
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<unsigned>(static_cast<std::uint16_t>(damage)) << 1);
    B(at::kByte660) = 5;
}

// original 0x44D1C0 (slot 63): for kind 4 with ability 0x9A, 0x904AA9 |=
// 0x20; 0x939F86 = 0; the HP delta Battle_CalcDamage(actor, target, 0xFFFF)
// / 4 (signed, truncated).
extern "C" void __cdecl Effect_QuarterDamage(void) {
    if (B(at::kActingKind) == 4 && W(at::kAbility) == 0x9A) B(at::kFlags2) = static_cast<unsigned char>(B(at::kFlags2) | 0x20);
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    SetW(at::kTargetDef, 0);
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<unsigned>(S32(damage) / 4));
}

// original 0x44D220 / 0x44D240 / 0x44D260 (slots 64, 65, 66): the HP delta
// 0x44FCE0(3 / 1 / 2) - the actor's HP over the divisor, varied; its eax.
extern "C" unsigned __cdecl Effect_ActorHpDamage3(void) {
    const U delta = BH_AT(A1, at::kHpDamage)(3);
    SetWord(Result() + 4, delta);
    return delta;
}
extern "C" unsigned __cdecl Effect_ActorHpDamage1(void) {
    const U delta = BH_AT(A1, at::kHpDamage)(1);
    SetWord(Result() + 4, delta);
    return delta;
}
extern "C" unsigned __cdecl Effect_ActorHpDamage2(void) {
    const U delta = BH_AT(A1, at::kHpDamage)(2);
    SetWord(Result() + 4, delta);
    return delta;
}

// original 0x44D280 / 0x44D330 / 0x44D340 (slots 67, 69, 70): 0x44FC60(4 /
// 8 / 0x80) - the common tail, then the status unless resisted; al.
extern "C" unsigned __cdecl Effect_InflictStatus4(void) { return BH_AT(A1, at::kMissInflict)(4); }
extern "C" unsigned __cdecl Effect_InflictStatus8(void) { return BH_AT(A1, at::kMissInflict)(8); }
extern "C" unsigned __cdecl Effect_InflictStatus80(void) { return BH_AT(A1, at::kMissInflict)(0x80); }

// original 0x44D290 (slot 68): the target's HP and status word (a member's
// +0x98 / +0x90, an enemy's +0xA4 / +0x92) read first; the HP delta
// Battle_CalcDamage(actor, the target's dword, 2). Then, the delta read again
// through the result cell: below that HP (signed against the word), the
// status's bit 6 clear and the delta not 0 - 0x44FCA0(0x40), status 0x40 on
// a target the hit leaves standing.
extern "C" void __cdecl Effect_DamageInflict40(void) {
    const U target = L(at::kTarget);
    const unsigned t = target & 0xFF;
    std::uint16_t hp, status;
    if (t < 3) {
        hp = Word(Party(t) + 0x98);
        status = Word(Party(t) + 0x90);
    } else {
        hp = Word(Enemy(t - 3) + 0xA4);
        status = Word(Enemy(t - 3) + 0x92);
    }
    const short damage = BH_CALL(Battle_CalcDamage)(B(at::kActor), target, 2);
    SetWord(Result() + 4, static_cast<std::uint16_t>(damage));
    const std::uint16_t delta = Word(Result() + 4);
    if (S16(delta) < static_cast<S32>(hp) && (status & 0x40) == 0 && delta != 0) BH_AT(A1, at::kInflictUnlessResisted)(0x40);
}

// original 0x44D350 (slot 71): 0x44FB30, then 0x44FCA0(0x20); al.
extern "C" unsigned __cdecl Effect_InflictStatus20(void) {
    MissTail();
    return BH_AT(A1, at::kInflictUnlessResisted)(0x20);
}

// original 0x44D360 (slot 72): the result's flags |= 2, the HP delta -5, the AP delta -1.
extern "C" void __cdecl Effect_Heal5Ap1(void) {
    Result()[8] = static_cast<unsigned char>(Result()[8] | 2);
    SetWord(Result() + 4, 0xFFFB);
    SetWord(Result() + 6, 0xFFFF);
}

// original 0x44D390 (slot 73): the attacker's power 0x939FE4 = the actor's
// +0xA4 (a member) / +0xB4 (an enemy), then four fifths of it (x 80 / 100,
// read back as a dword & 0xFFFF); the HP delta Battle_CalcDamage(the actor's
// dword, target, 0xFFFF).
extern "C" void __cdecl Effect_Attack80(void) {
    const U actor = L(at::kActor);
    SetW(at::kAttackerAtk, SideWord(actor & 0xFF, 0xA4, 0xB4));
    const U power = L(at::kAttackerAtk) & 0xFFFF;
    const unsigned target = B(at::kTarget);
    SetW(at::kAttackerAtk, static_cast<U>(static_cast<S32>(power * 80) / 100));
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<std::uint16_t>(damage));
}

// original 0x44D420 (slot 74): 0x904AA9 |= 0x40, the actor into 0x904B8A and
// 0x904B44 into 0x904B8B, jmp 0x44FB30.
extern "C" unsigned __cdecl Effect_FlagActorPair(void) {
    const unsigned actor = B(at::kActor);
    const unsigned other = B(at::kOtherActor);
    B(at::kFlags2) = static_cast<unsigned char>(B(at::kFlags2) | 0x40);
    B(at::kPairActor) = static_cast<unsigned char>(actor);
    B(at::kPairOther) = static_cast<unsigned char>(other);
    return MissTail();
}

// original 0x44D450 (slot 75): 0x44FB30; for a member target only,
// Char_RecalcStats(its character record by +0x148), then the party's stats
// derived again (RecalcParty above).
extern "C" void __cdecl Effect_TargetRecalcParty(void) {
    MissTail();
    const unsigned t = B(at::kTarget);
    if (t > 2) return;
    BH_CALL(Char_RecalcStats)(CharRecord(Party(t)[0x148]));
    RecalcParty("Effect_TargetRecalcParty (0x44D450)");
}

// original 0x44D580 (slot 76): 0x44F1D0(target, 0x40); the result's flags
// |= 2, the HP delta -20, the AP delta -4.
extern "C" void __cdecl Effect_Inflict40Heal20Ap4(void) {
    BH_AT(A2, at::kInflict)(B(at::kTarget), 0x40);
    Result()[8] = static_cast<unsigned char>(Result()[8] | 2);
    SetWord(Result() + 4, 0xFFEC);
    SetWord(Result() + 6, 0xFFFC);
}

// original 0x44D5C0 / 0x44D5D0 / 0x44D5E0 (slots 77, 78, 79) and 0x44D6C0
// (slot 85): the HP delta -1 / -80 / -240 / -5.
extern "C" void __cdecl Effect_Heal1(void) { SetWord(Result() + 4, 0xFFFF); }
extern "C" void __cdecl Effect_Heal80(void) { SetWord(Result() + 4, 0xFFB0); }
extern "C" void __cdecl Effect_Heal240(void) { SetWord(Result() + 4, 0xFF10); }
extern "C" void __cdecl Effect_Heal5(void) { SetWord(Result() + 4, 0xFFFB); }

// original 0x44D5F0 / 0x44D610 / 0x44DE00 (slots 80, 81, 102): the result's
// flags = 2, the AP delta -5 / -40 / -10.
extern "C" void __cdecl Effect_RestoreAp5(void) {
    Result()[8] = 2;
    SetWord(Result() + 6, 0xFFFB);
}
extern "C" void __cdecl Effect_RestoreAp40(void) {
    Result()[8] = 2;
    SetWord(Result() + 6, 0xFFD8);
}
extern "C" void __cdecl Effect_RestoreAp10(void) {
    Result()[8] = 2;
    SetWord(Result() + 6, 0xFFF6);
}

// ===========================================================================
// Slots 82..101
// ===========================================================================

// original 0x44D630 (slot 82): ability 0x66's power byte read, kind 4 and
// ability 0x66 set; the HP delta Effect_SkillDamage(actor, target, that power,
// 0), then halved (signed, truncated) through the result cell read again.
extern "C" void __cdecl Effect_AsAbility66Half(void) {
    const unsigned power = B(bof3::addr::NameTable_Abilities + 0x66 * at::kAbilityStride + 3);
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    B(at::kActingKind) = 4;
    SetW(at::kAbility, 0x66);
    const int damage = BH_CALL(Effect_SkillDamage)(actor, target, power, 0);
    SetWord(Result() + 4, static_cast<unsigned>(damage));
    unsigned char* const r = Result();
    SetWord(r + 4, static_cast<unsigned>(S16(Word(r + 4)) / 2));
}

// original 0x44D680 (slot 83): kind 4, ability 0x63, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility63(void) { return AsAbility(0x63, at::kSkillByAbility); }

// original 0x44D6A0 (slot 84): Battle_ClearStatus(target, 0xBFC), jmp 0x44C170
// (the HP delta the target's whole maximum, a heal).
extern "C" unsigned __cdecl Effect_CureBFCHealMax(void) {
    BH_CALL(Battle_ClearStatus)(B(at::kTarget), 0xBFC);
    return BH_AT(A0, at::kHealMaxHp)();
}

// original 0x44D6D0 (slot 87): 0x44FB30; the enemy (target - 3) & 0xFF: its
// +0xAA up by 2 and +0xAE up by 1, each when not 0 and its marker bit 0
// (+0xAB, +0xAF) clear - the marker set; then each held to 7. There is no
// member branch: a member target (index 253..255) would write past the image
// (ours aborts, section 7).
extern "C" void __cdecl Effect_EnemyRaiseAAAE(void) {
    MissTail();
    const unsigned index = static_cast<unsigned char>(B(at::kTarget) - 3);
    unsigned char* const e = EnemyWrite(index, "Effect_EnemyRaiseAAAE (0x44D6D0)");
    if (e[0xAA] != 0 && (e[0xAB] & 1) == 0) {
        e[0xAB] = static_cast<unsigned char>(e[0xAB] | 1);
        e[0xAA] = static_cast<unsigned char>(e[0xAA] + 2);
    }
    if (e[0xAE] != 0 && (e[0xAF] & 1) == 0) {
        e[0xAF] = static_cast<unsigned char>(e[0xAF] | 1);
        e[0xAE] = static_cast<unsigned char>(e[0xAE] + 1);
    }
    if (e[0xAA] > 7) e[0xAA] = 7;
    if (e[0xAE] > 7) e[0xAE] = 7;
}

// original 0x44D770 (slot 88): the HP delta Battle_CalcDamage(actor, target,
// 0xFFFF); then (the target read again) when it is not below the target's HP
// (signed words), the delta is that HP - 1: the hit never fells.
extern "C" void __cdecl Effect_AttackNoKill(void) {
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<std::uint16_t>(damage));
    const std::uint16_t hp = SideWord(B(at::kTarget), 0x98, 0xA4);
    unsigned char* const r = Result();
    if (S16(hp) > S16(Word(r + 4))) return;
    SetWord(r + 4, hp - 1u);
}

// original 0x44D7E0 (slot 89): unless 0x44F6A0(actor, target) answers the
// target resisted, the HP delta is half the target's HP (unsigned).
extern "C" void __cdecl Effect_HalveTargetHp(void) {
    if (Resisted() & 0xFF) return;
    const std::uint16_t hp = SideWord(B(at::kTarget), 0x98, 0xA4);
    SetWord(Result() + 4, hp >> 1);
}

// original 0x44D850 (slot 90): the target's +0x134 by ObjTrio's stride
// (whatever side: an enemy's index reads past the three members) with bit 0
// or 1: the result's flags = 1. Else a member's +0x143 = 0, and 0x44FCA0(0x800)
// answering not 0: flags = 1; else jmp 0x44FB30.
extern "C" void __cdecl Effect_Inflict800Unguarded(void) {
    const unsigned t = B(at::kTarget);
    if (Party(t)[0x134] & 3) {
        Result()[8] = 1;
        return;
    }
    if (t <= 2) Party(t)[0x143] = 0;
    if (BH_AT(A1, at::kInflictUnlessResisted)(0x800) & 0xFF) {
        Result()[8] = 1;
        return;
    }
    MissTail();
}

// original 0x44D8B0 (slot 91; not in the cut; R3D's 0x44EA70 also jumps
// here): resisted - the result's flags = 1; else the HP delta is the target's
// whole HP.
extern "C" void __cdecl Effect_DamageAllHp(void) {
    if (Resisted() & 0xFF) {
        Result()[8] = 1;
        return;
    }
    SetWord(Result() + 4, SideWord(B(at::kTarget), 0x98, 0xA4));
}

// original 0x44D920 (slot 92): the attacker's power 0x939FE4 = the actor's
// +0xA4 / +0xB4, then by Rand & 3: 0 halved, 1 kept, 2 one and a half times
// (16 bits), 3 doubled (16 bits); the HP delta Battle_CalcDamage(actor,
// target, 0xFFFF).
extern "C" void __cdecl Effect_RandomPowerAttack(void) {
    SetW(at::kAttackerAtk, SideWord(B(at::kActor), 0xA4, 0xB4));
    const U roll = static_cast<U>(BH_CALL(Rand)()) & 3;
    if (roll == 0) {
        SetW(at::kAttackerAtk, W(at::kAttackerAtk) >> 1);
    } else if (roll == 2) {
        const unsigned power = W(at::kAttackerAtk);
        SetW(at::kAttackerAtk, power + (power >> 1));
    } else if (roll == 3) {
        SetW(at::kAttackerAtk, L(at::kAttackerAtk) * 2);
    }
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<std::uint16_t>(damage));
}

// original 0x44D9D0 (slot 93): resisted - the result's flags = 1 and
// Battle_SetDamagePopup(0, target); else the target's +0x134 (a member) /
// +0x114 (an enemy) |= 0x2000 and Battle_RecalcStats(the target's dword).
// Both end in jmp 0x44FB30.
extern "C" unsigned __cdecl Effect_TargetFlag2000(void) {
    const char* const who = "Effect_TargetFlag2000 (0x44D9D0)";
    if (Resisted() & 0xFF) {
        Result()[8] = 1;
        BH_CALL(Battle_SetDamagePopup)(0, B(at::kTarget));
        return MissTail();
    }
    const U target = L(at::kTarget);
    const unsigned t = target & 0xFF;
    if (t <= 2) {
        unsigned char* const p = PartyWrite(t, who);
        SetLong(p + 0x134, static_cast<S32>(static_cast<U>(Long(p + 0x134)) | 0x2000));
    } else {
        unsigned char* const e = EnemyWrite(t - 3, who);
        SetLong(e + 0x114, static_cast<S32>(static_cast<U>(Long(e + 0x114)) | 0x2000));
    }
    BH_CALL(Battle_RecalcStats)(target);
    return MissTail();
}

// original 0x44DA70 (slot 94): the actor's status word (+0x90 / +0x92) loses
// bit 3, kept aside; 0x939FFC = 100; the HP delta Battle_CalcDamage(the
// actor's dword, target, 0xFFFF) / 2 (signed, truncated); then the actor read
// again gets the bit back (an OR).
extern "C" void __cdecl Effect_SureHitHalfDamage(void) {
    const char* const who = "Effect_SureHitHalfDamage (0x44DA70)";
    const U actor = L(at::kActor);
    const unsigned a = actor & 0xFF;
    unsigned char* const status = a < 3 ? PartyWrite(a, who) + 0x90 : EnemyWrite(a - 3, who) + 0x92;
    const unsigned w = Word(status);
    const unsigned bit = w & 8;
    SetWord(status, w & 0xFFF7);
    const unsigned target = B(at::kTarget);
    B(at::kHitPercent) = 0x64;
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<unsigned>(S32(damage) / 2));
    const unsigned again = B(at::kActor);
    unsigned char* const back = again < 3 ? PartyWrite(again, who) + 0x90 : EnemyWrite(again - 3, who) + 0x92;
    SetWord(back, Word(back) | bit);
}

// original 0x44DB40 (slot 95): 0x44FB30; the enemy (target - 3)'s words +0x94
// and +0x96 doubled, each held to 0xFFFF. No member branch: a member target
// writes below the enemies (ours aborts, section 7).
extern "C" void __cdecl Effect_EnemyDouble94And96(void) {
    MissTail();
    unsigned char* const e = EnemyWrite(static_cast<U>(B(at::kTarget)) - 3, "Effect_EnemyDouble94And96 (0x44DB40)");
    const unsigned a = Word(e + 0x94);
    const unsigned b = Word(e + 0x96);
    const unsigned a2 = a * 2 > 0xFFFF ? 0xFFFF : a * 2;
    const unsigned b2 = b * 2 > 0xFFFF ? 0xFFFF : b * 2;
    SetWord(e + 0x94, a2);
    SetWord(e + 0x96, b2);
}

// original 0x44DBC0 (slot 96): the HP delta 0; resisted - the result's flags =
// 1; else 0x44FB30, Battle_ReturnQueuedItem(target), Battle_RemoveFromTurnOrder(target).
extern "C" void __cdecl Effect_DropFromTurnOrder(void) {
    SetWord(Result() + 4, 0);
    if (Resisted() & 0xFF) {
        Result()[8] = 1;
        return;
    }
    MissTail();
    BH_CALL(Battle_ReturnQueuedItem)(B(at::kTarget));
    BH_CALL(Battle_RemoveFromTurnOrder)(B(at::kTarget));
}

// original 0x44DC10 (slot 97): 0x44FB30; 0x904B8E = 4; the target's +0x134
// |= 0x10 by ObjTrio's stride (no enemy branch: ours aborts past the three);
// then each slot i of the turn order from the cursor 0x904AE2 - 1 through
// 0x904AE3 inclusive (the end read again each pass) that is not 0xFF:
// Battle_ReturnQueuedItem(i) - the slot's index, not the actor in it - and
// the slot = 0xFF. The inclusive end reads the byte after the eleven when the
// order is full (section 7).
extern "C" void __cdecl Effect_FlushTurnOrder(void) {
    const char* const who = "Effect_FlushTurnOrder (0x44DC10)";
    MissTail();
    const unsigned t = B(at::kTarget);
    const unsigned first = B(at::kTurnCursor);
    B(at::kGate) = 4;
    unsigned char* const p = PartyWrite(t, who);
    SetLong(p + 0x134, static_cast<S32>(static_cast<U>(Long(p + 0x134)) | 0x10));
    unsigned i = static_cast<unsigned char>(first - 1);
    if (i > B(at::kTurnCount)) return;
    do {
        if (i > 11)
            bof3::Fatal("%s: turn order slot %u past the eleven and the byte after - the original reads and writes on "
                        "(docs/rest_3c.md section 7)",
                        who, i);
        if (B(at::kTurnOrder + i) != 0xFF) {
            BH_CALL(Battle_ReturnQueuedItem)(i);
            B(at::kTurnOrder + i) = 0xFF;
        }
        i = static_cast<unsigned char>(i + 1);
    } while (i <= B(at::kTurnCount));
}

// original 0x44DCA0 (slot 98): 0x44FB30; 0x44F650(10, 4) - the result
// record's byte +0x18 up by 10, held to -25..50; Battle_RecalcStats(target).
extern "C" void __cdecl Effect_RaiseStat4By10(void) {
    MissTail();
    BH_AT(A2, at::kRaiseStat)(0xA, 4);
    BH_CALL(Battle_RecalcStats)(B(at::kTarget));
}

// original 0x44DCC0 (slot 99): 0x904AA9 |= 0x20; the HP delta the actor's HP
// - 1, and the actor's HP = 1 (the actor read again for the store).
extern "C" void __cdecl Effect_ActorHpToDamage(void) {
    const char* const who = "Effect_ActorHpToDamage (0x44DCC0)";
    const unsigned a = B(at::kActor);
    B(at::kFlags2) = static_cast<unsigned char>(B(at::kFlags2) | 0x20);
    if (a < 3) {
        SetWord(Result() + 4, Word(Party(a) + 0x98) - 1u);
        SetWord(PartyWrite(B(at::kActor), who) + 0x98, 1);
    } else {
        SetWord(Result() + 4, Word(Enemy(a - 3) + 0xA4) - 1u);
        SetWord(EnemyWrite(static_cast<U>(B(at::kActor)) - 3, who) + 0xA4, 1);
    }
}

// original 0x44DD60 (slot 100): unless resisted, the HP delta the target's
// HP - 1 (0 when the HP is 1): to one HP.
extern "C" void __cdecl Effect_DamageToOneHp(void) {
    if (Resisted() & 0xFF) return;
    const std::uint16_t hp = SideWord(B(at::kTarget), 0x98, 0xA4);
    SetWord(Result() + 4, hp == 1 ? 0u : hp - 1u);
}

// original 0x44DDD0 (slot 101): the HP delta Effect_HealAmount(actor,
// target), then Battle_ClearStatus(target, 0xBFC).
extern "C" void __cdecl Effect_HealAndCureBFC(void) {
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    const int heal = BH_CALL(Effect_HealAmount)(actor, target);
    SetWord(Result() + 4, static_cast<unsigned>(heal));
    BH_CALL(Battle_ClearStatus)(B(at::kTarget), 0xBFC);
}

// ===========================================================================
// Slots 103..111
// ===========================================================================

// original 0x44DE20 (slot 103): 0x904B96 hits (the count read again each
// pass): hit i adds Battle_CalcDamage(actor, target, 0xFFFF) x f / 10
// (signed, truncated), f the s8 0x64E944[i] for i below 7 and 3 after; the
// sum held to -9999..9999 is the HP delta.
extern "C" void __cdecl Effect_MultiHit(void) {
    S32 sum = 0;
    unsigned i = 0;
    if (B(at::kHits) != 0) {
        do {
            const S32 scale = i < 7 ? static_cast<signed char>(B(at::kHitScale + i)) : 3;
            const unsigned target = B(at::kTarget);
            const unsigned actor = B(at::kActor);
            const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
            sum += static_cast<S32>(static_cast<U>(S32(damage)) * static_cast<U>(scale)) / 10;
            i = static_cast<unsigned char>(i + 1);
        } while (i < B(at::kHits));
        if (sum > 9999) sum = 9999;
        else if (sum < -9999) sum = -9999;
    }
    SetWord(Result() + 4, static_cast<U>(sum));
}

// original 0x44DEF0 (slot 104): the HP delta Effect_SkillDamage(actor, target,
// the ability's power byte (+3 of its NameTable_Abilities row, the word
// 0x904B80 unchecked), 0) - 0x44C040's body - less the target's +0xA6 (a
// member) / +0xB6 (an enemy), held at 0 from below (signed).
extern "C" void __cdecl Effect_SkillDamageLessDef(void) {
    const U ability = L(at::kAbility) & 0xFFFF;
    const unsigned target = B(at::kTarget);
    const unsigned power = B(bof3::addr::NameTable_Abilities + ability * at::kAbilityStride + 3);
    const unsigned actor = B(at::kActor);
    const int damage = BH_CALL(Effect_SkillDamage)(actor, target, power, 0);
    SetWord(Result() + 4, static_cast<unsigned>(damage));
    const unsigned t = B(at::kTarget);
    unsigned char* const r = Result();
    const std::uint16_t def = t <= 2 ? Word(Party(t) + 0xA6) : Word(Enemy(t - 3) + 0xB6);
    SetWord(r + 4, Word(r + 4) - static_cast<unsigned>(def));
    unsigned char* const r2 = Result();
    if (S16(Word(r2 + 4)) < 0) SetWord(r2 + 4, 0);
}

// original 0x44DF80 (slot 105): the HP delta Battle_CalcDamage(actor, target,
// 0xFFFF) / 2 (signed, truncated); not 0: 0x44FCA0(0x20).
extern "C" void __cdecl Effect_HalfDamageInflict20(void) {
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<unsigned>(S32(damage) / 2));
    if (Word(Result() + 4) != 0) BH_AT(A1, at::kInflictUnlessResisted)(0x20);
}

// original 0x44DFD0 (slot 106): by the actor's side -
//   a member: its character record's byte +0x1E below 5 - that byte and the
//   member's +0x9E up by one; Char_RecalcStats(the record), then the party's
//   stats derived again (RecalcParty);
//   an enemy whose maximum HP +0xB0 is not 0xFFFF: w its data record's word
//   +0x24 (by +0xF0); when the maximum is above w / 10, the maximum less w /
//   10, +0xD0 less w / 10 (the division by -10 the original's), HP held to
//   the new maximum.
// Then, unless 0x44F6A0(the actor's dword, target) answers resisted, the HP
// delta the target's HP - 1.
extern "C" void __cdecl Effect_ActorStepThenHpToOne(void) {
    const char* const who = "Effect_ActorStepThenHpToOne (0x44DFD0)";
    const unsigned a = B(at::kActor);
    if (a <= 2) {
        unsigned char* const m = PartyWrite(a, who);
        unsigned char* const record = CharRecord(m[0x148]);
        if (record[0x1E] < 5) {
            record[0x1E] = static_cast<unsigned char>(record[0x1E] + 1);
            m[0x9E] = static_cast<unsigned char>(m[0x9E] + 1);
        }
        BH_CALL(Char_RecalcStats)(CharRecord(m[0x148]));
        RecalcParty(who);
    } else {
        unsigned char* const e = Enemy(a - 3);
        const unsigned max = Word(e + 0xB0);
        if (max != 0xFFFF) {
            const S32 tenth = static_cast<S32>(Word(At(at::kEnemyData + e[0xF0] * at::kEnemyDataStride + 0x24))) / 10;
            if (static_cast<S32>(max) > tenth) {
                unsigned char* const w = EnemyWrite(a - 3, who);
                SetWord(w + 0xB0, max - static_cast<unsigned>(tenth));
                const S32 down = static_cast<S32>(Word(At(at::kEnemyData + w[0xF0] * at::kEnemyDataStride + 0x24))) / -10;
                SetWord(w + 0xD0, Word(w + 0xD0) + static_cast<unsigned>(down));
                const std::uint16_t new_max = Word(w + 0xB0);
                if (Word(w + 0xA4) > new_max) SetWord(w + 0xA4, new_max);
            }
        }
    }
    const U actor = L(at::kActor);
    if (BH_AT(A2, at::kResisted)(actor, B(at::kTarget)) & 0xFF) return;
    const unsigned t = B(at::kTarget);
    SetWord(Result() + 4, SideWord(t, 0x98, 0xA4) - 1u);
}

// original 0x44E260 (slot 107): 0x44FB30; the actor's +0x130 (a member) /
// +0x110 (an enemy) |= 0x8000.
extern "C" void __cdecl Effect_ActorFlag8000(void) {
    const char* const who = "Effect_ActorFlag8000 (0x44E260)";
    MissTail();
    const unsigned a = B(at::kActor);
    unsigned char* const f = a <= 2 ? PartyWrite(a, who) + 0x130 : EnemyWrite(a - 3, who) + 0x110;
    SetLong(f, static_cast<S32>(static_cast<U>(Long(f)) | 0x8000));
}

// original 0x44E2B0 / 0x44E330 (slots 108, 109): 0x44FB30; the actor's +0x134
// / +0x114 |= 0x40 (0x80), and its count +0x144 / +0x124 (+0x145 / +0x125) up
// by one below 2.
static void ActorFlagCount(U flag, U count_at, const char* who) {
    MissTail();
    const unsigned a = B(at::kActor);
    unsigned char* const r = a <= 2 ? PartyWrite(a, who) : EnemyWrite(a - 3, who);
    const U flags_at = a <= 2 ? 0x134 : 0x114;
    const U n_at = a <= 2 ? count_at : count_at - 0x20;
    SetLong(r + flags_at, static_cast<S32>(static_cast<U>(Long(r + flags_at)) | flag));
    if (r[n_at] < 2) r[n_at] = static_cast<unsigned char>(r[n_at] + 1);
}
extern "C" void __cdecl Effect_ActorFlag40Count(void) { ActorFlagCount(0x40, 0x144, "Effect_ActorFlag40Count (0x44E2B0)"); }
extern "C" void __cdecl Effect_ActorFlag80Count(void) { ActorFlagCount(0x80, 0x145, "Effect_ActorFlag80Count (0x44E330)"); }

// original 0x44E3B0 (slot 110): 0x44FB30; the actor's +0x134 / +0x114 |= 0x100.
extern "C" void __cdecl Effect_ActorFlag100(void) {
    const char* const who = "Effect_ActorFlag100 (0x44E3B0)";
    MissTail();
    const unsigned a = B(at::kActor);
    unsigned char* const f = a <= 2 ? PartyWrite(a, who) + 0x134 : EnemyWrite(a - 3, who) + 0x114;
    SetLong(f, static_cast<S32>(static_cast<U>(Long(f)) | 0x100));
}

// original 0x44E400 (slot 111): 0x44FB30; the target's (a member's +0x138,
// +0x13C / an enemy's +0x118, +0x11C) dwords zeroed, +0x130 / +0x110 &=
// 0xFFFE7FFF, +0x134 / +0x114 &= 0xFFFBC43F; Battle_RecalcStats(the target's dword).
extern "C" void __cdecl Effect_TargetClearBuffs(void) {
    const char* const who = "Effect_TargetClearBuffs (0x44E400)";
    MissTail();
    const U target = L(at::kTarget);
    const unsigned t = target & 0xFF;
    unsigned char* const r = t <= 2 ? PartyWrite(t, who) : EnemyWrite(t - 3, who);
    const U base = t <= 2 ? 0x130 : 0x110;
    SetLong(r + base + 8, 0);
    SetLong(r + base + 0xC, 0);
    SetLong(r + base, static_cast<S32>(static_cast<U>(Long(r + base)) & 0xFFFE7FFFu));
    SetLong(r + base + 4, static_cast<S32>(static_cast<U>(Long(r + base + 4)) & 0xFFFBC43Fu));
    BH_CALL(Battle_RecalcStats)(target);
}

void Rest3C_Inject() {
    if (bof3::WantsShadow("rest_3c")) rest_3c::SelfTest();
    BOF3_INJECT(Effect_AsAbility60);
    BOF3_INJECT(Effect_AsAbility68);
    BOF3_INJECT(Effect_AsAbility5B);
    BOF3_INJECT(Effect_AsAbility52RaiseStat1);
    BOF3_INJECT(Effect_AsAbility5ARaiseStat1);
    BOF3_INJECT(Effect_AsAbility5C);
    BOF3_INJECT(Effect_AsAbility64);
    BOF3_INJECT(Effect_AsAbility57Inflict10);
    BOF3_INJECT(Effect_AsAbility61);
    BOF3_INJECT(Effect_AsAbility46);
    BOF3_INJECT(Effect_AsAbility67);
    BOF3_INJECT(Effect_AsAbility56);
    BOF3_INJECT(Effect_DoubleDamage);
    BOF3_INJECT(Effect_QuarterDamage);
    BOF3_INJECT(Effect_ActorHpDamage3);
    BOF3_INJECT(Effect_ActorHpDamage1);
    BOF3_INJECT(Effect_ActorHpDamage2);
    BOF3_INJECT(Effect_InflictStatus4);
    BOF3_INJECT(Effect_DamageInflict40);
    BOF3_INJECT(Effect_InflictStatus8);
    BOF3_INJECT(Effect_InflictStatus80);
    BOF3_INJECT(Effect_InflictStatus20);
    BOF3_INJECT(Effect_Heal5Ap1);
    BOF3_INJECT(Effect_Attack80);
    BOF3_INJECT(Effect_FlagActorPair);
    BOF3_INJECT(Effect_TargetRecalcParty);
    BOF3_INJECT(Effect_Inflict40Heal20Ap4);
    BOF3_INJECT(Effect_Heal1);
    BOF3_INJECT(Effect_Heal80);
    BOF3_INJECT(Effect_Heal240);
    BOF3_INJECT(Effect_RestoreAp5);
    BOF3_INJECT(Effect_RestoreAp40);
    BOF3_INJECT(Effect_AsAbility66Half);
    BOF3_INJECT(Effect_AsAbility63);
    BOF3_INJECT(Effect_CureBFCHealMax);
    BOF3_INJECT(Effect_Heal5);
    BOF3_INJECT(Effect_EnemyRaiseAAAE);
    BOF3_INJECT(Effect_AttackNoKill);
    BOF3_INJECT(Effect_HalveTargetHp);
    BOF3_INJECT(Effect_Inflict800Unguarded);
    BOF3_INJECT(Effect_DamageAllHp);
    BOF3_INJECT(Effect_RandomPowerAttack);
    BOF3_INJECT(Effect_TargetFlag2000);
    BOF3_INJECT(Effect_SureHitHalfDamage);
    BOF3_INJECT(Effect_EnemyDouble94And96);
    BOF3_INJECT(Effect_DropFromTurnOrder);
    BOF3_INJECT(Effect_FlushTurnOrder);
    BOF3_INJECT(Effect_RaiseStat4By10);
    BOF3_INJECT(Effect_ActorHpToDamage);
    BOF3_INJECT(Effect_DamageToOneHp);
    BOF3_INJECT(Effect_HealAndCureBFC);
    BOF3_INJECT(Effect_RestoreAp10);
    BOF3_INJECT(Effect_MultiHit);
    BOF3_INJECT(Effect_SkillDamageLessDef);
    BOF3_INJECT(Effect_HalfDamageInflict20);
    BOF3_INJECT(Effect_ActorStepThenHpToOne);
    BOF3_INJECT(Effect_ActorFlag8000);
    BOF3_INJECT(Effect_ActorFlag40Count);
    BOF3_INJECT(Effect_ActorFlag80Count);
    BOF3_INJECT(Effect_ActorFlag100);
    BOF3_INJECT(Effect_TargetClearBuffs);
}
