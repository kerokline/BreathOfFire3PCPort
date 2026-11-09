// Moved here by tools/move_funcs.py from battle_damage.cpp, battle_e5.cpp, battle_misc.cpp, battle_odds.cpp, battle_setup.cpp, rest_3b.cpp, rest_3c.cpp, rest_3d.cpp (docs/source-layout-round.md).
#include "game/effect/effect.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/battle_damage.h"
#include "game/battle_damage_callees.h"
#include "game/battle_e5.h"
#include "game/battle_e5_callees.h"
#include "game/battle_misc.h"
#include "game/battle_misc_callees.h"
#include "game/battle_odds.h"
#include "game/battle_odds_callees.h"
#include "game/battle_setup.h"
#include "game/battle_setup_callees.h"
#include "game/battle_text.h"
#include "game/boss_harness.h"
#include "game/effect/effect_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3b.h"
#include "game/rest_3b_callees.h"
#include "game/rest_3c.h"
#include "game/rest_3c_callees.h"
#include "game/rest_3d.h"
#include "game/rest_3d_callees.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect::at;

}  // namespace

using namespace battle_damage;

namespace {

using namespace battle_misc;

using namespace battle_odds;

}  // namespace

using namespace battle_setup;

namespace {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

// An actor byte: 0..2 the party, 3 and up the enemies. Both address the
// working record by the byte alone, as the original does (`and eax, 0xFF`).
unsigned char* Party(unsigned actor) { return move_script::At(::bof3::Img(ObjTrio_addr) + 0x10 + (actor & 0xFF) * battle_damage::at::kPartyStride); }

unsigned char* Enemy(unsigned actor) { return move_script::At(::bof3::Img(Battle_Enemies_addr) + 0x80 + ((actor & 0xFF) - 3) * battle_damage::at::kEnemyStride); }

// Party(a) is +0x10 into the member record (0x802D50), Enemy(a) +0x80 into the
// enemy object (0x93B9E0): their typed views start at the records'
// starts, rv::As<records::PartyMember>(Party(a) - 0x10) and
// rv::As<records::BattleEnemy>(Enemy(a) + at::kEObject).
unsigned char Byte(std::uint32_t address) { return move_script::At(address)[0]; }

std::int32_t S32(std::uint32_t v) { return static_cast<std::int32_t>(v); }

std::uint32_t U32(std::int32_t v) { return static_cast<std::uint32_t>(v); }

short S16(unsigned v) { return static_cast<short>(static_cast<unsigned short>(v)); }

// 32-bit products wrap, as imul's low half does.
std::int32_t Mul(std::int32_t a, std::int32_t b) { return S32(U32(a) * U32(b)); }

unsigned char* Result() { return At(BattleDamage_Result); }

using U = std::uint32_t;

using S32BattleE5 = std::int32_t;

using move_script::SetLong;

unsigned char& B(U address) { return move_script::At(address)[0]; }

std::uint16_t W(U address) { return move_script::Word(move_script::At(address)); }

void SetW(U address, unsigned v) { move_script::SetWord(move_script::At(address), v); }

U L(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

S32BattleE5 S16BattleE5(std::uint16_t v) { return static_cast<std::int16_t>(v); }

unsigned char* Ptr(U cell) { return move_script::At(L(cell)); }

unsigned char* PartyBattleE5(unsigned member) { return move_script::At(::bof3::Img(ObjTrio_addr) + member * battle_e5::at::kPartyStride); }

unsigned char* EnemyObj(unsigned n) { return At(::bof3::Img(Battle_Enemies_addr) + n * battle_e5::at::kEnemyStride); }

// The callees nobody owns yet, by address (battle_e5_callees.h).
using Void0 = void (__cdecl*)();

using U1 = U (__cdecl*)(U);

using U2 = U (__cdecl*)(U, U);

using U3 = U (__cdecl*)(U, U, U);

// (a * 1000) / 10000 as the original computes it: the product and the
// thousandfold in 32 bits (wrapping), then a signed division truncated
// toward 0 (imul 0x68DB8BAD, sar 12, + the sign bit).
S32BattleE5 Tenth(U a) { return static_cast<S32BattleE5>(a * 1000u) / 10000; }

// A scaled word capped: stored when the result (as unsigned) is at most
// `cap`, else `cap` (cmp edx, cap; ja).
void ScaleCapped(unsigned char* word, U a, U cap) {
    const U q = static_cast<U>(Tenth(a));
    SetWord(word, q > cap ? cap : q);
}

U Key(const unsigned char* p) { return Low32(p); }

unsigned char* ResultBattleE5() { return Ptr(::bof3::Img(battle_e5::at::kResult.v)); }

// The drains' shared body (0x44C3D0 HP: the result's word +4, the party's
// +0x98 / +0xA0, the enemy's +0xA4 / +0xB0, flag 4, Battle_SetDamagePopup;
// 0x44C5C0 AP: +6, +0x9A / +0xA2, +0xA6 / +0xB2, flag 8, 0x453EB0), after
// the resist test and before the stat add:
//   the target 0x904B54 (read again after the test): its stat / 5 as the
//   result's delta, 0 for a target with flag 0x10000 (the party's +0x130, the
//   enemy's +0x110);
//   then the acting sprite 0x904B40's +8 (and for AP the result's +8) marked;
//   the actor 0x904B34: when its stat plus the delta (signed) runs past its
//   maximum, the acting sprite's +8 gets `flag`; 0x590E80(&stat, cap,
//   delta) - the cap word's upper half is the result pointer's, as the
//   original's register holds it -, its answer negated to the pop-up with the
//   actor, and the acting sprite's +8 cleared.
struct Drain {
    U at;
    U party_stat, party_max;
    U enemy_stat, enemy_max;
    unsigned char flag, mark;
    bool ap;
};

void DrainTarget(const Drain& d) {
    const unsigned t = B(::bof3::Img(battle_e5::at::kTarget.v));
    if (t <= 2) {
        unsigned char* const p = PartyBattleE5(t);
        SetWord(ResultBattleE5() + d.at, (L(Key(p) + 0x130) & 0x10000) ? 0u : Word(p + d.party_stat) / 5u);
    } else {
        unsigned char* const e = EnemyObj(t - 3);
        SetWord(ResultBattleE5() + d.at, (L(Key(e) + 0x110) & 0x10000) ? 0u : Word(e + d.enemy_stat) / 5u);
    }
}

void DrainActor(const Drain& d) {
    const unsigned a = B(::bof3::Img(battle_e5::at::kActor.v));
    unsigned char* stat;
    U max_at;
    if (a <= 2) {
        stat = PartyBattleE5(a) + d.party_stat;
        max_at = Key(PartyBattleE5(a)) + d.party_max;
    } else {
        stat = EnemyObj(a - 3) + d.enemy_stat;
        max_at = Key(EnemyObj(a - 3)) + d.enemy_max;
    }
    if (static_cast<S32BattleE5>(Word(stat)) + S16BattleE5(Word(ResultBattleE5() + d.at)) > static_cast<S32BattleE5>(W(max_at)))
        Ptr(::bof3::Img(battle_e5::at::kActingSprite2.v))[8] = static_cast<unsigned char>(Ptr(::bof3::Img(battle_e5::at::kActingSprite2.v))[8] | d.flag);
    const U result = BattleDamage_Result;
    const U change = BH_AT(U3, battle_e5::at::kStatAdd.v)(Key(stat), (result & 0xFFFF0000u) | W(max_at), Word(At(result) + d.at));
    if (d.ap) BH_AT(U2, battle_e5::at::kApPopup.v)(0u - change, B(::bof3::Img(battle_e5::at::kActor.v)));
    else BH_CALL(Battle_SetDamagePopup)(0u - change, B(::bof3::Img(battle_e5::at::kActor.v)));
    Ptr(::bof3::Img(battle_e5::at::kActingSprite2.v))[8] = 0;
}

constexpr Drain kDrainHp = {4, 0x98, 0xA0, 0xA4, 0xB0, 4, 1, false};

constexpr Drain kDrainAp = {6, 0x9A, 0xA2, 0xA6, 0xB2, 8, 2, true};

unsigned char* AtBattleMisc(battle_misc::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

battle_misc::U WordBattleMisc(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

battle_misc::U CurrentEnemy() { return Low32(BattleE3_CurrentEnemy.Native()); }

// Rows that fire once: EnemyAI_RowDone(actor, row) - skipped when al is set -
// then EnemyAI_ApplyAction and EnemyAI_SetRowDone(.., row, 1), each handed the
// current enemy read again. `actor` is what the row's test used.
void FireRow(battle_misc::U actor, battle_misc::U row_address, battle_misc::U row) {
    if (battle_misc::g.ai_row_done(actor, row) & 0xFF) return;
    battle_misc::g.ai_apply(CurrentEnemy(), row_address);
    battle_misc::g.ai_set_done(CurrentEnemy(), row, 1);
}

}  // namespace

namespace battle_odds {

namespace {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace battle_odds

namespace {

unsigned char* AtBattleOdds(battle_odds::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

unsigned char* P(unsigned actor) { return move_script::At(battle_setup::at::Party(actor)); }

unsigned char* E(unsigned actor) { return move_script::At(battle_setup::at::Enemy(actor)); }

using S32Rest3b = std::int32_t;

S32Rest3b S16Rest3b(std::uint16_t v) { return static_cast<std::int16_t>(v); }

unsigned char* PtrRest3b(U cell) { return move_script::At(L(cell)); }

unsigned char* ResultRest3b() { return PtrRest3b(::bof3::Img(rest_3b::at::kResult.v)); }

// The records as the originals index them: a party record by the byte
// itself (0x802D40 + 0x14C n, any n), an enemy object by the byte less 3
// (0x93B960 + 0x128 (n - 3), the difference in 32 bits - a byte below 3
// lands before the enemies). Neither is checked, as in the original.
unsigned char* PartyRest3b(unsigned n) { return move_script::At(::bof3::Img(ObjTrio_addr) + (n & 0xFF) * rest_3b::at::kPartyStride); }

unsigned char* EnemyRest3b(unsigned n) { return At(::bof3::Img(Battle_Enemies_addr) + static_cast<U>(static_cast<S32Rest3b>(n & 0xFF) - 3) * rest_3b::at::kEnemyStride); }

unsigned char* CharRecordRest3b(unsigned roster) { return move_script::At(::bof3::Img(CharacterRecords_addr) + (roster & 0xFF) * rest_3b::at::kCharStride); }

unsigned char* AbilityRow(unsigned id) { return move_script::At(::bof3::Img(rest_3b::at::kAbilityRows.v) + (id & 0xFFFF) * rest_3b::at::kAbilityStride); }

void MissTail() { BH_AT(Void0, rest_3b::at::kMissTail.v)(); }

void StatMod(U which) { BH_AT(U1, rest_3b::at::kStatMod.v)(which); }

void InflictMiss(U status) { BH_AT(U1, rest_3b::at::kInflictMiss.v)(status); }

void Inflict(U status) { BH_AT(U1, rest_3b::at::kRollInflict.v)(status); }

// The power word Battle_CalcDamage reads, from the acting actor's record:
// a member's word at `party_at`, an enemy's at `enemy_at` (the actor the
// dword 0x904B34's low byte, below 3 a member).
std::uint16_t ActorWord(unsigned actor, U party_at, U enemy_at) {
    return actor < 3 ? Word(PartyRest3b(actor) + party_at) : Word(EnemyRest3b(actor) + enemy_at);
}

// Battle_CalcDamage(actor, target, element): the two bytes read here, the
// answer's word into the result record's HP delta (the pointer read after the
// call).
std::uint16_t Hit(unsigned element) {
    const unsigned target = B(::bof3::Img(rest_3b::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3b::at::kActor.v));
    const short d = BH_CALL(Battle_CalcDamage)(actor, target, element);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>(d));
    return static_cast<std::uint16_t>(d);
}

// Half a hit: Battle_CalcDamage(actor, target, 0xFFFF) / 2 (signed, toward
// 0) as the HP delta; when that (read again) is not 0, 0x44FCA0(status).
void HalfHitInflict(U status) {
    const unsigned target = B(::bof3::Img(rest_3b::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3b::at::kActor.v));
    const short d = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>(static_cast<S32Rest3b>(d) / 2));
    if (static_cast<std::uint16_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change) != 0) Inflict(status);
}

// Effect_SkillDamage(actor, target, power, 0) as the HP delta.
void SkillHit(unsigned power) {
    const unsigned target = B(::bof3::Img(rest_3b::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3b::at::kActor.v));
    const int d = BH_CALL(Effect_SkillDamage)(actor, target, power, 0);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>(d));
}

// Battle_ClearStatus(target, mask), then a tail jump to 0x44FB30 (slots 5, 13..16).
void ClearThenMiss(U mask) {
    BH_CALL(Battle_ClearStatus)(B(::bof3::Img(rest_3b::at::kTarget.v)), mask);
    MissTail();
}

// Slots 35 and 41: the actor's stat word - a member's character record
// (roster 0x66972C[+0x89]) +0x44, an enemy's +0xD4; the hit rate 0x939FFC =
// 100, or the word 0x939FE8 halved plus 30 when that is at most 100; the
// power word 0x939FE4 = 0, then Stat_AddClamped(0x939FE4, the addend's word +
// the stat); the HP delta Battle_CalcDamage(actor, target, 0xFFFF) (both
// read again).
void StatSumHit(U addend) {
    const unsigned a = B(::bof3::Img(rest_3b::at::kActor.v));
    B(::bof3::Img(rest_3b::at::kRoundFlags2.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3b::at::kRoundFlags2.v)) | 0x20);
    const unsigned actor = L(::bof3::Img(rest_3b::at::kActor.v)) & 0xFF;
    const std::uint16_t stat =
        a <= 2 ? rv::As<records::CharRecord>(CharRecordRest3b(B(::bof3::Img(rest_3b::at::kRosterOf.v) + PartyRest3b(actor)[0x89])))->base.atk : rv::As<records::BattleEnemy>(EnemyRest3b(actor))->base_atk;
    const std::uint16_t half = static_cast<std::uint16_t>(Rest3b_PowerAdd2 >> 1);
    BattleE4_HitEnemy = static_cast<unsigned char>(half + 0x1E > 0x64 ? 0x64 : half + 0x1E);
    const std::uint16_t add = W(addend);
    SetWord(At(::bof3::Img(rest_3b::at::kPower.v)), 0);
    BH_CALL(Stat_AddClamped)(reinterpret_cast<unsigned short*>(At(::bof3::Img(rest_3b::at::kPower.v))), static_cast<std::uint16_t>(add + stat));
    Hit(0xFFFF);
}

// Slots 44..46 and 49: the acting kind 4, the ability word, a tail jump to
// slot 4.
void AsAbility(unsigned id) {
    B(::bof3::Img(rest_3b::at::kActingKind.v)) = 4;
    SetWord(At(::bof3::Img(rest_3b::at::kAbility.v)), id);
    BH_CALL(EffectSlot04_SkillPower)();
}

using S32Rest3c = std::int32_t;

S32Rest3c S16Rest3c(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

// The result record, read from its cell each time the original reads it.
unsigned char* ResultRest3c() { return Ptr(::bof3::Img(rest_3c::at::kResult.v)); }

// A record by an actor byte, as the original computes the address: unchecked
// (a read), the enemy's index the byte less 3 in 32 bits.
unsigned char* PartyRest3c(U member) { return At(::bof3::Img(ObjTrio_addr) + member * rest_3c::at::kPartyStride); }

unsigned char* EnemyRest3c(U index) { return At(::bof3::Img(Battle_Enemies_addr) + index * rest_3c::at::kEnemyStride); }

unsigned char* CharRecordRest3c(U character) { return At(::bof3::Img(CharacterRecords_addr) + character * rest_3c::at::kCharStride); }

// ... and for a write: a Fatal past the three members or the eight enemies,
// where the original writes on into what follows them.
unsigned char* PartyWrite(U member, const char* who) {
    if (member >= rest_3c::at::kPartyMax)
        bof3::Fatal("%s: member %u past ObjTrio's three - the original writes on into the window records (docs/rest_3c.md section 7)",
                    who, member);
    return PartyRest3c(member);
}

unsigned char* EnemyWrite(U index, const char* who) {
    if (index >= rest_3c::at::kEnemyMax)
        bof3::Fatal("%s: enemy %u (an actor byte less 3) past the eight objects - the original writes outside them "
                    "(docs/rest_3c.md section 7)",
                    who, index);
    return EnemyRest3c(index);
}

// The actor's or target's word at a member's or an enemy's offset (the HP
// +0x98 / +0xA4 pair and the like), by the byte's side: 0..2 a member.
std::uint16_t SideWord(unsigned who, U party_at, U enemy_at) {
    return who < rest_3c::at::kPartyMax ? Word(PartyRest3c(who) + party_at) : Word(EnemyRest3c(who - 3) + enemy_at);
}

// The party count 0x904AB0 as the loops over ObjTrio read it.
unsigned PartyCount(const char* who) {
    const unsigned n = B(::bof3::Img(rest_3c::at::kPartyCount.v));
    if (n > rest_3c::at::kPartyMax)
        bof3::Fatal("%s: the party count 0x904AB0 is %u, past ObjTrio's three members - the original walks on into "
                    "WindowRecords (docs/rest_3c.md section 7)",
                    who, n);
    return n;
}

using A0 = U (__cdecl*)();

using A1 = U (__cdecl*)(U);

using A2 = U (__cdecl*)(U, U);

U MissTailRest3c() { return BH_AT(A0, rest_3c::at::kMissTail.v)(); }

U Resisted() { return BH_AT(A2, rest_3c::at::kResisted.v)(B(::bof3::Img(rest_3c::at::kActor.v)), B(::bof3::Img(rest_3c::at::kTarget.v))); }

// Slots 50..61, 83: the acting kind 4 and an ability, then a tail jump to
// R3B's kind-4 damage (0x44C040), heal (0x44C120) or flag (0x44CF60).
U AsAbilityRest3c(unsigned ability, U tail) {
    B(::bof3::Img(rest_3c::at::kActingKind.v)) = 4;
    SetW(::bof3::Img(rest_3c::at::kAbility.v), ability);
    return BH_WORD(A0, tail)();
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
        unsigned char* const m = PartyRest3c(i);
        const unsigned char* const r = CharRecordRest3c(m[0x148]);
        for (unsigned k = 0; k < 6; ++k) m[0x92 + k] = r[0x12 + k];
        std::memcpy(m + 0xC0, r + 0x20, 32);
    }
    BH_CALL(Formation_ApplyStatMods)();
    n = PartyCount(who);
    for (unsigned i = 0; i < n; ++i) std::memmove(PartyRest3c(i) + 0xA0, PartyRest3c(i) + 0xC0, 32);
    BH_CALL(BattleForm_ApplyStats)();
    if (B(::bof3::Img(rest_3c::at::kPartyCount.v)) == 0) return;
    unsigned member = 0;
    do {
        BH_CALL(Battle_RecalcStats)(member);
        member = static_cast<unsigned char>(member + 1);
    } while (member < B(::bof3::Img(rest_3c::at::kPartyCount.v)));
}

}  // namespace

// original 0x44E2B0 / 0x44E330 (slots 108, 109): 0x44FB30; the actor's +0x134
// / +0x114 |= 0x40 (0x80), and its count +0x144 / +0x124 (+0x145 / +0x125) up
// by one below 2.
static void ActorFlagCount(U flag, U count_at, const char* who) {
    MissTailRest3c();
    const unsigned a = B(::bof3::Img(rest_3c::at::kActor.v));
    unsigned char* const r = a <= 2 ? PartyWrite(a, who) : EnemyWrite(a - 3, who);
    const U flags_at = a <= 2 ? 0x134 : 0x114;
    const U n_at = a <= 2 ? count_at : count_at - 0x20;
    SetLong(r + flags_at, static_cast<S32Rest3c>(static_cast<U>(Long(r + flags_at)) | flag));
    if (r[n_at] < 2) r[n_at] = static_cast<unsigned char>(r[n_at] + 1);
}

namespace {

using S32Rest3d = std::int32_t;

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

// The party size 0x904AB0 as slots 117's and 118's write loops read it: past
// ObjTrio's three the original writes on into WindowRecords; ours aborts, as
// R3C's restat loops do (docs/rest_3d.md section 7).
unsigned PartySizeForWrite(const char* who) {
    const unsigned n = B(::bof3::Img(rest_3d::at::kPartySize.v));
    if (n > 3)
        bof3::Fatal("%s: the party size 0x904AB0 is %u, past ObjTrio's three members - the original writes on into "
                    "WindowRecords (docs/rest_3d.md section 7)",
                    who, n);
    return n;
}

// The cells by side: HP (party +0x98, enemy +0xA4); the flag words +0x130 /
// +0x110 (0x10000: no damage; 0x200: no hit pose) and +0x134 / +0x114.
unsigned char* Hp(U actor) { return IsMember(actor) ? PartyRest3d(actor) + 0x98 : EnemyRest3d(actor) + 0xA4; }

unsigned char* Flags(U actor) { return IsMember(actor) ? PartyRest3d(actor) + 0x130 : EnemyRest3d(actor) + 0x110; }

unsigned char* Flags2(U actor) { return IsMember(actor) ? PartyRest3d(actor) + 0x134 : EnemyRest3d(actor) + 0x114; }

void OrLongRest3d(unsigned char* p, U bits) { move_script::SetLong(p, static_cast<S32Rest3d>(static_cast<U>(move_script::Long(p)) | bits)); }

void AndLong(unsigned char* p, U bits) { SetLong(p, static_cast<S32Rest3d>(static_cast<U>(Long(p)) & bits)); }

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

// Slots 116 and 124: Effect_HpBasedDamage(divisor) less the target's DEF
// (+0xA6 a member, +0xB6 an enemy; the side by the target read before the
// call, the record by the one read after), the HP delta, at least 0.
void HpLessDefence(int divisor) {
    const bool member = IsMember(Target());
    const U v = static_cast<U>(BH_CALL(Effect_HpBasedDamage)(divisor));
    const unsigned t = Target();
    const U def = member ? rv::As<records::PartyMember>(PartyRest3d(t))->def : rv::As<records::BattleEnemy>(EnemyRest3d(t))->def;
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(Result())->hp_change, v - def);
    if (S16Rest3d(static_cast<std::uint16_t>(rv::As<records::ActionRecord>(Result())->hp_change)) < 0) rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(Result())->hp_change, 0);
}

}  // namespace

