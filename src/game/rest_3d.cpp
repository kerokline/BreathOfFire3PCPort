// Round fourteen group R3D (wave three; docs/takeover-queue-round14.md,
// analysis/round14_cut.tsv's group column): the battle engine's band
// 0x44E4B0..0x44FF0E, each function read to its last instruction with capstone
// (2026-10-04) and taken through the boss harness's engine frame
// (boss_harness.h, docs/boss_harness.md section 10). docs/rest_3d.md has every
// function one row each.
//
//   - Effect_Handlers' slots 112..129 (0x64E73C + 4 * slot), void (void),
//     called by Effect_ApplyResult for the target 0x904B54 with the result
//     record *0x904B60;
//   - their helpers, which R3B's and R3C's slots call too: the no-hit mark,
//     the status rolls (one formula, four rate lookups), the inflict and its
//     two equipment tests, the stat-step byte and its rolled forms, the rolled
//     inflicts, the HP-based damage;
//   - the psi branch of Effect_SkillDamage and the weapon-status roll of
//     Battle_ApplyDamage;
//   - 0x44FF00, the Dragon command's part dispatcher (Battle_MenuSteps[7]).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatcher aborts past DragonCmd_Parts' seven entries and the HP damage on a
// divisor of 0 where the original jumps through whatever follows or faults
// (the owner's rule for an unchecked index, round9 doc section 6); the latent
// defects ours keeps are docs/rest_3d.md section 7. Every call goes through the
// harness (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in for
// the callees.
#include "game/rest_3d.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/rest_3d_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3d::at;
using U = std::uint32_t;
using S32 = std::int32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
std::uint16_t W(U address) { return Word(At(address)); }
U L(U address) { return static_cast<U>(Long(At(address))); }
S32 S16(U v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

// An actor's record as the originals index it: the low byte, 0..2 a party
// member (ObjTrio, 0x14C), else an enemy object at (actor - 3) * 0x128 - in
// 32 bits, unbounded either way (an index past the records reads or writes
// whatever lies there, on both sides).
bool IsMember(U actor) { return (actor & 0xFF) <= 2; }
U PartyAt(U actor) { return at::kParty + (actor & 0xFF) * at::kPartyStride; }
U EnemyAt(U actor) { return at::kEnemies + ((actor & 0xFF) - 3u) * at::kEnemyStride; }
unsigned char* Party(U actor) { return At(PartyAt(actor)); }
unsigned char* Enemy(U actor) { return At(EnemyAt(actor)); }

// The cells by side: HP (party +0x98, enemy +0xA4); the flag words +0x130 /
// +0x110 (0x10000: no damage; 0x200: no hit pose) and +0x134 / +0x114.
unsigned char* Hp(U actor) { return IsMember(actor) ? Party(actor) + 0x98 : Enemy(actor) + 0xA4; }
unsigned char* Flags(U actor) { return IsMember(actor) ? Party(actor) + 0x130 : Enemy(actor) + 0x110; }
unsigned char* Flags2(U actor) { return IsMember(actor) ? Party(actor) + 0x134 : Enemy(actor) + 0x114; }
void OrLong(unsigned char* p, U bits) { SetLong(p, static_cast<S32>(static_cast<U>(Long(p)) | bits)); }
void AndLong(unsigned char* p, U bits) { SetLong(p, static_cast<S32>(static_cast<U>(Long(p)) & bits)); }

unsigned char* Result() { return At(L(at::kResult)); }
unsigned Actor() { return B(at::kActor); }
unsigned Target() { return B(at::kTarget); }
U Ability() { return L(at::kAbility) & 0xFFFF; }
// NameTable_Abilities (24 bytes an ability): +3 the s8 stat step, +4 the word
// whose low nine bits are the status / element mask.
U AbilityMask() { return W(bof3::addr::NameTable_Abilities + 4 + Ability() * 24) & 0x1FF; }
unsigned char AbilityStep() { return B(bof3::addr::NameTable_Abilities + 3 + Ability() * 24); }

// The two terms of the status roll, read before the rate lookup:
// a = min(u16 0x939FEA * 100 / 500 + 50, 100) and
// d = max(125 - u16 0x939F8A * 100 / 500, 50) (signed 32-bit, as there).
S32 RollAttack() {
    const S32 a = static_cast<S32>(W(at::kAttackerInt)) * 100 / 500 + 50;
    return a > 100 ? 100 : a;
}
S32 RollDefence() {
    const S32 d = 125 - static_cast<S32>(W(at::kTargetInt)) * 100 / 500;
    return d < 50 ? 50 : d;
}

// The roll: the rate's ax (movsx) of -1 is "cannot be resisted" (al 0);
// otherwise Rand, and al 1 (resisted) when Rand % 100 is at least
// (rate * d * a, in 32 bits) / 10000, both signed.
unsigned char Roll(S32 a, S32 d, U rate_eax) {
    const S32 rate = S16(rate_eax);
    if (rate == -1) return 0;
    const S32 r = static_cast<S32>(BH_CALL(Rand)());
    const S32 p = static_cast<S32>(static_cast<U>(rate) * static_cast<U>(d) * static_cast<U>(a));
    return (r % 100) >= p / 10000 ? 1 : 0;
}

// The class lookups (0x44F030, 0x44F770): eax 0 to start; mask bit 0x40 the
// table's s16 by the actor's class byte +0xB5 (enemy +0xC5), 0x80 by +0xB6
// (+0xC6), 0x100 by +0xB7 (+0xC7), each later bit overriding.
S32 ClassLookup(U table, unsigned actor, unsigned mask) {
    const unsigned char* const c = IsMember(actor) ? Party(actor) + 0xB5 : Enemy(actor) + 0xC5;
    S32 v = 0;
    if (mask & 0x40) v = S16(W(table + 2u * c[0]));
    if (mask & 0x80) v = S16(W(table + 2u * c[1]));
    if (mask & 0x100) v = S16(W(table + 2u * c[2]));
    return v;
}

using Void0 = void (__cdecl*)();

}  // namespace

