// Group BG of the seventh round (docs/battle_sprites.md): 32 functions of the
// battle engine and two small helpers, each faithful to Capcom's PC code but
// one: since DIV-0185 Battle_SetHitPopup reads a null target block as bit 1
// clear instead of faulting.
//
//   0x452BF0 Battle_RollPendingFlag     0x453FA0 Battle_MemberAutoTarget
//   0x452F70 Battle_SetTargetFlags      0x454220 Battle_MemberOutAction
//   0x4530D0 Battle_SetTargetFlag40     0x454380 Battle_PlayHitSound
//   0x453190 Battle_AnyFlagF0           0x454410 Battle_SetHitPopup
//   0x453210 Battle_ActionSuitsTarget   0x454CC0 Sprite_SetTint
//   0x4532A0 Battle_ItemSuitsTarget     0x454D60 Tint_Release
//   0x4537A0 Battle_PickFlag8Member     0x454DF0 ClutMap_Mark
//   0x453A10 Battle_MemberCoinFlip      0x454F30 ClutMap_FindFree
//   0x453B10 Battle_SettleFlag8         0x455140 ClutMap_FindOwner
//   0x453C00 Formation_ApplyStatMods    0x4551A0 Sprite_SetClutStp
//   0x453DA0 Battle_SetDamagePopup
//   0x494280 Battle_InitEncounterKind   0x494A80 Battle_OpenEnemyNames
//   0x4942A0 Battle_InitBossEncounter   0x494EA0 Battle_EnemyKindFlag
//   0x4942C0 Battle_InitEnemies         0x494ED0 Battle_SetEnemyKindFlag
//   0x494320 Battle_SetupEnemy          0x494F00 Battle_SetEnemyOffset
//   0x4946C0 Battle_CopyEnemyData
//   0x588F00 Sprite_UpdateScreenSlot    0x587900 Sound_PlayById
//
// Actors: 0..2 are the party records at ObjTrio (0x802D40, 0x14C bytes each),
// 3..10 the enemy records at 0x93B960 (0x128 bytes each, index actor - 3);
// an enemy record's first 0x80 bytes are its sprite object, the battle data
// (docs/kinship-probe-battle-engine.md's EnemyWorkingRecords) starts at +0x80.
// The party's flags dword is +0x130, the enemies' +0x110.
//
// Every index is unchecked, as the originals have it; every call out goes
// through battle_sprites::g (battle_sprites_callees.h).
#include "game/battle_sprites.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_sprites_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sound_sp.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

// The group's functions, declared here for the pointers below; symbols.toml
// binds each name to this file (bof3/symbols.gen.h).
namespace battle_sprites {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

template <class F> F Raw(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }

const Callees kOriginals = {
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_ActorIsOut)),
    Rand,
    Raw<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_SetActorBit)),
    Raw<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::AbilityList_Add)),
    Raw<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Item_UseFlags)),   // group TWO's (2026-10-06): by address, the value unchanged
    Raw<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::BattleTask_Create)),
    Stat_AddClamped,
    Stat_AddCap100,
    Raw<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Window_Alloc)),
    Sound_PlayEffect,
    AreaMap_Elevation,
    Sprite_SetAnimationBank,
    Sprite_UpdateScreen,
    // rebound 2026-09-29 (round twelve, BE6): the values unchanged, the fuzz keys on them
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberRollByAction)),
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_ActionBitSet)),
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberListFull)),
    Battle_MemberCoinFlip,
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_AutoTargetCheck)),   // R4A's (round 14): by address, the value unchanged
    Raw<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberAutoFixed)),            // R4A's
    Raw<void (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_PickEnemyTarget)),
    Battle_MemberOutAction,
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberActionIs0E)),   // R4A's
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_RandomOtherMember)),  // R4A's
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_RandomEnemy)),
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_DefaultTarget)),
    ClutMap_Mark,
    ClutMap_FindFree,
    ClutMap_FindOwner,
    Raw<void (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_PlaceBossActors)),   // R3G's: by address, as the other groups' above
    Battle_InitBossEncounter,
    Battle_InitEnemies,
    Battle_SetupEnemy,
    Battle_CopyEnemyData,
    Battle_SetEnemyOffset,
    Battle_EnemyKindFlag,
};
Callees g = kOriginals;

namespace {

// Actor records, indexed in 32 bits as the originals' lea chains do.
unsigned char* Party(std::uint32_t i) { return At(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }
unsigned char* Enemy(std::uint32_t i) { return At(::bof3::Img(Battle_Enemies_addr) + i * battle_sprites::at::kEnemyStride); }
unsigned char* Ptr(const unsigned char* at) { return At(static_cast<std::uint32_t>(Long(at))); }
std::int32_t Address(const void* p) { return static_cast<std::int32_t>(Low32(p)); }
unsigned char* ActionRecord(std::uint32_t action) { return At(::bof3::Img(at::kActions.v) + action * at::kActionStride); }
// 0x904B40 holds the acting block's +0xC: the actor's action record (a
// member's +0x124, an enemy's +0x104; records.h ActionRecord).
std::uint16_t CurrentAction() { return rv::As<records::ActionRecord>(Ptr(At(::bof3::Img(BattleSprites_Action_addr))))->action_id; }
unsigned char Target() { return At(::bof3::Img(at::kTarget.v))[0]; }
void SetTarget(unsigned char t) { At(::bof3::Img(at::kTarget.v))[0] = t; }
void OrLong(unsigned char* at, std::uint32_t bits) { SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) | bits)); }
unsigned char* EnemyData(std::uint32_t id) { return At(::bof3::Img(battle_sprites::at::kEnemyData.v) + id * battle_sprites::at::kEnemyDataStride); }

