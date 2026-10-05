// Internal to rest_3c.cpp and rest_3c_fuzz.cpp: the cells group R3C's
// Effect_Handlers slots touch that symbols.toml has no name for, and the
// callees of other groups, by address. docs/rest_3c.md.
//
// Callees by address (ours since R3B and R3D merged, named by symbol below,
// the values unchanged: round fourteen's rebinding, docs/round-14-cleanup.md;
// each read to its arguments and its answer, capstone 2026-10-04):
//   R3B's (this wave, 0x4468B0..0x44CFE0; none in the cut, all four between
//   its starts - its "not listed"):
//   0x44C040  (): kind-4 damage: result +4 = Effect_SkillDamage(actor, target,
//             the ability 0x904B80's power byte, 0); eax that answer. The tail
//             of nine of ours (each sets 0x904B35 = 4 and an ability first).
//   0x44C120  (): result +4 = Effect_HealAmount(actor, target); eax its answer.
//   0x44C170  (): a member target: result +4 = -(its maximum HP +0xA0); an
//             enemy's (+0xB0, 0xFFFF tested) R3B's to read.
//   0x44CF60  (): 0x44FB30, then the target's +0x134 (party) / +0x114 (enemy)
//             |= 0x200, the side picked by the ACTOR's byte (R3B's to read).
//   R3D's (this wave, 0x44E4B0..0x44FF00):
//   0x44F1D0  (target, status): a status put on the target (0x904B98's bits,
//             0x44F460, Battle_ClearStatus ...). Its first word: cmp bl, 2 and
//             and esi, 0xFF, then handed on whole to callees that read a byte;
//             its slot's low word is overwritten with the status word.
//   0x44F650  (delta, which): result record byte +0x14 + (which & 0xFF) +=
//             the s16 delta, held to -25..50.
//   0x44F6A0  (actor, target): al the target resisted (the standard row).
//   0x44FB30  (): the effect's common tail: 0x904AA9 |= 0x20, the target's
//             +0x130 |= 0x200, its +0x12C bit 0 cleared (enemy +0x110 / +0x10C).
//   0x44FBB0  (which): 0x44FB30; resisted: al 1; else 0x44F650(the ability's
//             s8 +3, which), Battle_RecalcStats(target), al 0.
//   0x44FC60  (status): 0x44FB30; resisted: al 1; else 0x44F1D0(target,
//             status), al 0.
//   0x44FCA0  (status): 0x44FC60 without the leading 0x44FB30.
//   0x44FCE0  (divisor): the actor's HP / divisor x a variance x the ability's
//             element affinity / 10000; 0 for a target with flag 0x10000; ax.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_3c {
namespace at {

using U = std::uint32_t;

// --- the acting cells (the battle bytes 0x904AA0..0x904BA0) ---
constexpr U kFlags2 = 0x904AA9;           // u8: the round flags' second byte (0x20, 0x40 set here)
constexpr U kPartyCount = 0x904AB0;       // u8: the members in battle, 0..3 (ObjTrio's)
constexpr U kTurnOrder = 0x904ACC;        // u8[11]: the actors in turn order, 0xFF a spent slot
constexpr U kTurnCursor = 0x904AE2;       // u8: the order's front (the loops start one before it)
constexpr U kTurnCount = 0x904AE3;        // u8: how many of kTurnOrder are filled, 0..11
constexpr U kActor = 0x904B34;            // u8: the acting actor 0..10 (read as a dword where noted)
constexpr U kActingKind = 0x904B35;       // u8: 4 an ability
constexpr U kOtherActor = 0x904B44;       // u8: the battle's other target cell (copied by slot 74)
constexpr U kTarget = 0x904B54;           // u8: the effect's target 0..10 (read as a dword where noted)
constexpr U kResult = 0x904B60;           // unsigned char *: the result record (+4 HP, +6 AP delta, +8 flags)
constexpr U kAbility = 0x904B80;          // u16: the acting ability
constexpr U kPairActor = 0x904B8A;        // u8: slot 74 stores the actor here ...
constexpr U kPairOther = 0x904B8B;        // u8: ... and kOtherActor here
constexpr U kGate = 0x904B8E;             // u8: slot 97 stores 4
constexpr U kHits = 0x904B96;             // u8: slot 103's hit count
constexpr U kByte660 = 0x904660;          // u8: slot 62 stores 5 (Skill_CanUse's four bytes for ability 0x15 start here)

// --- the damage formula's cells (battle_damage_callees.h) ---
constexpr U kTargetDef = 0x939F86;        // u16: zeroed by slots 62 and 63 before Battle_CalcDamage
constexpr U kAttackerAtk = 0x939FE4;      // u16: the attacker's power, read as a dword & 0xFFFF
constexpr U kHitPercent = 0x939FFC;       // u8: slot 94 stores 100

// --- the records ---
constexpr U kParty = 0x802D40;            // ObjTrio, stride 0x14C
constexpr U kPartyStride = 0x14C;
constexpr unsigned kPartyMax = 3;
constexpr U kEnemies = 0x93B960;          // the enemies' objects, stride 0x128
constexpr U kEnemyStride = 0x128;
constexpr unsigned kEnemyMax = 8;
constexpr U kCharRecords = 0x903A70;      // CharacterRecords, stride 0xA4
constexpr U kCharStride = 0xA4;
constexpr U kEnemyData = 0x8C55C8;        // the area's enemy data records, stride 0x8C (+0x24 the word slot 106 reads)
constexpr U kEnemyDataStride = 0x8C;
constexpr U kAbilityStride = 0x18;        // NameTable_Abilities: +3 the power byte
constexpr U kHitScale = 0x64E944;         // .data s8[7], Capcom's: slot 103's per-hit factor in tenths (3 past the seventh)

// --- the callees of R3B and R3D, ours ---
constexpr U kSkillByAbility = bof3::addr::EffectSlot04_SkillPower;   // R3B's
constexpr U kHealByAbility = bof3::addr::EffectSlot07_Heal;    // R3B's
constexpr U kHealMaxHp = bof3::addr::EffectSlot11_HealFull;        // R3B's
constexpr U kFlag200 = bof3::addr::EffectSlot47_MissMark200;          // R3B's
constexpr U kInflict = bof3::addr::Battle_InflictStatus;          // R3D's
constexpr U kRaiseStat = bof3::addr::Effect_StepStatByte;        // R3D's
constexpr U kResisted = bof3::addr::Battle_StatusResisted;         // R3D's
constexpr U kMissTail = bof3::addr::Effect_NoHitReaction;         // R3D's
constexpr U kRaiseByAbility = bof3::addr::Effect_RollStatStepQuiet;   // R3D's
constexpr U kMissInflict = bof3::addr::Effect_RollInflictQuiet;      // R3D's
constexpr U kInflictUnlessResisted = bof3::addr::Effect_RollInflict;   // R3D's
constexpr U kHpDamage = bof3::addr::Effect_HpBasedDamage;         // R3D's

}  // namespace at
}  // namespace rest_3c
