// Internal to battle_e5.cpp and battle_e5_fuzz.cpp: the cells group BE5's
// battle code touches that symbols.toml has no name for, and the callees
// nobody owns yet, by raw address. docs/battle_e5.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x447F40  (): BE4's (this wave): the window records 16..19 and the target
//             prompt it opens (0x8033A0 = 1, 0x8033A1 = 8, 0x8033A2 = 2 ...,
//             read to its first stores only); called by the Dragon run's four
//             closing steps. BE4 names it.
//   0x4525B0  (): BE6's (this wave): the Dragon command's task (read to its
//             first instructions only: 0x904B78 = 0, then a loop over the
//             0x904B87 genes at 0x904B84). BE6 names it.
//   0x453300  (actor): BE6's: its first read is the byte (cmp al, 2), then
//             the whole word is handed on (push eax at 0x45330F). BE6 names it.
//   0x453EB0  (amount, actor): BE6's: BattleTask_Create(0, 1), then the actor
//             byte (and eax, 0xFF) and the amount's word - Battle_SetDamagePopup's
//             shape. BE6 names it.
//   0x44F1D0  (actor, status): nobody's (a part-7 row); in the engine set.
//   0x44F6A0  (actor, target): nobody's; al: the target resisted (0x44F770's
//             cmp cl, 2 and and eax, 0xFF: the target's byte only).
//   0x44FB30  (): nobody's; the effect's miss tail.
//   0x590E80  (u16 *stat, cap, delta): nobody's; the stat add with a cap
//             (cx / di / ax: the two words' low halves), eax the change.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // bof3::addr, for the rebound callees below

