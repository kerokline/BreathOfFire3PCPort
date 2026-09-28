// Internal to scena_sc3.cpp and scena_sc3_fuzz.cpp: the cells chapters 3 and 4
// read and write, the chapter tables they dispatch through, and the callees
// nobody owns yet, by address (docs/scena_sc3.md section 6). Every address
// here is load-bearing (CLAUDE.md rule 3).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc3 {

namespace at {

// The scenario bytes (docs/field-modes.md section 2).
constexpr std::uint32_t kState = 0x8034E2;        // s8: the chapter's state (0 start, 1 area entry, 2 the run)
constexpr std::uint32_t kEffectByte = 0x8034E3;   // s8: an effect slot kept across frames (indexed signed)
constexpr std::uint32_t kRun = 0x8034E4;          // s8: MoveScript_Var7, the scene the run plays
constexpr std::uint32_t kStep = 0x8034E5;         // u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;        // u16: the scene's timer
constexpr std::uint32_t kCounters = 0x903848;     // four script counter bytes (MoveScript_CounterOps); cleared as a dword
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kSlot = 0x903850;         // u8 (read back as a dword's low byte): the effect slot just taken
constexpr std::uint32_t kFlagBank = 0x929ED0;     // unsigned char *: the flag row Flags_Test / Flags_Set are given
constexpr std::uint32_t kRow3 = 0x903FA8;         // chapter 3's flag row (Cond_Flags +0x18), cleared by its start
constexpr std::uint32_t kRow3Byte2 = 0x903FAA;    // its bits 0x10..0x17
constexpr std::uint32_t kRow4 = 0x903FB0;         // chapter 4's flag row (Cond_Flags +0x20), cleared by its start
constexpr std::uint32_t kStoryFlags = 0x904650;   // a flag row passed by address (two Flags_Set calls)

// The field's cells.
constexpr std::uint32_t kPassFlags = 0x7E0918;    // Draw_PassFlags
constexpr std::uint32_t kArea = 0x904EFC;         // Game_AreaNumber, u16
constexpr std::uint32_t kByteFD = 0x8034F1;       // Cond_ByteFD
constexpr std::uint32_t kByteFE = 0x905E20;       // Cond_ByteFE
constexpr std::uint32_t kStatusBits = 0x8034E1;   // Field_StatusBits
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
constexpr std::uint32_t kKind2Mode = 0x905E68;    // u8 after Field_Kind2X: tested for 1 by chapter 3's area 0x45
constexpr std::uint32_t kKind2Hold = 0x929F12;    // Field_Kind2Hold
constexpr std::uint32_t kF3Divisor = 0x937F8C;    // MoveScript_F3Divisor, u16
constexpr std::uint32_t kCamDistance = 0x903840;  // Camera_Distance, u16
constexpr std::uint32_t kYaw = 0x929EC8;          // Camera_Angles[0], s16
constexpr std::uint32_t kAngleY = 0x929ECA;       // Camera_Angles[1], s16
constexpr std::uint32_t kPitch = 0x929ECC;        // Camera_Angles[2] (Cond_AngleFB's low half), s16
constexpr std::uint32_t kElevation = 0x929F1C;    // MapView_Elevation, s32
constexpr std::uint32_t kRedraw = 0x905E69;       // MapView_Redraw
constexpr std::uint32_t kMusicTrack = 0x904131;   // Music_Track
constexpr std::uint32_t kGameMode = 0x66C7E8;     // Game_Mode, u16
constexpr std::uint32_t kEffectState = 0x66972C;  // MoveScript_EffectState's first byte
constexpr std::uint32_t kLoadNext = 0x929F0C;     // u8 set 0xFE with the mode 7 switch (Gfx_UnpackNext +0x30)
constexpr std::uint32_t kLoadByte = 0x929F00;     // u8 cleared with it (Gfx_UnpackNext +0x24)
constexpr std::uint32_t kMemberCount3 = 0x929EC3; // u8 cleared with it (Field_MemberCount +3)
constexpr std::uint32_t kMemberCount2 = 0x929EC2; // u8 set 1 with it (Field_MemberCount +2)

// The party's field objects (ObjTrio, stride 0x14C) and the sprite pool.
constexpr std::uint32_t kMembers = 0x802D40;
constexpr std::uint32_t kMemberStride = 0x14C;
constexpr std::uint32_t kLeaderKind = 0x802D48;   // member 0 +8
constexpr std::uint32_t kSprites = 0x7DEE80;      // Sprite_Objects, 0xA4-byte records
constexpr std::uint32_t kSpriteStride = 0xA4;
constexpr std::uint32_t kSpriteCurrent = 0x937F88;
constexpr std::uint32_t kActiveMember = 0x9035A4;
constexpr std::uint32_t kEffects = 0x7E11E0;      // Effect_Objects, 0x80-byte records
constexpr std::uint32_t kFrameCounter = 0x937F94;

// Chapter 3's tables (0x660F50 Scena03_Hooks and the call tables are SCH's).
constexpr std::uint32_t kStates3 = 0x660F64;      // Scena03_States, 3 entries; Scena03_Runs follows at +0xC
constexpr std::uint32_t kRuns3 = 0x660F70;        // Scena03_Runs, 9 entries (run 0 the bare ret 0x437CC0)
constexpr std::uint32_t kBob4 = 0x660F94;         // Scena03_Bob4, 4 s8
constexpr std::uint32_t kBob2 = 0x660F98;         // Scena03_Bob2, 2 s8
constexpr std::uint32_t kObjects3 = 0x660F9C;     // Scena03_ObjectHandlers, 9 entries by object +0x86
constexpr std::uint32_t kCells3 = 0x660FC0;       // Scena03_Cells, one 5-byte cell record
constexpr std::uint32_t kCellHandlers3 = 0x660FC8;  // Scena03_CellHandlers, 1 entry
// Chapter 4's.
constexpr std::uint32_t kStates4 = 0x660FE4;      // Scena04_States, 3 entries; Scena04_Runs follows at +0xC
constexpr std::uint32_t kRuns4 = 0x660FF0;        // Scena04_Runs, 3 entries
constexpr std::uint32_t kQuake4 = 0x660FFC;       // Scena04_Quake, 4 s8
constexpr std::uint32_t kTremor4 = 0x661000;      // Scena04_TremorSteps, 4 s8
constexpr std::uint32_t kObjects4 = 0x661004;     // Scena04_ObjectHandlers, 1 entry
constexpr std::uint32_t kCells4 = 0x661008;       // Scena04_Cells, one 5-byte cell record
constexpr std::uint32_t kCellHandlers4 = 0x661010;  // Scena04_CellHandlers, 1 entry

}  // namespace at