// ===========================================================================
// The status rolls and their rate lookups
// ===========================================================================

// original 0x44F030 (PSX 0x8009FD08): Effect_SkillDamage's affinity when its
// psi word's low byte is set - the s16 of 0x64E97C by the target's class byte
// for the last of the mask's bits 0x40 / 0x80 / 0x100 set (+0xB5..+0xB7 of a
// member, +0xC5..+0xC7 of an enemy), 0 when none is.
extern "C" int __cdecl Battle_PsiStatusDeathAffinity(unsigned target, unsigned mask) {
    return ClassLookup(at::kPsiRates, target, mask);
}

// original 0x44F770: the status rate - as Battle_PsiStatusDeathAffinity with
// the table 0x64E96C, and 0xFFFF (the rolls' "cannot be resisted") when the
// mask has none of 0x1C0.
extern "C" int __cdecl Battle_StatusResistRate(unsigned actor, unsigned mask) {
    const S32 v = ClassLookup(at::kResistRates, actor, mask);
    return (mask & 0x1C0) == 0 ? 0xFFFF : v;
}

// original 0x44FA10: the other rate - the s16 of 0x64E98C by the class byte
// +0xB4 (enemy +0xC4) when the mask's byte has 0x20, else 0 (so its roll is
// never "cannot be resisted").
extern "C" int __cdecl Battle_StatusResistRate20(unsigned actor, unsigned mask) {
    if ((mask & 0x20) == 0) return 0;
    const unsigned char c = IsMember(actor) ? Party(actor)[0xB4] : Enemy(actor)[0xC4];
    return S16(W(at::kResistRates20 + 2u * c));
}

// original 0x44F6A0: the status roll for the acting ability: Roll with the
// rate Battle_StatusResistRate(target, the ability's mask). `actor` is not
// read. al 1: resisted.
extern "C" unsigned char __cdecl Battle_StatusResisted(unsigned actor, unsigned target) {
    (void)actor;
    const S32 a = RollAttack();
    const S32 d = RollDefence();
    const U rate = static_cast<U>(BH_CALL(Battle_StatusResistRate)(target, AbilityMask()));
    return Roll(a, d, rate);
}

// original 0x44F880: the same roll with the caller's mask (its low nine bits).
extern "C" unsigned char __cdecl Battle_StatusResistedMask(unsigned actor, unsigned target, unsigned mask) {
    (void)actor;
    const S32 a = RollAttack();
    const S32 d = RollDefence();
    const U rate = static_cast<U>(BH_CALL(Battle_StatusResistRate)(target, mask & 0x1FF));
    return Roll(a, d, rate);
}

// original 0x44F940: the roll on Battle_StatusResistRate20 (the ability's mask).
extern "C" unsigned char __cdecl Battle_StatusResisted20(unsigned actor, unsigned target) {
    (void)actor;
    const S32 a = RollAttack();
    const S32 d = RollDefence();
    const U rate = static_cast<U>(BH_CALL(Battle_StatusResistRate20)(target, AbilityMask()));
    return Roll(a, d, rate);
}

