// Internal to scena_sc7.cpp and scena_sc7_fuzz.cpp: the cells chapters 7 and 8
// read and write, the chapter tables they dispatch through, and the callees
// nobody owns yet, by address (docs/scena_sc7.md section 6). Every address
// here is load-bearing (CLAUDE.md rule 3).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc7 {

namespace at {

// The scenario bytes (docs/field-modes.md section 2).
// kStatusBits: now Field_StatusBits (symbols.toml). Field_StatusBits
constexpr std::uint32_t kState = 0x8034E2;        // s8: the chapter's state (0 start, 1 area entry, 2 the run)
// kRun: now MoveScript_Var7 (symbols.toml). s8: MoveScript_Var7, the scene the run plays
// kStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the scene's step
// kTimer: now MoveScript_PartyRecords[0x66] (symbols.toml). u16: the scene's timer
// kByteFD: now Cond_ByteFD (symbols.toml). Cond_ByteFD
constexpr std::uint32_t kCounters = 0x903848;     // four script counter bytes, cleared as a dword by chapter 8
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter2 = 0x90384A;
constexpr std::uint32_t kCounter3 = 0x90384B;
constexpr std::uint32_t kSlot = 0x903850;         // u8 (a u16 store in two places): the effect or sprite slot just taken
// kFlagBank: now AreaW0c_FlagRow (symbols.toml). unsigned char *: the flag row Flags_Test / Flags_Set are given
// kRow6: now Cond_Flags[0x30] (symbols.toml). chapter 6's flag row, tested by two of chapter 7's object handlers
// kRow7: now Cond_Flags[0x38] (symbols.toml). chapter 7's flag row (Cond_Flags +0x38), cleared by its start
// kRow8: now Cond_Flags[0x40] (symbols.toml). chapter 8's flag row (Cond_Flags +0x40), cleared by its start
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the story flags (Cond_Flags +0xA0), passed by address
// kEventFlags: now ScenaSc7_EventFlags (symbols.toml). a flag row passed by address (chapter 8 run 3)
// kPartyList5: now Cond_Flags[0xd5] (symbols.toml). two bytes of the party lists chapter 8 run 8 keeps and puts back
// kPartyList6: now Cond_Flags[0xd6] (symbols.toml).
constexpr std::uint32_t kKeyItems = 0x904554;     // 32 key-item bytes (0x591900 appends to them)

// The field's cells.
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
// kArea: now Game_AreaNumber (symbols.toml). Game_AreaNumber, u16
// kByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
constexpr std::uint32_t kScriptFlags = 0x9039A2;  // Field_ScriptFlags, u16
constexpr std::uint32_t kScriptFlagsHi = 0x9039A3;
// kWait: now MoveScript_WaitWordDA (symbols.toml). MoveScript_WaitWordDA, u16
// kRequest: now Field_Request (symbols.toml). Field_Request, u8
constexpr std::uint32_t kMessage = 0x7DEE48;      // u16: the message id last opened
// kPendingKind: now AreaW0c_PendingKind (symbols.toml). u8: the pending area change's kind
// kAreaTrack: now AreaW1f_MusicCurrent (symbols.toml). u8: the track the next area wants
// kAreaTransition: now ModeFlow_AreaTransition (symbols.toml). u8: GameMode_Field's transition kind for the area change
// kKind2X: now Field_Kind2X (symbols.toml). Field_Kind2X, s32
// kKind2Z: now Field_Kind2Z (symbols.toml). Field_Kind2Z, s32
// kKind2Hold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
// kLoadByte: now ScenaSc6_Load10 (symbols.toml). u8 set 1 by chapter 8 run 11 (Gfx_UnpackNext +0x34)
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). MoveScript_F3Divisor, u16
// kMoveSpeeds: now Field_MoveSpeeds (symbols.toml). Field_MoveSpeeds (bytes +4 and +5 read)
// kCamDistance: now Camera_Distance (symbols.toml). Camera_Distance, u16
// kShiftY: now Camera_ShiftY (symbols.toml). Camera_ShiftY, s16
constexpr std::uint32_t kFocusObject = 0x903804;  // a sprite pointer after Camera_ShiftY: chapter 8 runs 7 and 8 read through it
// kYaw: now Camera_Angles (symbols.toml). Camera_Angles[0], s16
// kAngleY: now Camera_Angles[1] (symbols.toml). Camera_Angles[1], s16
constexpr std::uint32_t kPitch = 0x929ECC;        // Camera_Angles[2] (Cond_AngleFB's low half), s16
// kRedraw: now MapView_Redraw (symbols.toml). MapView_Redraw
// kMusicTrack: now Music_Track (symbols.toml). Music_Track
constexpr std::uint32_t kMemberCount = 0x929EC0;  // Field_MemberCount, u8
// kFrameCounter: now Frame_Counter (symbols.toml). Frame_Counter
// kEffectState: now MoveScript_EffectState (symbols.toml). MoveScript_EffectState: bytes [4], [7] and [8] index the character records
constexpr std::uint32_t kEffectArg = 0x669744;    // MoveScript_EffectArg, s8 by member 0's +0x89
constexpr std::uint32_t kDrawPool = 0x9039F0;    // three cells chapter 8 run 2 sets (+3, +4, +6 u16; DrawItemPool_Top +0x1F..+0x22)
constexpr std::uint32_t kTextRecords = 0x904CE0;  // Text_Records: an item's 16-byte name copied to +0 and +0x20

