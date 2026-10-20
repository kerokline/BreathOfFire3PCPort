// Internal to battle_e4.cpp and battle_e4_fuzz.cpp: the cells group BE4's
// battle-engine code touches that symbols.toml has no name for, and the
// callees nobody owns yet, by raw address. docs/battle_e4.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x44FDE0  (): BE5's (round twelve, wave one), called after an equipment
//             change by BattleEquip_Apply and BattleEquip_RemoveSlot.
//   0x453300  (actor): BE6's, called once per member after the same; it
//             reads the argument's low byte and hands the word on to
//             0x453560, which masks it to a byte (read 2026-09-29).
//   0x591810  (category, item) -> al: the item's flag byte; no start list
//             has it (boss_harness's standard row by address).
//   0x494E70  (): BattleEnemy_ClearStates, the eight enemies' +0..+4 zeroed; ours since R3G
//             (boss_harness's standard row, kThrough).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // the constants below name their functions since 2026-10-01 (round twelve's debt 2): the same values, so the fuzz keys stand

namespace battle_e4 {
namespace at {

using U = std::uint32_t;

// --- the party and the enemies (ObjTrio stride 0x14C; the enemies' objects
// 0x93B960 + (actor - 3) * 0x128, as every original indexes them) ----------
// kParty: now ObjTrio (symbols.toml).
constexpr U kPartyStride = 0x14C;
// kEnemies: now Battle_Enemies (symbols.toml).
constexpr U kEnemyStride = 0x128;
constexpr U kEnemyWork = 0x93B9E0;        // an enemy's working record (+0x80): its 12-byte name first

// --- the character records (8 of 0xA4; +0x12..+0x17 the six equipment bytes)
// kCharRecords: now CharacterRecords (symbols.toml).
constexpr U kCharStride = 0xA4;
// kMemberChar: now Cond_Flags[0xd5] (symbols.toml). u8[3]: the members' character bytes (0x66972C's index)
// kCharOf: now Character_IndexById (symbols.toml). u8[]: Character_IndexById - a character record's index by that byte

// --- the battle bytes (0x904AA0..0x904BA0) ------------------------------------
constexpr U kPhase = 0x904AA0;
constexpr U kStep = 0x904AA1;
// kStep2: now BattleE4_Step2 (symbols.toml).
// kStep3: now BattleE4_Step3 (symbols.toml). the command's state (BattleItemCmd_States, the escape states)
constexpr U kStep4 = 0x904AA4;            // the sub-state (a dword whose low byte the dispatchers index by)
constexpr U kFlags = 0x904AA8;            // u16: the round flags
// kFight: now EventBattle_Id (symbols.toml). u8: the event battle (0 none)
// kAutoMember: now BattleE4_AutoMember (symbols.toml). u8: the member AutoBattle_FillCommands filled last
// kPicking: now BattleE4_Picking (symbols.toml). u8: a target pick is up
constexpr U kPartyCount = 0x904AB0;       // u8 (read as a dword's low byte too)
// kPartyDivisor: now Battle_PartyStanding (symbols.toml). u8: Escape_Roll divides the party's sum by it
// kEnemyCount: now Battle_EnemyCount (symbols.toml). u8: the enemies set up (the target wrap)
// kEnemyDivisor: now Battle_EnemiesLeft (symbols.toml). u8: enemies left (Escape_Roll divides by it)
constexpr U kEntryOrder = 0x904AB6;       // u8[3]: the members by entry order, 0xFF past the last
// kCommandsChosen: now BattleActorCopies_EntryCount (symbols.toml). u8: members whose command is chosen
constexpr U kOrder = 0x904ACC;            // u8[]: the turn order
// kOrderCursor: now BossSe_RoundSlot (symbols.toml). u8: its front (Battle_OrderPushFront moves it down one)
// kEscapeRun: now BattlePhases_Initiative (symbols.toml). u8: Escape_Roll's second chance when 1
// kEscapeTries: now BattlePhases_AE6 (symbols.toml). u8: Escape_Roll counts its calls here
// kEndHold: now BattleMisc_BannerKinds (symbols.toml). u8: Escape_End waits while it is set
constexpr U kActor = 0x904B34;            // u8: the acting actor
constexpr U kActionKind = 0x904B35;       // u8
constexpr U kTarget = 0x904B44;           // u8
constexpr U kAbility = 0x904B80;          // u16
// kCharged: now BattleFlow_AnimGate (symbols.toml). u8
constexpr U kFaceStep = 0x904AAC;         // u8: Escape_Begin's index into 0x64E4F4 (a pair of s8 per value)

// --- the command menu (battle_menu_states.md) ---------------------------------
// kMenuActor: now BattleE4_MenuActor (symbols.toml). unsigned char *: the member choosing (+5 its party index)
constexpr U kCommand = 0x939FA0;          // unsigned char *: its command record (the member's +0x124)
// kPartyDefBonus: now BattleDamage_TargetDef (symbols.toml). u16: added to the party's mean +0xA6 (Battle_PartyDefenceMean)
// kEvadeParty: now BattleE3_EvadeBoost (symbols.toml). u8: Battle_HitOrMissParty's percentage
// kHitEnemy: now BattleE4_HitEnemy (symbols.toml). dword, low byte: Battle_HitOrMissEnemy's percentage
constexpr U kRepeatLatch = 0x7E01B8;      // u16: Input_AutoRepeat's latch, zeroed as a pick begins
// kInputPressed: now Input_Pressed (symbols.toml). Input_Pressed (a u16; EquipMenu reads the dword)
// kConfirmButtons: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons
// kCancelButtons: now Field_CancelButtons (symbols.toml). Field_CancelButtons

// --- the window records (WindowRecords, 0x24 each) ------------------------------
// kRecord4: now WindowRecords[4] (symbols.toml). record 4: +0 set as the item menu hands back, +3 by Escape_Begin
constexpr U kList = 0x8033A0;             // record 16: the item list (+0xA category, +0xC cursor)
constexpr U kCands = 0x8033C4;            // record 17: the equipment candidates (+8 category, +0xA top, +0xB cursor, +0xD item)
constexpr U kSlots = 0x8033E8;            // record 18: the six slots (+0xA cursor, +0xB chosen, +0xC member, +0xD, +0x10 word)
constexpr U kHand = 0x80340C;             // record 19: the hand
constexpr U kAbove = 0x803454;            // record 21: the window above the list (+0xA the option 0 / 1, +0xB)
constexpr U kPreview = 0x675F18;          // u8[6]: the slots with the candidate put in (BattleEquip_Preview)
constexpr U kEquipCats = 0x64E4E4;        // u8[6] .data: the six slots' inventory categories (Apply)
constexpr U kUnequipCats = 0x64E4EC;      // u8[6] .data: the same for RemoveSlot
constexpr U kFacePairs = 0x64E4F4;        // s8 pairs .data: Escape_Begin's x / z steps
constexpr U kChanceRow = 0x64E524;        // u8[6] .data: Escape_Chance's row
constexpr U kBannerLines = 0x64E52C;      // u16 [][10] .data: BattleBanner_AddLine's message ids
constexpr U kTileSizes = 0x64E268;        // u16 w, h by the size byte (BattleWin_DrawTile's)
inline auto& kTint = SharedScratch;  // u8 r, g, b: BattleWin_DrawTileTint's colour
constexpr U kWakeChance = 0x64E3E8;       // u8[] .data: Battle_WakeRoll's percentage by the counter (1, 2)
constexpr U kWakeResist = 0x64E3E0;       // u8[] .data: its second roll's table by the actor's +0xB6 / +0xC6

// --- the battle message queue (BattleQueue_Push's: 16 of 8, the write index) ---
constexpr U kQueue = 0x93C2C0;
// kQueueWrite: now BattleActorCopies_QueueWrite (symbols.toml).

// --- the inventory's list pointers (ids, then counts, five each) --------------
constexpr U kInventoryIds = 0x656B00;
constexpr U kInventoryCounts = 0x656B14;

// --- the escape's field cells --------------------------------------------------
// kKind2Z: now Field_Kind2Z (symbols.toml). Field_Kind2Z (long; its high word 0x905E62 moved)
// kKind2X: now Field_Kind2X (symbols.toml). Field_Kind2X (long; its high word 0x905E66 moved)
// kKind2Hold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
// kElevation: now MapView_Elevation (symbols.toml). MapView_Elevation
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). MoveScript_F3Divisor (u16)
// kFaWord: now MoveScript_FAWord (symbols.toml). MoveScript_FAWord (u16)
constexpr U kFlee = 0x93B8E0;             // u8: zeroed by Escape_Begin
// kFleeText: now BattleE1_BannerText (symbols.toml). const char *: Escape_Begin's banner text
// kGeneMember: now BattleActorCopies_ItemActor (symbols.toml). u8: the member whose list is up
constexpr U kListReset = 0x929F04;        // u8: zeroed as the candidates open

// --- the text record the names are copied into ---------------------------------
constexpr U kTextRecord0 = 0x904CE0;      // Text_Records[0]

// --- callees nobody owns yet ----------------------------------------------------
constexpr U kAfterEquip = bof3::addr::BattleForm_ApplyStats;       // () BE5's
constexpr U kMemberRefresh = bof3::addr::Battle_RecalcStats;    // (actor) BE6's
constexpr U kItemFlags = bof3::addr::Item_UseFlags;        // (category, item) -> al; group TWO's (2026-10-06), the value unchanged
constexpr U kEnemiesClear = bof3::addr::BattleEnemy_ClearStates;     // ()

}  // namespace at
}  // namespace battle_e4