// original 0x44FA70 (Battle_ApplyDamage's weapon-status test): the roll with
// the mask 0x80.
extern "C" unsigned char __cdecl Battle_StatusResisted80(unsigned attacker, unsigned target) {
    (void)attacker;
    const S32 a = RollAttack();
    const S32 d = RollDefence();
    const U rate = static_cast<U>(BH_CALL(Battle_StatusResistRate)(target, 0x80));
    return Roll(a, d, rate);
}

// ===========================================================================
// The inflict and its equipment tests
// ===========================================================================

// original 0x44F460: al 0 when `actor` is a party member (its byte at most 2)
// and the byte `item` is one of Field_State's +0x96 / +0x97 (the two
// accessory bytes a member's record copies from its character record +0x16 /
// +0x17); else 1. Field_State is read, not the actor's own record.
extern "C" unsigned char __cdecl Battle_LacksAccessory(unsigned actor, unsigned item) {
    if ((actor & 0xFF) > 2) return 1;
    const unsigned char* const fs = Field_State;
    const auto b = static_cast<unsigned char>(item);
    if (fs[0x96] == b) return 0;
    if (fs[0x97] == b) return 0;
    return 1;
}

// original 0x44F490: the same on Field_State's +0x94 (the character record's
// +0x14, an armour byte).
extern "C" unsigned char __cdecl Battle_LacksArmour(unsigned actor, unsigned item) {
    if ((actor & 0xFF) > 2) return 1;
    const unsigned char* const fs = Field_State;
    return fs[0x94] == static_cast<unsigned char>(item) ? 0 : 1;
}

// original 0x44F1D0 (PSX 0x800A0170 by the sibling's EnemyAI evidence, not
// paired): the status bits of `status` added to `target`. The target's status
// word (+0x90 a member, +0x92 an enemy) is read once, before anything (the
// original keeps it in its argument slot): call it cur. With s the status's
// byte, each new bit is ORed into 0x904B98 (0x904B99 for 0x800):
//   0x80, 1, 2: when cur lacks it;
//   0x40: Battle_LacksAccessory(target, 6), cur without 0x40 or 0x800 -
//         Battle_ClearStatus(target, 0x38), the bit, Battle_ReturnQueuedItem,
//         Battle_RemoveFromTurnOrder;
//   0x20: Battle_LacksAccessory(target, 8), cur without 0x64 or 0x800 - the
//         bit, Battle_ReturnQueuedItem, the actor's byte 0x93A009 + 0x84 n
//         zeroed, a member's +0x125 = 0, +0x130 bit 1 and +0x134 bit 2
//         cleared (an enemy's +0x105 and +0x110 bit 1);
//   8:    Battle_LacksAccessory(target, 9) and Battle_LacksArmour(target,
//         0x2B), cur without 0xC or 0x800;
//   0x10: cur without 0x14 or 0x800;
//   4:    cur without 4 or 0x800 - Battle_ClearStatus(target, 0x38), the bit,
//         the item, the turn order;
//   0x800 (the status's word): cur without it - Battle_ClearStatus(target,
//         0x7E), 0x904B99 bit 3, the item, the turn order.
// Then Sprite_ReleaseTint(the record), the status word |= 0x904B98 (both read
// after the call), and Battle_StatusTint(the word read again, with the upper
// half of the record's offset above it, as the original's register holds it).
// `target` is handed on whole, as the original pushes it.
extern "C" void __cdecl Battle_InflictStatus(unsigned target, unsigned status) {
    const bool member = IsMember(target);
    const U offset = member ? (target & 0xFF) * at::kPartyStride : ((target & 0xFF) - 3u) * at::kEnemyStride;
    const U record = (member ? at::kParty : at::kEnemies) + offset;
    const U word_at = record + (member ? 0x90u : 0x92u);
    const U cur = W(word_at);
    const U s = status & 0xFF;
    auto mark = [](unsigned char bit) { B(at::kNewStatus) = static_cast<unsigned char>(B(at::kNewStatus) | bit); };
    if ((s & 0x80) && !(cur & 0x80)) mark(0x80);
    if ((s & 1) && !(cur & 1)) mark(1);
    if ((s & 2) && !(cur & 2)) mark(2);
    if (s & 0x40) {
        const unsigned char lacks = BH_CALL(Battle_LacksAccessory)(target, 6);
        if (lacks && !(cur & 0x40) && !(cur & 0x800)) {
            BH_CALL(Battle_ClearStatus)(target, 0x38);
            mark(0x40);
            BH_CALL(Battle_ReturnQueuedItem)(target);
            BH_CALL(Battle_RemoveFromTurnOrder)(target);
        }
    }
    if (s & 0x20) {
        const unsigned char lacks = BH_CALL(Battle_LacksAccessory)(target, 8);
        if (lacks && !(cur & 0x64) && !(cur & 0x800)) {
            mark(0x20);
            BH_CALL(Battle_ReturnQueuedItem)(target);
            B(at::kActorTaskByte + (target & 0xFF) * at::kActorTaskStride) = 0;
            if (member) {
                unsigned char* const p = Party(target);
                p[0x125] = 0;
                AndLong(p + 0x130, 0xFFFFFFFDu);
                AndLong(p + 0x134, 0xFFFFFFFBu);
            } else {
                unsigned char* const e = Enemy(target);
                e[0x105] = 0;
                AndLong(e + 0x110, 0xFFFFFFFDu);
            }
        }
    }
    if (s & 8) {
        if (BH_CALL(Battle_LacksAccessory)(target, 9)) {
            const unsigned char lacks = BH_CALL(Battle_LacksArmour)(target, 0x2B);
            if (lacks && !(cur & 0xC) && !(cur & 0x800)) mark(8);
        }
    }
    if ((s & 0x10) && !(cur & 0x14) && !(cur & 0x800)) mark(0x10);
    if ((s & 4) && !(cur & 4) && !(cur & 0x800)) {
        BH_CALL(Battle_ClearStatus)(target, 0x38);
        mark(4);
        BH_CALL(Battle_ReturnQueuedItem)(target);
        BH_CALL(Battle_RemoveFromTurnOrder)(target);
    }
    if ((status & 0x800) && !(cur & 0x800)) {
        BH_CALL(Battle_ClearStatus)(target, 0x7E);
        B(at::kNewStatusHi) = static_cast<unsigned char>(B(at::kNewStatusHi) | 8);
        BH_CALL(Battle_ReturnQueuedItem)(target);
        BH_CALL(Battle_RemoveFromTurnOrder)(target);
    }
    BH_CALL(Sprite_ReleaseTint)(At(record));
    SetWord(At(word_at), W(word_at) | W(at::kNewStatus));
    BH_CALL(Battle_StatusTint)((offset & 0xFFFF0000u) | W(word_at));
}