// The four action ids that want a target that is out (Battle_ActionSuitsTarget,
// Battle_MemberOutAction): by their shape revivals - unconfirmed.
bool OutAction(std::uint16_t action) { return action == 0x4C || action == 0x4D || action == 0xB4 || action == 0xB5; }

}  // namespace
}  // namespace battle_sprites

using namespace battle_sprites;

// original 0x452BF0 (BATTLE.EMI#15 0x800A5EC0, branch for branch): for the
// kinds 0x1B, 0x3F and 0x44 of the byte at Field_State +0x92 (PSX +0x82), a
// one-in-twelve roll per target that is not out and whose byte +0xB7 (party)
// or +0xC7 (enemy) - the status class of the resistance grid - is under 6;
// a hit sets bit 0x100 (PSX: pending effect) of its flags. A side target
// (0xC0 bits) rolls every member of that side - bit 0x80 the party, else the
// enemies - and answers 1 if any hit; a single target answers 1 on a hit.
// The single target is read again after each call, as the original does.
// Rand's remainder is the signed one of idiv.
extern "C" unsigned char __cdecl Battle_RollPendingFlag(void) {
    const unsigned char kind = Field_State[0x92];
    if (kind != 0x1B && kind != 0x3F && kind != 0x44) return 0;
    const unsigned char t = Target();
    if (t & 0xC0) {
        unsigned char hits = 0;
        if (t & 0x80) {
            for (unsigned i = 0; i <= 2; ++i) {
                if (battle_sprites::g.actor_out(i)) continue;
                if (Party(i)[0xB7] >= 6) continue;
                if (battle_sprites::g.rand() % 12 != 0) continue;
                OrLong(Party(i) + 0x130, 0x100);
                ++hits;
            }
        } else {
            for (unsigned i = 3; i <= 10; ++i) {
                if (battle_sprites::g.actor_out(i)) continue;
                if (Enemy(i - 3)[0xC7] >= 6) continue;
                if (battle_sprites::g.rand() % 12 != 0) continue;
                OrLong(Enemy(i - 3) + 0x110, 0x100);
                ++hits;
            }
        }
        return hits ? 1 : 0;
    }
    if (t > 2) {
        if (battle_sprites::g.actor_out(t)) return 0;
        if (Enemy(Target() - 3u)[0xC7] >= 6) return 0;
        if (battle_sprites::g.rand() % 12 != 0) return 0;
        OrLong(Enemy(Target() - 3u) + 0x110, 0x100);
        return 1;
    }
    if (battle_sprites::g.actor_out(t)) return 0;
    if (Party(Target())[0xB7] >= 6) return 0;
    if (battle_sprites::g.rand() % 12 != 0) return 0;
    OrLong(Party(Target()) + 0x130, 0x100);
    return 1;
}

// original 0x452F70 (0x800A6570): ORs the low 16 bits of `bits` into the
// flags of the target (or of every member of a side that is not out: 0x80
// the party, then 0x40 the enemies - both for 0xC0), sets its state byte +1
// to 0xB and marks it (0x446FB0). A single target is not tested for being
// out: only the call to 0x4456C0 decides, and it is made for the single
// target too.
extern "C" void __cdecl Battle_SetTargetFlags(unsigned target, unsigned bits) {
    const unsigned char t = static_cast<unsigned char>(target);
    const std::uint32_t low = bits & 0xFFFFu;
    if (t & 0xC0) {
        if (t & 0x80) {
            for (unsigned i = 0; i <= 2; ++i) {
                if (battle_sprites::g.actor_out(i)) continue;
                OrLong(Party(i) + 0x130, low);
                rv::As<records::PartyMember>(Party(i))->state = 0xB;
                battle_sprites::g.mark_actor(i);
            }
        }
        if (t & 0x40) {
            for (unsigned i = 3; i <= 10; ++i) {
                if (battle_sprites::g.actor_out(i)) continue;
                OrLong(Enemy(i - 3) + 0x110, low);
                rv::As<records::BattleEnemy>(Enemy(i - 3))->state = 0xB;
                battle_sprites::g.mark_actor(i);
            }
        }
        return;
    }
    if (battle_sprites::g.actor_out(target)) return;
    if (t <= 2) {
        OrLong(Party(t) + 0x130, low);
        rv::As<records::PartyMember>(Party(t))->state = 0xB;
    } else {
        OrLong(Enemy(t - 3u) + 0x110, low);
        rv::As<records::BattleEnemy>(Enemy(t - 3u))->state = 0xB;
    }
    battle_sprites::g.mark_actor(target);
}

// original 0x4530D0 (0x800A6780): flag bit 0x40 on the target, or on every
// member of a side that is not out (0x80 the party, then 0x40 the enemies).
// A single target gets it without the out test.
extern "C" void __cdecl Battle_SetTargetFlag40(unsigned target) {
    const unsigned char t = static_cast<unsigned char>(target);
    if (t & 0xC0) {
        if (t & 0x80)
            for (unsigned i = 0; i <= 2; ++i)
                if (!battle_sprites::g.actor_out(i)) OrLong(Party(i) + 0x130, 0x40);
        if (t & 0x40)
            for (unsigned i = 3; i <= 10; ++i)
                if (!battle_sprites::g.actor_out(i)) OrLong(Enemy(i - 3) + 0x110, 0x40);
        return;
    }
    if (t <= 2) OrLong(Party(t) + 0x130, 0x40);
    else OrLong(Enemy(t - 3u) + 0x110, 0x40);
}

