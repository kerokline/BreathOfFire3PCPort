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
// kEffectByte: now MoveScript_PartyRecords[0x63] (symbols.toml). s8: an effect slot kept across frames (indexed signed)
// kRun: now MoveScript_Var7 (symbols.toml). s8: MoveScript_Var7, the scene the run plays
// kStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the scene's step
// kTimer: now MoveScript_PartyRecords[0x66] (symbols.toml). u16: the scene's timer
constexpr std::uint32_t kCounters = 0x903848;     // four script counter bytes (MoveScript_CounterOps); cleared as a dword
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kSlot = 0x903850;         // u8 (read back as a dword's low byte): the effect slot just taken
// kFlagBank: now AreaW0c_FlagRow (symbols.toml). unsigned char *: the flag row Flags_Test / Flags_Set are given
// kRow3: now Cond_Flags[0x18] (symbols.toml). chapter 3's flag row (Cond_Flags +0x18), cleared by its start
// kRow3Byte2: now Cond_Flags[0x1a] (symbols.toml). its bits 0x10..0x17
// kRow4: now Cond_Flags[0x20] (symbols.toml). chapter 4's flag row (Cond_Flags +0x20), cleared by its start
// kStoryFlags: now ScenaSc7_EventFlags (symbols.toml). a flag row passed by address (two Flags_Set calls)

// The field's cells.
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
// kArea: now Game_AreaNumber (symbols.toml). Game_AreaNumber, u16
// kByteFD: now Cond_ByteFD (symbols.toml). Cond_ByteFD
// kByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
// kStatusBits: now Field_StatusBits (symbols.toml). Field_StatusBits
constexpr std::uint32_t kScriptFlags = 0x9039A2;  // Field_ScriptFlags, u16
constexpr std::uint32_t kScriptFlagsHi = 0x9039A3;
// kWait: now MoveScript_WaitWordDA (symbols.toml). MoveScript_WaitWordDA, u16
// kRequest: now Field_Request (symbols.toml). Field_Request, u8
constexpr std::uint32_t kMessage = 0x7DEE48;      // u16: the message id last opened
constexpr std::uint32_t kPendingKind = 0x937F98;  // u8: the pending area change's kind
// kAreaTrack: now AreaW1f_MusicCurrent (symbols.toml). u8: the track the next area wants
// kAreaTransition: now ModeFlow_AreaTransition (symbols.toml). u8: GameMode_Field's transition kind for the area change
// kKind2X: now Field_Kind2X (symbols.toml). Field_Kind2X, s32
// kKind2Z: now Field_Kind2Z (symbols.toml). Field_Kind2Z, s32
// kKind2Mode: now AreaW1a_EntryZone (symbols.toml). u8 after Field_Kind2X: tested for 1 by chapter 3's area 0x45
// kKind2Hold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). MoveScript_F3Divisor, u16
// kCamDistance: now Camera_Distance (symbols.toml). Camera_Distance, u16
// kYaw: now Camera_Angles (symbols.toml). Camera_Angles[0], s16
// kAngleY: now Camera_Angles[1] (symbols.toml). Camera_Angles[1], s16
constexpr std::uint32_t kPitch = 0x929ECC;        // Camera_Angles[2] (Cond_AngleFB's low half), s16
// kElevation: now MapView_Elevation (symbols.toml). MapView_Elevation, s32
// kRedraw: now MapView_Redraw (symbols.toml). MapView_Redraw
// kMusicTrack: now Music_Track (symbols.toml). Music_Track
// kGameMode: now Game_Mode (symbols.toml). Game_Mode, u16
// kEffectState: now MoveScript_EffectState (symbols.toml). MoveScript_EffectState's first byte
// kLoadNext: now ModeStates_ShopObject (symbols.toml). u8 set 0xFE with the mode 7 switch (Gfx_UnpackNext +0x30)
// kLoadByte: now BattleE1_Mode (symbols.toml). u8 cleared with it (Gfx_UnpackNext +0x24)
// kMemberCount3: now ScenaSc1_ShopByteB (symbols.toml). u8 cleared with it (Field_MemberCount +3)
constexpr std::uint32_t kMemberCount2 = 0x929EC2; // u8 set 1 with it (Field_MemberCount +2)

