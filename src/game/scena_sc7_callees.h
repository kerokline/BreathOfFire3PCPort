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
constexpr std::uint32_t kStatusBits = 0x8034E1;   // Field_StatusBits
constexpr std::uint32_t kState = 0x8034E2;        // s8: the chapter's state (0 start, 1 area entry, 2 the run)
constexpr std::uint32_t kRun = 0x8034E4;          // s8: MoveScript_Var7, the scene the run plays
constexpr std::uint32_t kStep = 0x8034E5;         // u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;        // u16: the scene's timer
constexpr std::uint32_t kByteFD = 0x8034F1;       // Cond_ByteFD
constexpr std::uint32_t kCounters = 0x903848;     // four script counter bytes, cleared as a dword by chapter 8
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter2 = 0x90384A;
constexpr std::uint32_t kCounter3 = 0x90384B;
constexpr std::uint32_t kSlot = 0x903850;         // u8 (a u16 store in two places): the effect or sprite slot just taken
constexpr std::uint32_t kFlagBank = 0x929ED0;     // unsigned char *: the flag row Flags_Test / Flags_Set are given
constexpr std::uint32_t kRow6 = 0x903FC0;         // chapter 6's flag row, tested by two of chapter 7's object handlers
constexpr std::uint32_t kRow7 = 0x903FC8;         // chapter 7's flag row (Cond_Flags +0x38), cleared by its start
constexpr std::uint32_t kRow8 = 0x903FD0;         // chapter 8's flag row (Cond_Flags +0x40), cleared by its start
constexpr std::uint32_t kStoryFlags = 0x904030;   // the story flags (Cond_Flags +0xA0), passed by address
constexpr std::uint32_t kEventFlags = 0x904650;   // a flag row passed by address (chapter 8 run 3)
constexpr std::uint32_t kPartyList5 = 0x904065;   // two bytes of the party lists chapter 8 run 8 keeps and puts back
constexpr std::uint32_t kPartyList6 = 0x904066;
constexpr std::uint32_t kKeyItems = 0x904554;     // 32 key-item bytes (0x591900 appends to them)

// The field's cells.
constexpr std::uint32_t kPassFlags = 0x7E0918;    // Draw_PassFlags
constexpr std::uint32_t kArea = 0x904EFC;         // Game_AreaNumber, u16
constexpr std::uint32_t kByteFE = 0x905E20;       // Cond_ByteFE
constexpr std::uint32_t kScriptFlags = 0x9039A2;  // Field_ScriptFlags, u16
constexpr std::uint32_t kScriptFlagsHi = 0x9039A3;
constexpr std::uint32_t kWait = 0x66C810;         // MoveScript_WaitWordDA, u16
constexpr std::uint32_t kRequest = 0x66C7D8;      // Field_Request, u8
constexpr std::uint32_t kMessage = 0x7DEE48;      // u16: the message id last opened
constexpr std::uint32_t kPendingKind = 0x937F98;  // u8: the pending area change's kind
constexpr std::uint32_t kAreaTrack = 0x904CD0;    // u8: the track the next area wants
constexpr std::uint32_t kAreaTransition = 0x904EE0;  // u8: GameMode_Field's transition kind for the area change
constexpr std::uint32_t kKind2X = 0x905E64;       // Field_Kind2X, s32
constexpr std::uint32_t kKind2Z = 0x905E60;       // Field_Kind2Z, s32
constexpr std::uint32_t kKind2Hold = 0x929F12;    // Field_Kind2Hold
constexpr std::uint32_t kLoadByte = 0x929F10;     // u8 set 1 by chapter 8 run 11 (Gfx_UnpackNext +0x34)
constexpr std::uint32_t kF3Divisor = 0x937F8C;    // MoveScript_F3Divisor, u16
constexpr std::uint32_t kMoveSpeeds = 0x6697F0;   // Field_MoveSpeeds (bytes +4 and +5 read)
constexpr std::uint32_t kCamDistance = 0x903840;  // Camera_Distance, u16
constexpr std::uint32_t kShiftY = 0x903802;       // Camera_ShiftY, s16
constexpr std::uint32_t kFocusObject = 0x903804;  // a sprite pointer after Camera_ShiftY: chapter 8 runs 7 and 8 read through it
constexpr std::uint32_t kYaw = 0x929EC8;          // Camera_Angles[0], s16
constexpr std::uint32_t kAngleY = 0x929ECA;       // Camera_Angles[1], s16
constexpr std::uint32_t kPitch = 0x929ECC;        // Camera_Angles[2] (Cond_AngleFB's low half), s16
constexpr std::uint32_t kRedraw = 0x905E69;       // MapView_Redraw
constexpr std::uint32_t kMusicTrack = 0x904131;   // Music_Track
constexpr std::uint32_t kMemberCount = 0x929EC0;  // Field_MemberCount, u8
constexpr std::uint32_t kFrameCounter = 0x937F94; // Frame_Counter
constexpr std::uint32_t kEffectState = 0x66972C;  // MoveScript_EffectState: bytes [4], [7] and [8] index the character records
constexpr std::uint32_t kEffectArg = 0x669744;    // MoveScript_EffectArg, s8 by member 0's +0x89
constexpr std::uint32_t kDrawPool = 0x9039F0;    // three cells chapter 8 run 2 sets (+3, +4, +6 u16; DrawItemPool_Top +0x1F..+0x22)
constexpr std::uint32_t kTextRecords = 0x904CE0;  // Text_Records: an item's 16-byte name copied to +0 and +0x20