// original 0x453190 (0x800A690C): 1 if any present actor (byte +0 bit 0) has
// any of bits 4..7 in its flags' low byte, party first; else 0.
extern "C" unsigned char __cdecl Battle_AnyFlagF0(void) {
    for (unsigned i = 0; i <= 2; ++i)
        if ((rv::As<records::PartyMember>(Party(i))->flags & 1) && (Party(i)[0x130] & 0xF0)) return 1;
    for (unsigned i = 3; i <= 10; ++i)
        if ((rv::As<records::BattleEnemy>(Enemy(i - 3))->flags & 1) && (Enemy(i - 3)[0x110] & 0xF0)) return 1;
    return 0;
}

// original 0x453210 (0x800A69E4): whether the current action suits its
// target: 0 when the action record's flags have bit 4; otherwise, for a
// target that is out (0x4456C0), 1 unless the action is one of the four out
// actions, and for one that is not, 1 only if it is.
extern "C" unsigned char __cdecl Battle_ActionSuitsTarget(void) {
    const std::uint16_t action = CurrentAction();
    if (ActionRecord(action)[0] & 0x10) return 0;
    if (battle_sprites::g.actor_out(Target())) return OutAction(action) ? 0 : 1;
    return OutAction(action) ? 1 : 0;
}

// original 0x4532A0 (0x800A6A9C): the same for an item: 0x591810(the id's high
// byte, the id) with bit 4 answers 0; then the ids 0x0E and 0x128 play the
// out actions' part.
extern "C" unsigned char __cdecl Battle_ItemSuitsTarget(void) {
    const std::uint16_t item = CurrentAction();
    if (battle_sprites::g.item_class(item >> 8, item) & 0x10) return 0;
    const bool out_item = item == 0x0E || item == 0x128;
    if (battle_sprites::g.actor_out(Target())) return out_item ? 0 : 1;
    return out_item ? 1 : 0;
}

// original 0x4537A0 (no PSX twin found): picks a party member for the current
// action and sets its flag bit 3 and state 0xA, answering 1; 0 when none
// qualifies. The test is 0x453910 when bit 9 of the action record's u16 +4
// is set, else Battle_MemberCoinFlip. A single party target that passes is
// taken (the target byte read again as a dword); otherwise every member is
// tested and one of those that pass is drawn by Rand, signed remainder.
extern "C" unsigned char __cdecl Battle_PickFlag8Member(void) {
    unsigned char passed[3] = {};
    unsigned char count = 0;
    const bool second = ((battle_sprites::Word(ActionRecord(CurrentAction()) + 4) >> 9) & 1) != 0;
    const unsigned char t = Target();
    auto test = [second](unsigned i) { return second ? battle_sprites::g.member_chance_b(i) : battle_sprites::g.member_chance(i); };
    unsigned char pick = 0;
    bool taken = false;
    if (!(t & 0xC0) && t <= 2 && test(t)) {
        pick = static_cast<unsigned char>(battle_sprites::Long(battle_sprites::At(::bof3::Img(at::kTarget.v))));
        taken = true;
    }
    if (!taken) {
        for (unsigned i = 0; i <= 2; ++i)
            if (test(i)) passed[count++] = static_cast<unsigned char>(i);
        if (!count) return 0;
        pick = passed[battle_sprites::g.rand() % static_cast<int>(count)];
    }
    OrLong(Party(pick) + 0x130, 8);
    rv::As<records::PartyMember>(Party(pick))->state = 0xA;
    return 1;
}

// original 0x453A10 (0x800A76E0; PSX 0x800A76E0): 0 if member `i` is out, lacks flag bit 0,
// its byte +0x124 is not the acting actor, or 0x453AC0 says so; else a coin
// flip, 1 on an even Rand (the signed remainder's low byte tested).
extern "C" unsigned char __cdecl Battle_MemberCoinFlip(unsigned i) {
    if (battle_sprites::g.actor_out(i)) return 0;
    unsigned char* const p = Party(i & 0xFFu);
    if (!(p[0x130] & 1)) return 0;
    if (p[0x124] != battle_sprites::At(::bof3::Img(at::kActor.v))[0]) return 0;
    if (battle_sprites::g.slots_full(i)) return 0;
    return battle_sprites::g.rand() % 2 == 0 ? 1 : 0;
}

// original 0x453B10 (0x800A783C): for each party member not out: with flag
// bit 3, unless 0x453A90 has the action's low byte, the action (when its
// record's byte +5 has bit 1) is handed to 0x590C90 twice - (id, member, 0,
// 1), then (id's low byte, member +0x148, 0, 0) - and its bit set in the
// 0x904088 set, and the answer becomes 1; without bit 3, the flags are
// stored back unchanged (the original's `and al, 0xF7` of a clear bit). The
// action pointer and id are read again after every call.
extern "C" unsigned char __cdecl Battle_SettleFlag8(void) {
    unsigned char answer = 0;
    for (unsigned i = 0; i <= 2; ++i) {
        auto* const member_rec = rv::As<records::PartyMember>(Party(i));
        if (battle_sprites::g.actor_out(i)) continue;
        const std::uint32_t flags = static_cast<std::uint32_t>(static_cast<std::int32_t>(member_rec->flags_130));
        if (!(flags & 8)) {
            member_rec->flags_130 = static_cast<std::int32_t>(flags & ~8u);
            continue;
        }
        if (battle_sprites::g.action_flag(Ptr(battle_sprites::At(::bof3::Img(BattleSprites_Action_addr)))[2])) continue;
        const std::uint16_t action = CurrentAction();
        if (ActionRecord(action)[5] & 2) {
            battle_sprites::g.inventory_put(action, i, 0, 1);
            battle_sprites::g.inventory_put(Ptr(battle_sprites::At(::bof3::Img(BattleSprites_Action_addr)))[2], member_rec->char_index, 0, 0);
            const std::uint16_t again = CurrentAction();
            OrLong(battle_sprites::At(::bof3::Img(Cond_Flags_addr) + 0xf8 + (again >> 5) * 4u), 1u << (again & 31));
        }
        answer = 1;
    }
    return answer;
}