// The party's field objects (ObjTrio, stride 0x14C), the character records
// (CharacterRecords, stride 0xA4: +9, +0xB, +0xC, +0x11, +0x12..+0x15, +0x18,
// +0x58, +0x5B, +0x5C written or read here) and the sprite and effect pools.
constexpr std::uint32_t kMembers = 0x802D40;
constexpr std::uint32_t kMemberStride = 0x14C;
// kLeaderKind: now ObjTrio[8] (symbols.toml). member 0 +8
// kLeaderSlot: now ObjTrio[0xb] (symbols.toml). member 0 +0xB: chapter 7 run 4 keeps its effect slot here
// kLeaderX: now ObjTrio[0x34] (symbols.toml). member 0 +0x34
// kLeaderZ: now ObjTrio[0x38] (symbols.toml). member 0 +0x38
// kMember1: now Field_Members[4] (symbols.toml). member 1 +0
constexpr std::uint32_t kCharRecords = 0x903A70; // CharacterRecords
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kRecord4 = 0x903D00;      // record 4, the one chapter 8 run 11 sets up (Char_RecalcStats is given it)
// kSprites: now Sprite_Objects (symbols.toml). Sprite_Objects, 0xA4-byte records
constexpr std::uint32_t kSpriteStride = 0xA4;
// kSpritesExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
// kKind2Sprite: now Sprite_Kind2 (symbols.toml). Sprite_Kind2
// kSpriteCurrent: now Sprite_Current (symbols.toml).
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kEffects: now Effect_Objects (symbols.toml). Effect_Objects, 0x80-byte records

// Chapter 8's own cells in .data: a kept area change (+0 u8 flags, +4 x,
// +8 z), two kept party-list bytes (+0xC, +0xD) and a kept effect slot (+0xF).
// kKept: now Scena08_Kept (symbols.toml).
// kKeptFlags: now Scena08_Kept (symbols.toml).
// kKeptX: now Scena08_Kept[4] (symbols.toml).
// kKeptZ: now Scena08_Kept[8] (symbols.toml).
// kKeptList5: now Scena08_Kept[0xc] (symbols.toml).
// kKeptList6: now Scena08_Kept[0xd] (symbols.toml).
// kKeptEffect: now Scena08_Kept[0xf] (symbols.toml).

// Chapter 7's tables (0x6611B8 Scena07_Hooks and the call tables are SCH's).
// kStates7: now Scena07_States (symbols.toml). Scena07_States, 3 entries; Scena07_Runs follows at +0xC
// kRuns7: now Scena07_Runs (symbols.toml). Scena07_Runs, 9 entries (run 0 the bare ret 0x437CC0)
// kTimed7: now Scena07_TimedRecords (symbols.toml). Scena07_TimedEffects, 12 records of 6 bytes (x, z, s16 frame)
// kPlaced7: now Scena07_PlacedObjects (symbols.toml). Scena07_PlacedObjects, 14 event-op records of 0x11 bytes
constexpr std::uint32_t kPlacedSeat7 = 0x661338;  // Scena07_PlacedSeats, 14 bytes (0: none)
constexpr std::uint32_t kShake7 = 0x661244;       // Scena07_Shake, 4 s8
// kObjects7: now Scena07_ObjectHandlers (symbols.toml). Scena07_ObjectHandlers, 5 entries by object +0x86
// kCells7: now Scena07_Cells (symbols.toml). Scena07_Cells, one 5-byte cell record
// kCellHandlers7: now Scena07_CellHandlers (symbols.toml). Scena07_CellHandlers, 1 entry
// Chapter 8's.
// kStates8: now Scena08_States (symbols.toml). Scena08_States, 3 entries; Scena08_Runs follows at +0xC
// kRuns8: now Scena08_Runs (symbols.toml). Scena08_Runs, 12 entries (run 0 the bare ret 0x437CC0)
// kPairOps8: now Scena08_PairOps (symbols.toml). Scena08_PairOps: two 16-byte event-op records (+0, +0x10)
// kObjects8: now Scena08_ObjectHandlers (symbols.toml). Scena08_ObjectHandlers, 3 entries by object +0x86

}  // namespace at

// Callees nobody owns this wave, by address (docs/scena_sc7.md section 6).
constexpr std::uint32_t kPlaceParty = bof3::addr::Party_PlaceForBattle;       // (x, z, u8): the party placed at a point for an event battle
constexpr std::uint32_t kPartyRestore = bof3::addr::Party_HealJoined;     // (): the party's records refreshed
constexpr std::uint32_t kStatusBit80 = bof3::addr::Field_SetStatus80;      // (): Field_StatusBits |= 0x80
constexpr std::uint32_t kCellFind = bof3::addr::Field_CellTriggerAt;         // (records, n, x, z): the cell record matched, negative none
constexpr std::uint32_t kSpriteFindFree = bof3::addr::Sprite_FindFree;   // (): a free Sprite_Objects index 0..0x1D, 0xFF none
constexpr std::uint32_t kMusicStop = bof3::addr::Sound_StopMusic;        // (): the music buffer stopped
constexpr std::uint32_t kKeyItemAdd = bof3::addr::KeyItem_Add;       // (u8): into the first free of 32 bytes at 0x904554
constexpr std::uint32_t kCall591BE0 = bof3::addr::Zenny_Add;       // (0xBB8, 0): chapter 8 run 4 calls it once (not read here)
constexpr std::uint32_t kCall498DE0 = bof3::addr::Char_LevelUp;       // (4): chapter 8's join calls it before the record's stats (not read here)

}  // namespace scena_sc7
