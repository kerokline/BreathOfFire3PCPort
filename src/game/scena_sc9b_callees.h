// Internal to scena_sc9b.cpp and scena_sc9b_fuzz.cpp: the cells chapter 9's
// tail and chapter 10 (0x557170..0x55C040, group SC9b of round ten) read and
// write, their tables, and the raw addresses of the callees they reach that
// nobody owns yet. docs/scena_sc9b.md.
//
// Calls into code this group does not own, by raw address (SH_AT) - every one
// the engine's, group SX's this wave (Scenario_CallB 0x5341C0, named by SCH and
// not taken; Field_StartEventBattle 0x4410B0 and EventOp_0x 0x57A010, group
// SE's; Scena07_PartyHas89State2 0x550F80, group SC7's, are called by name):
//   0x56D800  (records, n, x, z): the cell record the area and the cell match,
//             or a negative byte for none (chapter 9's cell hook)
//   0x532ED0  (x, z, kind): the party placed before an event battle
//   0x56D6F0  Field_StatusBits |= 0x80
//   0x591900  (u8): the byte into the first free of 32 at 0x904554
//   0x587B80  (): a jmp to 0x5A6FF0, the music buffer stopped
//   0x57CD90  (): the first free Sprite_Objects index 0..0x1D, 0xFF for none
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc9b {

namespace at {

// The scenario bytes (docs/field-modes.md section 2).
constexpr std::uint32_t kState = 0x8034E2;      // s8: the chapter's state, 0..2
constexpr std::uint32_t kRun = 0x8034E4;        // s8: MoveScript_Var7, which scene
constexpr std::uint32_t kStep = 0x8034E5;       // u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;      // u16: a countdown the runs keep
constexpr std::uint32_t kCondFD = 0x8034F1;     // Cond_ByteFD
constexpr std::uint32_t kCounters = 0x903848;   // four script counters (MoveScript_CounterOps)
constexpr std::uint32_t kEffectSlot = 0x903850; // u8 (a word in Scena10_SpriteOp): the slot just taken
constexpr std::uint32_t kFlagBits = 0x929ED0;   // unsigned char *: the bits Flags_Test / Set / Clear take
constexpr std::uint32_t kStoryFlags = 0x904030; // Cond_Flags + 0xA0, handed to Flags_Set / Clear as a literal
constexpr std::uint32_t kRow657 = 0x904657;     // a row handed to Flags_Set as a literal (run 7 step 0xF)
constexpr std::uint32_t kArea = 0x904EFC;       // u16 Game_AreaNumber
constexpr std::uint32_t kPassFlags = 0x7E0918;  // Draw_PassFlags
constexpr std::uint32_t kScriptFlags = 0x9039A2;   // u16 Field_ScriptFlags
constexpr std::uint32_t kRequest = 0x66C7D8;    // Field_Request
constexpr std::uint32_t kWait = 0x66C810;       // u16 MoveScript_WaitWordDA
constexpr std::uint32_t kRedraw = 0x905E69;     // MapView_Redraw
constexpr std::uint32_t kCamDist = 0x903840;    // u16 Camera_Distance
constexpr std::uint32_t kShiftY = 0x903802;     // u16 Camera_ShiftY
constexpr std::uint32_t kMusicCurrent = 0x904CD0;  // u8: Music_Play returns at once while its track equals it
constexpr std::uint32_t kCondFE = 0x905E20;     // Cond_ByteFE
constexpr std::uint32_t kEffects = 0x7E11E0;    // Effect_Objects: records of 0x80
constexpr std::uint32_t kEffectStride = 0x80;
constexpr std::uint32_t kAngleX = 0x929EC8;     // s16 Camera_Angles +0
constexpr std::uint32_t kAngleY = 0x929ECA;     // s16 Camera_Angles +2
constexpr std::uint32_t kAngleFB = 0x929ECC;    // s16 Cond_AngleFB
constexpr std::uint32_t kMemberCount = 0x929EC0;   // u8 Field_MemberCount
constexpr std::uint32_t kHold = 0x929F12;       // Field_Kind2Hold
constexpr std::uint32_t kObjTrio = 0x802D40;    // ObjTrio: records of 0x14C
constexpr std::uint32_t kObjStride = 0x14C;
constexpr std::uint32_t kLeaderByte8 = 0x802D48;   // u8 ObjTrio +0x8, tested against 4..6 / 0..2
constexpr std::uint32_t kLeaderByteB = 0x802D4B;   // u8 ObjTrio +0xB, cleared by the area 0x79 entry
constexpr std::uint32_t kObjTrioX = 0x802D74;   // s32 ObjTrio + 0x34
constexpr std::uint32_t kObjTrioZ = 0x802D78;   // s32 ObjTrio + 0x38
constexpr unsigned kMemberKind = 0x89;          // ObjTrio record +0x89: the member's character byte
constexpr unsigned kMemberState = 0x137;        // ObjTrio record +0x137: tested against 3
constexpr std::uint32_t kKind2Z = 0x905E60;     // s32 Field_Kind2Z
constexpr std::uint32_t kKind2X = 0x905E64;     // s32 Field_Kind2X
constexpr std::uint32_t kKind2ZHigh = 0x905E62; // u16: Field_Kind2Z's whole part, moved by 7
constexpr std::uint32_t kSpriteKind2X = 0x7E0974;  // s32 Sprite_Kind2 + 0x34
constexpr std::uint32_t kSpriteKind2Z = 0x7E0978;  // s32 Sprite_Kind2 + 0x38
constexpr std::uint32_t kSpriteKind2Y = 0x7E097E;  // u16 Sprite_Kind2 + 0x3E
constexpr std::uint32_t kSprite0X = 0x7DEEB4;   // s32 Sprite_Objects + 0x34
constexpr std::uint32_t kSprite0Z = 0x7DEEB8;   // s32 Sprite_Objects + 0x38
constexpr std::uint32_t kSpriteCurrent = 0x937F88;  // unsigned char *Sprite_Current
constexpr std::uint32_t kF3Divisor = 0x937F8C;  // u16 MoveScript_F3Divisor
constexpr std::uint32_t kClutDirty = 0x937F90;  // Gfx_ClutStripDirty
constexpr std::uint32_t kFrameCounter = 0x937F94;  // Frame_Counter
constexpr std::uint32_t kByte937F98 = 0x937F98; // u8 set 0xFF / 1 beside the area changes
constexpr std::uint32_t kByte904EE0 = 0x904EE0; // u8 set 0xFF / 0 / 0xD beside the area changes
constexpr std::uint32_t kInputHeld = 0x7E1BE8;  // u16 Input_Held
constexpr std::uint32_t kOtSlot = 0x92BF19;     // Draw_OtSlot
constexpr std::uint32_t kSortOnX = 0x905BA8;    // Draw_SortOnX
constexpr std::uint32_t kClut = 0x80F580;       // Gfx_ClutStrip: 0x2000 16-bit colours, greyed by run 1 step 7
constexpr unsigned kClutWords = 0x2000;
constexpr std::uint32_t kTally = 0x903A10;      // bytes run 7 step 1 fills from the members, Scena10_TallyMet reads
constexpr std::uint32_t kTallyWord = 0x903A14;  // the dword runs 3 and 4 clear beside 0x903A10
constexpr std::uint32_t kMoveSpeed3 = 0x6697F3; // u8 Field_MoveSpeeds + 3
constexpr std::uint32_t kTextRecords = 0x904CE0;   // Text_Records: 16 bytes of the item's name record
constexpr std::uint32_t kMoveObject = 0x929E80; // unsigned char *MoveScript_Object

// Chapter 9's tables (symbols.toml [[data]]) beyond group SC9a's: the cell
// records 0x56D800 searches and the cell hook's jump table, back to back
// between Scena09_Objects and Scena10_Hooks.
constexpr std::uint32_t kCells9 = 0x661490;     // Scena09_Cells: 14 records of 5 bytes
constexpr unsigned kCell9Count = 14;
constexpr std::uint32_t kCellHooks9 = 0x6614D8; // Scena09_CellHooks: 14
// Chapter 10's tables, after Scena10_Hooks 0x661510.
constexpr std::uint32_t kPickups = 0x661528;    // Scena10_Pickups: 16 records of 6 bytes (flag, item, x, z)
constexpr unsigned kPickupCount = 16;
constexpr std::uint32_t kStates10 = 0x661588;   // Scena10_States, 3
constexpr unsigned kState10Count = 3;
constexpr std::uint32_t kRuns10 = 0x661594;     // Scena10_Runs, 14 (entries 8 and 9 are 0)
constexpr unsigned kRun10Count = 14;
constexpr std::uint32_t kSpriteScript = 0x6615D0;  // Scena10_SpriteScript: the op bytes Scena10_SpriteOp hands EventOp_0x
constexpr std::uint32_t kShakeSteps = 0x6615E4; // Scena10_ShakeSteps: 4 signed bytes by Frame_Counter & 3
constexpr std::uint32_t kMsgKindsA = 0x6615E8;  // Scena10_MsgKindsA: 4 member bytes, then 4 message ids at +4
constexpr std::uint32_t kMsgKindsB = 0x6615F4;  // Scena10_MsgKindsB: 5 member bytes, then 5 message ids at +8
constexpr std::uint32_t kMsgKindsC = 0x661608;  // Scena10_MsgKindsC: 3 member bytes, then 3 message ids at +4
constexpr std::uint32_t kObjects10 = 0x661614;  // Scena10_Objects, 13 (entry 10 is 0)
constexpr unsigned kObject10Count = 13;
// Chapter 10's cells beside run 11's of chapter 9 (Scena09_Slot11 0x6BC730).
constexpr std::uint32_t kShaking = 0x6BC734;    // u8 Scena10_Shaking: Scena10_Shake moves the camera while set
constexpr std::uint32_t kSlot10 = 0x6BC735;     // u8 Scena10_Slot: the chapter's effect slot

// Callees nobody owns (above).
constexpr std::uint32_t kCellFind = bof3::addr::Field_CellTriggerAt;
constexpr std::uint32_t kPartyPlace = bof3::addr::Party_PlaceForBattle;
constexpr std::uint32_t kSetBit80 = bof3::addr::Field_SetStatus80;
constexpr std::uint32_t kKeyItemAdd = bof3::addr::KeyItem_Add;
constexpr std::uint32_t kMusicStop = bof3::addr::Sound_StopMusic;
constexpr std::uint32_t kSpriteFindFree = bof3::addr::Sprite_FindFree;

}  // namespace at

using VoidFn = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
using KeyItemFn = unsigned char (__cdecl*)(unsigned value);
using PlaceFn = void (__cdecl*)(int x, int z, unsigned kind);
using CellFindFn = unsigned char (__cdecl*)(const void* records, unsigned n, int x, int z);
// A Scena09_CellHooks entry as Scena09_CellHook's tail jump reaches it: (x, z)
// in place, al the answer.
using CellEntry = unsigned char (__cdecl*)(int x, int z);
// A Scena10_Objects entry as Scena10_ObjectTrigger calls it (the object, the
// flag bits' pointer).
using ObjectEntry = void (__cdecl*)(unsigned char* object, std::uint32_t bits);

}  // namespace scena_sc9b