// original 0x453C00 (0x800A79AC, the PSX's Formation_ApplyStatMods): by the
// formation byte, 1..4 (anything else nothing), through Stat_AddClamped and
// Stat_AddCap100 on the party records' stats (+0xC4.. the base block):
//   1: member 0's +0xC4 up by half, +0xC6 down by a quarter, +0xD9 doubled
//      (capped at 100);
//   2: every member up to the party count not out: +0xC8 down by a quarter;
//   3: the same members take member 0's +0xC8 (read each time), then +0xC6
//      down by half;
//   4: member 2's +0xCA up by half, members 0 and 1's down by a quarter.
// The party count is read again every turn. Deltas are 16 bits.
extern "C" void __cdecl Formation_ApplyStatMods(void) {
    auto stat = [](std::uint32_t address) { return reinterpret_cast<unsigned short*>(battle_sprites::At(address)); };
    auto half = [](std::uint32_t address) { return static_cast<unsigned>(battle_sprites::Word(battle_sprites::At(address)) >> 1); };
    auto quarter = [](std::uint32_t address) { return static_cast<unsigned>(battle_sprites::Word(battle_sprites::At(address)) >> 2); };
    switch (static_cast<std::uint32_t>(battle_sprites::Long(battle_sprites::At(::bof3::Img(Cond_Flags_addr) + 0xd0)) & 0xFF) - 1) {
    case 0:
        battle_sprites::g.add_clamped(stat(::bof3::Img(0x802E04)), half(::bof3::Img(0x802E04)));
        battle_sprites::g.add_clamped(stat(::bof3::Img(0x802E06)), 0u - quarter(::bof3::Img(0x802E06)));
        battle_sprites::g.add_cap100(battle_sprites::At(::bof3::Img(0x802E19)), battle_sprites::At(::bof3::Img(0x802E19))[0]);
        return;
    case 1:
        for (unsigned char i = 0; i < battle_sprites::At(::bof3::Img(at::kPartyCount.v))[0]; ++i)
            if (!battle_sprites::g.actor_out(i)) battle_sprites::g.add_clamped(stat(::bof3::Img(0x802E08) + i * at::kPartyStride), 0u - quarter(::bof3::Img(0x802E08) + i * at::kPartyStride));
        return;
    case 2:
        for (unsigned char i = 0; i < battle_sprites::At(::bof3::Img(at::kPartyCount.v))[0]; ++i) {
            if (battle_sprites::g.actor_out(i)) continue;
            battle_sprites::SetWord(battle_sprites::At(::bof3::Img(0x802E08) + i * at::kPartyStride), battle_sprites::Word(battle_sprites::At(::bof3::Img(0x802E08))));
            battle_sprites::g.add_clamped(stat(::bof3::Img(0x802E06) + i * at::kPartyStride), 0u - half(::bof3::Img(0x802E06) + i * at::kPartyStride));
        }
        return;
    case 3:
        battle_sprites::g.add_clamped(stat(::bof3::Img(0x8030A2)), half(::bof3::Img(0x8030A2)));
        battle_sprites::g.add_clamped(stat(::bof3::Img(0x802E0A)), 0u - quarter(::bof3::Img(0x802E0A)));
        battle_sprites::g.add_clamped(stat(::bof3::Img(0x802F56)), 0u - quarter(::bof3::Img(0x802F56)));
        return;
    default:
        return;
    }
}

// original 0x453DA0 (0x800A7BC0): a pop-up for an amount on an actor: a free
// 0x84-byte record from 0x435180(0, 1) - not checked for 0xFF, which would
// land past the 48 records - gets +0xB = 0, +0x80 = the actor's record, +0x60
// = the amount's magnitude (signed 16), +0x27 = 6 for a negative amount else
// 0, and +7 by the actor's byte +0x12C (party) / +0x10C (enemy): 4 without
// bit 0; 2 (and +0x27 = 6) with bit 2 and not bit 4; else 0 for a non-zero
// amount or with bit 4, and 3 for zero.
extern "C" void __cdecl Battle_SetDamagePopup(unsigned amount, unsigned actor) {
    const unsigned slot = battle_sprites::g.popup_slot(0, 1);
    unsigned char* const rec = battle_sprites::At(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFFu) * at::kPopupStride);
    auto* const popup = rv::As<records::BattleTask>(rec);
    rec[0xB] = 0;
    const unsigned char who = static_cast<unsigned char>(actor);
    unsigned char bits;
    if (who <= 2) {
        bits = Party(who)[0x12C];
        popup->owner = Address(Party(who));
    } else {
        bits = rv::As<records::BattleEnemy>(Enemy(who - 3u))->shown;
        popup->owner = Address(Enemy(who - 3u));
    }
    const std::int32_t value = static_cast<short>(amount);
    if (value < 0) {
        popup->value = -value;
        popup->clut = 6;
    } else {
        popup->value = value;
        popup->clut = 0;
    }
    if (!(bits & 1)) {
        popup->mode = 4;
        return;
    }
    if (!(bits & 0x10) && (bits & 4)) {
        popup->clut = 6;
        popup->mode = 2;
        return;
    }
    if (value != 0 || (bits & 0x10)) {
        popup->mode = 0;
        return;
    }
    popup->mode = 3;
}

