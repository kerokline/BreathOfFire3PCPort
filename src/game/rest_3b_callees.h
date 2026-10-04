// Internal to rest_3b.cpp and rest_3b_fuzz.cpp: the cells group R3B's battle
// code touches that symbols.toml has no name for, and the callees nobody owns
// yet, by raw address. docs/rest_3b.md.
//
// Raw-address callees (round fourteen's rebinding pass names them; all R3D's,
// this wave, merged before R3B by the round's order):
//   0x44FB30  (): the effect's miss tail - 0x904AA9 |= 0x20, the target's
//             second flags (+0x134 / +0x114) |= 0x200 and its +0x130 / +0x110
//             byte's bit 0 cleared. Called, and tail-jumped to by six slots.
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
//   0x44F6A0  (actor, target): nobody's; al: the target resisted (the
//             engine's standard row).
#pragma once

#include <cstdint>

namespace rest_3b {
namespace at {

using U = std::uint32_t;

// --- the battle's bytes (0x904AA0..0x904BA0, the harness's battle region) ---
constexpr U kStep1 = 0x904AA1;            // u8: the phase's step (the menus' cancels set it)
constexpr U kStep2 = 0x904AA2;            // u8: the command (Battle_MenuSteps' index)
constexpr U kStep3 = 0x904AA3;            // u8: the command's step (Escape_States' index)
constexpr U kStep4 = 0x904AA4;            // u8: the step's sub-state (the side tables' index)
constexpr U kRoundFlags = 0x904AA8;       // u8: the round flags' low byte (bit 7 set by slot 34)
constexpr U kRoundFlags2 = 0x904AA9;      // u8: the round flags' high byte (bit 5 set by slots 32, 33, 35, 41)
constexpr U kPicking = 0x904AAF;          // u8: a target pick is up
constexpr U kPartyCount = 0x904AB0;       // u8: the party slots in the battle
constexpr U kPartyUp = 0x904AB1;          // u8: counted up when a party member is raised
constexpr U kEnemiesLeft = 0x904AB3;      // u8: counted up when an enemy is raised
constexpr U kActor = 0x904B34;            // u8 (read as a dword by some): the acting actor, 0..2 a member, 3.. an enemy
constexpr U kActingKind = 0x904B35;       // u8: 4 an ability (slots 44..49 set it)
constexpr U kTarget = 0x904B54;           // u8 (read as a dword by some): the effect's target
constexpr U kResult = 0x904B60;           // unsigned char *: the result record (+4 HP delta, +6 AP delta, +8 flags)
constexpr U kAbility = 0x904B80;          // u16: the acting ability

// --- the command menu's cells (docs/battle_menu_states.md) ---
constexpr U kMenuActor = 0x939EC4;        // unsigned char *: the acting member's ObjTrio record
constexpr U kCommand = 0x939FA0;          // unsigned char *: the member's command record (+0 target, +2 word)
constexpr U kPower = 0x939FE4;            // u16: the power word Battle_CalcDamage reads (several slots set it)
constexpr U kPowerAdd = 0x939FE6;         // u16: slot 35's addend
constexpr U kPowerAdd2 = 0x939FE8;        // u16 (read as a dword): slot 41's addend; both slots' hit-rate source
constexpr U kHitRate = 0x939FFC;          // u8: 100, or (0x939FE8 / 2 + 30) below it (slots 34, 35, 41)

// --- the keys (symbols.toml Input_Pressed, Field_ConfirmButtons, Field_CancelButtons) ---
constexpr U kInputPressed = 0x7E1BEC;
constexpr U kConfirmButtons = 0x90358E;
constexpr U kCancelButtons = 0x903590;
constexpr U kRepeatLatch = 0x7E01B8;      // u16: Input_AutoRepeat's latch

// --- the window records the menus' steps touch (WindowRecords 0x803160, stride 0x24) ---
constexpr U kWin2State = 0x8031AB;        // record 2's +3 (0x8031A8 + 3)
constexpr U kWin3State = 0x8031CF;        // record 3's +3
constexpr U kWin16Closing = 0x8033A3;     // record 16's +3: the ability window's closing byte
constexpr U kWin16Page = 0x8033AB;        // record 16's +0xB: Char_AbilityList's type
constexpr U kWin16Cursor = 0x8033AC;      // record 16's +0xC (a dword, the low byte the row)
constexpr U kAbilityActor = 0x929F06;     // u8: the ability window's member (Char_AbilityList's first word)

// --- the party (ObjTrio, stride 0x14C), the enemies (stride 0x128), the character records ---
constexpr U kParty = 0x802D40;
constexpr U kPartyStride = 0x14C;
constexpr U kEnemies = 0x93B960;
constexpr U kEnemyStride = 0x128;
constexpr U kCharRecords = 0x903A70;      // CharacterRecords: eight of 0xA4; +0xC the EXP dword, +0x44 a stat word
constexpr U kCharStride = 0xA4;
constexpr U kRosterOf = 0x66972C;         // .data u8 by character id: the roster index (MoveScript_EffectState's address)
constexpr U kAbilityRows = 0x65C4D8;      // NameTable_Abilities: 24 bytes an id; +0 flags, +3 power, +4 element word
constexpr U kAbilityStride = 0x18;

// --- the .data tables the dispatchers jump through ---
constexpr U kItemSideSteps = 0x64E43C;    // 4: BattleItem_SideDispatch's (named here)
constexpr U kItemCmdSideSteps = 0x64E4A0; // 4: BattleItemCmd_SideSteps (BE4's name)
constexpr U kItemCmdEquipSteps = 0x64E4B0;   // 4: BattleItemCmd_EquipSteps (BE4's name)
constexpr U kEscapeStates = 0x64E4FC;     // 3: Escape_States (BE4's name)

// --- the callees nobody owns yet (R3D's) ---
constexpr U kMissTail = 0x44FB30;
constexpr U kStatMod = 0x44FBB0;
constexpr U kInflictMiss = 0x44FC60;
constexpr U kInflict = 0x44FCA0;
constexpr U kHpShare = 0x44FCE0;
constexpr U kResisted = 0x44F6A0;

}  // namespace at
}  // namespace rest_3b