// ===========================================================================
// The shared tails of the effect slots
// ===========================================================================

// original 0x44FB30 (23 sites in R3B and R3C, ten here, Effect_QuarterAttack's
// tail): the round flags' 0x2000 (0x904AA9 bit 5: no hit sound or pop-up), and
// the target's +0x130 bit 9 set (no hit pose) and +0x12C bit 0 cleared (the
// damage pop-up's flag; an enemy's +0x110 and +0x10C).
extern "C" void __cdecl Effect_NoHitReaction(void) {
    const unsigned t = Target();
    B(at::kRoundFlagsHi) = static_cast<unsigned char>(B(at::kRoundFlagsHi) | 0x20);
    if (IsMember(t)) {
        unsigned char* const p = Party(t);
        OrLong(p + 0x130, 0x200);
        p[0x12C] = static_cast<unsigned char>(p[0x12C] & 0xFE);
    } else {
        unsigned char* const e = Enemy(t);
        OrLong(e + 0x110, 0x200);
        e[0x10C] = static_cast<unsigned char>(e[0x10C] & 0xFE);
    }
}

// original 0x44F650: the result record's s8 stat step +0x14 + (stat & 0xFF)
// moved by the signed word `step`: above 50 made 50, below -25 made -25, else
// the byte plus the step's byte.
extern "C" void __cdecl Effect_StepStatByte(unsigned step, unsigned stat) {
    unsigned char* const p = At(L(at::kResult) + (stat & 0xFF) + 0x14);
    const S32 v = static_cast<signed char>(*p) + S16(step);
    if (v > 50) *p = 50;
    else if (v < -25) *p = 0xE7;
    else *p = static_cast<unsigned char>(*p + step);
}

namespace {

// 0x44FBB0 / 0x44FC10's shared body after the mark: the roll; resisted al 1;
// else the ability's s8 step (+3) through Effect_StepStatByte(step, stat),
// Battle_RecalcStats(the target, with the stat word's upper bytes above it as
// the original's register holds it), al 0.
unsigned char RollStatStep(unsigned stat) {
    if (BH_CALL(Battle_StatusResisted)(Actor(), Target())) return 1;
    const auto step = static_cast<signed char>(AbilityStep());
    BH_CALL(Effect_StepStatByte)(static_cast<U>(static_cast<S32>(step)), stat);
    BH_CALL(Battle_RecalcStats)((stat & 0xFFFFFF00u) | Target());
    return 0;
}

// 0x44FC60 / 0x44FCA0's: the roll; resisted al 1; else
// Battle_InflictStatus(the target, status), al 0.
unsigned char RollInflict(unsigned status) {
    if (BH_CALL(Battle_StatusResisted)(Actor(), Target())) return 1;
    BH_CALL(Battle_InflictStatus)(Target(), status);
    return 0;
}

}  // namespace

