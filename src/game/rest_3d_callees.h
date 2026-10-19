// Internal to rest_3d.cpp and rest_3d_fuzz.cpp: the cells group R3D's battle
// code touches that symbols.toml has no name for, and the one callee nobody of
// an earlier wave owns, by raw address. docs/rest_3d.md.
//
// Callees by address (ours since R3C merged, named by symbol below, the value
// unchanged: round fourteen's rebinding, docs/round-14-cleanup.md):
//   0x44D8B0  (): R3C's (this wave, an Effect_Handlers helper of its band);
//             Effect123_Ability6A sets the acting kind 4 and the ability 0x6A
//             and jumps to it (a tail call). R3C names it.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_3d {
namespace at {

using U = std::uint32_t;

// --- the battle bytes (0x904AA0..0x904BA0, the harness's battle frame) ---
constexpr U kRoundFlagsHi = 0x904AA9;     // u8: the round flags' high byte (bit 5: no hit sound or pop-up)
// kDragonPart: now BattleE4_Step3 (symbols.toml). u8: the Dragon command's part (DragonCmd_PartDispatch's index)
constexpr U kPartySize = 0x904AB0;       // u8: the party's members in battle (read whole, the low byte used)
constexpr U kActor = 0x904B34;            // u8: the acting actor, 0..2 a member, 3..10 an enemy
constexpr U kActingKind = 0x904B35;       // u8: 4 an ability
constexpr U kTarget = 0x904B54;           // u8: the effect's target (Effect_ApplyResult's)
// kResult: now BattleDamage_Result (symbols.toml). unsigned char *: the result record (+4 the HP delta, +6 the AP
                                          //   delta, +8 a mark, +0x14.. the eight s8 stat steps)
constexpr U kAbility = 0x904B80;          // u16: the acting ability
constexpr U kNewStatus = 0x904B98;        // u16: the status bits an inflict adds this action
constexpr U kNewStatusHi = 0x904B99;      // its high byte

// --- the action's two stat blocks (8 dwords each, copied by the hit's receiver) ---
// kAttackerInt: now BattleDamage_AttackerInt (symbols.toml). u16: the attacker's block +0xA (the roll's first term)
// kTargetInt: now BattleDamage_TargetInt (symbols.toml). u16: the target's block +0xA (the roll's second term)

// --- the party (ObjTrio, stride 0x14C) and the enemies' objects (stride 0x128) ---
// kParty: now ObjTrio (symbols.toml).
constexpr U kPartyStride = 0x14C;
constexpr U kEnemies = 0x93B960;          // indexed as the originals do: (actor - 3) * 0x128
constexpr U kEnemyStride = 0x128;
// the per-actor bytes Battle_InflictStatus clears (+9 of 0x84-byte records from
// 0x93A000 by the actor, 0..10 inside BattleTask_Create's slots)
constexpr U kActorTaskByte = 0x93A009;
constexpr U kActorTaskStride = 0x84;

// --- the character records the raise of slot 118 writes and copies ---
constexpr U kCharacterStride = 0xA4;      // CharacterRecords' stride

// --- the .data tables of s16 the rolls and the HP damage read (symbols.toml [[data]]) ---
// kResistRates: now Battle_StatusResistRates (symbols.toml). Battle_StatusResistRates: 8, by a class byte (unbounded)
// kPsiRates: now Battle_PsiAffinityRates (symbols.toml). Battle_PsiAffinityRates: 8, by a class byte (unbounded)
constexpr U kResistRates20 = 0x64E98C;    // Battle_StatusResistRates20: 8, by a class byte (unbounded)
constexpr U kHpVariance = 0x64E9AC;       // Effect_HpDamageVariance: 8, by Rand & 7

// --- the callee of R3C, ours ---
constexpr U kAbility6AHelper = bof3::addr::Effect_DamageAllHp;  // R3C's

}  // namespace at
}  // namespace rest_3d