// original 0x453FA0 (0x800A7FF0): a party member's automatic target. +0x125
// is copied to 0x904B35 first. With +0x90 bit 5, flags +0x134 bits 14 or 16,
// or +0x134 bit 0: unless 0x452DD0 allows it, nothing but the settle below;
// without +0x134 bit 0, a random pick - Rand & 7 of 3 and up takes
// 0x454310(member) when 0x904AB1 is not 1, else 0x445730((Rand & 7) + 3) -
// then the settle: +0x125 = 1 and 0x904B35 = 1. Otherwise: +0x134 bit 0
// hands over to 0x454290; battle flag bit 4 to 0x446B00; a side in +0x124 is
// the target; then by +0x125: 0 and 1 take +0x124 unless it is out (then
// 0x446B00), 2 and 3 nothing, 4 and 5 fall back on 0x454310 / 0x445730 /
// Battle_RandomEnemy 0x435C80 by the tests in docs/battle_sprites.md section 1; above 5
// nothing.
extern "C" void __cdecl Battle_MemberAutoTarget(unsigned member) {
    unsigned char* const p = Party(member & 0xFFu);
    auto* const member_rec = rv::As<records::PartyMember>(p);
    battle_sprites::At(::bof3::Img(at::kAutoMode.v))[0] = p[0x125];
    if ((p[0x90] & 0x20) || (static_cast<std::int32_t>(member_rec->flags_134) & 0x14000) || (p[0x134] & 1)) {
        if (!battle_sprites::g.auto_allowed(member)) {
            p[0x125] = 1;
            battle_sprites::At(::bof3::Img(at::kAutoMode.v))[0] = 1;
            return;
        }
        if (!(p[0x134] & 1)) {
            const unsigned char roll = static_cast<unsigned char>(battle_sprites::g.rand() & 7);
            if (static_cast<signed char>(roll) >= 3 && Battle_PartyStanding != 1)
                SetTarget(battle_sprites::g.pick_party(member));
            else
                SetTarget(battle_sprites::g.pick_enemy_b(static_cast<unsigned char>((battle_sprites::g.rand() & 7) + 3)));
            p[0x125] = 1;
            battle_sprites::At(::bof3::Img(at::kAutoMode.v))[0] = 1;
            return;
        }
    }
    if (p[0x134] & 1) {
        battle_sprites::g.auto_fixed(member);
        return;
    }
    if (battle_sprites::At(::bof3::Img(at::kBattleFlags.v))[0] & 0x10) {
        battle_sprites::g.no_target();
        return;
    }
    const unsigned char first = p[0x124];
    if (first & 0xC0) {
        SetTarget(first);
        return;
    }
    // A party target of the member's own choosing: 0x454310 unless 0x904AB1
    // is 1, then the member itself.
    auto party_pick = [member]() {
        if (Battle_PartyStanding != 1) SetTarget(battle_sprites::g.pick_party(member));
        else SetTarget(static_cast<unsigned char>(member));
    };
    switch (p[0x125]) {
    case 0:
    case 1:
        if (battle_sprites::g.actor_out(first)) {
            battle_sprites::g.no_target();
            return;
        }
        SetTarget(first);
        return;
    case 4: {
        if (!battle_sprites::g.actor_out(first) || battle_sprites::g.out_action(member)) {
            SetTarget(p[0x124]);
            return;
        }
        const unsigned char t = p[0x124];
        if (t > 2) {
            SetTarget(battle_sprites::g.pick_enemy_b(t));
            return;
        }
        if (ActionRecord(rv::As<records::PartyMember>(p)->queued_id)[0] & 0x20) {
            battle_sprites::g.no_target();
            return;
        }
        party_pick();
        return;
    }
    case 5: {
        if (!battle_sprites::g.actor_out(first)) {
            SetTarget(p[0x124]);
            return;
        }
        const unsigned char same = battle_sprites::g.action_is_e(member);
        const unsigned char t = p[0x124];
        if (same) {
            SetTarget(t);
            return;
        }
        if (t > 2) {
            SetTarget(battle_sprites::g.pick_enemy_b(t));
            return;
        }
        if (battle_sprites::g.item_class(p[0x127], p[0x126]) & 0x20) {
            battle_sprites::g.no_target();
            return;
        }
        const unsigned char again = p[0x124];
        if (again <= 2) {
            party_pick();
            return;
        }
        if (Battle_EnemiesLeft != 1) SetTarget(battle_sprites::g.pick_enemy_a(again));
        else SetTarget(battle_sprites::g.pick_enemy_b(again));
        return;
    }
    default:
        return;
    }
}

// original 0x454220 (no twin found; it follows 0x453FA0's jump table): 1 if
// party member `i`'s action +0x126 is one of the four out actions.
extern "C" unsigned char __cdecl Battle_MemberOutAction(unsigned i) {
    return OutAction(rv::As<records::PartyMember>(Party(i & 0xFFu))->queued_id) ? 1 : 0;
}