// original 0x44FBB0 (seven sites in R3B and R3C): Effect_NoHitReaction, then
// RollStatStep.
extern "C" unsigned char __cdecl Effect_RollStatStepQuiet(unsigned stat) {
    BH_CALL(Effect_NoHitReaction)();
    return RollStatStep(stat);
}

// original 0x44FC10 (MagicFx_ApplyBuff's roll): RollStatStep without the mark.
extern "C" unsigned char __cdecl Effect_RollStatStep(unsigned stat) { return RollStatStep(stat); }

// original 0x44FC60 (seven sites in R3B and R3C): Effect_NoHitReaction, then
// RollInflict.
extern "C" unsigned char __cdecl Effect_RollInflictQuiet(unsigned status) {
    BH_CALL(Effect_NoHitReaction)();
    return RollInflict(status);
}

// original 0x44FCA0 (twelve sites, R3B, R3C and slots 122 and 128):
// RollInflict without the mark.
extern "C" unsigned char __cdecl Effect_RollInflict(unsigned status) { return RollInflict(status); }

// original 0x44FCE0: an HP-based amount. The actor's (0x904B34) HP / divisor
// (signed, the whole word), times the s16 0x64E9AC[Rand & 7], times the
// affinity (100 when the ability's mask has none of 0x1F, else
// Battle_ElementAffinity(the target, the mask)'s ax), / 10000 signed; ax
// zeroed (the upper half kept) when the target (read again) has flag 0x10000.
// A divisor of 0 faults in the original (idiv): ours aborts. Its callers pass
// 1 and 2.
extern "C" int __cdecl Effect_HpBasedDamage(int divisor) {
    const U hp = Word(Hp(Actor()));
    const U mask = AbilityMask();
    S32 affinity = 100;
    if (mask & 0x1F) affinity = S16(static_cast<U>(BH_CALL(Battle_ElementAffinity)(Target(), mask)));
    const U roll = static_cast<U>(BH_CALL(Rand)()) & 7;
    const S32 variance = S16(W(at::kHpVariance + 2 * roll));
    if (divisor == 0)
        bof3::Fatal("Effect_HpBasedDamage: a divisor of 0 - the original's idiv faults (docs/rest_3d.md section 7)");
    const S32 quotient = static_cast<S32>(hp) / divisor;
    const S32 product = static_cast<S32>(static_cast<U>(variance) * static_cast<U>(quotient) * static_cast<U>(affinity));
    S32 v = product / 10000;
    if (static_cast<U>(Long(Flags(Target()))) & 0x10000) v = static_cast<S32>(static_cast<U>(v) & 0xFFFF0000u);
    return v;
}

// ===========================================================================
// Effect_Handlers' slots 112..129
// ===========================================================================

namespace {

// Slots 116 and 124: Effect_HpBasedDamage(divisor) less the target's DEF
// (+0xA6 a member, +0xB6 an enemy; the side by the target read before the
// call, the record by the one read after), the HP delta, at least 0.
void HpLessDefence(int divisor) {
    const bool member = IsMember(Target());
    const U v = static_cast<U>(BH_CALL(Effect_HpBasedDamage)(divisor));
    const unsigned t = Target();
    const U def = member ? Word(Party(t) + 0xA6) : Word(Enemy(t) + 0xB6);
    SetWord(Result() + 4, v - def);
    if (S16(Word(Result() + 4)) < 0) SetWord(Result() + 4, 0);
}

}  // namespace

// original 0x44E4B0 (slot 112): on an odd Rand, the target not resisting
// (Battle_StatusResisted): the HP delta its HP less 1.
extern "C" void __cdecl Effect112_HpToOne(void) {
    if ((BH_CALL(Rand)() & 1) == 0) return;
    if (BH_CALL(Battle_StatusResisted)(Actor(), Target())) return;
    SetWord(Result() + 4, Word(Hp(Target())) - 1u);
}

// original 0x44E530 (slot 113): Effect_NoHitReaction, then the actor's flag
// 0x10000 (the flag Battle_CalcDamage and Effect_SkillDamage answer 0 for).
extern "C" void __cdecl Effect113_ActorNullDamage(void) {
    BH_CALL(Effect_NoHitReaction)();
    OrLong(Flags(Actor()), 0x10000);
}

// original 0x44E580 (slot 114): the result's mark +8 = 2, then the AP delta
// Effect_SkillDamage(actor, target, the ability's +3 byte, 1) (the psi
// branch), its ax into the result read again.
extern "C" void __cdecl Effect114_SkillApDamage(void) {
    Result()[8] = 2;
    const U power = AbilityStep();
    const int v = BH_CALL(Effect_SkillDamage)(Actor(), Target(), power, 1);
    SetWord(Result() + 6, static_cast<U>(v));
}