// original 0x44AAD0 (PSX 0x80098BB0 EnemyAI_TurnCheck): the current enemy's four
// per-turn rows. For row i: the enemy (kCurrentEnemy, read at the row's start)
// names a script by its byte +0xF0; the row is 16 bytes at kAiScripts + script
// * 0x8C + i * 16, its byte 0 the opcode, through the index table 0x44AE64
// and the jump table 0x44AE14:
//   0..8          EnemyAI_CondElement(1, 2, 4, 8, 0x10, 0x20, 0x40, 0x100, 0x80)
//                 al set: fire once, the enemy read again after the test
//   9             kActingKind 4 and the enemy's word +0x108 non-zero: fire once
//   0xA           kActingKind 1 and word +0x108 non-zero: fire once
//   0x16 / 0x17   byte +0x92 bit 3 / bit 7: fire once
//   0x18          byte +0xAA zero: fire once
//   0x21..0x23    EnemyAI_CondElement(1 / 2 / 4): EnemyAI_ApplyAction only
//   0x24          kActingKind 1 and word +0x108 non-zero: EnemyAI_ApplyAction only
//   0x25          the u16 +0xA4 not above the s16 +0x108: fire once
//   anything else (above 0x25 through the range check): nothing
// then 0x44B920, whose eax is returned. The row index is handed on as a dword
// whose upper bytes are the caller's ecx (the original keeps it in its pushed
// ecx slot); both callees read its low byte.
extern "C" unsigned long __cdecl EnemyAI_TurnCheck(void) {
    static constexpr battle_misc::U kMasks[9] = {1, 2, 4, 8, 0x10, 0x20, 0x40, 0x100, 0x80};
    for (battle_misc::U i = 0; i < 4; ++i) {
        const battle_misc::U actor = CurrentEnemy();
        const battle_misc::U row = ::bof3::Img(kAiScripts.v) + AtBattleMisc(actor + 0xF0)[0] * 0x8Cu + i * 16;
        const battle_misc::U op = AtBattleMisc(row)[0];
        const battle_misc::U acting = AtBattleMisc(::bof3::Img(kActingKind.v))[0];
        switch (op) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
            if (battle_misc::g.ai_condition(kMasks[op]) & 0xFF) FireRow(CurrentEnemy(), row, i);
            break;
        case 9:
            if (acting == 4 && WordBattleMisc(AtBattleMisc(actor + 0x108)) != 0) FireRow(actor, row, i);
            break;
        case 0xA:
            if (acting == 1 && WordBattleMisc(AtBattleMisc(actor + 0x108)) != 0) FireRow(actor, row, i);
            break;
        case 0x16:
            if (AtBattleMisc(actor + 0x92)[0] & 8) FireRow(actor, row, i);
            break;
        case 0x17:
            if (AtBattleMisc(actor + 0x92)[0] & 0x80) FireRow(actor, row, i);
            break;
        case 0x18:
            if (AtBattleMisc(actor + 0xAA)[0] == 0) FireRow(actor, row, i);
            break;
        case 0x21: case 0x22: case 0x23:
            if (battle_misc::g.ai_condition(1u << (op - 0x21)) & 0xFF) battle_misc::g.ai_apply(CurrentEnemy(), row);
            break;
        case 0x24:
            if (acting == 1 && WordBattleMisc(AtBattleMisc(actor + 0x108)) != 0) battle_misc::g.ai_apply(actor, row);
            break;
        case 0x25:
            if (static_cast<std::int32_t>(WordBattleMisc(AtBattleMisc(actor + 0xA4))) <=
                static_cast<std::int16_t>(WordBattleMisc(AtBattleMisc(actor + 0x108))))
                FireRow(actor, row, i);
            break;
        default:
            break;
        }
    }
    return battle_misc::g.ai_finish();
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
        if (battle_damage::g.is_out(e + 3)) continue;
        unsigned char* const obj = Enemy(e + 3) + battle_damage::at::kEObject;
        auto* const er = rv::As<records::BattleEnemy>(obj);
        for (unsigned r = 0; r < 4; ++r) {
            const unsigned char* const row = At(::bof3::Img(battle_damage::at::kAiScripts.v) + er->data_id * battle_damage::at::kAiScriptStride + 16 * r);
            const unsigned hp = er->hp, ap = er->ap, max_hp = er->max_hp, max_ap = er->max_ap;
            const std::uint32_t counter = U32(Long(At(::bof3::Img(battle_damage::at::kTurnCounter.v))));
            bool holds;
            switch (row[0]) {
            case 0x0B: holds = hp <= (max_hp >> 2); break;
            case 0x0C: holds = hp <= (max_hp >> 1); break;
            case 0x0D: holds = static_cast<std::int32_t>(ap) <= static_cast<std::int32_t>(max_ap / 5); break;
            case 0x0E: {
                unsigned char count = 0;
                for (unsigned k = 0; k < 8; ++k)
                    if (!battle_damage::g.is_out(k + 3) && (Enemy(k + 3)[battle_damage::at::kEStatus + 1] & 0x20)) ++count;
                holds = count != 0;
                break;
            }
            case 0x0F: holds = Battle_EnemyCount > Battle_EnemiesLeft; break;
            case 0x10: holds = Battle_EnemiesLeft == 1; break;
            case 0x11: holds = hp > (max_hp >> 1); break;
            case 0x12: holds = ap > (max_ap >> 1); break;
            case 0x13: holds = counter == 2; break;
            case 0x14: holds = counter == 10; break;
            case 0x15: holds = static_cast<std::int32_t>(rv::As<records::PartyMember>(Party(0) - 0x10)->level) - static_cast<std::int32_t>(er->level) > 5; break;
            case 0x19: holds = (counter & 1) == 0; break;
            case 0x1A: holds = (counter & 1) != 0; break;
            case 0x1B: holds = counter % 3 == 0; break;
            case 0x1C: holds = counter % 3 != 0; break;
            case 0x1D: holds = Battle_PartyStanding == 1; break;
            case 0x1E: holds = Battle_PartyStanding != 1; break;
            case 0x1F: holds = Battle_EnemiesLeft == Battle_EnemyCount; break;
            case 0x20: holds = Battle_EnemiesLeft != 1; break;
            case 0x26: holds = battle_damage::g.ai_rows_left(r, e) != 0; break;
            case 0x27: holds = BattleDamage_AiByteB97 == 1; break;
            default: continue;
            }
            if (holds) {
                if (!battle_damage::g.row_done(obj, r)) {
                    battle_damage::g.ai_apply(obj, row);
                    battle_damage::g.ai_set_done(obj, r, 1);
                }
            } else if (battle_damage::g.row_done(obj, r)) {
                battle_damage::g.ai_set_done(obj, r, 0);
            }
        }
    }
    battle_damage::g.ai_finish();
}

// original 0x44B240 (EnemyAI_TurnCheck's sibling EnemyAI_ChooseActions,
// condition 0x26): 1 when every row of enemy `enemy`'s script but `row` is
// done - rows 0..3 in turn, skipping `row`: a row whose byte 0 is 0x63 answers
// 0, and so does one EnemyAI_RowDone says is not done. The script byte +0xF0
// is read again for each row. al out, eax above it the caller's.
extern "C" unsigned char __cdecl EnemyAI_OtherRowsDone(unsigned row, unsigned enemy) {
    const unsigned skip = row & 0xFF;
    unsigned char* const obj = EnemyObj(enemy & 0xFF);
    for (unsigned i = 0; i < 4; ++i) {
        if (i == skip) continue;
        const unsigned script = obj[0xF0];
        if (B(::bof3::Img(battle_e5::at::kAiScripts.v) + script * 0x8C + i * 16) == 0x63) return 0;
        if ((BH_CALL(EnemyAI_RowDone)(obj, i) & 0xFF) == 0) return 0;
    }
    return 1;
}

// original 0x44B2C0 (PSX EnemyAI_RowDone 0x80099830): bit `row` (its low
// byte, as a shift count of 0..31 - 8 and up answer 0) of the enemy object's
// byte +0xF1, the rows that have fired.
extern "C" unsigned char __cdecl EnemyAI_RowDone(const unsigned char* enemy, unsigned row) {
    return static_cast<unsigned char>((static_cast<unsigned>(enemy[0xF1]) >> (row & 0x1F)) & 1);
}

// original 0x44B2E0: the enemy's done bit for `row` (+0xF1 bit row & 31 -
// `shl al, cl` on a byte, so rows 8..31 name no bit) set when `on`'s byte is
// not 0, else cleared.
extern "C" void __cdecl EnemyAI_SetRowDone(unsigned char* enemy, unsigned row, unsigned on) {
    auto* const enemy_rec = rv::As<records::BattleEnemy>(enemy);
    const unsigned shift = row & 31;
    const unsigned char bit = static_cast<unsigned char>(shift < 8 ? 1u << shift : 0u);
    if (on & 0xFF) enemy_rec->ai_rows_done = static_cast<unsigned char>(enemy_rec->ai_rows_done | bit);
    else enemy_rec->ai_rows_done = static_cast<unsigned char>(enemy_rec->ai_rows_done & ~bit);
}

// original 0x44B320: whether the action that just hit carries one of `mask`'s
// elements. 0 unless the current enemy's word +0x108 is not 0; then by the
// acting kind 0x904B35: 4 (an ability) - the ability's element word (24-byte
// rows at 0x65C4DC by the word 0x904B80) against the mask's low word; 1 (an
// attack) by a party member 0..2 and the mask's bit 8 clear - the member's
// weapon +0x92 and its element byte (0x657463, 28 bytes a weapon) against
// the mask's low byte. Anything else 0. al out.
extern "C" unsigned char __cdecl EnemyAI_CondElement(unsigned mask) {
    if (static_cast<std::uint16_t>(BattleE3_CurrentEnemy->hp_change) == 0) return 0;
    const unsigned actor = B(::bof3::Img(battle_e5::at::kActor.v));
    const unsigned kind = B(::bof3::Img(battle_e5::at::kActingKind.v));
    if (kind == 4) {
        const unsigned ability = L(::bof3::Img(battle_e5::at::kAbility.v)) & 0xFFFF;
        return (W(::bof3::Img(battle_e5::at::kAbilityRows.v) + ability * 24) & mask & 0xFFFF) != 0;
    }
    if (kind != 1 || actor > 2 || (mask & 0x100) != 0) return 0;
    const unsigned weapon = rv::As<records::PartyMember>(PartyBattleE5(actor))->weapon;
    return (B(::bof3::Img(battle_e5::at::kWeaponElement.v) + weapon * 28) & mask & 0xFF) != 0;
}