// original 0x454380 (0x800A8764, the PSX's Battle_PlayHitSound): the sound
// Sound_PlayEffect plays for a hit: the class is the table 0x657465 at the
// party actor's +0x92 (every 0x1C bytes), or the enemy's +0x101; the id the
// u16 at 0x64F10C + (class + 7 * critical) * 2, critical bit 7 of the battle
// flags.
extern "C" void __cdecl Battle_PlayHitSound(void) {
    const unsigned critical = battle_sprites::At(::bof3::Img(at::kBattleFlags.v))[0] >> 7;
    const unsigned char actor = battle_sprites::At(::bof3::Img(at::kActor.v))[0];
    const unsigned hit_class = actor <= 2 ? battle_sprites::At(::bof3::Img(at::kHitClass.v))[rv::As<records::PartyMember>(Party(actor))->weapon * 0x1Cu] : rv::As<records::BattleEnemy>(Enemy(actor - 3u))->hit_class;
    battle_sprites::g.play_effect(battle_sprites::Word(battle_sprites::At(::bof3::Img(at::kHitSounds.v) + (hit_class + critical * 7) * 2)));
}

// original 0x454410 (0x800A8840): a hit pop-up: a free record from
// 0x435180(0, 3), +0x80 = Sprite_Current, +7 = 7 when the block at 0x904B50
// has +0xC bit 1, else the actor's hit class; +0x2E / +0x30 = the sprite's
// +0x2E / +0x30 plus a signed offset - the enemy block's (0x939AD8) +0xF2 /
// +0xF3 for an enemy sprite (+5 above 2), the pair at 0x64DF70 by the
// sprite's +8 and Field_State +0x89 for a party one - +0x32 the sprite's,
// and +8 the owning record's byte +8. The sprite and the two block pointers
// are read once; the table indices again for the second offset.
//
// DIVERGENCE DIV-0185 (D397): 0x904B50 is the target's block, written by
// BattleAction_BeginStep / AbilityCheck only for a target of 0..10 and
// cleared by nothing, so a side target (0x40 / 0x80 / 0xC0) leaves the last
// one-target action's block there - and in a session with no such action
// yet, 0. The original tests +0xC unchecked and faults reading 0x0000000C:
// an enemy that acts first in a fresh session and casts Giant Growth (game
// byte 0x12, side target 0x80, the pop-up path) - measured 2026-10-29 on the
// owner's crash and forced on giantGrowth (Capcom's function the same,
// 0x45448D). Ours: a null block counts as bit 1 clear, the actor's hit class;
// a written one takes the original's path unchanged, stale or not.
// BOF3X_ORIGINAL=Battle_SetHitPopup leaves Capcom's function, its read with it.
extern "C" void __cdecl Battle_SetHitPopup(void) {
    const unsigned slot = battle_sprites::g.popup_slot(0, 3) & 0xFFu;
    const unsigned char actor = battle_sprites::At(::bof3::Img(at::kActor.v))[0];
    const unsigned char* const block = Ptr(battle_sprites::At(::bof3::Img(BattleSprites_HitBlock_addr)));
    unsigned char* const rec = battle_sprites::At(::bof3::Img(BattleTask_Slots_addr) + slot * at::kPopupStride);
    auto* const popup = rv::As<records::BattleTask>(rec);
    unsigned char* const s = Sprite_Current;
    popup->owner = Address(s);
    if (block == nullptr)   // DIV-0185: no target block yet, the bit read as clear
        bof3::Log("DIV-0185    Battle_SetHitPopup: no target block (actor %u, target 0x%02X), the actor's hit class", actor,
                  Target());
    const bool fixed = block != nullptr && (block[0xC] & 2) != 0;
    if (actor <= 2) popup->mode = fixed ? 7 : battle_sprites::At(::bof3::Img(at::kHitClass.v))[rv::As<records::PartyMember>(Party(actor))->weapon * 0x1Cu];
    else popup->mode = fixed ? 7 : rv::As<records::BattleEnemy>(Enemy(actor - 3u))->hit_class;
    if (s[5] > 2) {
        const unsigned char* const offsets = Ptr(battle_sprites::At(::bof3::Img(BattleE3_CurrentEnemy_addr)));
        rv::Store<unsigned>(popup->screen_x, static_cast<unsigned>(static_cast<signed char>(offsets[0xF2]) + battle_sprites::Word(s + 0x2E)));
        rv::Store<unsigned>(popup->screen_y, static_cast<unsigned>(static_cast<signed char>(offsets[0xF3]) + battle_sprites::Word(s + 0x30)));
        popup->digit_phase = battle_sprites::Word(s + 0x32);
        rec[8] = Enemy(s[5] - 3u)[8];
        return;
    }
    const unsigned char* const field = Field_State;
    rv::Store<unsigned>(popup->screen_x, static_cast<unsigned>(BattleWin_MemberTargetOffsets[(s[8] + field[0x89] * 4u) * 2] + battle_sprites::Word(s + 0x2E)));
    rv::Store<unsigned>(popup->screen_y, static_cast<unsigned>(BattleWin_MemberTargetOffsets[0x1 + (s[8] + field[0x89] * 4u) * 2] + battle_sprites::Word(s + 0x30)));
    popup->digit_phase = battle_sprites::Word(s + 0x32);
    rec[8] = Party(s[5])[8];
}

