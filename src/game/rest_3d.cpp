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
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3d_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3d::at;
using U = std::uint32_t;
using S32Rest3d = std::int32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
std::uint16_t W(U address) { return move_script::Word(move_script::At(address)); }
U L(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }
S32Rest3d S16Rest3d(U v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

// An actor's record as the originals index it: the low byte, 0..2 a party
// member (ObjTrio, 0x14C), else an enemy object at (actor - 3) * 0x128 - in
// 32 bits, unbounded either way (an index past the records reads or writes
// whatever lies there, on both sides).
bool IsMember(U actor) { return (actor & 0xFF) <= 2; }
U PartyAt(U actor) { return ::bof3::Img(ObjTrio_addr) + (actor & 0xFF) * rest_3d::at::kPartyStride; }
U EnemyAt(U actor) { return ::bof3::Img(Battle_Enemies_addr) + ((actor & 0xFF) - 3u) * rest_3d::at::kEnemyStride; }
unsigned char* PartyRest3d(U actor) { return move_script::At(PartyAt(actor)); }
unsigned char* EnemyRest3d(U actor) { return move_script::At(EnemyAt(actor)); }

// The cells by side: HP (party +0x98, enemy +0xA4); the flag words +0x130 /
// +0x110 (0x10000: no damage; 0x200: no hit pose) and +0x134 / +0x114.
unsigned char* Hp(U actor) { return IsMember(actor) ? PartyRest3d(actor) + 0x98 : EnemyRest3d(actor) + 0xA4; }
unsigned char* Flags(U actor) { return IsMember(actor) ? PartyRest3d(actor) + 0x130 : EnemyRest3d(actor) + 0x110; }
void OrLongRest3d(unsigned char* p, U bits) { move_script::SetLong(p, static_cast<S32Rest3d>(static_cast<U>(move_script::Long(p)) | bits)); }

unsigned ActorRest3d() { return B(::bof3::Img(rest_3d::at::kActor.v)); }
unsigned Target() { return B(::bof3::Img(rest_3d::at::kTarget.v)); }
U Ability() { return L(::bof3::Img(rest_3d::at::kAbility.v)) & 0xFFFF; }
// NameTable_Abilities (24 bytes an ability): +3 the s8 stat step, +4 the word
// whose low nine bits are the status / element mask.
U AbilityMask() { return W(::bof3::Img(bof3::addr::NameTable_Abilities) + 4 + Ability() * 24) & 0x1FF; }
unsigned char AbilityStep() { return B(::bof3::Img(bof3::addr::NameTable_Abilities) + 3 + Ability() * 24); }

// The two terms of the status roll, read before the rate lookup:
// a = min(u16 0x939FEA * 100 / 500 + 50, 100) and
// d = max(125 - u16 0x939F8A * 100 / 500, 50) (signed 32-bit, as there).
S32Rest3d RollAttack() {
    const S32Rest3d a = BattleDamage_AttackerInt * 100 / 500 + 50;
    return a > 100 ? 100 : a;
}
S32Rest3d RollDefence() {
    const S32Rest3d d = 125 - BattleDamage_TargetInt * 100 / 500;
    return d < 50 ? 50 : d;
}

// The roll: the rate's ax (movsx) of -1 is "cannot be resisted" (al 0);
// otherwise Rand, and al 1 (resisted) when Rand % 100 is at least
// (rate * d * a, in 32 bits) / 10000, both signed.
unsigned char Roll(S32Rest3d a, S32Rest3d d, U rate_eax) {
    const S32Rest3d rate = S16Rest3d(rate_eax);
    if (rate == -1) return 0;
    const S32Rest3d r = static_cast<S32Rest3d>(BH_CALL(Rand)());
    const S32Rest3d p = static_cast<S32Rest3d>(static_cast<U>(rate) * static_cast<U>(d) * static_cast<U>(a));
    return (r % 100) >= p / 10000 ? 1 : 0;
}

// The class lookups (0x44F030, 0x44F770): eax 0 to start; mask bit 0x40 the
// table's s16 by the actor's class byte +0xB5 (enemy +0xC5), 0x80 by +0xB6
// (+0xC6), 0x100 by +0xB7 (+0xC7), each later bit overriding.
S32Rest3d ClassLookup(U table, unsigned actor, unsigned mask) {
    const unsigned char* const c = IsMember(actor) ? PartyRest3d(actor) + 0xB5 : EnemyRest3d(actor) + 0xC5;
    S32Rest3d v = 0;
    if (mask & 0x40) v = S16Rest3d(W(table + 2u * c[0]));
    if (mask & 0x80) v = S16Rest3d(W(table + 2u * c[1]));
    if (mask & 0x100) v = S16Rest3d(W(table + 2u * c[2]));
    return v;
}

}  // namespace

// ===========================================================================
// The status rolls and their rate lookups
// ===========================================================================

// ===========================================================================
// The inflict and its equipment tests
// ===========================================================================

// ===========================================================================
// The shared tails of the effect slots
// ===========================================================================

// ===========================================================================
// Effect_Handlers' slots 112..129
// ===========================================================================

// ===========================================================================
// The Dragon command's part dispatcher
// ===========================================================================

// ============================================================================

void Rest3D_Inject() {
    if (bof3::WantsShadow("rest_3d")) rest_3d::SelfTest();
}
