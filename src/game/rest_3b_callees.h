// Internal to rest_3b.cpp and rest_3b_fuzz.cpp: the cells group R3B's battle
// code touches that symbols.toml has no name for, and the callees of another
// group, by address. docs/rest_3b.md.
//
// Callees by address (all R3D's, this wave; ours since R3D merged, named by
// symbol below, the values unchanged: round fourteen's rebinding,
// docs/round-14-cleanup.md):
//   0x44FB30  (): the effect's miss tail - 0x904AA9 |= 0x20, the target's
//             flags +0x130 / +0x110 |= 0x200 and its +0x12C / +0x10C byte's
//             bit 0 cleared (capstone 2026-10-05: 0x802E70 / 0x802E6C, as
//             R3D's Effect_NoHitReaction has it). Called, and tail-jumped to by six slots.
//   0x44FBB0  (which): 0x44FB30, then 0x44F6A0(actor, target) resisting: al 1;
//             else 0x44F650(the ability's s8 at NameTable_Abilities +3, which)
//             - the result record's byte +0x14 + which moved and clamped to
//             -25..50 - and Battle_RecalcStats(target); al 0.
//   0x44FC60  (status): 0x44FB30, then 0x44F6A0 resisting: al 1; else
//             0x44F1D0(target, status); al 0.
//   0x44FCA0  (status): the same without 0x44FB30.
//   0x44FCE0  (divisor): ax, a share of the actor's HP (its HP / divisor by
//             the element affinity and a Rand roll; 0 for a target with flag
//             0x10000). Read to its last instruction for its signature only.
//   0x44F6A0  (actor, target): al: the target resisted (the engine's
//             standard row).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_3b {
namespace at {

using U = std::uint32_t;

// --- the battle's bytes (0x904AA0..0x904BA0, the harness's battle region) ---
// kStep1: now BattleActions_Step (symbols.toml). u8: the phase's step (the menus' cancels set it)
// kStep2: now BattleE4_Step2 (symbols.toml). u8: the command (Battle_MenuSteps' index)
// kStep3: now BattleE4_Step3 (symbols.toml). u8: the command's step (Escape_States' index)
// kStep4: now BattleE4_Step4 (symbols.toml). u8: the step's sub-state (the side tables' index)
constexpr U kRoundFlags = 0x904AA8;       // u8: the round flags' low byte (bit 7 set by slot 34)
constexpr U kRoundFlags2 = 0x904AA9;      // u8: the round flags' high byte (bit 5 set by slots 32, 33, 35, 41)
// kPicking: now BattleE4_Picking (symbols.toml). u8: a target pick is up
constexpr U kPartyCount = 0x904AB0;       // u8: the party slots in the battle
// kPartyUp: now Battle_PartyStanding (symbols.toml). u8: counted up when a party member is raised
// kEnemiesLeft: now BattleE2_EnemiesLeft (symbols.toml). u8: counted up when an enemy is raised
constexpr U kActor = 0x904B34;            // u8 (read as a dword by some): the acting actor, 0..2 a member, 3.. an enemy
constexpr U kActingKind = 0x904B35;       // u8: 4 an ability (slots 44..49 set it)
constexpr U kTarget = 0x904B54;           // u8 (read as a dword by some): the effect's target
constexpr U kResult = 0x904B60;           // unsigned char *: the result record (+4 HP delta, +6 AP delta, +8 flags)
constexpr U kAbility = 0x904B80;          // u16: the acting ability

// --- the command menu's cells (docs/battle_menu_states.md) ---
constexpr U kMenuActor = 0x939EC4;        // unsigned char *: the acting member's ObjTrio record
constexpr U kCommand = 0x939FA0;          // unsigned char *: the member's command record (+0 target, +2 word)
constexpr U kPower = 0x939FE4;            // u16: the power word Battle_CalcDamage reads (several slots set it)
// kPowerAdd: now Rest3b_PowerAdd (symbols.toml). u16: slot 35's addend
// kPowerAdd2: now Rest3b_PowerAdd2 (symbols.toml). u16 (read as a dword): slot 41's addend; both slots' hit-rate source
// kHitRate: now BattleE4_HitEnemy (symbols.toml). u8: 100, or (0x939FE8 / 2 + 30) below it (slots 34, 35, 41)

// --- the keys (symbols.toml Input_Pressed, Field_ConfirmButtons, Field_CancelButtons) ---
// kInputPressed: now Input_Pressed (symbols.toml).
// kConfirmButtons: now Field_ConfirmButtons (symbols.toml).
// kCancelButtons: now Field_CancelButtons (symbols.toml).
constexpr U kRepeatLatch = 0x7E01B8;      // u16: Input_AutoRepeat's latch

// --- the window records the menus' steps touch (WindowRecords 0x803160, stride 0x24) ---
// kWin2State: now Window2_State (symbols.toml). record 2's +3 (0x8031A8 + 3)
constexpr U kWin3State = 0x8031CF;        // record 3's +3
// kWin16Closing: now Window16_State (symbols.toml). record 16's +3: the ability window's closing byte
// kWin16Page: now Window16_Page (symbols.toml). record 16's +0xB: Char_AbilityList's type
constexpr U kWin16Cursor = 0x8033AC;      // record 16's +0xC (a dword, the low byte the row)
// kAbilityActor: now BattleActorCopies_ItemActor (symbols.toml). u8: the ability window's member (Char_AbilityList's first word)

// --- the party (ObjTrio, stride 0x14C), the enemies (stride 0x128), the character records ---
// kParty: now ObjTrio (symbols.toml).
constexpr U kPartyStride = 0x14C;
constexpr U kEnemies = 0x93B960;
constexpr U kEnemyStride = 0x128;
constexpr U kCharRecords = 0x903A70;      // CharacterRecords: eight of 0xA4; +0xC the EXP dword, +0x44 a stat word
constexpr U kCharStride = 0xA4;
constexpr U kRosterOf = 0x66972C;         // .data u8 by character id: the roster index (Character_IndexById's address)
constexpr U kAbilityRows = 0x65C4D8;      // NameTable_Abilities: 24 bytes an id; +0 flags, +3 power, +4 element word
constexpr U kAbilityStride = 0x18;

// --- the .data tables the dispatchers jump through ---
// kItemSideSteps: now BattleItem_SideSteps (symbols.toml). 4: BattleItem_SideDispatch's (named here)
// kItemCmdSideSteps: now BattleItemCmd_SideSteps (symbols.toml). 4: BattleItemCmd_SideSteps (BE4's name)
// kItemCmdEquipSteps: now BattleItemCmd_EquipSteps (symbols.toml). 4: BattleItemCmd_EquipSteps (BE4's name)
// kEscapeStates: now Escape_States (symbols.toml). 3: Escape_States (BE4's name)

// --- the callees of R3D, ours ---
constexpr U kMissTail = bof3::addr::Effect_NoHitReaction;
constexpr U kStatMod = bof3::addr::Effect_RollStatStepQuiet;
constexpr U kInflictMiss = bof3::addr::Effect_RollInflictQuiet;
constexpr U kRollInflict = bof3::addr::Effect_RollInflict;
constexpr U kHpShare = bof3::addr::Effect_HpBasedDamage;
constexpr U kResisted = bof3::addr::Battle_StatusResisted;

}  // namespace at
}  // namespace rest_3b