// original 0x454CC0 (GAME.EMI 0x8019751C; PSX 0x8019751C): takes the first free of the 32
// tint records (MoveScript_TintRecords, 12 bytes: +0 bit 0 in use), 0xFF when
// none is: +0 = (a << 1) + 0x41 as a byte, with bit 7 when the sprite's +0x28
// is 0 and its +0x24 bit 2 is clear; +6 = ClutMap_Mark(+0x28, index, 1) - the
// clut slot claimed; +1 = the sprite's +0x27 and +5 its +0x28 (read after the
// call), +8 the sprite, +2..+4 = r g b. Answers the index.
extern "C" unsigned char __cdecl Sprite_SetTint(unsigned char* sprite, unsigned char r, unsigned char g_, unsigned char b,
                                                unsigned char a) {
    unsigned i = 0;
    while (battle_sprites::At(::bof3::Img(MoveScript_TintRecords_addr) + i * 12)[0] & 1)
        if (++i == 32) return 0xFF;
    unsigned char* const rec = battle_sprites::At(::bof3::Img(MoveScript_TintRecords_addr) + i * 12);
    const unsigned char head = static_cast<unsigned char>((a << 1) + 0x41);
    rec[0] = head;
    if (sprite[0x28] == 0 && !(sprite[0x24] & 4)) rec[0] = static_cast<unsigned char>(head | 0x80);
    rec[6] = battle_sprites::g.clut_mark(sprite[0x28], i, 1);
    rec[1] = sprite[0x27];
    rec[5] = sprite[0x28];
    battle_sprites::SetLong(rec + 8, Address(sprite));
    rec[2] = r;
    rec[3] = g_;
    rec[4] = b;
    return static_cast<unsigned char>(i);
}

// original 0x454D60 (0x80197670; PSX 0x80197670): releases tint record `index` (unchecked)
// if in use: bit 7 clears bit 2 of the sprite's +0x24, +0 = 0, the sprite's
// +0x27 back from +1, and its clut cells freed (ClutMap_Mark(+5, index, 0)).
extern "C" void __cdecl Tint_Release(unsigned char index) {
    unsigned char* const rec = battle_sprites::At(::bof3::Img(MoveScript_TintRecords_addr) + index * 12u);
    const unsigned char head = rec[0];
    if (!(head & 1)) return;
    if (head & 0x80) Ptr(rec + 8)[0x24] &= 0xFB;
    rec[0] = 0;
    Ptr(rec + 8)[0x27] = rec[1];
    battle_sprites::g.clut_mark(rec[5], index, 0);
}

// original 0x454DF0 (0x80197784; PSX 0x80197784): the body Sprite_SetTint and Tint_Release
// share. Claim: ClutMap_FindFree(kind) and the owner written; release:
// ClutMap_FindOwner(owner) and 0xFF written - over the cells the callee left
// in SharedScratch bytes 0 / 1 (row, cell), read after the call: kind 0 one
// cell, 1 sixteen, 2 two, 3 four, 4 eight, anything else none. Answers what
// the callee answered. Neither the row nor a run past the map's 64 cells is
// checked; a release that found nothing still writes over the last cell the
// scratch names.
extern "C" unsigned char __cdecl ClutMap_Mark(unsigned kind, unsigned owner, unsigned claim) {
    unsigned char answer, fill;
    if (claim & 0xFF) {
        answer = battle_sprites::g.clut_find(kind);
        fill = static_cast<unsigned char>(owner);
    } else {
        answer = battle_sprites::g.clut_owner(owner);
        fill = 0xFF;
    }
    unsigned char* const cells = battle_sprites::At(::bof3::Img(at::kClutMap.v) + at::kCells.b_00 * 16u + at::kCells.b_01);
    unsigned n;
    switch (kind & 0xFF) {
    case 0: n = 1; break;
    case 1: n = 16; break;
    case 2: n = 2; break;
    case 3: n = 4; break;
    case 4: n = 8; break;
    default: return answer;
    }
    for (unsigned i = n; i-- > 0;) cells[i] = fill;
    return answer;
}

// original 0x454F30 (0x801978BC; PSX 0x801978BC): the first free clut cell (0xFF in the
// 4 x 16 map) that starts a free run of the kind's size, aligned to it:
// every free cell tried is left in SharedScratch bytes 0 / 1 (row, cell),
// so a failed search leaves the last free cell there. Answers the clut slot
// - kind 0 0xC0 + row * 16 + cell, 1 (cell 0, the whole row) 0x1C + row,
// 2 0xE0 + row * 8 + cell / 2, 3 0x70 + row * 4 + cell / 4, 4 0x38 + row * 2
// + cell / 8 - or 0xFF.
extern "C" unsigned char __cdecl ClutMap_FindFree(unsigned kind) {
    const unsigned char* const map = battle_sprites::At(::bof3::Img(at::kClutMap.v));
    auto free_run = [map](unsigned row, unsigned cell, unsigned size) {
        for (unsigned k = cell; k < cell + size; ++k)
            if (map[row * 16 + k] != 0xFF) return false;
        return true;
    };
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned cell = 0; cell < 16; ++cell) {
            if (map[row * 16 + cell] != 0xFF) continue;
            at::kCells.b_01 = static_cast<unsigned char>(cell);
            at::kCells.b_00 = static_cast<unsigned char>(row);
            switch (kind & 0xFF) {
            case 0: return static_cast<unsigned char>((row + 0xC) * 16 + cell);
            case 1:
                if (cell == 0 && free_run(row, 0, 16)) return static_cast<unsigned char>(row + 0x1C);
                break;
            case 2:
                if (!(cell & 1) && free_run(row, cell, 2)) return static_cast<unsigned char>((cell >> 1) + row * 8 + 0xE0);
                break;
            case 3:
                if (!(cell & 3) && free_run(row, cell, 4)) return static_cast<unsigned char>((cell >> 2) + row * 4 + 0x70);
                break;
            case 4:
                if (!(cell & 7) && free_run(row, cell, 8)) return static_cast<unsigned char>((cell >> 3) + row * 2 + 0x38);
                break;
            default: break;
            }
        }
    return 0xFF;
}