// original 0x44B3A0: an AI row applied to an enemy object. By the row's kind
// (byte +1, 1..7 through the jump table 0x44B5C4):
//   1  the word +0x90 = the row's byte +2;
//   2  Sprite_Current the enemy for Battle_ClearStatus(enemy +5, 0xFFFF),
//      then 0x44F1D0(Sprite_Current +5, +0x92 | row +2) with the upper half
//      of ClearStatus's eax in both words, as the original's registers carry
//      it; Sprite_Current put back;
//   3  for each bit of row +2 from the top, EnemyAI_ScaleStat(enemy, bit,
//      row +3);
//   4  the same through EnemyAI_SetAttrByte;
//   5  row +2 bit 1: +0xAA = row +3; bit 0: +0xAE = row +3;
//   6  row +2 bit 1: the word +0x96 scaled by row +3 / 10, bit 0: +0x94,
//      each capped at 0xFFFF;
//   7  0x904B97 = row +2;
// then always: +0x8E = row +4, row +8..+15 into +0x9C..+0xA3,
// 0x453300(enemy +5) (the word's upper bytes are row + 16's, the copy's
// pointer), and - HP +0xA4 and the row's word +6 both not 0, the message
// count 0x93C2A2 below 8 - an enemy message: enemy +5 minus 3, the word (a
// system message BattleAction_EnemyMessages / BattleCommit_QueueMessages show).
extern "C" void __cdecl EnemyAI_ApplyAction(unsigned char* enemy, const unsigned char* row) {
    auto* const enemy_rec = rv::As<records::BattleEnemy>(enemy);
    switch (row[1]) {
    case 1:
        enemy_rec->flags_90 = row[2];
        break;
    case 2: {
        unsigned char* const saved = Sprite_Current;
        Sprite_Current = enemy;
        const U cleared = BH_CALL(Battle_ClearStatus)(enemy_rec->actor, 0xFFFF);
        const U status = enemy[0x92];
        const unsigned char* const current = Sprite_Current;
        const U word = (cleared & 0xFFFF0000u) | status | row[2];
        BH_AT(U2, battle_e5::at::kInflictStatus.v)((word & 0xFFFFFF00u) | current[5], word);
        Sprite_Current = saved;
        break;
    }
    case 3: {
        unsigned bits = row[2];
        for (unsigned i = 0; i < 8; ++i, bits <<= 1)
            if (bits & 0x80) BH_CALL(EnemyAI_ScaleStat)(enemy, i, row[3]);
        break;
    }
    case 4: {
        unsigned bits = row[2];
        for (unsigned i = 0; i < 8; ++i, bits <<= 1)
            if (bits & 0x80) BH_CALL(EnemyAI_SetAttrByte)(enemy, i, row[3]);
        break;
    }
    case 5:
        if (row[2] & 2) enemy_rec->drop0_row = row[3];
        if (row[2] & 1) enemy[0xAE] = row[3];
        break;
    case 6:
        if (row[2] & 2) ScaleCapped(enemy + 0x96, static_cast<U>(row[3]) * enemy_rec->exp, 0xFFFF);
        if (row[2] & 1) ScaleCapped(enemy + 0x94, static_cast<U>(row[3]) * enemy_rec->zenny, 0xFFFF);
        break;
    case 7:
        BattleDamage_AiByteB97 = row[2];
        break;
    default:
        break;
    }
    enemy_rec->odds_index = row[4];
    for (unsigned i = 0; i < 8; ++i) enemy[0x9C + i] = row[8 + i];
    const U tail = Low32(row + 16);
    BH_AT(U1, battle_e5::at::kStatusPick.v)((tail & 0xFFFFFF00u) | enemy_rec->actor);
    if (enemy_rec->hp != 0 && Word(row + 6) != 0) {
        const unsigned n = BattleE5_EnemyMessageCount;
        if (n < 8) {
            B(::bof3::Img(battle_e5::at::kEnemyMessages.v) + n * 4) = static_cast<unsigned char>(enemy_rec->actor - 3);
            SetW(::bof3::Img(battle_e5::at::kEnemyMessages.v) + n * 4 + 2, Word(row + 6));
            BattleE5_EnemyMessageCount = static_cast<unsigned char>(n + 1);
        }
    }
}

// original 0x44B5E0 (called by EnemyAI_ApplyAction for kind 3): one of the
// enemy's stats scaled by factor / 10 (Tenth): which 0 HP +0xA4 capped at
// +0xD0, 1 AP +0xA6 capped at +0xD2, 2..5 +0xD4 / +0xD6 / +0xD8 / +0xDA
// capped at 999, 6 +0x98 by the factor squared, capped at 999; others
// nothing.
extern "C" void __cdecl EnemyAI_ScaleStat(unsigned char* enemy, unsigned which, unsigned factor) {
    auto* const enemy_rec = rv::As<records::BattleEnemy>(enemy);
    const U f = factor & 0xFF;
    switch (which & 0xFF) {
    case 0: ScaleCapped(enemy + 0xA4, enemy_rec->hp * f, enemy_rec->base_max_hp); break;
    case 1: ScaleCapped(enemy + 0xA6, enemy_rec->ap * f, enemy_rec->base_max_ap); break;
    case 2: ScaleCapped(enemy + 0xD4, enemy_rec->base_atk * f, 999); break;
    case 3: ScaleCapped(enemy + 0xD6, enemy_rec->base_def * f, 999); break;
    case 4: ScaleCapped(enemy + 0xD8, enemy_rec->base_agi * f, 999); break;
    case 5: ScaleCapped(enemy + 0xDA, enemy_rec->base_intel * f, 999); break;
    case 6: ScaleCapped(enemy + 0x98, enemy_rec->level * f * f, 999); break;
    default: break;
    }
}

// original 0x44B870 (called by EnemyAI_ApplyAction for kind 4): the enemy's
// byte +0xDF + which for which 0..6, +0xE7 for 7 (+0xE6 is skipped), set to
// the value's byte; others nothing. The cut's 0x44B8D0 is case 5 (+0xE4).
extern "C" void __cdecl EnemyAI_SetAttrByte(unsigned char* enemy, unsigned which, unsigned value) {
    const unsigned w = which & 0xFF;
    if (w <= 6) enemy[0xDF + w] = static_cast<unsigned char>(value);
    else if (w == 7) enemy[0xE7] = static_cast<unsigned char>(value);
}

// original 0x44B920 (EnemyAI_TurnCheck's and EnemyAI_ChooseActions' tail):
// the enemy messages 0x939FC0 (0x93C2A2 entries) with each one dropped whose
// enemy's +0x8C byte and message word match one kept before it; the kept ones
// written back in order and counted. The original keeps them in an eight-dword
// stack buffer: a ninth kept entry would land on its return address, so ours
// aborts there. Its eax is not read (the one caller of EnemyAI_TurnCheck,
// 0x436908, loads eax again at once).
extern "C" void __cdecl EnemyAI_DedupMessages(void) {
    const unsigned n = BattleE5_EnemyMessageCount;
    U kept[8];
    unsigned k = 0;
    for (unsigned i = 0; i < n; ++i) {
        const U entry = ::bof3::Img(battle_e5::at::kEnemyMessages.v) + 4 * i;
        bool dup = false;
        if (k != 0) {
            const unsigned char group = EnemyObj(B(entry))[0x8C];
            for (unsigned j = 0; j < k && !dup; ++j)
                dup = group == EnemyObj(kept[j] & 0xFF)[0x8C] && W(entry + 2) == (kept[j] >> 16);
        }
        if (dup) continue;
        if (k == 8)
            bof3::Fatal("EnemyAI_DedupMessages: a ninth distinct entry of %u - the original writes it past its eight-dword "
                        "buffer, onto its return address (docs/battle_e5.md section 7)",
                        n);
        kept[k++] = L(entry);
    }
    BattleE5_EnemyMessageCount = static_cast<unsigned char>(k);
    for (unsigned i = 0; i < k; ++i) SetLong(At(::bof3::Img(battle_e5::at::kEnemyMessages.v) + 4 * i), static_cast<S32BattleE5>(kept[i]));
}

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
    const unsigned target0 = Byte(::bof3::Img(battle_damage::at::kTarget.v));
    if (target0 <= 2) Party(target0)[battle_damage::at::kPShown] = static_cast<unsigned char>(Party(target0)[battle_damage::at::kPShown] | 0x31);
    else rv::As<records::BattleEnemy>(Enemy(target0) + battle_damage::at::kEObject)->shown = static_cast<unsigned char>(rv::As<records::BattleEnemy>(Enemy(target0) + battle_damage::at::kEObject)->shown | 0x31);
    const std::uint32_t acting = U32(Long(At(::bof3::Img(battle_damage::at::kActing.v))));
    const std::uint32_t ability = U32(Long(At(::bof3::Img(battle_damage::at::kAbility.v))));
    const unsigned actor = acting & 0xFF;
    const unsigned kind = (acting >> 8) & 0xFF;
    if (!(kind == 4 && (ability & 0xFFFF) == 0xA3)) {
        unsigned char* const power = At(::bof3::Img(BattleDamage_AttackerInt_addr));
        if (actor <= 2) {
            unsigned char* const p = Party(actor);
            auto* const pr = rv::As<records::PartyMember>(p - 0x10);
            if (p[battle_damage::at::kPFlags2] & 0x80) {
                const unsigned times = pr->count_80 + 1u;
                pr->count_80 = 0;
                SetWord(power, times * Word(power));
                pr->flags_134 = S32(U32(static_cast<std::int32_t>(pr->flags_134)) & ~0x80u);
            }
        } else {
            unsigned char* const e = Enemy(actor);
            auto* const er = rv::As<records::BattleEnemy>(e - 0x80);
            if (e[battle_damage::at::kEFlags2] & 0x80) {
                const unsigned times = er->count_80 + 1u;
                er->count_80 = 0;
                SetWord(power, times * Word(power));
                er->flags_114 = S32(U32(static_cast<std::int32_t>(er->flags_114)) & ~0x80u);
            }
        }
    }
    unsigned handler;
    if (kind == 4) {
        handler = Byte(::bof3::Img(battle_damage::at::kSkillHandler.v) + (ability & 0xFFFF));
    } else {
        const unsigned item = Word(At(BattleSprites_Action) + 2);
        handler = At(U32(Long(At(::bof3::Img(battle_damage::at::kItemHandlers.v) + 4 * (item >> 8)))))[item & 0xFF];
    }
    reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(U32(Long(At(::bof3::Img(Effect_Handlers_addr) + 4 * handler)))))();

    const unsigned t = Byte(::bof3::Img(battle_damage::at::kTarget.v));
    if (t <= 2 ? (static_cast<std::int32_t>(rv::As<records::PartyMember>(Party(t) - 0x10)->flags_130) & 0x10000) : (static_cast<std::int32_t>(rv::As<records::BattleEnemy>(Enemy(t) + battle_damage::at::kEObject)->flags_110) & 0x10000)) {
        rv::Store<unsigned>(rv::As<records::ActionRecord>(Result())->hp_change, 0);
        rv::Store<unsigned>(rv::As<records::ActionRecord>(Result())->ap_change, 0);
    }
    unsigned char* r = Result();
    if (rv::As<records::ActionRecord>(r)->hp_change > 9999) {
        rv::Store<unsigned>(rv::As<records::ActionRecord>(r)->hp_change, 9999);
        r = Result();
    }
    if (rv::As<records::ActionRecord>(r)->hp_change < -9999) {
        rv::Store<unsigned>(rv::As<records::ActionRecord>(r)->hp_change, static_cast<unsigned>(-9999));
        r = Result();
    }
    const std::int32_t hp_delta = rv::As<records::ActionRecord>(r)->hp_change;
    if (hp_delta != 0) {
        if (t <= 2) {
            unsigned char* const p = Party(t);
            auto* const pr = rv::As<records::PartyMember>(p - 0x10);
            const std::int32_t hp = pr->hp;
            if (hp_delta > 0) {
                if (hp > hp_delta) {
                    pr->hp = U32(hp - hp_delta);
                } else {
                    if (hp_delta - hp < pr->survive && (p[battle_damage::at::kPFlags2] & 3) == 0)
                        pr->flags_130 = S32(U32(static_cast<std::int32_t>(pr->flags_130)) | 4);
                    pr->hp = 0;
                }
            } else {
                const std::int32_t max = pr->max_hp;
                if (hp - hp_delta < max) {
                    rv::Store<unsigned>(pr->hp, U32(hp - hp_delta));
                } else {
                    rv::Store<unsigned>(pr->hp, U32(max));
                    p[battle_damage::at::kPShown] = static_cast<unsigned char>((p[battle_damage::at::kPShown] & 0xEF) | 5);
                }
                r = Result();
            }
        } else {
            unsigned char* const e = Enemy(t);
            auto* const er = rv::As<records::BattleEnemy>(e - 0x80);
            const std::int32_t hp = er->hp;
            if (hp != 0xFFFF) {
                if (hp_delta > 0) {
                    rv::Store<unsigned>(er->hp, hp > hp_delta ? U32(hp - hp_delta) : 0);
                } else {
                    const std::int32_t max = er->max_hp;
                    if (hp - hp_delta < max) {
                        rv::Store<unsigned>(er->hp, U32(hp - hp_delta));
                    } else {
                        rv::Store<unsigned>(er->hp, U32(max));
                        er->shown = static_cast<unsigned char>((er->shown & 0xEF) | 5);
                    }
                    r = Result();
                }
            }
        }
    }
    if (rv::As<records::ActionRecord>(r)->ap_change > 9999) {
        rv::Store<unsigned>(rv::As<records::ActionRecord>(r)->ap_change, 9999);
        r = Result();
    }
    if (rv::As<records::ActionRecord>(r)->ap_change < -9999) {
        rv::Store<unsigned>(rv::As<records::ActionRecord>(r)->ap_change, static_cast<unsigned>(-9999));
        r = Result();
    }
    const std::int32_t ap_delta = rv::As<records::ActionRecord>(r)->ap_change;
    if (ap_delta == 0) return;
    if (t <= 2) {
        unsigned char* const p = Party(t);
        auto* const pr = rv::As<records::PartyMember>(p - 0x10);
        const std::int32_t ap = pr->ap;
        if (ap_delta > 0) {
            rv::Store<unsigned>(pr->ap, ap > ap_delta ? U32(ap - ap_delta) : 0);
        } else {
            const std::int32_t max = pr->max_ap;
            if (ap - ap_delta < max) {
                pr->ap = U32(ap - ap_delta);
            } else {
                pr->ap = U32(max);
                p[battle_damage::at::kPShown] = static_cast<unsigned char>((p[battle_damage::at::kPShown] & 0xDF) | 0xA);
            }
        }
        return;
    }
    unsigned char* const e = Enemy(t);
    auto* const er = rv::As<records::BattleEnemy>(e - 0x80);
    const std::int32_t ap = er->ap;
    if (ap == 0xFFFF) return;
    if (ap_delta > 0) {
        rv::Store<unsigned>(er->ap, ap > ap_delta ? U32(ap - ap_delta) : 0);
    } else if (ap - ap_delta < er->max_hp) {
        er->ap = U32(ap - ap_delta);
    } else {
        er->ap = er->max_ap;
        er->shown = static_cast<unsigned char>((er->shown & 0xDF) | 0xA);
    }
}

// original 0x44BED0 (slot 0): a tail jump to 0x44FB30, the miss tail.
extern "C" void __cdecl EffectSlot00_Miss(void) { MissTail(); }