// original 0x44E5D0 (slot 115): the target not resisting Battle_StatusResisted20:
// the HP delta its whole HP.
extern "C" void __cdecl Effect115_HpToZero(void) {
    if (BH_CALL(Battle_StatusResisted20)(Actor(), Target())) return;
    SetWord(Result() + 4, Word(Hp(Target())));
}

// original 0x44E640 (slot 116): HpLessDefence(1).
extern "C" void __cdecl Effect116_HpLessDefence(void) { HpLessDefence(1); }

// original 0x44E6C0 (slot 117): Effect_NoHitReaction; for a member target,
// every member below the party size 0x904AB0 gets +0x134 bit 10, then the
// target loses it. An enemy target: nothing more.
extern "C" void __cdecl Effect117_PartyFlag400Others(void) {
    BH_CALL(Effect_NoHitReaction)();
    const unsigned t = Target();
    if (!IsMember(t)) return;
    const unsigned n = B(at::kPartySize);
    for (unsigned m = 0; m < n; ++m) OrLong(Party(m) + 0x134, 0x400);
    AndLong(Party(t) + 0x134, 0xFFFFFBFFu);
}

// original 0x44E720 (slot 118): a member target only. Its character (+0x148)
// record's byte +0x1E below 9: that byte and the member's +0x9E one up. Then
// Char_RecalcStats(the character record), and for every member below the
// party size: +0x92..+0x97 from its character record's +0x12..+0x17 and
// +0xC0..+0xDF from its +0x20..+0x3F; Formation_ApplyStatMods; every member's
// +0xC0..+0xDF back to +0xA0..+0xBF; BattleForm_ApplyStats;
// Battle_RecalcStats(m) for each member (the size read again after each
// call); the HP delta minus the target's (read again) max HP +0xA0.
extern "C" void __cdecl Effect118_RaiseCharByte1E(void) {
    const unsigned t = Target();
    if (!IsMember(t)) return;
    {
        unsigned char* const p = Party(t);
        unsigned char* const c = At(bof3::addr::CharacterRecords + p[0x148] * at::kCharacterStride);
        if (c[0x1E] < 9) {
            c[0x1E] = static_cast<unsigned char>(c[0x1E] + 1);
            p[0x9E] = static_cast<unsigned char>(p[0x9E] + 1);
        }
        BH_CALL(Char_RecalcStats)(At(bof3::addr::CharacterRecords + p[0x148] * at::kCharacterStride));
    }
    unsigned n = B(at::kPartySize);
    for (unsigned m = 0; m < n; ++m) {
        unsigned char* const p = Party(m);
        const unsigned char* const c = At(bof3::addr::CharacterRecords + p[0x148] * at::kCharacterStride);
        for (unsigned i = 0; i < 6; ++i) p[0x92 + i] = c[0x12 + i];
        for (unsigned i = 0; i < 0x20; ++i) p[0xC0 + i] = c[0x20 + i];
    }
    BH_CALL(Formation_ApplyStatMods)();
    n = B(at::kPartySize);
    for (unsigned m = 0; m < n; ++m) {
        unsigned char* const p = Party(m);
        for (unsigned i = 0; i < 0x20; ++i) p[0xA0 + i] = p[0xC0 + i];
    }
    BH_CALL(BattleForm_ApplyStats)();
    if (B(at::kPartySize) != 0) {
        unsigned m = 0;
        do {
            BH_CALL(Battle_RecalcStats)(m);
            m = (m + 1) & 0xFF;
        } while (m < B(at::kPartySize));
    }
    SetWord(Result() + 4, 0u - Word(Party(Target()) + 0xA0));
}

// original 0x44E8B0 (slot 119): Effect_NoHitReaction; the actor's +0x134 (an
// enemy's +0x114) bit 12; Battle_RecalcStats(the target).
extern "C" void __cdecl Effect119_ActorFlag1000(void) {
    BH_CALL(Effect_NoHitReaction)();
    const unsigned t = Target();
    OrLong(Flags2(Actor()), 0x1000);
    BH_CALL(Battle_RecalcStats)(t);
}