namespace battle_e5 {
namespace at {

using U = std::uint32_t;

// --- the battle's cells (0x904AA0..0x904BA0, the harness's battle bytes) ---
// kStep1: now BattleActions_Step (symbols.toml). u8: the phase's step (the Dragon run's commit sets 1)
// kStep2: now BattleE4_Step2 (symbols.toml). u8: the command (BattleMenu_ConfirmDispatch's index)
constexpr U kStep3 = 0x904AA3;            // u8: the Dragon command's part (0x44FF00's index into 0x64ECCC)
// kStep4: now BattleE4_Step4 (symbols.toml). u8: the part's step (the seven dispatchers' index)
// kPick: now BossSb_Countdown (symbols.toml). u8: the Dragon menu's line 0..2; bit 7 while a list is up
// kGeneRow: now BattleE5_GeneRow (symbols.toml). u8: the gene grid's row 0..2, 0xFF the "done" line
// kGeneCol: now BattleE1_Roster (symbols.toml). u8: the gene grid's column 0..5
constexpr U kFlags = 0x904AA8;            // u8: the round flags' low byte (bit 7 set by Effect_QuarterAttack)
// kCommitClear: now BattleE4_Picking (symbols.toml). u8: zeroed by the commit
// kCommitCount: now BattleActorCopies_EntryCount (symbols.toml). u8: counted up by the commit
constexpr U kActor = 0x904B34;            // u8: the acting actor
constexpr U kActingKind = 0x904B35;       // u8: 4 an ability, 1 an attack
constexpr U kTarget = 0x904B54;           // u8: the effect's target
constexpr U kActingSprite2 = 0x904B40;    // unsigned char *: +8 the drain's flags
constexpr U kResult = 0x904B60;           // unsigned char *: the result record (+4 HP, +6 AP delta, +8 flags)
constexpr U kWord72 = 0x904B72;           // u16: zeroed by the Dragon run's opening
constexpr U kAsk = 0x904B74;              // u16 (read as u32 for the hand): the store prompt's answer 0 / 1
// kGeneCost: now BattleActions_ApCost (symbols.toml). u8: 0x4525B0's (read against the member's AP)
constexpr U kAbility = 0x904B80;          // u16: the acting ability
constexpr U kGenes = 0x904B84;            // u8[3]: the genes picked
// kGeneCount: now BattleE5_GeneCount (symbols.toml). u8: how many of kGenes are picked, 0..3
// kAiFlag: now BattleDamage_AiByteB97 (symbols.toml). u8: set by an AI row of kind 7

// --- the command menu's cells (docs/battle_menu_states.md) ---
constexpr U kMenuActor = 0x939EC4;        // unsigned char *: the acting member's ObjTrio record
constexpr U kCommandRecord = 0x939FA0;    // unsigned char *: the member's +0x124

// --- the keys (symbols.toml Input_Pressed, Field_ConfirmButtons, Field_CancelButtons: their
// names are macros in symbols.gen.h, so the fuzz seeds them by these) ---
// kInputPressed: now Input_Pressed (symbols.toml).
// kConfirmButtons: now Field_ConfirmButtons (symbols.toml).
// kCancelButtons: now Field_CancelButtons (symbols.toml).

// --- the Dragon run's tables ---
constexpr U kSlots = 0x904608;            // 18 dwords: three gene bytes and a fourth (0xFF: empty); 0..5 the first list
constexpr U kSlots2 = 0x904620;           // kSlots + 6 * 4: the second list, 0..11
constexpr U kGeneBits = 0x904650;         // u32: the genes held, bit n gene n (18)
constexpr U kGeneFlags = 0x939A60;        // u8[18]: kGeneBits spread one byte a gene; cleared as a gene is picked
constexpr U kGeneCostTable = 0x64EC9C;    // u8 by gene (.data, Capcom's): what a gene adds to the slot's cost
constexpr U kRepeatLatch = 0x7E01B8;      // u16: Input_AutoRepeat's latch, zeroed by the opening

// --- the message window's line list (8-byte entries at 0x93C2C0) ---
constexpr U kMsgCount = 0x93C2A1;         // u8: the entries; the last is (count - 1) & 15
constexpr U kMsgFlag = 0x93C2C1;          // u8 of the entry: set by the commit
constexpr U kMsgText = 0x93C2C4;          // const unsigned char * of the entry

// --- the enemy AI's scripts and the enemy messages a turn shows ---
constexpr U kAiScripts = 0x8C5600;        // the area's AI scripts, 0x8C bytes each: four rows of 16
constexpr U kEnemyMessages = 0x939FC0;          // 8 entries of 4 bytes: +0 the enemy (actor - 3), +2 a system message (BattleAction_EnemyMessages)
// kEnemyMessageCount: now BattleE5_EnemyMessageCount (symbols.toml). u8: entries in kEnemyMessages, 8 at most
constexpr U kEnemies = 0x93B960;          // the enemies' objects, stride 0x128
constexpr U kEnemyStride = 0x128;
constexpr U kCurrentEnemy = 0x939AD8;     // unsigned char *
constexpr U kAbilityRows = 0x65C4DC;      // .data: 24 bytes an ability, +0 the element word
constexpr U kWeaponElement = 0x657463;    // .data: NameTable_Weapons' element byte, 28 bytes a weapon

// --- the party (ObjTrio, stride 0x14C) ---
// kParty: now ObjTrio (symbols.toml).
constexpr U kPartyStride = 0x14C;

// --- the window records (WindowRecords 0x803160, stride 0x24) the run uses ---
constexpr U kWin4 = 0x803160 + 4 * 0x24;     // 0x8031F0
constexpr U kWin16 = 0x803160 + 16 * 0x24;   // 0x8033A0
constexpr U kWin17 = 0x803160 + 17 * 0x24;   // 0x8033C4
constexpr U kWin18 = 0x803160 + 18 * 0x24;   // 0x8033E8: the first list; +0x10 / +0x12 its cursor
constexpr U kWin19 = 0x803160 + 19 * 0x24;   // 0x80340C: the second list; +0x10 / +0x12 its cursor
constexpr U kWin21 = 0x803160 + 21 * 0x24;   // 0x803454: the Dragon menu

// --- the Dragon run's .data step tables (0x44FF00 indexes 0x64ECCC by kStep3) ---
constexpr U kPartTable = 0x64ECCC;        // 7: the seven parts (not ours: 0x44FF00 reads it)
// kLoadSteps: now DragonCmd_LoadSteps (symbols.toml). 2
// kMenuSteps: now DragonCmd_MenuSteps (symbols.toml). 5
// kSlotsSteps: now DragonCmd_SlotsSteps (symbols.toml). 7
// kGenesSteps: now DragonCmd_GenesSteps (symbols.toml). 5
// kSlots2Steps: now DragonCmd_Slots2Steps (symbols.toml). 7
// kStoreSteps: now DragonCmd_StoreSteps (symbols.toml). 7 (the last BE6's 0x451480)

// --- the callees nobody owns yet ---
constexpr U kTargetPrompt = bof3::addr::ItemMenu_SetupForMember;     // BE4's
constexpr U kDragonTask = bof3::addr::DragonGenes_SumCost;       // BE6's
constexpr U kStatusPick = bof3::addr::Battle_RecalcStats;       // BE6's
constexpr U kApPopup = bof3::addr::Battle_SetApPopup;          // BE6's
constexpr U kInflictStatus = bof3::addr::Battle_InflictStatus;          // R3D's (round fourteen)
constexpr U kResisted = bof3::addr::Battle_StatusResisted;         // R3D's (round fourteen)
constexpr U kMissTail = bof3::addr::Effect_NoHitReaction;         // R3D's (round fourteen)
constexpr U kStatAdd = bof3::addr::Stat_AddClampedTo;          // R2F's (round fourteen)

}  // namespace at
}  // namespace battle_e5