// The party's field objects (ObjTrio, stride 0x14C), the character records
// (CharacterRecords, stride 0xA4: +9, +0xB, +0xC, +0x11, +0x12..+0x15, +0x18,
// +0x58, +0x5B, +0x5C written or read here) and the sprite and effect pools.
constexpr std::uint32_t kMembers = 0x802D40;
constexpr std::uint32_t kMemberStride = 0x14C;
constexpr std::uint32_t kLeaderKind = 0x802D48;   // member 0 +8
constexpr std::uint32_t kLeaderSlot = 0x802D4B;   // member 0 +0xB: chapter 7 run 4 keeps its effect slot here
constexpr std::uint32_t kLeaderX = 0x802D74;      // member 0 +0x34
constexpr std::uint32_t kLeaderZ = 0x802D78;      // member 0 +0x38
constexpr std::uint32_t kMember1 = 0x802E8C;      // member 1 +0
constexpr std::uint32_t kCharRecords = 0x903A70; // CharacterRecords
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kRecord4 = 0x903D00;      // record 4, the one chapter 8 run 11 sets up (Char_RecalcStats is given it)
constexpr std::uint32_t kSprites = 0x7DEE80;      // Sprite_Objects, 0xA4-byte records
constexpr std::uint32_t kSpriteStride = 0xA4;
constexpr std::uint32_t kSpritesExtra = 0x802000; // Sprite_ObjectsExtra
constexpr std::uint32_t kKind2Sprite = 0x7E0940;  // Sprite_Kind2
constexpr std::uint32_t kSpriteCurrent = 0x937F88;
constexpr std::uint32_t kActiveMember = 0x9035A4; // Field_ActiveMember
constexpr std::uint32_t kEffects = 0x7E11E0;      // Effect_Objects, 0x80-byte records

// Chapter 8's own cells in .data: a kept area change (+0 u8 flags, +4 x,
// +8 z), two kept party-list bytes (+0xC, +0xD) and a kept effect slot (+0xF).
constexpr std::uint32_t kKept = 0x6BC720;
constexpr std::uint32_t kKeptFlags = 0x6BC720;
constexpr std::uint32_t kKeptX = 0x6BC724;
constexpr std::uint32_t kKeptZ = 0x6BC728;
constexpr std::uint32_t kKeptList5 = 0x6BC72C;
constexpr std::uint32_t kKeptList6 = 0x6BC72D;
constexpr std::uint32_t kKeptEffect = 0x6BC72F;

// Chapter 7's tables (0x6611B8 Scena07_Hooks and the call tables are SCH's).
constexpr std::uint32_t kStates7 = 0x6611CC;      // Scena07_States, 3 entries; Scena07_Runs follows at +0xC
constexpr std::uint32_t kRuns7 = 0x6611D8;        // Scena07_Runs, 9 entries (run 0 the bare ret 0x437CC0)
constexpr std::uint32_t kTimed7 = 0x6611FC;       // Scena07_TimedEffects, 12 records of 6 bytes (x, z, s16 frame)
constexpr std::uint32_t kPlaced7 = 0x661248;      // Scena07_PlacedObjects, 14 event-op records of 0x11 bytes
constexpr std::uint32_t kPlacedSeat7 = 0x661338;  // Scena07_PlacedSeats, 14 bytes (0: none)
constexpr std::uint32_t kShake7 = 0x661244;       // Scena07_Shake, 4 s8
constexpr std::uint32_t kObjects7 = 0x661348;     // Scena07_ObjectHandlers, 5 entries by object +0x86
constexpr std::uint32_t kCells7 = 0x66135C;       // Scena07_Cells, one 5-byte cell record
constexpr std::uint32_t kCellHandlers7 = 0x661364; // Scena07_CellHandlers, 1 entry
// Chapter 8's.
constexpr std::uint32_t kStates8 = 0x66137C;      // Scena08_States, 3 entries; Scena08_Runs follows at +0xC
constexpr std::uint32_t kRuns8 = 0x661388;        // Scena08_Runs, 12 entries (run 0 the bare ret 0x437CC0)
constexpr std::uint32_t kPairOps8 = 0x6613B8;     // Scena08_PairOps: two 16-byte event-op records (+0, +0x10)
constexpr std::uint32_t kObjects8 = 0x6613D8;     // Scena08_ObjectHandlers, 3 entries by object +0x86

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