// original 0x455140 (0x80197B20; PSX 0x80197B20): the first cell of the clut map owned by
// `owner`, left in SharedScratch bytes 0 / 1 (row, cell); answers the owner,
// or 0xFF (and the scratch untouched) when it owns none.
extern "C" unsigned char __cdecl ClutMap_FindOwner(unsigned owner) {
    const unsigned char o = static_cast<unsigned char>(owner);
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned cell = 0; cell < 16; ++cell)
            if (battle_sprites::At(::bof3::Img(at::kClutMap.v))[row * 16 + cell] == o) {
                at::kCells.b_00 = static_cast<unsigned char>(row);
                at::kCells.b_01 = static_cast<unsigned char>(cell);
                return o;
            }
    return 0xFF;
}

// original 0x4551A0 (GAME.EMI 0x80197B9C, term for term; PSX 0x80197B9C): sets bit 15 - the
// PlayStation's semi-transparency bit - on entries 1.. of the current sprite's
// clut in Gfx_ClutStrip, and Gfx_ClutStripDirty. The sprite's +0x27 (the
// clut slot) divided by the table 0x6528CC at its +0x28 (the clut kind)
// gives the strip row (+0x10 with +0x24 bit 2), the remainder times
// 0x6528D4's byte the column; 0x6528C4's byte * 16 the entry count, read
// again every turn. A kind whose divisor is 0 (5 and up) divides by zero, as
// it does in the original.
extern "C" void __cdecl Sprite_SetClutStp(void) {
    const unsigned char* const s = Sprite_Current;
    const unsigned kind = s[0x28];
    const int slot = s[0x27];
    volatile int divisor = battle_sprites::At(::bof3::Img(at::kClutDivisor.v))[kind];
    unsigned char row = static_cast<unsigned char>(slot / divisor);
    const unsigned char column = static_cast<unsigned char>((slot % divisor) * battle_sprites::At(::bof3::Img(at::kClutStep.v))[kind]);
    if (s[0x24] & 4) row = static_cast<unsigned char>(row + 0x10);
    unsigned count = battle_sprites::At(::bof3::Img(at::kClutCount.v))[kind] << 4;
    unsigned i = 1;
    if (count > i) {
        unsigned char* entry = reinterpret_cast<unsigned char*>(Gfx_ClutStrip) + 2 + ((row << 8) + column) * 2u;
        do {
            entry[1] |= 0x80;
            count = battle_sprites::At(::bof3::Img(at::kClutCount.v))[s[0x28]] << 4;
            ++i;
            entry += 2;
        } while (i < count);
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x588F00 (0x8014D154; PSX 0x8014D154): Sprite_Current +0x29 = Draw_OtSlot, then
// (a tail jump) Sprite_UpdateScreen.
extern "C" void __cdecl Sprite_UpdateScreenSlot(void) {
    Sprite_Current[0x29] = Draw_OtSlot;
    battle_sprites::g.update_screen();
}

// original 0x587900 (PSX 0x8015E210): Sound_PlayEffect(id). Ops BC, BD and D1 push more
// arguments; only the first is read.
//
// The PlayStation has three here (US 0x8015D71C / 0x8015D7C8 / 0x8015D820;
// JP 0x8015E10C / 0x8015E1B8 / 0x8015E210); the port folded them into one.
// Every caller but four is the first's (a jal scan of the US disc: 503 calls
// in 176 overlays; 0x8015D7C8 only from op BC and SCENA01's three, 0x8015D820
// only from ops BD and D1), and those four are ours and call sound_sp's
// level plays under DIV-0172 (move_groups.cpp, scena_sc1.cpp).
// DIVERGENCE DIV-0171: with the switch, the first's play: the cue follows
// Sprite_Current (sound_sp.h).
extern "C" void __cdecl Sound_PlayById(unsigned short id) {
    if (sound_sp::g_position) {
        sound_sp::PlayPositional(id);
        return;
    }
    battle_sprites::g.play_effect(id);
}

void BattleSprites_Inject() {
    if (bof3::WantsShadow("battle_sprites")) battle_sprites::SelfTest();
    BOF3_INJECT(Battle_RollPendingFlag);
    BOF3_INJECT(Battle_SetTargetFlags);
    BOF3_INJECT(Battle_SetTargetFlag40);
    BOF3_INJECT(Battle_AnyFlagF0);
    BOF3_INJECT(Battle_ActionSuitsTarget);
    BOF3_INJECT(Battle_ItemSuitsTarget);
    BOF3_INJECT(Battle_PickFlag8Member);
    BOF3_INJECT(Battle_MemberCoinFlip);
    BOF3_INJECT(Battle_SettleFlag8);
    BOF3_INJECT(Formation_ApplyStatMods);
    BOF3_INJECT(Battle_SetDamagePopup);
    BOF3_INJECT(Battle_MemberAutoTarget);
    BOF3_INJECT(Battle_MemberOutAction);
    BOF3_INJECT(Battle_PlayHitSound);
    BOF3_INJECT(Battle_SetHitPopup);
    BOF3_INJECT(Sprite_SetTint);
    BOF3_INJECT(Tint_Release);
    BOF3_INJECT(ClutMap_Mark);
    BOF3_INJECT(ClutMap_FindFree);
    BOF3_INJECT(ClutMap_FindOwner);
    BOF3_INJECT(Sprite_SetClutStp);
    BOF3_INJECT(Sprite_UpdateScreenSlot);
    BOF3_INJECT(Sound_PlayById);
    // DIV-0187 writes the enemy's +0x8F here; Capcom's Battle_SetupEnemy
    // would write the kind's bit and the cheat would be silently half off.
    if (Cheats_EnemyInfoShown() && !bof3::IsEnabled(bof3::addr::Battle_SetupEnemy))
        bof3::Fatal("BOF3X_ENEMY_INFO=1 needs Battle_SetupEnemy ours, and it is disabled");
}
