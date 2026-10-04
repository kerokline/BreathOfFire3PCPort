// Internal to rest_3a.cpp and rest_3a_fuzz.cpp: the cells group R3A's
// functions touch that symbols.toml has no name for (or names as data, whose
// names are macros there), and the .data tables they read in place.
// docs/rest_3a.md.
//
// No raw-address callee: every function the group calls is ours by now (the
// earlier waves' and rounds') or the group's own, called by name; the stack
// tables' entries are called by the addresses the originals build them from
// (boss_harness::Phase), which in the game are Capcom's entries or the jmp
// the inject put there.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_3a {
namespace at {

using U = std::uint32_t;

// --- the world map task (area 33's frame states, WorldMap33_FrameStates) ---
constexpr U kMapMode = 0x9045FA;      // u8: the map's mode byte (2 holds the frame where it is)

// --- BATE, game mode 9 (the byte block 0x929F00; battle_e1_callees.h) ---
constexpr U kMode = 0x929F00;         // u8: BATE's state (BattleExtra_States)
constexpr U kModeStep = 0x929F01;     // u8: the state's step
constexpr U kModeSub = 0x929F02;      // u8: the step's sub-step
constexpr U kModeRequest = 0x904C9F;  // u8: BattleExtra_Start's state - 2 (scena_sc6.md: set with the request 7)
constexpr U kGameStep = 0x66C7EA;     // u16 Game_Step
constexpr U kStates = 0x64ADAC;       // BattleExtra_States: 4, by kMode
constexpr U kTallySteps = 0x64ADBC;   // BattleExtra_TallySteps: 3, by kModeStep
constexpr U kTallyOpenSteps = 0x64ADC8;  // BattleExtra_TallyOpenSteps: 2, by kModeSub

// BATE's equipment screen: the window records it sets up (WindowRecords
// 0x803160, 0x24 each: records 0..4), the six chosen bytes the list edits,
// character record 7 (CharacterRecords + 7 * 0xA4) and its six equipment
// bytes +0x12..+0x17, the categories by equipment slot (.data, six bytes).
constexpr U kWindows = 0x803160;
constexpr U kWin0Cursor = 0x80316C;   // window 0's +0xC: the party index set up
constexpr U kWin0Category = 0x803168; // window 0's +8
constexpr U kWin0Item = 0x80316D;     // window 0's +0xD
constexpr U kWin1Slot = 0x80318E;     // window 1's +0xA: the slot cursor (0 or 3: BattleExtra_EquipSlotInput's xor 3)
constexpr U kWin13Dim = 0x803341;     // window 13's +0xD: 1 when Item_EquipMask's answer has bit 0 clear
constexpr U kPartyList = 0x904062;    // u8 x 3: the party list's first three entries
constexpr U kChosen = 0x675EB8;       // u8 x 6: the list's choice per equipment slot (window 0 and 1's +0x20 point here)
constexpr unsigned kChosenCount = 6;
constexpr U kGuest = 0x903EEC;        // character record 7
constexpr U kGuestEquip = 0x903EFE;   // its +0x12..+0x17
constexpr U kCategories = 0x64AE20;   // .data, u8 x 6: the item category of each equipment slot

// --- the battle engine ---
constexpr U kStep = 0x904AA1;
constexpr U kSubStep = 0x904AA2;
constexpr U kFlags = 0x904AA8;        // the round flags (a dword is read for bit 15)
constexpr U kMusicFlags = 0x904AE5;   // u8: bit 0x40 keeps the battle's music
constexpr U kActor = 0x904B34;        // u8: the acting actor
constexpr U kAction = 0x904B40;       // unsigned char *: the action record (+2 the ability id)
constexpr U kAreaNumber = 0x904EFC;   // u16 Game_AreaNumber
constexpr U kWaitWord = 0x66C810;     // u16 MoveScript_WaitWordDA: a transition runs while non-zero
constexpr U kPassFlags = 0x7E0918;    // u8 Draw_PassFlags
constexpr U kLossSteps = 0x64AF70;    // BattleEnd_LossSteps: 3, by kSubStep
constexpr U kRestoreSteps = 0x64AF7C; // BattleEnd_RestoreSteps: 5, by kSubStep

// The effect tasks: the slot being run, its owner, Sprite_Current's cells.
constexpr U kTaskCurrent = 0x93B8C4;  // unsigned char *: the slot BattleTask_RunAll runs
constexpr U kTaskOwner = 0x93B940;    // unsigned char *: that slot's +0x80
constexpr U kFieldState = 0x905D98;   // unsigned char *: Field_State, the member a party step runs for
constexpr U kCameraDistance = 0x903840;  // u16 Camera_Distance
constexpr U kTint = 0x903850;         // u8 x 3: DamageScratch, the tint's r g b BattleWin_DrawTileTint reads
constexpr U kRedraw = 0x905E69;       // u8 MapView_Redraw
constexpr U kBannerParty = 0x669DFC;  // .data: the banner string a party actor's flash shows (a pointer)
constexpr U kBannerEnemy = 0x669E00;  // .data: an enemy actor's
constexpr U kAbilityFlags = 0x65C4DD; // Ability_Records + 0x15: a byte per 24-byte record (bit 1 read here)
constexpr U kStatusIcons = 0x64B048;  // .data, u8 x 16: the status icon's animation, 0xFF none
constexpr unsigned kStatusIconCount = 16;
constexpr U kPartySet = 0x90412C;     // u8: the party set (PartySet_Select)
constexpr U kRestoreFilesA = 0x64EA84;  // .data: a pointer to 20 u16 DAT files by party set (+8 0 or 1)
constexpr U kRestoreFilesB = 0x64EA88;  // .data: the same for any other +8
constexpr unsigned kPartySetCount = 20;
constexpr U kPalettes = 0x80D380;     // the members' palettes, 0x40 bytes each
constexpr U kMembers = 0x802D40;      // ObjTrio
constexpr U kMemberSize = 0x14C;
constexpr U kEnemies = 0x93B960;      // the eight enemy objects
constexpr U kEnemySize = 0x128;
constexpr U kEnemyCurrent = 0x939AD8; // unsigned char *: the enemy BattleEnemy_SetAnimation runs on

// --- New Game ---
constexpr U kCharRecords = 0x903A70;  // CharacterRecords, 8 of 0xA4
constexpr U kCharSize = 0xA4;
constexpr unsigned kCharCount = 8;
constexpr U kDefaultRecords = 0x64B390;  // Char_DefaultRecords: seven, then the whelp's at 0x64B80C
constexpr U kWhelpRecord = 0x64B80C;
constexpr U kWhelpSlot = 0x669736;    // u8 Char_WhelpSlot
constexpr U kLevelDivisors = 0x64B8B0;  // .data: a (divisor, addend) byte pair per default record

}  // namespace at
}  // namespace rest_3a