// Callees nobody owns this wave, by address (docs/scena_sc3.md section 6).
// SE's two shared helpers and SCH's Scenario_CallB are raw here until the
// round's rebinding pass.
constexpr std::uint32_t kLeaderState5 = bof3::addr::Field_StartEventBattle;     // group SE: (u8) - member 0's +1 = 5, 0x904AAA = the byte, 0x905BA5 |= 0x10
constexpr std::uint32_t kEventObjFace = bof3::addr::EventObj_Face;     // group SE: EventObj_Face (Sprite_Current's facing)
constexpr std::uint32_t kCallB = 0x5341C0;            // Scenario_CallB (SCH names it), the caller's arguments in place
constexpr std::uint32_t kPlaceParty = bof3::addr::Party_PlaceForBattle;       // (x, z, u8): the party placed at a point
constexpr std::uint32_t kPartyRestore = bof3::addr::Party_HealJoined;     // (): the party's records refreshed (Char_RecalcStats)
constexpr std::uint32_t kStatusBit80 = bof3::addr::Field_SetStatus80;      // (): Field_StatusBits |= 0x80
constexpr std::uint32_t kCellFind = bof3::addr::Field_CellTriggerAt;         // (records, n, x, z): the cell record matched, 0xFF none
constexpr std::uint32_t kSpriteFindFree = bof3::addr::Sprite_FindFree;   // (): a free Sprite_Objects index 0..0x1D, 0xFF none
constexpr std::uint32_t kMusicStop = 0x587B80;        // (): the music buffer stopped (jmp 0x5A6FF0)
constexpr std::uint32_t kItemEvent = bof3::addr::AbilityList_Add;        // (u8 id, x, u8, y): 0x97 with the effect state's byte
constexpr std::uint32_t kKeyItemAdd = bof3::addr::KeyItem_Add;       // (u8): into the first free of 32 bytes at 0x904554

}  // namespace scena_sc3