// The party's field objects (ObjTrio, stride 0x14C) and the sprite pool.
constexpr std::uint32_t kMembers = 0x802D40;
constexpr std::uint32_t kMemberStride = 0x14C;
// kLeaderKind: now ObjTrio[8] (symbols.toml). member 0 +8
// kSprites: now Sprite_Objects (symbols.toml). Sprite_Objects, 0xA4-byte records
constexpr std::uint32_t kSpriteStride = 0xA4;
// kSpriteCurrent: now Sprite_Current (symbols.toml).
// kActiveMember: now Field_ActiveMember (symbols.toml).
// kEffects: now Effect_Objects (symbols.toml). Effect_Objects, 0x80-byte records
// kFrameCounter: now Frame_Counter (symbols.toml).

// Chapter 3's tables (0x660F50 Scena03_Hooks and the call tables are SCH's).
// kStates3: now Scena03_States (symbols.toml). Scena03_States, 3 entries; Scena03_Runs follows at +0xC
// kRuns3: now Scena03_Runs (symbols.toml). Scena03_Runs, 9 entries (run 0 the bare ret 0x437CC0)
// kBob4: now Scena03_Bob4 (symbols.toml). Scena03_Bob4, 4 s8
// kBob2: now Scena03_Bob2 (symbols.toml). Scena03_Bob2, 2 s8
// kObjects3: now Scena03_ObjectHandlers (symbols.toml). Scena03_ObjectHandlers, 9 entries by object +0x86
// kCells3: now Scena03_Cells (symbols.toml). Scena03_Cells, one 5-byte cell record
// kCellHandlers3: now Scena03_CellHandlers (symbols.toml). Scena03_CellHandlers, 1 entry
// Chapter 4's.
// kStates4: now Scena04_States (symbols.toml). Scena04_States, 3 entries; Scena04_Runs follows at +0xC
// kRuns4: now Scena04_Runs (symbols.toml). Scena04_Runs, 3 entries
constexpr std::uint32_t kQuake4 = 0x660FFC;       // Scena04_Quake, 4 s8
constexpr std::uint32_t kTremor4 = 0x661000;      // Scena04_TremorSteps, 4 s8
// kObjects4: now Scena04_ObjectHandlers (symbols.toml). Scena04_ObjectHandlers, 1 entry
// kCells4: now Scena04_Cells (symbols.toml). Scena04_Cells, one 5-byte cell record
// kCellHandlers4: now Scena04_CellHandlers (symbols.toml). Scena04_CellHandlers, 1 entry

}  // namespace at

// Callees nobody owns this wave, by address (docs/scena_sc3.md section 6).
// SE's two shared helpers and SCH's Scenario_CallB are raw here until the
// round's rebinding pass.
constexpr std::uint32_t kLeaderState5 = bof3::addr::Field_StartEventBattle;     // group SE: (u8) - member 0's +1 = 5, 0x904AAA = the byte, 0x905BA5 |= 0x10
constexpr std::uint32_t kEventObjFace = bof3::addr::EventObj_Face;     // group SE: EventObj_Face (Sprite_Current's facing)
constexpr std::uint32_t kCallB = bof3::addr::Scenario_CallB;            // Scenario_CallB (SCH names it), the caller's arguments in place
constexpr std::uint32_t kPlaceParty = bof3::addr::Party_PlaceForBattle;       // (x, z, u8): the party placed at a point
constexpr std::uint32_t kPartyRestore = bof3::addr::Party_HealJoined;     // (): the party's records refreshed (Char_RecalcStats)
constexpr std::uint32_t kStatusBit80 = bof3::addr::Field_SetStatus80;      // (): Field_StatusBits |= 0x80
constexpr std::uint32_t kCellFind = bof3::addr::Field_CellTriggerAt;         // (records, n, x, z): the cell record matched, 0xFF none
constexpr std::uint32_t kSpriteFindFree = bof3::addr::Sprite_FindFree;   // (): a free Sprite_Objects index 0..0x1D, 0xFF none
constexpr std::uint32_t kMusicStop = bof3::addr::Sound_StopMusic;        // (): the music buffer stopped (jmp 0x5A6FF0)
constexpr std::uint32_t kItemEvent = bof3::addr::AbilityList_Add;        // (u8 id, x, u8, y): 0x97 with the effect state's byte
constexpr std::uint32_t kKeyItemAdd = bof3::addr::KeyItem_Add;       // (u8): into the first free of 32 bytes at 0x904554

}  // namespace scena_sc3