// original 0x44BEE0 (slot 1): the power word 0x939FE4 = the actor's +0xA4
// (a member) or +0xB4 (an enemy); Rand & 3: 0 halves it, 3 adds its half
// (16 bits), 1 and 2 leave it; then the HP delta Battle_CalcDamage(actor,
// target, 0xFFFF).
extern "C" void __cdecl EffectSlot01_VariedHit(void) {
    SetWord(At(::bof3::Img(rest_3b::at::kPower.v)), ActorWord(B(::bof3::Img(rest_3b::at::kActor.v)), 0xA4, 0xB4));
    const U roll = static_cast<U>(BH_CALL(Rand)()) & 3;
    if (roll == 0) {
        SetWord(At(::bof3::Img(rest_3b::at::kPower.v)), W(::bof3::Img(rest_3b::at::kPower.v)) >> 1);
    } else if (roll == 3) {
        const std::uint16_t p = W(::bof3::Img(rest_3b::at::kPower.v));
        SetWord(At(::bof3::Img(rest_3b::at::kPower.v)), static_cast<std::uint16_t>(p + (p >> 1)));
    }
    Hit(0xFFFF);
}

// original 0x44BF70 (slot 2): the HP delta Battle_CalcDamage(actor, target,
// 4); then (the target read again) when the delta, signed, is below the
// target's HP (+0x98 a member, +0xA4 an enemy) and not 0: 0x44FCA0(4).
extern "C" void __cdecl EffectSlot02_HitInflict4(void) {
    Hit(4);
    const unsigned t = B(::bof3::Img(rest_3b::at::kTarget.v));
    const S32Rest3b hp = t < 3 ? rv::As<records::PartyMember>(PartyRest3b(t))->hp : rv::As<records::BattleEnemy>(EnemyRest3b(t))->hp;
    const std::uint16_t d = static_cast<std::uint16_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change);
    if (S16Rest3b(d) < hp && d != 0) Inflict(4);
}

// original 0x44BFF0 (slot 3): half a hit, then 0x44FCA0(0x20).
extern "C" void __cdecl EffectSlot03_HalfHitInflict20(void) { HalfHitInflict(0x20); }

// original 0x44C040 (slot 4; R3C's slots 50, 51, 52, 55, 56, 58, 60 and 0x44D680,
// and slots 44, 45, 46, 49 below, jump here after setting the ability): not in
// the cut. Effect_SkillDamage(actor, target, the ability's power byte
// (NameTable_Abilities +3), 0) as the HP delta.
extern "C" void __cdecl EffectSlot04_SkillPower(void) {
    const unsigned ability = L(::bof3::Img(rest_3b::at::kAbility.v)) & 0xFFFF;
    const unsigned target = B(::bof3::Img(rest_3b::at::kTarget.v));
    const unsigned power = AbilityRow(ability)[3];
    const unsigned actor = B(::bof3::Img(rest_3b::at::kActor.v));
    const int d = BH_CALL(Effect_SkillDamage)(actor, target, power, 0);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>(d));
}

// original 0x44C080 (slot 5): Battle_ClearStatus(target, 0x80), the miss tail.
extern "C" void __cdecl EffectSlot05_Clear80(void) { ClearThenMiss(0x80); }

// original 0x44C0A0 (slot 6): when the actor's party record (by the actor
// byte, whichever side) has character +0x89 0x0A: 0x44FB30, then the HP
// delta 9999. Else 0x44FCE0(3) less the target's enemy object's +0xB6 (by
// the target byte, whichever side; 16 bits). Then a negative delta is 0.
extern "C" void __cdecl EffectSlot06_HpThirdHit(void) {
    if (PartyRest3b(B(::bof3::Img(rest_3b::at::kActor.v)))[0x89] == 0x0A) {
        MissTail();
        rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, 9999);
    } else {
        const U share = BH_AT(U1, rest_3b::at::kHpShare.v)(3);
        const unsigned t = B(::bof3::Img(rest_3b::at::kTarget.v));
        rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>(share - Word(EnemyRest3b(t) + 0xB6)));
    }
    if (S16Rest3b(static_cast<std::uint16_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change)) < 0) rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, 0);
}

// original 0x44C120 (slot 7; R3C's 0x44D120 jumps here): not in the cut.
// Effect_HealAmount(actor, target) as the HP delta.
extern "C" void __cdecl EffectSlot07_Heal(void) {
    const unsigned target = B(::bof3::Img(rest_3b::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3b::at::kActor.v));
    const int d = BH_CALL(Effect_HealAmount)(actor, target);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>(d));
}

// original 0x44C140 (Effect_Handlers slot 8): the result record's HP delta
// (word +4 of *kResult) = -20, a heal of 20 (positive is damage,
// Effect_ApplyResult). eax the record pointer.
extern "C" unsigned long __cdecl Effect_Heal20(void) {
    const battle_odds::U result = BattleDamage_Result;
    rv::Store<battle_odds::U>(rv::As<records::ActionRecord>(AtBattleOdds(result))->hp_change, 0xFFEC);  // the result record (records::ActionRecord)
    return result;
}

// original 0x44C150 (slot 9): the HP delta -40.
extern "C" void __cdecl EffectSlot09_Heal40(void) { rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, 0xFFD8); }

// original 0x44C160 (slot 10): the HP delta -100.
extern "C" void __cdecl EffectSlot10_Heal100(void) { rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, 0xFF9C); }

// original 0x44C170 (slot 11; R3C's 0x44D6A0 jumps here): not in the cut. A
// member target: the HP delta minus its maximum HP (+0xA0). An enemy: its
// +0xB0 0xFFFF gives the delta 0xFFFF and its +0x10C |= 4 (the target read
// again); else minus the +0xB0.
extern "C" void __cdecl EffectSlot11_HealFull(void) {
    const unsigned t = B(::bof3::Img(rest_3b::at::kTarget.v));
    if (t < 3) {
        rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>(0u - rv::As<records::PartyMember>(PartyRest3b(t))->max_hp));
        return;
    }
    const std::uint16_t max = rv::As<records::BattleEnemy>(EnemyRest3b(t))->max_hp;
    if (max == 0xFFFF) {
        rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, 0xFFFF);
        unsigned char* const e = EnemyRest3b(B(::bof3::Img(rest_3b::at::kTarget.v)));
        e[0x10C] = static_cast<unsigned char>(e[0x10C] | 4);
        return;
    }
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>(0u - max));
}

// original 0x44C1F0 (slot 12): the HP delta -5; then (Rand & 0x7F) at most
// 0x26: Battle_ClearStatus(target, 0x68).
extern "C" void __cdecl EffectSlot12_Heal5Clear68(void) {
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, 0xFFFB);
    const U roll = static_cast<U>(BH_CALL(Rand)()) & 0x7F;
    if (roll <= 0x26) BH_CALL(Battle_ClearStatus)(B(::bof3::Img(rest_3b::at::kTarget.v)), 0x68);
}

// original 0x44C220 (slot 13): as slot 5 (the same bytes).
extern "C" void __cdecl EffectSlot13_Clear80(void) { ClearThenMiss(0x80); }

// original 0x44C240 (slot 14): Battle_ClearStatus(target, 8), the miss tail.
extern "C" void __cdecl EffectSlot14_Clear8(void) { ClearThenMiss(8); }

// original 0x44C260 (slot 15): ... 0x100.
extern "C" void __cdecl EffectSlot15_Clear100(void) { ClearThenMiss(0x100); }

// original 0x44C280 (slot 16): ... 0xBFC.
extern "C" void __cdecl EffectSlot16_ClearBFC(void) { ClearThenMiss(0xBFC); }

// original 0x44C2A0 (slot 17): a target whose status word (+0x90 a member,
// +0x92 an enemy) has 0x4000 (the bit Battle_ActorIsOut reads as out): the HP
// delta 0xFFFF, Battle_ClearStatus(target, 0x4000), then (the target read
// again) 0x904AB1 (a member) or 0x904AB3 (an enemy) one up. Else the delta 0.
extern "C" void __cdecl EffectSlot17_RaiseDown(void) {
    const unsigned t = B(::bof3::Img(rest_3b::at::kTarget.v));
    const std::uint16_t status = t <= 2 ? rv::As<records::PartyMember>(PartyRest3b(t))->status : rv::As<records::BattleEnemy>(EnemyRest3b(t))->status;
    if ((status & 0x4000) == 0) {
        rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, 0);
        return;
    }
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, 0xFFFF);
    BH_CALL(Battle_ClearStatus)(B(::bof3::Img(rest_3b::at::kTarget.v)), 0x4000);
    if (B(::bof3::Img(rest_3b::at::kTarget.v)) <= 2) Battle_PartyStanding = static_cast<unsigned char>(Battle_PartyStanding + 1);
    else Battle_EnemiesLeft = static_cast<unsigned char>(Battle_EnemiesLeft + 1);
}

// original 0x44C330 (slot 18): half a hit, then 0x44FCA0(0x80).
extern "C" void __cdecl EffectSlot18_HalfHitInflict80(void) { HalfHitInflict(0x80); }

// original 0x44C380 (slot 19): 0x44FBB0(0).
extern "C" void __cdecl EffectSlot19_StatMod0(void) { StatMod(0); }

// original 0x44C390 (slot 20): the result's +8 = 2, the AP delta -20.
extern "C" void __cdecl EffectSlot20_ApHeal20(void) {
    ResultRest3b()[8] = 2;
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->ap_change, 0xFFEC);
}

// original 0x44C3B0 (slot 21): the result's +8 = 2, the AP delta -100.
extern "C" void __cdecl EffectSlot21_ApHeal100(void) {
    ResultRest3b()[8] = 2;
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->ap_change, 0xFF9C);
}

// original 0x44C3D0 (Effect_Handlers slot 22): the HP drain. 0x44F6A0(actor,
// target) resisting: the result's +8 = 1, nothing else. Else the target's HP
// / 5 as the HP delta, the acting sprite's +8 = 1, and the actor's HP raised
// by it (Drain above).
extern "C" void __cdecl Effect_DrainHp(void) {
    if (BH_AT(U2, battle_e5::at::kResisted.v)(B(::bof3::Img(battle_e5::at::kActor.v)), B(::bof3::Img(battle_e5::at::kTarget.v))) & 0xFF) {
        ResultBattleE5()[8] = 1;
        return;
    }
    DrainTarget(kDrainHp);
    Ptr(::bof3::Img(battle_e5::at::kActingSprite2.v))[8] = kDrainHp.mark;
    DrainActor(kDrainHp);
}

// original 0x44C5C0 (Effect_Handlers slot 23; 0x44EB50, slot 124, sets the
// acting kind 4 and the ability 0x4E and jumps here): the AP drain, as the
// HP drain on AP, with the result's +8 = 2 as well and the pop-up 0x453EB0.
extern "C" void __cdecl Effect_DrainAp(void) {
    if (BH_AT(U2, battle_e5::at::kResisted.v)(B(::bof3::Img(battle_e5::at::kActor.v)), B(::bof3::Img(battle_e5::at::kTarget.v))) & 0xFF) {
        ResultBattleE5()[8] = 2;
        return;
    }
    DrainTarget(kDrainAp);
    Ptr(::bof3::Img(battle_e5::at::kActingSprite2.v))[8] = kDrainAp.mark;
    ResultBattleE5()[8] = kDrainAp.mark;
    DrainActor(kDrainAp);
}

// original 0x44C7C0 (slot 24): 0x44FBB0(1).
extern "C" void __cdecl EffectSlot24_StatMod1(void) { StatMod(1); }

// original 0x44C7D0 (slot 25): 0x44FBB0(2).
extern "C" void __cdecl EffectSlot25_StatMod2(void) { StatMod(2); }

// original 0x44C7E0 (slot 26): 0x44FC60(0x40).
extern "C" void __cdecl EffectSlot26_Inflict40(void) { InflictMiss(0x40); }

// original 0x44C7F0 (slot 27): 0x44FC60(0x20).
extern "C" void __cdecl EffectSlot27_Inflict20(void) { InflictMiss(0x20); }

// original 0x44C800 (slot 28): a member target only. Its status word +0x90
// read; then Rand & 3 not 0 and the word's 0x4000: (the target read again)
// the HP delta minus a quarter of its maximum (a member's +0xA0, an enemy's
// +0xB0: negated, then shifted right 2, signed), 0x904AB1 / 0x904AB3 one up,
// and Battle_ClearStatus(target, 0x4000). Else the result's +8 = 1.
extern "C" void __cdecl EffectSlot28_RaiseQuarter(void) {
    if (B(::bof3::Img(rest_3b::at::kTarget.v)) > 2) return;
    const std::uint16_t status = Word(PartyRest3b(B(::bof3::Img(rest_3b::at::kTarget.v))) + 0x90);
    const U roll = static_cast<U>(BH_CALL(Rand)());
    if ((roll & 3) == 0 || (status & 0x4000) == 0) {
        ResultRest3b()[8] = 1;
        return;
    }
    const unsigned t = B(::bof3::Img(rest_3b::at::kTarget.v));
    if (t <= 2) {
        const S32Rest3b max = rv::As<records::PartyMember>(PartyRest3b(t))->max_hp;
        rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>((0 - max) >> 2));
        Battle_PartyStanding = static_cast<unsigned char>(Battle_PartyStanding + 1);
    } else {
        const S32Rest3b max = rv::As<records::BattleEnemy>(EnemyRest3b(t))->max_hp;
        rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>((0 - max) >> 2));
        Battle_EnemiesLeft = static_cast<unsigned char>(Battle_EnemiesLeft + 1);
    }
    BH_CALL(Battle_ClearStatus)(B(::bof3::Img(rest_3b::at::kTarget.v)), 0x4000);
}

// original 0x44C8E0 (slot 29): a member target only. Its +0x91 bit 0x40
// (status 0x4000): the HP delta -10000, 0x904AB1 one up (stored before the
// call), Battle_ClearStatus(target, 0x4000). Else the delta 0.
extern "C" void __cdecl EffectSlot29_RaiseFull(void) {
    if (B(::bof3::Img(rest_3b::at::kTarget.v)) > 2) return;
    if ((PartyRest3b(B(::bof3::Img(rest_3b::at::kTarget.v)))[0x91] & 0x40) == 0) {
        rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, 0);
        return;
    }
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, 0xD8F0);
    const unsigned char up = static_cast<unsigned char>(Battle_PartyStanding + 1);
    const unsigned t = B(::bof3::Img(rest_3b::at::kTarget.v));
    Battle_PartyStanding = up;
    BH_CALL(Battle_ClearStatus)(t, 0x4000);
}

// original 0x44C940 (slot 30): half a hit, then 0x44FCA0(8).
extern "C" void __cdecl EffectSlot30_HalfHitInflict8(void) { HalfHitInflict(8); }

// original 0x44C990 (Effect_Handlers slot 31): the HP delta half an attack -
// Battle_CalcDamage(actor, target, 0xFFFF), actor the byte kActing, target
// the byte kTarget, the weapon's element; its low word shifted right by one,
// arithmetically (sar ax, 1). The two bytes are pushed in registers whose
// upper bits are the entry's (mov al / mov cl); Battle_CalcDamage reads their
// low bytes only. eax is Battle_CalcDamage's with the low word halved; the
// result pointer is read after the call.
extern "C" unsigned long __cdecl Effect_HalfAttack(void) {
    const battle_odds::U target = AtBattleOdds(::bof3::Img(kTarget.v))[0];
    const battle_odds::U actor = AtBattleOdds(::bof3::Img(kActing.v))[0];
    const battle_odds::U amount = battle_odds::g.calc_damage(actor, target, 0xFFFF);
    const battle_odds::U half = static_cast<std::uint16_t>(static_cast<std::int16_t>(amount) >> 1);
    rv::Store<battle_odds::U>(rv::As<records::ActionRecord>(AtBattleOdds(BattleDamage_Result))->hp_change, half);
    return (amount & 0xFFFF0000u) | half;
}

// original 0x44C9C0 (slot 32): 0x904AA9 |= 0x20; the HP delta
// Battle_CalcDamage(actor, target, 0xFFFF) shifted right 1 (16 bits,
// signed), plus 1.
extern "C" void __cdecl EffectSlot32_HalfHitPlus1(void) {
    const unsigned target = B(::bof3::Img(rest_3b::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3b::at::kActor.v));
    B(::bof3::Img(rest_3b::at::kRoundFlags2.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3b::at::kRoundFlags2.v)) | 0x20);
    const short d = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>((d >> 1) + 1));
}