// original 0x44E920 (slot 120): Battle_StatusResisted: the mark +8 = 1 and
// Battle_SetDamagePopup(0, the target); else the target's +0x134 (+0x114)
// bit 11 and Battle_RecalcStats(the target's dword). Either way a tail jump
// to Effect_NoHitReaction.
extern "C" void __cdecl Effect120_TargetFlag800(void) {
    if (BH_CALL(Battle_StatusResisted)(Actor(), Target())) {
        Result()[8] = 1;
        BH_CALL(Battle_SetDamagePopup)(0, Target());
    } else {
        const U t = L(at::kTarget);
        OrLong(Flags2(t), 0x800);
        BH_CALL(Battle_RecalcStats)(t);
    }
    BH_CALL(Effect_NoHitReaction)();
}

// original 0x44E9C0 (slot 121): Effect_NoHitReaction; the actor's +0x134
// (+0x114) bit 14 and its byte +0x142 (+0x122) zeroed; Battle_RecalcStats(the
// target).
extern "C" void __cdecl Effect121_ActorFlag4000(void) {
    BH_CALL(Effect_NoHitReaction)();
    const unsigned a = Actor();
    const unsigned t = Target();
    OrLong(Flags2(a), 0x4000);
    if (IsMember(a)) Party(a)[0x142] = 0;
    else Enemy(a)[0x122] = 0;
    BH_CALL(Battle_RecalcStats)(t);
}

// original 0x44EA40 (slot 122): Effect_NoHitReaction, then Effect_RollInflict
// with 0x20, 0x80 and 8 in turn (each its own roll).
extern "C" void __cdecl Effect122_InflictThree(void) {
    BH_CALL(Effect_NoHitReaction)();
    BH_CALL(Effect_RollInflict)(0x20);
    BH_CALL(Effect_RollInflict)(0x80);
    BH_CALL(Effect_RollInflict)(8);
}

// original 0x44EA70 (slot 123): the acting kind 4 and the ability 0x6A, then a
// tail jump to R3C's 0x44D8B0.
extern "C" void __cdecl Effect123_Ability6A(void) {
    B(at::kActingKind) = 4;
    SetWord(At(at::kAbility), 0x6A);
    BH_AT(Void0, at::kAbility6AHelper)();
}

// original 0x44EA90 (slot 124): HpLessDefence(2).
extern "C" void __cdecl Effect124_HalfHpLessDefence(void) { HpLessDefence(2); }

// original 0x44EB10 (slot 125): Effect_NoHitReaction; the target not resisting
// Battle_StatusResistedMask(actor, target, 0x80): Battle_InflictStatus(the
// target, 8).
extern "C" void __cdecl Effect125_Inflict8Roll80(void) {
    BH_CALL(Effect_NoHitReaction)();
    if (BH_CALL(Battle_StatusResistedMask)(Actor(), Target(), 0x80)) return;
    BH_CALL(Battle_InflictStatus)(Target(), 8);
}

// original 0x44EB50 (slot 126; group BE5's doc called it slot 124): the acting
// kind 4 and the ability 0x4E, then a tail jump to Effect_DrainAp.
extern "C" void __cdecl Effect126_Ability4EDrainAp(void) {
    B(at::kActingKind) = 4;
    SetWord(At(at::kAbility), 0x4E);
    BH_CALL(Effect_DrainAp)();
}

// original 0x44EB70 (slot 127): the HP delta Battle_CalcDamage(actor, target,
// 0xFFFF) (the target read before the call); an enemy target (that read) whose
// object's +0x8D is 4 (the target read again) doubled (a 16-bit shift).
extern "C" void __cdecl Effect127_AttackDoubledKind4(void) {
    const unsigned t = Target();
    const short v = BH_CALL(Battle_CalcDamage)(Actor(), t, 0xFFFF);
    SetWord(Result() + 4, static_cast<std::uint16_t>(v));
    if (IsMember(t)) return;
    if (Enemy(Target())[0x8D] == 4) SetWord(Result() + 4, static_cast<U>(Word(Result() + 4)) << 1);
}

// original 0x44EBE0 (slot 128): the HP delta Battle_CalcDamage(actor, target,
// 0xFFFF) / 2 (signed, toward 0); when it is not 0 and Battle_StatusResisted
// says not resisted, Effect_RollInflict(4) (a second roll).
extern "C" void __cdecl Effect128_HalfAttackInflict4(void) {
    const short v = BH_CALL(Battle_CalcDamage)(Actor(), Target(), 0xFFFF);
    SetWord(Result() + 4, static_cast<U>(static_cast<S32>(v) / 2));
    if (Word(Result() + 4) == 0) return;
    if (BH_CALL(Battle_StatusResisted)(Actor(), Target())) return;
    BH_CALL(Effect_RollInflict)(4);
}