// original 0x44C9F0 (slot 33): 0x904AA9 |= 0x20; the power word = the
// actor's +0xA4 / +0xB4 plus a quarter of it (16 bits); the HP delta
// Battle_CalcDamage(actor, target, 0xFFFF). When that (read again) is not 0
// and 0x44F6A0(actor, target) answers al 0: Battle_ReturnQueuedItem(target),
// Battle_RemoveFromTurnOrder(target) (the target read again for each).
extern "C" void __cdecl EffectSlot33_HitDropTurn(void) {
    const unsigned actor = B(::bof3::Img(rest_3b::at::kActor.v));
    B(::bof3::Img(rest_3b::at::kRoundFlags2.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3b::at::kRoundFlags2.v)) | 0x20);
    const std::uint16_t base = ActorWord(L(::bof3::Img(rest_3b::at::kActor.v)) & 0xFF, 0xA4, 0xB4);
    SetWord(At(::bof3::Img(rest_3b::at::kPower.v)), static_cast<std::uint16_t>(base + (base >> 2)));
    const unsigned target = B(::bof3::Img(rest_3b::at::kTarget.v));
    const short d = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>(d));
    if (static_cast<std::uint16_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change) == 0) return;
    const unsigned t2 = B(::bof3::Img(rest_3b::at::kTarget.v));
    const unsigned a2 = B(::bof3::Img(rest_3b::at::kActor.v));
    if (BH_AT(U2, rest_3b::at::kResisted.v)(a2, t2) & 0xFF) return;
    BH_CALL(Battle_ReturnQueuedItem)(B(::bof3::Img(rest_3b::at::kTarget.v)));
    BH_CALL(Battle_RemoveFromTurnOrder)(B(::bof3::Img(rest_3b::at::kTarget.v)));
}

// original 0x44CAB0 (slot 34): the actor's status word (+0x90 / +0x92) bit 8
// kept and cleared; 0x904AA8 |= 0x80, 0x939FFC = 100; the HP delta
// Battle_CalcDamage(actor, target, 0xFFFF); then the kept bit put back into
// the status word of the actor read again.
extern "C" void __cdecl EffectSlot34_HitIgnore8(void) {
    const unsigned actor = B(::bof3::Img(rest_3b::at::kActor.v));
    unsigned char* const word = actor < 3 ? PartyRest3b(actor) + 0x90 : EnemyRest3b(actor) + 0x92;
    const std::uint16_t status = Word(word);
    const std::uint16_t kept = status & 8;
    SetWord(word, status & 0xFFF7);
    const unsigned target = B(::bof3::Img(rest_3b::at::kTarget.v));
    B(::bof3::Img(rest_3b::at::kRoundFlags.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3b::at::kRoundFlags.v)) | 0x80);
    BattleE4_HitEnemy = 0x64;
    const short d = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3b())->hp_change, static_cast<std::uint16_t>(d));
    const unsigned a2 = B(::bof3::Img(rest_3b::at::kActor.v));
    unsigned char* const back = a2 < 3 ? PartyRest3b(a2) + 0x90 : EnemyRest3b(a2) + 0x92;
    SetWord(back, Word(back) | kept);
}

// original 0x44CB90 (slot 35): StatSumHit with the addend 0x939FE6.
extern "C" void __cdecl EffectSlot35_StatSumHit(void) { StatSumHit(::bof3::Img(Rest3b_PowerAdd_addr)); }

// original 0x44CC60 (slot 36): Effect_SkillDamage(actor, target, 0x14, 0).
extern "C" void __cdecl EffectSlot36_Skill20(void) { SkillHit(0x14); }

// original 0x44CC90 (slot 37): 0x44FBB0(3).
extern "C" void __cdecl EffectSlot37_StatMod3(void) { StatMod(3); }

// original 0x44CCA0 (Effect_Handlers slot 38; PSX 0x8009BEE0): Rand & 0x7F below 0x20 (a
// quarter): the round flags |= 0x80 and the HP delta Battle_CalcDamage(actor,
// target, 0xFFFF). Else a miss: the result's +8 = 1, Battle_SetDamagePopup(0,
// target), then a tail jump to 0x44FB30.
extern "C" void __cdecl Effect_QuarterAttack(void) {
    if ((BH_CALL(Rand)() & 0x7F) < 0x20) {
        const unsigned target = B(::bof3::Img(battle_e5::at::kTarget.v));
        const unsigned actor = B(::bof3::Img(battle_e5::at::kActor.v));
        B(::bof3::Img(battle_e5::at::kFlags.v)) = static_cast<unsigned char>(B(::bof3::Img(battle_e5::at::kFlags.v)) | 0x80);
        const short delta = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
        rv::Store<unsigned>(rv::As<records::ActionRecord>(ResultBattleE5())->hp_change, static_cast<std::uint16_t>(delta));
        return;
    }
    ResultBattleE5()[8] = 1;
    BH_CALL(Battle_SetDamagePopup)(0, B(::bof3::Img(battle_e5::at::kTarget.v)));
    BH_AT(Void0, battle_e5::at::kMissTail.v)();
}

// original 0x44CD00 (slot 39): Battle_CalcDamage(actor, target, the ability's
// element word (NameTable_Abilities +4) & 0x1FF) as the HP delta.
extern "C" void __cdecl EffectSlot39_ElementHit(void) {
    const unsigned element = Word(AbilityRow(L(::bof3::Img(rest_3b::at::kAbility.v))) + 4) & 0x1FF;
    Hit(element);
}

// original 0x44CD40 (slot 40): the power word = the actor's +0xAA (a member)
// or +0xBA (an enemy); Battle_CalcDamage(actor, target, 0xFFFF).
extern "C" void __cdecl EffectSlot40_StatAAHit(void) {
    SetWord(At(::bof3::Img(rest_3b::at::kPower.v)), ActorWord(L(::bof3::Img(rest_3b::at::kActor.v)) & 0xFF, 0xAA, 0xBA));
    Hit(0xFFFF);
}

// original 0x44CDB0 (slot 41): StatSumHit with the addend 0x939FE8.
extern "C" void __cdecl EffectSlot41_StatSumHit2(void) { StatSumHit(::bof3::Img(Rest3b_PowerAdd2_addr)); }

// original 0x44CE80 (slot 42): the power word = the actor's +0xA4 / +0xB4
// doubled (16 bits); Battle_CalcDamage(actor, target, 0x20).
extern "C" void __cdecl EffectSlot42_DoubleHit20(void) {
    SetWord(At(::bof3::Img(rest_3b::at::kPower.v)), static_cast<std::uint16_t>(ActorWord(L(::bof3::Img(rest_3b::at::kActor.v)) & 0xFF, 0xA4, 0xB4) << 1));
    Hit(0x20);
}

// original 0x44CEF0 (slot 43): 0x44FC60(0x10).
extern "C" void __cdecl EffectSlot43_Inflict10(void) { InflictMiss(0x10); }

// original 0x44CF00 (slot 44): as ability 0x66, slot 4.
extern "C" void __cdecl EffectSlot44_Skill66(void) { AsAbility(0x66); }

// original 0x44CF20 (slot 45): as ability 0x65.
extern "C" void __cdecl EffectSlot45_Skill65(void) { AsAbility(0x65); }

// original 0x44CF40 (slot 46): as ability 0x62.
extern "C" void __cdecl EffectSlot46_Skill62(void) { AsAbility(0x62); }

// original 0x44CF60 (slot 47; R3C's 0x44D160 jumps here): not in the cut.
// 0x44FB30; then, by the ACTOR's side, the TARGET's second flags |= 0x200: a
// member actor the party record by the target byte (+0x134), an enemy actor
// the enemy object by it (+0x114) - unchecked, as the original (section 5).
extern "C" void __cdecl EffectSlot47_MissMark200(void) {
    MissTail();
    const unsigned actor = B(::bof3::Img(rest_3b::at::kActor.v));
    const unsigned t = L(::bof3::Img(rest_3b::at::kTarget.v)) & 0xFF;
    unsigned char* const flags = actor <= 2 ? PartyRest3b(t) + 0x134 : EnemyRest3b(t) + 0x114;
    SetLong(flags, static_cast<S32Rest3b>(static_cast<U>(Long(flags)) | 0x200));
}

// original 0x44CFC0 (slot 48): the acting kind 4, the ability 0x55, 0x44FBB0(0).
extern "C" void __cdecl EffectSlot48_StatMod0Skill55(void) {
    B(::bof3::Img(rest_3b::at::kActingKind.v)) = 4;
    SetWord(At(::bof3::Img(rest_3b::at::kAbility.v)), 0x55);
    StatMod(0);
}

// original 0x44CFE0 (slot 49): as ability 0x5D, slot 4.
extern "C" void __cdecl EffectSlot49_Skill5D(void) { AsAbility(0x5D); }

// original 0x44D000 (Effect_Handlers slot 50): kind 4, ability 0x60, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility60(void) { return AsAbilityRest3c(0x60, rest_3c::at::kSkillByAbility.v); }

// original 0x44D020 (slot 51): kind 4, ability 0x68, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility68(void) { return AsAbilityRest3c(0x68, rest_3c::at::kSkillByAbility.v); }

// original 0x44D040 (slot 52): kind 4, ability 0x5B, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility5B(void) { return AsAbilityRest3c(0x5B, rest_3c::at::kSkillByAbility.v); }

// original 0x44D060 (slot 53): kind 4, ability 0x52, then 0x44FBB0(1) - the
// target's stat byte 1 raised by the ability's power unless it resists; al.
extern "C" unsigned __cdecl Effect_AsAbility52RaiseStat1(void) {
    B(::bof3::Img(rest_3c::at::kActingKind.v)) = 4;
    SetW(::bof3::Img(rest_3c::at::kAbility.v), 0x52);
    return BH_AT(A1, rest_3c::at::kRaiseByAbility.v)(1);
}

// original 0x44D080 (slot 54): kind 4, ability 0x5A, then 0x44FBB0(1); al.
extern "C" unsigned __cdecl Effect_AsAbility5ARaiseStat1(void) {
    B(::bof3::Img(rest_3c::at::kActingKind.v)) = 4;
    SetW(::bof3::Img(rest_3c::at::kAbility.v), 0x5A);
    return BH_AT(A1, rest_3c::at::kRaiseByAbility.v)(1);
}

// original 0x44D0A0 (slot 55): kind 4, ability 0x5C, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility5C(void) { return AsAbilityRest3c(0x5C, rest_3c::at::kSkillByAbility.v); }

// original 0x44D0C0 (slot 56): kind 4, ability 0x64, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility64(void) { return AsAbilityRest3c(0x64, rest_3c::at::kSkillByAbility.v); }

// original 0x44D0E0 (slot 57): kind 4, ability 0x57, then 0x44FC60(0x10) -
// status 0x10 on the target unless it resists; al.
extern "C" unsigned __cdecl Effect_AsAbility57Inflict10(void) {
    B(::bof3::Img(rest_3c::at::kActingKind.v)) = 4;
    SetW(::bof3::Img(rest_3c::at::kAbility.v), 0x57);
    return BH_AT(A1, rest_3c::at::kMissInflict.v)(0x10);
}

// original 0x44D100 (slot 58): kind 4, ability 0x61, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility61(void) { return AsAbilityRest3c(0x61, rest_3c::at::kSkillByAbility.v); }

// original 0x44D120 (slot 59): kind 4, ability 0x46, jmp 0x44C120 (the heal).
extern "C" unsigned __cdecl Effect_AsAbility46(void) { return AsAbilityRest3c(0x46, rest_3c::at::kHealByAbility.v); }

// original 0x44D140 (slot 60): kind 4, ability 0x67, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility67(void) { return AsAbilityRest3c(0x67, rest_3c::at::kSkillByAbility.v); }

// original 0x44D160 (slot 61): kind 4, ability 0x56, jmp 0x44CF60 (flag 0x200).
extern "C" unsigned __cdecl Effect_AsAbility56(void) { return AsAbilityRest3c(0x56, rest_3c::at::kFlag200.v); }

// original 0x44D180 (slot 62): 0x904AA9 |= 0x20, 0x939F86 = 0, the HP delta
// Battle_CalcDamage(actor, target, 0xFFFF) doubled (16 bits), then 0x904660 = 5.
extern "C" void __cdecl Effect_DoubleDamage(void) {
    const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3c::at::kActor.v));
    B(::bof3::Img(rest_3c::at::kFlags2.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3c::at::kFlags2.v)) | 0x20);
    BattleDamage_TargetDef = 0;
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<unsigned>(static_cast<std::uint16_t>(damage)) << 1);
    Rest3c_Byte660 = 5;
}

// original 0x44D1C0 (slot 63): for kind 4 with ability 0x9A, 0x904AA9 |=
// 0x20; 0x939F86 = 0; the HP delta Battle_CalcDamage(actor, target, 0xFFFF)
// / 4 (signed, truncated).
extern "C" void __cdecl Effect_QuarterDamage(void) {
    if (B(::bof3::Img(rest_3c::at::kActingKind.v)) == 4 && W(::bof3::Img(rest_3c::at::kAbility.v)) == 0x9A) B(::bof3::Img(rest_3c::at::kFlags2.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3c::at::kFlags2.v)) | 0x20);
    const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3c::at::kActor.v));
    BattleDamage_TargetDef = 0;
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<unsigned>(S32Rest3c(damage) / 4));
}

// original 0x44D220 / 0x44D240 / 0x44D260 (slots 64, 65, 66): the HP delta
// 0x44FCE0(3 / 1 / 2) - the actor's HP over the divisor, varied; its eax.
extern "C" unsigned __cdecl Effect_ActorHpDamage3(void) {
    const U delta = BH_AT(A1, rest_3c::at::kHpDamage.v)(3);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, delta);
    return delta;
}

extern "C" unsigned __cdecl Effect_ActorHpDamage1(void) {
    const U delta = BH_AT(A1, rest_3c::at::kHpDamage.v)(1);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, delta);
    return delta;
}

extern "C" unsigned __cdecl Effect_ActorHpDamage2(void) {
    const U delta = BH_AT(A1, rest_3c::at::kHpDamage.v)(2);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, delta);
    return delta;
}

// original 0x44D280 / 0x44D330 / 0x44D340 (slots 67, 69, 70): 0x44FC60(4 /
// 8 / 0x80) - the common tail, then the status unless resisted; al.
extern "C" unsigned __cdecl Effect_InflictStatus4(void) { return BH_AT(A1, rest_3c::at::kMissInflict.v)(4); }

// original 0x44D290 (slot 68): the target's HP and status word (a member's
// +0x98 / +0x90, an enemy's +0xA4 / +0x92) read first; the HP delta
// Battle_CalcDamage(actor, the target's dword, 2). Then, the delta read again
// through the result cell: below that HP (signed against the word), the
// status's bit 6 clear and the delta not 0 - 0x44FCA0(0x40), status 0x40 on
// a target the hit leaves standing.
extern "C" void __cdecl Effect_DamageInflict40(void) {
    const U target = L(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned t = target & 0xFF;
    std::uint16_t hp, status;
    if (t < 3) {
        hp = rv::As<records::PartyMember>(PartyRest3c(t))->hp;
        status = rv::As<records::PartyMember>(PartyRest3c(t))->status;
    } else {
        hp = rv::As<records::BattleEnemy>(EnemyRest3c(t - 3))->hp;
        status = rv::As<records::BattleEnemy>(EnemyRest3c(t - 3))->status;
    }
    const short damage = BH_CALL(Battle_CalcDamage)(B(::bof3::Img(rest_3c::at::kActor.v)), target, 2);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<std::uint16_t>(damage));
    const std::uint16_t delta = static_cast<std::uint16_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change);
    if (S16Rest3c(delta) < static_cast<S32Rest3c>(hp) && (status & 0x40) == 0 && delta != 0) BH_AT(A1, rest_3c::at::kInflictUnlessResisted.v)(0x40);
}

extern "C" unsigned __cdecl Effect_InflictStatus8(void) { return BH_AT(A1, rest_3c::at::kMissInflict.v)(8); }

extern "C" unsigned __cdecl Effect_InflictStatus80(void) { return BH_AT(A1, rest_3c::at::kMissInflict.v)(0x80); }

// original 0x44D350 (slot 71): 0x44FB30, then 0x44FCA0(0x20); al.
extern "C" unsigned __cdecl Effect_InflictStatus20(void) {
    MissTailRest3c();
    return BH_AT(A1, rest_3c::at::kInflictUnlessResisted.v)(0x20);
}