// original 0x44EC40 (slot 129): Effect_NoHitReaction; for each actor 0..2 not
// out (Battle_ActorIsOut), the TARGET's member record +0x134 bit 18 cleared,
// and for each actor 3..10 not out the target's enemy record +0x114 bit 18
// cleared (the target read again after each test, indexed as a member and as an
// enemy whatever it is: docs/rest_3d.md section 7); then the target (the last read)
// gets the bit.
extern "C" void __cdecl Effect129_TargetSoleFlag40000(void) {
    BH_CALL(Effect_NoHitReaction)();
    for (unsigned m = 0; m <= 2; ++m) {
        if (BH_CALL(Battle_ActorIsOut)(m)) continue;
        AndLong(Party(Target()) + 0x134, 0xFFFBFFFFu);
    }
    unsigned t = 0;
    for (unsigned m = 3; m <= 10; ++m) {
        const unsigned char out = BH_CALL(Battle_ActorIsOut)(m);
        t = Target();
        if (!out) AndLong(Enemy(t) + 0x114, 0xFFFBFFFFu);
    }
    OrLong(Flags2(t), 0x40000);
}

// ===========================================================================
// The Dragon command's part dispatcher
// ===========================================================================

// original 0x44FF00 (Battle_MenuSteps[7], BattleMenu_ConfirmDispatch's entry
// for the Dragon command): jmp [DragonCmd_Parts + 4 * byte 0x904AA3]. Seven
// parts (BE5's dispatchers and DragonCmd_Open); a part past them is a Fatal
// where the original jumps through what follows (DragonCmd_LoadSteps' cells).
// The jmp leaves the caller's word and the part's eax in place: ours hands
// the word on and answers the part's eax.
extern "C" unsigned long __cdecl DragonCmd_PartDispatch(unsigned long through) {
    const unsigned part = B(at::kDragonPart);
    if (part >= DragonCmd_Parts_count)
        bof3::Fatal("DragonCmd_PartDispatch: 0x904AA3 is %u, past the %u parts of DragonCmd_Parts - the original jumps "
                    "through the dword after (docs/rest_3d.md section 7)",
                    part, DragonCmd_Parts_count);
    using Part = unsigned long (__cdecl*)(unsigned long);
    const auto entry = reinterpret_cast<Part>(static_cast<std::uintptr_t>(DragonCmd_Parts[part]));
    return entry(through);   // as read: the fuzz swaps the table's cells for its recorders
}

// ============================================================================

void Rest3D_Inject() {
    if (bof3::WantsShadow("rest_3d")) rest_3d::SelfTest();
    BOF3_INJECT(Effect112_HpToOne);
    BOF3_INJECT(Effect113_ActorNullDamage);
    BOF3_INJECT(Effect114_SkillApDamage);
    BOF3_INJECT(Effect115_HpToZero);
    BOF3_INJECT(Effect116_HpLessDefence);
    BOF3_INJECT(Effect117_PartyFlag400Others);
    BOF3_INJECT(Effect118_RaiseCharByte1E);
    BOF3_INJECT(Effect119_ActorFlag1000);
    BOF3_INJECT(Effect120_TargetFlag800);
    BOF3_INJECT(Effect121_ActorFlag4000);
    BOF3_INJECT(Effect122_InflictThree);
    BOF3_INJECT(Effect123_Ability6A);
    BOF3_INJECT(Effect124_HalfHpLessDefence);
    BOF3_INJECT(Effect125_Inflict8Roll80);
    BOF3_INJECT(Effect126_Ability4EDrainAp);
    BOF3_INJECT(Effect127_AttackDoubledKind4);
    BOF3_INJECT(Effect128_HalfAttackInflict4);
    BOF3_INJECT(Effect129_TargetSoleFlag40000);
    BOF3_INJECT(Battle_PsiStatusDeathAffinity);
    BOF3_INJECT(Battle_InflictStatus);
    BOF3_INJECT(Battle_LacksAccessory);
    BOF3_INJECT(Battle_LacksArmour);
    BOF3_INJECT(Effect_StepStatByte);
    BOF3_INJECT(Battle_StatusResisted);
    BOF3_INJECT(Battle_StatusResistRate);
    BOF3_INJECT(Battle_StatusResistedMask);
    BOF3_INJECT(Battle_StatusResisted20);
    BOF3_INJECT(Battle_StatusResistRate20);
    BOF3_INJECT(Battle_StatusResisted80);
    BOF3_INJECT(Effect_NoHitReaction);
    BOF3_INJECT(Effect_RollStatStepQuiet);
    BOF3_INJECT(Effect_RollStatStep);
    BOF3_INJECT(Effect_RollInflictQuiet);
    BOF3_INJECT(Effect_RollInflict);
    BOF3_INJECT(Effect_HpBasedDamage);
    BOF3_INJECT(DragonCmd_PartDispatch);
}