// original 0x44D360 (slot 72): the result's flags |= 2, the HP delta -5, the AP delta -1.
extern "C" void __cdecl Effect_Heal5Ap1(void) {
    ResultRest3c()[8] = static_cast<unsigned char>(ResultRest3c()[8] | 2);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, 0xFFFB);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->ap_change, 0xFFFF);
}

// original 0x44D390 (slot 73): the attacker's power 0x939FE4 = the actor's
// +0xA4 (a member) / +0xB4 (an enemy), then four fifths of it (x 80 / 100,
// read back as a dword & 0xFFFF); the HP delta Battle_CalcDamage(the actor's
// dword, target, 0xFFFF).
extern "C" void __cdecl Effect_Attack80(void) {
    const U actor = L(::bof3::Img(rest_3c::at::kActor.v));
    SetW(::bof3::Img(rest_3c::at::kAttackerAtk.v), SideWord(actor & 0xFF, 0xA4, 0xB4));
    const U power = L(::bof3::Img(rest_3c::at::kAttackerAtk.v)) & 0xFFFF;
    const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
    SetW(::bof3::Img(rest_3c::at::kAttackerAtk.v), static_cast<U>(static_cast<S32Rest3c>(power * 80) / 100));
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<std::uint16_t>(damage));
}

// original 0x44D420 (slot 74): 0x904AA9 |= 0x40, the actor into 0x904B8A and
// 0x904B44 into 0x904B8B, jmp 0x44FB30.
extern "C" unsigned __cdecl Effect_FlagActorPair(void) {
    const unsigned actor = B(::bof3::Img(rest_3c::at::kActor.v));
    const unsigned other = B(::bof3::Img(rest_3c::at::kOtherActor.v));
    B(::bof3::Img(rest_3c::at::kFlags2.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3c::at::kFlags2.v)) | 0x40);
    BattleFlow_ActorAt = static_cast<unsigned char>(actor);
    BattlePhases_B8B = static_cast<unsigned char>(other);
    return MissTailRest3c();
}

// original 0x44D450 (slot 75): 0x44FB30; for a member target only,
// Char_RecalcStats(its character record by +0x148), then the party's stats
// derived again (RecalcParty above).
extern "C" void __cdecl Effect_TargetRecalcParty(void) {
    MissTailRest3c();
    const unsigned t = B(::bof3::Img(rest_3c::at::kTarget.v));
    if (t > 2) return;
    BH_CALL(Char_RecalcStats)(CharRecordRest3c(PartyRest3c(t)[0x148]));
    RecalcParty("Effect_TargetRecalcParty (0x44D450)");
}

// original 0x44D580 (slot 76): 0x44F1D0(target, 0x40); the result's flags
// |= 2, the HP delta -20, the AP delta -4.
extern "C" void __cdecl Effect_Inflict40Heal20Ap4(void) {
    BH_AT(A2, rest_3c::at::kInflictStatus.v)(B(::bof3::Img(rest_3c::at::kTarget.v)), 0x40);
    ResultRest3c()[8] = static_cast<unsigned char>(ResultRest3c()[8] | 2);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, 0xFFEC);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->ap_change, 0xFFFC);
}

// original 0x44D5C0 / 0x44D5D0 / 0x44D5E0 (slots 77, 78, 79) and 0x44D6C0
// (slot 85): the HP delta -1 / -80 / -240 / -5.
extern "C" void __cdecl Effect_Heal1(void) { rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, 0xFFFF); }

extern "C" void __cdecl Effect_Heal80(void) { rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, 0xFFB0); }

extern "C" void __cdecl Effect_Heal240(void) { rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, 0xFF10); }

// original 0x44D5F0 / 0x44D610 / 0x44DE00 (slots 80, 81, 102): the result's
// flags = 2, the AP delta -5 / -40 / -10.
extern "C" void __cdecl Effect_RestoreAp5(void) {
    ResultRest3c()[8] = 2;
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->ap_change, 0xFFFB);
}

extern "C" void __cdecl Effect_RestoreAp40(void) {
    ResultRest3c()[8] = 2;
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->ap_change, 0xFFD8);
}

// original 0x44D630 (slot 82): ability 0x66's power byte read, kind 4 and
// ability 0x66 set; the HP delta Effect_SkillDamage(actor, target, that power,
// 0), then halved (signed, truncated) through the result cell read again.
extern "C" void __cdecl Effect_AsAbility66Half(void) {
    const unsigned power = B(::bof3::Img(bof3::addr::NameTable_Abilities) + 0x66 * rest_3c::at::kAbilityStride + 3);
    const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3c::at::kActor.v));
    B(::bof3::Img(rest_3c::at::kActingKind.v)) = 4;
    SetW(::bof3::Img(rest_3c::at::kAbility.v), 0x66);
    const int damage = BH_CALL(Effect_SkillDamage)(actor, target, power, 0);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<unsigned>(damage));
    unsigned char* const r = ResultRest3c();
    rv::As<records::ActionRecord>(r)->hp_change = static_cast<unsigned>(S16Rest3c(static_cast<std::uint16_t>(rv::As<records::ActionRecord>(r)->hp_change)) / 2);
}

// original 0x44D680 (slot 83): kind 4, ability 0x63, jmp 0x44C040.
extern "C" unsigned __cdecl Effect_AsAbility63(void) { return AsAbilityRest3c(0x63, rest_3c::at::kSkillByAbility.v); }

// original 0x44D6A0 (slot 84): Battle_ClearStatus(target, 0xBFC), jmp 0x44C170
// (the HP delta the target's whole maximum, a heal).
extern "C" unsigned __cdecl Effect_CureBFCHealMax(void) {
    BH_CALL(Battle_ClearStatus)(B(::bof3::Img(rest_3c::at::kTarget.v)), 0xBFC);
    return BH_AT(A0, rest_3c::at::kHealMaxHp.v)();
}

extern "C" void __cdecl Effect_Heal5(void) { rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, 0xFFFB); }

// original 0x44D6D0 (slot 87): 0x44FB30; the enemy (target - 3) & 0xFF: its
// +0xAA up by 2 and +0xAE up by 1, each when not 0 and its marker bit 0
// (+0xAB, +0xAF) clear - the marker set; then each held to 7. There is no
// member branch: a member target (index 253..255) would write past the image
// (ours aborts, section 7).
extern "C" void __cdecl Effect_EnemyRaiseAAAE(void) {
    MissTailRest3c();
    const unsigned index = static_cast<unsigned char>(B(::bof3::Img(rest_3c::at::kTarget.v)) - 3);
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
    const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3c::at::kActor.v));
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<std::uint16_t>(damage));
    const std::uint16_t hp = SideWord(B(::bof3::Img(rest_3c::at::kTarget.v)), 0x98, 0xA4);
    unsigned char* const r = ResultRest3c();
    if (S16Rest3c(hp) > S16Rest3c(static_cast<std::uint16_t>(rv::As<records::ActionRecord>(r)->hp_change))) return;
    rv::As<records::ActionRecord>(r)->hp_change = hp - 1u;
}

// original 0x44D7E0 (slot 89): unless 0x44F6A0(actor, target) answers the
// target resisted, the HP delta is half the target's HP (unsigned).
extern "C" void __cdecl Effect_HalveTargetHp(void) {
    if (Resisted() & 0xFF) return;
    const std::uint16_t hp = SideWord(B(::bof3::Img(rest_3c::at::kTarget.v)), 0x98, 0xA4);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, hp >> 1);
}

// original 0x44D850 (slot 90): the target's +0x134 by ObjTrio's stride
// (whatever side: an enemy's index reads past the three members) with bit 0
// or 1: the result's flags = 1. Else a member's +0x143 = 0, and 0x44FCA0(0x800)
// answering not 0: flags = 1; else jmp 0x44FB30.
extern "C" void __cdecl Effect_Inflict800Unguarded(void) {
    const unsigned t = B(::bof3::Img(rest_3c::at::kTarget.v));
    if (PartyRest3c(t)[0x134] & 3) {
        ResultRest3c()[8] = 1;
        return;
    }
    if (t <= 2) PartyRest3c(t)[0x143] = 0;
    if (BH_AT(A1, rest_3c::at::kInflictUnlessResisted.v)(0x800) & 0xFF) {
        ResultRest3c()[8] = 1;
        return;
    }
    MissTailRest3c();
}

// original 0x44D8B0 (slot 91; not in the cut; R3D's 0x44EA70 also jumps
// here): resisted - the result's flags = 1; else the HP delta is the target's
// whole HP.
extern "C" void __cdecl Effect_DamageAllHp(void) {
    if (Resisted() & 0xFF) {
        ResultRest3c()[8] = 1;
        return;
    }
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, SideWord(B(::bof3::Img(rest_3c::at::kTarget.v)), 0x98, 0xA4));
}

// original 0x44D920 (slot 92): the attacker's power 0x939FE4 = the actor's
// +0xA4 / +0xB4, then by Rand & 3: 0 halved, 1 kept, 2 one and a half times
// (16 bits), 3 doubled (16 bits); the HP delta Battle_CalcDamage(actor,
// target, 0xFFFF).
extern "C" void __cdecl Effect_RandomPowerAttack(void) {
    SetW(::bof3::Img(rest_3c::at::kAttackerAtk.v), SideWord(B(::bof3::Img(rest_3c::at::kActor.v)), 0xA4, 0xB4));
    const U roll = static_cast<U>(BH_CALL(Rand)()) & 3;
    if (roll == 0) {
        SetW(::bof3::Img(rest_3c::at::kAttackerAtk.v), W(::bof3::Img(rest_3c::at::kAttackerAtk.v)) >> 1);
    } else if (roll == 2) {
        const unsigned power = W(::bof3::Img(rest_3c::at::kAttackerAtk.v));
        SetW(::bof3::Img(rest_3c::at::kAttackerAtk.v), power + (power >> 1));
    } else if (roll == 3) {
        SetW(::bof3::Img(rest_3c::at::kAttackerAtk.v), L(::bof3::Img(rest_3c::at::kAttackerAtk.v)) * 2);
    }
    const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3c::at::kActor.v));
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<std::uint16_t>(damage));
}

// original 0x44D9D0 (slot 93): resisted - the result's flags = 1 and
// Battle_SetDamagePopup(0, target); else the target's +0x134 (a member) /
// +0x114 (an enemy) |= 0x2000 and Battle_RecalcStats(the target's dword).
// Both end in jmp 0x44FB30.
extern "C" unsigned __cdecl Effect_TargetFlag2000(void) {
    const char* const who = "Effect_TargetFlag2000 (0x44D9D0)";
    if (Resisted() & 0xFF) {
        ResultRest3c()[8] = 1;
        BH_CALL(Battle_SetDamagePopup)(0, B(::bof3::Img(rest_3c::at::kTarget.v)));
        return MissTailRest3c();
    }
    const U target = L(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned t = target & 0xFF;
    if (t <= 2) {
        unsigned char* const p = PartyWrite(t, who);
        rv::As<records::PartyMember>(p)->flags_134 = static_cast<S32Rest3c>(static_cast<U>(static_cast<std::int32_t>(rv::As<records::PartyMember>(p)->flags_134)) | 0x2000);
    } else {
        unsigned char* const e = EnemyWrite(t - 3, who);
        rv::As<records::BattleEnemy>(e)->flags_114 = static_cast<S32Rest3c>(static_cast<U>(static_cast<std::int32_t>(rv::As<records::BattleEnemy>(e)->flags_114)) | 0x2000);
    }
    BH_CALL(Battle_RecalcStats)(target);
    return MissTailRest3c();
}

// original 0x44DA70 (slot 94): the actor's status word (+0x90 / +0x92) loses
// bit 3, kept aside; 0x939FFC = 100; the HP delta Battle_CalcDamage(the
// actor's dword, target, 0xFFFF) / 2 (signed, truncated); then the actor read
// again gets the bit back (an OR).
extern "C" void __cdecl Effect_SureHitHalfDamage(void) {
    const char* const who = "Effect_SureHitHalfDamage (0x44DA70)";
    const U actor = L(::bof3::Img(rest_3c::at::kActor.v));
    const unsigned a = actor & 0xFF;
    unsigned char* const status = a < 3 ? PartyWrite(a, who) + 0x90 : EnemyWrite(a - 3, who) + 0x92;
    const unsigned w = Word(status);
    const unsigned bit = w & 8;
    SetWord(status, w & 0xFFF7);
    const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
    BattleE4_HitEnemy = 0x64;
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<unsigned>(S32Rest3c(damage) / 2));
    const unsigned again = B(::bof3::Img(rest_3c::at::kActor.v));
    unsigned char* const back = again < 3 ? PartyWrite(again, who) + 0x90 : EnemyWrite(again - 3, who) + 0x92;
    SetWord(back, Word(back) | bit);
}

// original 0x44DB40 (slot 95): 0x44FB30; the enemy (target - 3)'s words +0x94
// and +0x96 doubled, each held to 0xFFFF. No member branch: a member target
// writes below the enemies (ours aborts, section 7).
extern "C" void __cdecl Effect_EnemyDouble94And96(void) {
    MissTailRest3c();
    unsigned char* const e = EnemyWrite(static_cast<U>(B(::bof3::Img(rest_3c::at::kTarget.v))) - 3, "Effect_EnemyDouble94And96 (0x44DB40)");
    const unsigned a = rv::As<records::BattleEnemy>(e)->zenny;
    const unsigned b = rv::As<records::BattleEnemy>(e)->exp;
    const unsigned a2 = a * 2 > 0xFFFF ? 0xFFFF : a * 2;
    const unsigned b2 = b * 2 > 0xFFFF ? 0xFFFF : b * 2;
    rv::As<records::BattleEnemy>(e)->zenny = a2;
    rv::As<records::BattleEnemy>(e)->exp = b2;
}

// original 0x44DBC0 (slot 96): the HP delta 0; resisted - the result's flags =
// 1; else 0x44FB30, Battle_ReturnQueuedItem(target), Battle_RemoveFromTurnOrder(target).
extern "C" void __cdecl Effect_DropFromTurnOrder(void) {
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, 0);
    if (Resisted() & 0xFF) {
        ResultRest3c()[8] = 1;
        return;
    }
    MissTailRest3c();
    BH_CALL(Battle_ReturnQueuedItem)(B(::bof3::Img(rest_3c::at::kTarget.v)));
    BH_CALL(Battle_RemoveFromTurnOrder)(B(::bof3::Img(rest_3c::at::kTarget.v)));
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
    MissTailRest3c();
    const unsigned t = B(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned first = BossSe_RoundSlot;
    BattleFlow_AnimGate = 4;
    unsigned char* const p = PartyWrite(t, who);
    rv::As<records::PartyMember>(p)->flags_134 = static_cast<S32Rest3c>(static_cast<U>(static_cast<std::int32_t>(rv::As<records::PartyMember>(p)->flags_134)) | 0x10);
    unsigned i = static_cast<unsigned char>(first - 1);
    if (i > BattleDamage_TurnCount) return;
    do {
        if (i > 11)
            bof3::Fatal("%s: turn order slot %u past the eleven and the byte after - the original reads and writes on "
                        "(docs/rest_3c.md section 7)",
                        who, i);
        if (B(::bof3::Img(rest_3c::at::kTurnOrder.v) + i) != 0xFF) {
            BH_CALL(Battle_ReturnQueuedItem)(i);
            B(::bof3::Img(rest_3c::at::kTurnOrder.v) + i) = 0xFF;
        }
        i = static_cast<unsigned char>(i + 1);
    } while (i <= BattleDamage_TurnCount);
}

// original 0x44DCA0 (slot 98): 0x44FB30; 0x44F650(10, 4) - the result
// record's byte +0x18 up by 10, held to -25..50; Battle_RecalcStats(target).
extern "C" void __cdecl Effect_RaiseStat4By10(void) {
    MissTailRest3c();
    BH_AT(A2, rest_3c::at::kRaiseStat.v)(0xA, 4);
    BH_CALL(Battle_RecalcStats)(B(::bof3::Img(rest_3c::at::kTarget.v)));
}

// original 0x44DCC0 (slot 99): 0x904AA9 |= 0x20; the HP delta the actor's HP
// - 1, and the actor's HP = 1 (the actor read again for the store).
extern "C" void __cdecl Effect_ActorHpToDamage(void) {
    const char* const who = "Effect_ActorHpToDamage (0x44DCC0)";
    const unsigned a = B(::bof3::Img(rest_3c::at::kActor.v));
    B(::bof3::Img(rest_3c::at::kFlags2.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3c::at::kFlags2.v)) | 0x20);
    if (a < 3) {
        rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, rv::As<records::PartyMember>(PartyRest3c(a))->hp - 1u);
        rv::As<records::PartyMember>(PartyWrite(B(::bof3::Img(rest_3c::at::kActor.v)), who))->hp = 1;
    } else {
        rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, rv::As<records::BattleEnemy>(EnemyRest3c(a - 3))->hp - 1u);
        rv::As<records::BattleEnemy>(EnemyWrite(static_cast<U>(B(::bof3::Img(rest_3c::at::kActor.v))) - 3, who))->hp = 1;
    }
}

// original 0x44DD60 (slot 100): unless resisted, the HP delta the target's
// HP - 1 (0 when the HP is 1): to one HP.
extern "C" void __cdecl Effect_DamageToOneHp(void) {
    if (Resisted() & 0xFF) return;
    const std::uint16_t hp = SideWord(B(::bof3::Img(rest_3c::at::kTarget.v)), 0x98, 0xA4);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, hp == 1 ? 0u : hp - 1u);
}

// original 0x44DDD0 (slot 101): the HP delta Effect_HealAmount(actor,
// target), then Battle_ClearStatus(target, 0xBFC).
extern "C" void __cdecl Effect_HealAndCureBFC(void) {
    const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3c::at::kActor.v));
    const int heal = BH_CALL(Effect_HealAmount)(actor, target);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<unsigned>(heal));
    BH_CALL(Battle_ClearStatus)(B(::bof3::Img(rest_3c::at::kTarget.v)), 0xBFC);
}

extern "C" void __cdecl Effect_RestoreAp10(void) {
    ResultRest3c()[8] = 2;
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->ap_change, 0xFFF6);
}

// original 0x44DE20 (slot 103): 0x904B96 hits (the count read again each
// pass): hit i adds Battle_CalcDamage(actor, target, 0xFFFF) x f / 10
// (signed, truncated), f the s8 0x64E944[i] for i below 7 and 3 after; the
// sum held to -9999..9999 is the HP delta.
extern "C" void __cdecl Effect_MultiHit(void) {
    S32Rest3c sum = 0;
    unsigned i = 0;
    if (Rest3c_Hits != 0) {
        do {
            const S32Rest3c scale = i < 7 ? static_cast<signed char>(B(::bof3::Img(rest_3c::at::kHitScale.v) + i)) : 3;
            const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
            const unsigned actor = B(::bof3::Img(rest_3c::at::kActor.v));
            const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
            sum += static_cast<S32Rest3c>(static_cast<U>(S32Rest3c(damage)) * static_cast<U>(scale)) / 10;
            i = static_cast<unsigned char>(i + 1);
        } while (i < Rest3c_Hits);
        if (sum > 9999) sum = 9999;
        else if (sum < -9999) sum = -9999;
    }
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<U>(sum));
}

// original 0x44DEF0 (slot 104): the HP delta Effect_SkillDamage(actor, target,
// the ability's power byte (+3 of its NameTable_Abilities row, the word
// 0x904B80 unchecked), 0) - 0x44C040's body - less the target's +0xA6 (a
// member) / +0xB6 (an enemy), held at 0 from below (signed).
extern "C" void __cdecl Effect_SkillDamageLessDef(void) {
    const U ability = L(::bof3::Img(rest_3c::at::kAbility.v)) & 0xFFFF;
    const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned power = B(::bof3::Img(bof3::addr::NameTable_Abilities) + ability * rest_3c::at::kAbilityStride + 3);
    const unsigned actor = B(::bof3::Img(rest_3c::at::kActor.v));
    const int damage = BH_CALL(Effect_SkillDamage)(actor, target, power, 0);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<unsigned>(damage));
    const unsigned t = B(::bof3::Img(rest_3c::at::kTarget.v));
    unsigned char* const r = ResultRest3c();
    const std::uint16_t def = t <= 2 ? rv::As<records::PartyMember>(PartyRest3c(t))->def : rv::As<records::BattleEnemy>(EnemyRest3c(t - 3))->def;
    rv::As<records::ActionRecord>(r)->hp_change = static_cast<std::uint16_t>(rv::As<records::ActionRecord>(r)->hp_change) - static_cast<unsigned>(def);
    unsigned char* const r2 = ResultRest3c();
    if (S16Rest3c(static_cast<std::uint16_t>(rv::As<records::ActionRecord>(r2)->hp_change)) < 0) rv::As<records::ActionRecord>(r2)->hp_change = 0;
}

// original 0x44DF80 (slot 105): the HP delta Battle_CalcDamage(actor, target,
// 0xFFFF) / 2 (signed, truncated); not 0: 0x44FCA0(0x20).
extern "C" void __cdecl Effect_HalfDamageInflict20(void) {
    const unsigned target = B(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned actor = B(::bof3::Img(rest_3c::at::kActor.v));
    const short damage = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, static_cast<unsigned>(S32Rest3c(damage) / 2));
    if (static_cast<std::uint16_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change) != 0) BH_AT(A1, rest_3c::at::kInflictUnlessResisted.v)(0x20);
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
    const unsigned a = B(::bof3::Img(rest_3c::at::kActor.v));
    if (a <= 2) {
        unsigned char* const m = PartyWrite(a, who);
        unsigned char* const record = CharRecordRest3c(m[0x148]);
        if (record[0x1E] < 5) {
            record[0x1E] = static_cast<unsigned char>(record[0x1E] + 1);
            m[0x9E] = static_cast<unsigned char>(m[0x9E] + 1);
        }
        BH_CALL(Char_RecalcStats)(CharRecordRest3c(m[0x148]));
        RecalcParty(who);
    } else {
        unsigned char* const e = EnemyRest3c(a - 3);
        const unsigned max = rv::As<records::BattleEnemy>(e)->max_hp;
        if (max != 0xFFFF) {
            const S32Rest3c tenth = static_cast<S32Rest3c>(Word(At(::bof3::Img(rest_3c::at::kEnemyData.v) + e[0xF0] * rest_3c::at::kEnemyDataStride + 0x24))) / 10;
            if (static_cast<S32Rest3c>(max) > tenth) {
                unsigned char* const w = EnemyWrite(a - 3, who);
                rv::As<records::BattleEnemy>(w)->max_hp = max - static_cast<unsigned>(tenth);
                const S32Rest3c down = static_cast<S32Rest3c>(Word(At(::bof3::Img(rest_3c::at::kEnemyData.v) + w[0xF0] * rest_3c::at::kEnemyDataStride + 0x24))) / -10;
                rv::As<records::BattleEnemy>(w)->base_max_hp = rv::As<records::BattleEnemy>(w)->base_max_hp + static_cast<unsigned>(down);
                const std::uint16_t new_max = rv::As<records::BattleEnemy>(w)->max_hp;
                if (rv::As<records::BattleEnemy>(w)->hp > new_max) rv::As<records::BattleEnemy>(w)->hp = new_max;
            }
        }
    }
    const U actor = L(::bof3::Img(rest_3c::at::kActor.v));
    if (BH_AT(A2, rest_3c::at::kResisted.v)(actor, B(::bof3::Img(rest_3c::at::kTarget.v))) & 0xFF) return;
    const unsigned t = B(::bof3::Img(rest_3c::at::kTarget.v));
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(ResultRest3c())->hp_change, SideWord(t, 0x98, 0xA4) - 1u);
}

// original 0x44E260 (slot 107): 0x44FB30; the actor's +0x130 (a member) /
// +0x110 (an enemy) |= 0x8000.
extern "C" void __cdecl Effect_ActorFlag8000(void) {
    const char* const who = "Effect_ActorFlag8000 (0x44E260)";
    MissTailRest3c();
    const unsigned a = B(::bof3::Img(rest_3c::at::kActor.v));
    unsigned char* const f = a <= 2 ? PartyWrite(a, who) + 0x130 : EnemyWrite(a - 3, who) + 0x110;
    SetLong(f, static_cast<S32Rest3c>(static_cast<U>(Long(f)) | 0x8000));
}

extern "C" void __cdecl Effect_ActorFlag40Count(void) { ActorFlagCount(0x40, 0x144, "Effect_ActorFlag40Count (0x44E2B0)"); }

extern "C" void __cdecl Effect_ActorFlag80Count(void) { ActorFlagCount(0x80, 0x145, "Effect_ActorFlag80Count (0x44E330)"); }

// original 0x44E3B0 (slot 110): 0x44FB30; the actor's +0x134 / +0x114 |= 0x100.
extern "C" void __cdecl Effect_ActorFlag100(void) {
    const char* const who = "Effect_ActorFlag100 (0x44E3B0)";
    MissTailRest3c();
    const unsigned a = B(::bof3::Img(rest_3c::at::kActor.v));
    unsigned char* const f = a <= 2 ? PartyWrite(a, who) + 0x134 : EnemyWrite(a - 3, who) + 0x114;
    SetLong(f, static_cast<S32Rest3c>(static_cast<U>(Long(f)) | 0x100));
}

// original 0x44E400 (slot 111): 0x44FB30; the target's (a member's +0x138,
// +0x13C / an enemy's +0x118, +0x11C) dwords zeroed, +0x130 / +0x110 &=
// 0xFFFE7FFF, +0x134 / +0x114 &= 0xFFFBC43F; Battle_RecalcStats(the target's dword).
extern "C" void __cdecl Effect_TargetClearBuffs(void) {
    const char* const who = "Effect_TargetClearBuffs (0x44E400)";
    MissTailRest3c();
    const U target = L(::bof3::Img(rest_3c::at::kTarget.v));
    const unsigned t = target & 0xFF;
    unsigned char* const r = t <= 2 ? PartyWrite(t, who) : EnemyWrite(t - 3, who);
    const U base = t <= 2 ? 0x130 : 0x110;
    SetLong(r + base + 8, 0);
    SetLong(r + base + 0xC, 0);
    SetLong(r + base, static_cast<S32Rest3c>(static_cast<U>(Long(r + base)) & 0xFFFE7FFFu));
    SetLong(r + base + 4, static_cast<S32Rest3c>(static_cast<U>(Long(r + base + 4)) & 0xFFFBC43Fu));
    BH_CALL(Battle_RecalcStats)(target);
}

// original 0x44E4B0 (slot 112): on an odd Rand, the target not resisting
// (Battle_StatusResisted): the HP delta its HP less 1.
extern "C" void __cdecl Effect112_HpToOne(void) {
    if ((BH_CALL(Rand)() & 1) == 0) return;
    if (BH_CALL(Battle_StatusResisted)(ActorRest3d(), Target())) return;
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(Result())->hp_change, Word(Hp(Target())) - 1u);
}

// original 0x44E530 (slot 113): Effect_NoHitReaction, then the actor's flag
// 0x10000 (the flag Battle_CalcDamage and Effect_SkillDamage answer 0 for).
extern "C" void __cdecl Effect113_ActorNullDamage(void) {
    BH_CALL(Effect_NoHitReaction)();
    OrLongRest3d(Flags(ActorRest3d()), 0x10000);
}

// original 0x44E580 (slot 114): the result's mark +8 = 2, then the AP delta
// Effect_SkillDamage(actor, target, the ability's +3 byte, 1) (the psi
// branch), its ax into the result read again.
extern "C" void __cdecl Effect114_SkillApDamage(void) {
    Result()[8] = 2;
    const U power = AbilityStep();
    const int v = BH_CALL(Effect_SkillDamage)(ActorRest3d(), Target(), power, 1);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(Result())->ap_change, static_cast<U>(v));
}

// original 0x44E5D0 (slot 115): the target not resisting Battle_StatusResisted20:
// the HP delta its whole HP.
extern "C" void __cdecl Effect115_HpToZero(void) {
    if (BH_CALL(Battle_StatusResisted20)(ActorRest3d(), Target())) return;
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(Result())->hp_change, Word(Hp(Target())));
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
    const unsigned n = PartySizeForWrite("Effect117_PartyFlag400Others");
    for (unsigned m = 0; m < n; ++m) OrLongRest3d(PartyRest3d(m) + 0x134, 0x400);
    AndLong(PartyRest3d(t) + 0x134, 0xFFFFFBFFu);
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
        unsigned char* const p = PartyRest3d(t);
        unsigned char* const c = At(::bof3::Img(CharacterRecords_addr) + p[0x148] * rest_3d::at::kCharacterStride);
        if (c[0x1E] < 9) {
            c[0x1E] = static_cast<unsigned char>(c[0x1E] + 1);
            p[0x9E] = static_cast<unsigned char>(p[0x9E] + 1);
        }
        BH_CALL(Char_RecalcStats)(At(::bof3::Img(CharacterRecords_addr) + p[0x148] * rest_3d::at::kCharacterStride));
    }
    unsigned n = PartySizeForWrite("Effect118_RaiseCharByte1E");
    for (unsigned m = 0; m < n; ++m) {
        unsigned char* const p = PartyRest3d(m);
        const unsigned char* const c = At(::bof3::Img(CharacterRecords_addr) + p[0x148] * rest_3d::at::kCharacterStride);
        for (unsigned i = 0; i < 6; ++i) p[0x92 + i] = c[0x12 + i];
        for (unsigned i = 0; i < 0x20; ++i) p[0xC0 + i] = c[0x20 + i];
    }
    BH_CALL(Formation_ApplyStatMods)();
    n = PartySizeForWrite("Effect118_RaiseCharByte1E");
    for (unsigned m = 0; m < n; ++m) {
        unsigned char* const p = PartyRest3d(m);
        for (unsigned i = 0; i < 0x20; ++i) p[0xA0 + i] = p[0xC0 + i];
    }
    BH_CALL(BattleForm_ApplyStats)();
    if (B(::bof3::Img(rest_3d::at::kPartySize.v)) != 0) {
        unsigned m = 0;
        do {
            BH_CALL(Battle_RecalcStats)(m);
            m = (m + 1) & 0xFF;
        } while (m < B(::bof3::Img(rest_3d::at::kPartySize.v)));
    }
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(Result())->hp_change, 0u - rv::As<records::PartyMember>(PartyRest3d(Target()))->max_hp);
}

// original 0x44E8B0 (slot 119): Effect_NoHitReaction; the actor's +0x134 (an
// enemy's +0x114) bit 12; Battle_RecalcStats(the target).
extern "C" void __cdecl Effect119_ActorFlag1000(void) {
    BH_CALL(Effect_NoHitReaction)();
    const unsigned t = Target();
    OrLongRest3d(Flags2(ActorRest3d()), 0x1000);
    BH_CALL(Battle_RecalcStats)(t);
}

// original 0x44E920 (slot 120): Battle_StatusResisted: the mark +8 = 1 and
// Battle_SetDamagePopup(0, the target); else the target's +0x134 (+0x114)
// bit 11 and Battle_RecalcStats(the target's dword). Either way a tail jump
// to Effect_NoHitReaction.
extern "C" void __cdecl Effect120_TargetFlag800(void) {
    if (BH_CALL(Battle_StatusResisted)(ActorRest3d(), Target())) {
        Result()[8] = 1;
        BH_CALL(Battle_SetDamagePopup)(0, Target());
    } else {
        const U t = L(::bof3::Img(rest_3d::at::kTarget.v));
        OrLongRest3d(Flags2(t), 0x800);
        BH_CALL(Battle_RecalcStats)(t);
    }
    BH_CALL(Effect_NoHitReaction)();
}

// original 0x44E9C0 (slot 121): Effect_NoHitReaction; the actor's +0x134
// (+0x114) bit 14 and its byte +0x142 (+0x122) zeroed; Battle_RecalcStats(the
// target).
extern "C" void __cdecl Effect121_ActorFlag4000(void) {
    BH_CALL(Effect_NoHitReaction)();
    const unsigned a = ActorRest3d();
    const unsigned t = Target();
    OrLongRest3d(Flags2(a), 0x4000);
    if (IsMember(a)) PartyRest3d(a)[0x142] = 0;
    else EnemyRest3d(a)[0x122] = 0;
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
    B(::bof3::Img(rest_3d::at::kActingKind.v)) = 4;
    SetWord(At(::bof3::Img(rest_3d::at::kAbility.v)), 0x6A);
    BH_AT(Void0, rest_3d::at::kAbility6AHelper.v)();
}

// original 0x44EA90 (slot 124): HpLessDefence(2).
extern "C" void __cdecl Effect124_HalfHpLessDefence(void) { HpLessDefence(2); }

// original 0x44EB10 (slot 125): Effect_NoHitReaction; the target not resisting
// Battle_StatusResistedMask(actor, target, 0x80): Battle_InflictStatus(the
// target, 8).
extern "C" void __cdecl Effect125_Inflict8Roll80(void) {
    BH_CALL(Effect_NoHitReaction)();
    if (BH_CALL(Battle_StatusResistedMask)(ActorRest3d(), Target(), 0x80)) return;
    BH_CALL(Battle_InflictStatus)(Target(), 8);
}

// original 0x44EB50 (slot 126; group BE5's doc called it slot 124): the acting
// kind 4 and the ability 0x4E, then a tail jump to Effect_DrainAp.
extern "C" void __cdecl Effect126_Ability4EDrainAp(void) {
    B(::bof3::Img(rest_3d::at::kActingKind.v)) = 4;
    SetWord(At(::bof3::Img(rest_3d::at::kAbility.v)), 0x4E);
    BH_CALL(Effect_DrainAp)();
}

// original 0x44EB70 (slot 127): the HP delta Battle_CalcDamage(actor, target,
// 0xFFFF) (the target read before the call); an enemy target (that read) whose
// object's +0x8D is 4 (the target read again) doubled (a 16-bit shift).
extern "C" void __cdecl Effect127_AttackDoubledKind4(void) {
    const unsigned t = Target();
    const short v = BH_CALL(Battle_CalcDamage)(ActorRest3d(), t, 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(Result())->hp_change, static_cast<std::uint16_t>(v));
    if (IsMember(t)) return;
    if (EnemyRest3d(Target())[0x8D] == 4) rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(Result())->hp_change, static_cast<U>(static_cast<std::uint16_t>(rv::As<records::ActionRecord>(Result())->hp_change)) << 1);
}

// original 0x44EBE0 (slot 128): the HP delta Battle_CalcDamage(actor, target,
// 0xFFFF) / 2 (signed, toward 0); when it is not 0 and Battle_StatusResisted
// says not resisted, Effect_RollInflict(4) (a second roll).
extern "C" void __cdecl Effect128_HalfAttackInflict4(void) {
    const short v = BH_CALL(Battle_CalcDamage)(ActorRest3d(), Target(), 0xFFFF);
    rv::Store<std::uint32_t>(rv::As<records::ActionRecord>(Result())->hp_change, static_cast<U>(static_cast<S32Rest3d>(v) / 2));
    if (static_cast<std::uint16_t>(rv::As<records::ActionRecord>(Result())->hp_change) == 0) return;
    if (BH_CALL(Battle_StatusResisted)(ActorRest3d(), Target())) return;
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
        AndLong(PartyRest3d(Target()) + 0x134, 0xFFFBFFFFu);
    }
    unsigned t = 0;
    for (unsigned m = 3; m <= 10; ++m) {
        const unsigned char out = BH_CALL(Battle_ActorIsOut)(m);
        t = Target();
        if (!out) AndLong(EnemyRest3d(t) + 0x114, 0xFFFBFFFFu);
    }
    OrLongRest3d(Flags2(t), 0x40000);
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
    const std::uint32_t scaled = (BattleDamage_AttackerInt + 100u) * (power & 0xFFFF);
    std::uint32_t v = scaled * 100u / 100u;
    std::int32_t resist = 100 - static_cast<std::int32_t>(BattleDamage_TargetInt / 5u);
    if (resist < 50) resist = 50;
    v = U32(resist) * v / 100u;
    const unsigned mask =
        Word(At(::bof3::Img(bof3::addr::NameTable_Abilities) + (U32(Long(At(::bof3::Img(battle_damage::at::kAbility.v)))) & 0xFFFF) * battle_damage::at::kAbilityStride + 4)) & 0x1FF;
    if (mask != 0) {
        const short affinity = static_cast<short>((psi & 0xFF) ? battle_damage::g.psi_affinity(target, mask) : battle_damage::g.element_affinity(target, mask));
        v = U32(Mul(affinity, S32(v)) / 100);
    }
    const unsigned roll = U32(battle_damage::g.rand()) & 7;
    std::int32_t amount = Mul(Word(At(::bof3::Img(battle_damage::at::kPowerVariance.v) + 2 * roll)), S32(v)) / 10000;
    if (Byte(::bof3::Img(battle_damage::at::kTarget.v)) <= 2 && Cond_Flags[0xd0] == 2 && amount > 0) amount -= amount >> 2;
    if ((target & 0xFF) <= 2) {
        auto* const pr = rv::As<records::PartyMember>(Party(target) - 0x10);
        if (static_cast<std::int32_t>(pr->flags_134) & 0x200) amount /= 2;
        if (static_cast<std::int32_t>(pr->flags_130) & 0x10000) return 0;
    } else {
        auto* const er = rv::As<records::BattleEnemy>(Enemy(target) + battle_damage::at::kEObject);
        if (static_cast<std::int32_t>(er->flags_114) & 0x200) amount /= 2;
        if (static_cast<std::int32_t>(er->flags_110) & 0x10000) return 0;
    }
    return amount;
}

// original 0x44EE80 (PSX Battle_ElementAffinity 0x8009FA78): for each of bits
// 0..4 of the mask's low byte (fire, ice, lightning, earth, wind), the s16
// 0x64E95C[the target's class byte for it] added up - the party's five at
// +0x9F, an enemy's at +0x3F. Summed in 32 bits (the PSX in 16; every caller
// takes the low word).
extern "C" int __cdecl Battle_ElementAffinity(unsigned target, unsigned mask) {
    const unsigned char* const classes =
        (target & 0xFF) <= 2 ? Party(target) + battle_damage::at::kPElements : Enemy(target) + battle_damage::at::kEElements;
    std::int32_t sum = 0;
    for (unsigned k = 0; k < 5; ++k)
        if (mask & (1u << k)) sum += S16(Word(At(::bof3::Img(battle_damage::at::kElementTable.v) + 2 * classes[k])));
    return sum;
}

// original 0x44F030 (PSX 0x8009FE88, section 2): Effect_SkillDamage's affinity when its
// psi word's low byte is set - the s16 of 0x64E97C by the target's class byte
// for the last of the mask's bits 0x40 / 0x80 / 0x100 set (+0xB5..+0xB7 of a
// member, +0xC5..+0xC7 of an enemy), 0 when none is.
extern "C" int __cdecl Battle_PsiStatusDeathAffinity(unsigned target, unsigned mask) {
    return ClassLookup(::bof3::Img(Battle_PsiAffinityRates_addr), target, mask);
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
    const std::uint32_t id = U32(Long(At(::bof3::Img(battle_damage::at::kAbility.v)))) & 0xFFFF;
    const std::uint32_t power = Byte(::bof3::Img(bof3::addr::NameTable_Abilities) + id * battle_damage::at::kAbilityStride + 3);
    const std::uint32_t base = power * (BattleDamage_AttackerInt + 100u) * 100u / 100u;
    const unsigned holy = (target & 0xFF) <= 2 ? rv::As<records::PartyMember>(Party(target) - 0x10)->holy : rv::As<records::BattleEnemy>(Enemy(target) + battle_damage::at::kEObject)->holy;
    return Mul(S16(Word(At(::bof3::Img(battle_damage::at::kHealTable.v) + 2 * holy))), S32(base)) / -10000;
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
    const U offset = member ? (target & 0xFF) * rest_3d::at::kPartyStride : ((target & 0xFF) - 3u) * rest_3d::at::kEnemyStride;
    const U record = (member ? ::bof3::Img(ObjTrio_addr) : ::bof3::Img(Battle_Enemies_addr)) + offset;
    const U word_at = record + (member ? 0x90u : 0x92u);
    const U cur = W(word_at);
    const U s = status & 0xFF;
    auto mark = [](unsigned char bit) { B(::bof3::Img(rest_3d::at::kNewStatus.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3d::at::kNewStatus.v)) | bit); };
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
            B(::bof3::Img(rest_3d::at::kActorTaskByte.v) + (target & 0xFF) * rest_3d::at::kActorTaskStride) = 0;
            if (member) {
                unsigned char* const p = PartyRest3d(target);
                p[0x125] = 0;
                AndLong(p + 0x130, 0xFFFFFFFDu);
                AndLong(p + 0x134, 0xFFFFFFFBu);
            } else {
                unsigned char* const e = EnemyRest3d(target);
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
        B(::bof3::Img(rest_3d::at::kNewStatusHi.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3d::at::kNewStatusHi.v)) | 8);
        BH_CALL(Battle_ReturnQueuedItem)(target);
        BH_CALL(Battle_RemoveFromTurnOrder)(target);
    }
    BH_CALL(Sprite_ReleaseTint)(At(record));
    SetWord(At(word_at), W(word_at) | W(::bof3::Img(rest_3d::at::kNewStatus.v)));
    BH_CALL(Battle_StatusTint)((offset & 0xFFFF0000u) | W(word_at));
}

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

// original 0x44F4B0 (PSX 0x800A0680, the same): clears the actor's status
// bits (party +0x90, enemy +0x92) that the mask names - 0x4000, 0x800, 0x80,
// 0x40, 0x10, 8, 4, 2, 1 each clear their own bit - and, when the mask has
// 0x20 and the status has it, bit 0x20 too with the 12 bytes at +0x34 put
// back from 0x93A034 + actor * 0x84. Then Sprite_ReleaseTint(the actor
// object), the status stored, and 0x446BB0(status) with Sprite_Current the
// actor object for the call. Answers the status word (the original's ax; its
// upper half is 0x446BB0's leftovers, which none of the 23 callers reads -
// most are effect handlers whose answer Effect_ApplyResult drops).
extern "C" unsigned __cdecl Battle_ClearStatus(unsigned actor, unsigned mask) {
    const unsigned a = actor & 0xFF;
    const bool party = a <= 2;
    unsigned char* const o = party ? P(a) : E(a);
    unsigned char* const status = o + (party ? battle_setup::at::kPStatus : battle_setup::at::kEStatus);
    unsigned s = Word(status) & ~(mask & 0x48DFu);
    if ((mask & 0x20) && (s & 0x20)) {
        s &= ~0x20u;
        std::memcpy(o + battle_setup::at::kPos, At(::bof3::Img(BattleTask_Slots_addr) + 0x34 + a * battle_setup::at::kSavedPosStride), 12);
    }
    battle_setup::g.release_tint(o);
    SetWord(status, s);
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = o;
    battle_setup::g.status_tint(s);
    Sprite_Current = saved;
    return s & 0xFFFF;
}

// original 0x44F650: the result record's s8 stat step +0x14 + (stat & 0xFF)
// moved by the signed word `step`: above 50 made 50, below -25 made -25, else
// the byte plus the step's byte.
extern "C" void __cdecl Effect_StepStatByte(unsigned step, unsigned stat) {
    unsigned char* const p = At(BattleDamage_Result + (stat & 0xFF) + 0x14);
    const S32Rest3d v = static_cast<signed char>(*p) + S16Rest3d(step);
    if (v > 50) *p = 50;
    else if (v < -25) *p = 0xE7;
    else *p = static_cast<unsigned char>(*p + step);
}

// original 0x44F6A0: the status roll for the acting ability: Roll with the
// rate Battle_StatusResistRate(target, the ability's mask). `actor` is not
// read. al 1: resisted.
extern "C" unsigned char __cdecl Battle_StatusResisted(unsigned actor, unsigned target) {
    (void)actor;
    const S32Rest3d a = RollAttack();
    const S32Rest3d d = RollDefence();
    const U rate = static_cast<U>(BH_CALL(Battle_StatusResistRate)(target, AbilityMask()));
    return Roll(a, d, rate);
}

void Effect_Inject() {
    BOF3_INJECT(EnemyAI_TurnCheck);
    BOF3_INJECT(EnemyAI_ChooseActions);
    BOF3_INJECT(EnemyAI_OtherRowsDone);
    BOF3_INJECT(EnemyAI_RowDone);
    BOF3_INJECT(EnemyAI_SetRowDone);
    BOF3_INJECT(EnemyAI_CondElement);
    BOF3_INJECT(EnemyAI_ApplyAction);
    BOF3_INJECT(EnemyAI_ScaleStat);
    BOF3_INJECT(EnemyAI_SetAttrByte);
    BOF3_INJECT(EnemyAI_DedupMessages);
    BOF3_INJECT(Effect_ApplyResult);
    BOF3_INJECT(EffectSlot00_Miss);
    BOF3_INJECT(EffectSlot01_VariedHit);
    BOF3_INJECT(EffectSlot02_HitInflict4);
    BOF3_INJECT(EffectSlot03_HalfHitInflict20);
    BOF3_INJECT(EffectSlot04_SkillPower);
    BOF3_INJECT(EffectSlot05_Clear80);
    BOF3_INJECT(EffectSlot06_HpThirdHit);
    BOF3_INJECT(EffectSlot07_Heal);
    BOF3_INJECT(Effect_Heal20);
    BOF3_INJECT(EffectSlot09_Heal40);
    BOF3_INJECT(EffectSlot10_Heal100);
    BOF3_INJECT(EffectSlot11_HealFull);
    BOF3_INJECT(EffectSlot12_Heal5Clear68);
    BOF3_INJECT(EffectSlot13_Clear80);
    BOF3_INJECT(EffectSlot14_Clear8);
    BOF3_INJECT(EffectSlot15_Clear100);
    BOF3_INJECT(EffectSlot16_ClearBFC);
    BOF3_INJECT(EffectSlot17_RaiseDown);
    BOF3_INJECT(EffectSlot18_HalfHitInflict80);
    BOF3_INJECT(EffectSlot19_StatMod0);
    BOF3_INJECT(EffectSlot20_ApHeal20);
    BOF3_INJECT(EffectSlot21_ApHeal100);
    BOF3_INJECT(Effect_DrainHp);
    BOF3_INJECT(Effect_DrainAp);
    BOF3_INJECT(EffectSlot24_StatMod1);
    BOF3_INJECT(EffectSlot25_StatMod2);
    BOF3_INJECT(EffectSlot26_Inflict40);
    BOF3_INJECT(EffectSlot27_Inflict20);
    BOF3_INJECT(EffectSlot28_RaiseQuarter);
    BOF3_INJECT(EffectSlot29_RaiseFull);
    BOF3_INJECT(EffectSlot30_HalfHitInflict8);
    BOF3_INJECT(Effect_HalfAttack);
    BOF3_INJECT(EffectSlot32_HalfHitPlus1);
    BOF3_INJECT(EffectSlot33_HitDropTurn);
    BOF3_INJECT(EffectSlot34_HitIgnore8);
    BOF3_INJECT(EffectSlot35_StatSumHit);
    BOF3_INJECT(EffectSlot36_Skill20);
    BOF3_INJECT(EffectSlot37_StatMod3);
    BOF3_INJECT(Effect_QuarterAttack);
    BOF3_INJECT(EffectSlot39_ElementHit);
    BOF3_INJECT(EffectSlot40_StatAAHit);
    BOF3_INJECT(EffectSlot41_StatSumHit2);
    BOF3_INJECT(EffectSlot42_DoubleHit20);
    BOF3_INJECT(EffectSlot43_Inflict10);
    BOF3_INJECT(EffectSlot44_Skill66);
    BOF3_INJECT(EffectSlot45_Skill65);
    BOF3_INJECT(EffectSlot46_Skill62);
    BOF3_INJECT(EffectSlot47_MissMark200);
    BOF3_INJECT(EffectSlot48_StatMod0Skill55);
    BOF3_INJECT(EffectSlot49_Skill5D);
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
    BOF3_INJECT(Effect_SkillDamage);
    BOF3_INJECT(Battle_ElementAffinity);
    BOF3_INJECT(Battle_PsiStatusDeathAffinity);
    BOF3_INJECT(Effect_HealAmount);
    BOF3_INJECT(Battle_InflictStatus);
    BOF3_INJECT(Battle_LacksAccessory);
    BOF3_INJECT(Battle_LacksArmour);
    BOF3_INJECT(Battle_ClearStatus);
    BOF3_INJECT(Effect_StepStatByte);
    BOF3_INJECT(Battle_StatusResisted);
}
