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
// kState: now MoveScript_PartyRecords[0x62] (symbols.toml). s8: the chapter's state, 0..2
constexpr std::uint32_t kRun = 0x8034E4;        // s8: MoveScript_Var7, which scene
// kStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;      // u16: a countdown the runs keep
// kCondFD: now Cond_ByteFD (symbols.toml). Cond_ByteFD
constexpr std::uint32_t kCounters = 0x903848;   // four script counters (MoveScript_CounterOps)
inline auto& kEffectSlot = SharedScratch;  // u8 (a word in Scena10_SpriteOp): the slot just taken
constexpr std::uint32_t kEffectSlot_addr = SharedScratch_addr; // kEffectSlot's address, for the raw accessors that take one
// kFlagBits: now Chapter_FlagRow (symbols.toml). unsigned char *: the bits Flags_Test / Set / Clear take
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0xA0, handed to Flags_Set / Clear as a literal
constexpr std::uint32_t kRow657 = 0x904657;     // a row handed to Flags_Set as a literal (run 7 step 0xF)
// kArea: now Game_AreaNumber (symbols.toml). u16 Game_AreaNumber
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
constexpr std::uint32_t kScriptFlags = 0x9039A2;   // u16 Field_ScriptFlags
// kRequest: now Field_Request (symbols.toml). Field_Request
// kWait: now Transition_Busy (symbols.toml). u16 Transition_Busy
// kRedraw: now MapView_Redraw (symbols.toml). MapView_Redraw
// kCamDist: now Camera_Distance (symbols.toml). u16 Camera_Distance
// kShiftY: now Camera_ShiftY (symbols.toml). u16 Camera_ShiftY
// kMusicCurrent: now AreaW1f_MusicCurrent (symbols.toml). u8: Music_Play returns at once while its track equals it
// kCondFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
constexpr std::uint32_t kEffects = 0x7E11E0;    // Effect_Objects: records of 0x80
constexpr std::uint32_t kEffectStride = 0x80;
// kAngleX: now Camera_Angles (symbols.toml). s16 Camera_Angles +0
// kAngleY: now Camera_Angles[1] (symbols.toml). s16 Camera_Angles +2
constexpr std::uint32_t kAngleFB = 0x929ECC;    // s16 Cond_AngleFB
// kMemberCount: now Field_MemberCount (symbols.toml). u8 Field_MemberCount
// kHold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
// kObjTrio: now ObjTrio (symbols.toml). ObjTrio: records of 0x14C
constexpr std::uint32_t kObjStride = 0x14C;
// kLeaderByte8: now ObjTrio[0].direction (symbols.toml). u8 ObjTrio +0x8, tested against 4..6 / 0..2
// kLeaderByteB: now ObjTrio_addr + 0xb (symbols.toml). u8 ObjTrio +0xB, cleared by the area 0x79 entry
// kObjTrioX: now ObjTrio record 0 x (records::PartyMember).
// kObjTrioZ: now ObjTrio record 0 z (records::PartyMember).
constexpr unsigned kMemberKind = 0x89;          // ObjTrio record +0x89: the member's character byte
constexpr unsigned kMemberState = 0x137;        // ObjTrio record +0x137: tested against 3
// kKind2Z: now Field_Kind2Z (symbols.toml). s32 Field_Kind2Z
// kKind2X: now Field_Kind2X (symbols.toml). s32 Field_Kind2X
constexpr std::uint32_t kKind2ZHigh = 0x905E62; // u16: Field_Kind2Z's whole part, moved by 7
// kSpriteKind2X: now Sprite_Kind2 x (records::SpriteHead).
// kSpriteKind2Z: now Sprite_Kind2 z (records::SpriteHead).
// kSpriteKind2Y: now Sprite_Kind2 height_whole (records::SpriteHead).
// kSprite0X: now Sprite_Objects record 0 x (records::SpriteObject).
// kSprite0Z: now Sprite_Objects record 0 z (records::SpriteObject).
// kSpriteCurrent: now Sprite_Current (symbols.toml). unsigned char *Sprite_Current
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). u16 MoveScript_F3Divisor
// kClutDirty: now Gfx_ClutStripDirty (symbols.toml). Gfx_ClutStripDirty
// kFrameCounter: now Frame_Counter (symbols.toml). Frame_Counter
// kByte937F98: now Area_EnterTransition (symbols.toml). u8 set 0xFF / 1 beside the area changes
// kByte904EE0: now ModeFlow_AreaTransition (symbols.toml). u8 set 0xFF / 0 / 0xD beside the area changes
// kInputHeld: now Input_Held (symbols.toml). u16 Input_Held
// kOtSlot: now Draw_OtSlot (symbols.toml). Draw_OtSlot
// kSortOnX: now Draw_SortOnX (symbols.toml). Draw_SortOnX
// kClut: now Gfx_ClutStrip (symbols.toml). Gfx_ClutStrip: 0x2000 16-bit colours, greyed by run 1 step 7
constexpr unsigned kClutWords = 0x2000;
constexpr std::uint32_t kTally = 0x903A10;      // bytes run 7 step 1 fills from the members, Scena10_TallyMet reads
// kTallyWord: now ScenaSc9b_TallyWord (symbols.toml). the dword runs 3 and 4 clear beside 0x903A10
// kMoveSpeed3: now Field_MoveSpeeds[3] (symbols.toml). u8 Field_MoveSpeeds + 3
constexpr std::uint32_t kTextRecords = 0x904CE0;   // Text_Records: 16 bytes of the item's name record
// kMoveObject: now MoveScript_Object (symbols.toml). unsigned char *MoveScript_Object

// Chapter 9's tables (symbols.toml [[data]]) beyond group SC9a's: the cell
// records 0x56D800 searches and the cell hook's jump table, back to back
// between Scena09_Objects and Scena10_Hooks.
// kCells9: now Scena09_Cells (symbols.toml). Scena09_Cells: 14 records of 5 bytes
constexpr unsigned kCell9Count = 14;
// kCellHooks9: now Scena09_CellHooks (symbols.toml). Scena09_CellHooks: 14
// Chapter 10's tables, after Scena10_Hooks 0x661510.
// kPickups: now Scena10_Pickups (symbols.toml). Scena10_Pickups: 16 records of 6 bytes (flag, item, x, z)
constexpr unsigned kPickupCount = 16;
// kStates10: now Scena10_States (symbols.toml). Scena10_States, 3
constexpr unsigned kState10Count = 3;
// kRuns10: now Scena10_Runs (symbols.toml). Scena10_Runs, 14 (entries 8 and 9 are 0)
constexpr unsigned kRun10Count = 14;
// kSpriteScript: now Scena10_SpriteScript (symbols.toml). Scena10_SpriteScript: the op bytes Scena10_SpriteOp hands EventOp_0x
constexpr std::uint32_t kShakeSteps = 0x6615E4; // Scena10_ShakeSteps: 4 signed bytes by Frame_Counter & 3
// kMsgKindsA: now Scena10_MsgKindsA (symbols.toml). Scena10_MsgKindsA: 4 member bytes, then 4 message ids at +4
// kMsgKindsB: now Scena10_MsgKindsB (symbols.toml). Scena10_MsgKindsB: 5 member bytes, then 5 message ids at +8
// kMsgKindsC: now Scena10_MsgKindsC (symbols.toml). Scena10_MsgKindsC: 3 member bytes, then 3 message ids at +4
// kObjects10: now Scena10_Objects (symbols.toml). Scena10_Objects, 13 (entry 10 is 0)
constexpr unsigned kObject10Count = 13;
// Chapter 10's cells beside run 11's of chapter 9 (Scena09_Slot11 0x6BC730).
// kShaking: now Scena10_Shaking (symbols.toml). u8 Scena10_Shaking: Scena10_Shake moves the camera while set
// kSlot10: now Scena10_Slot (symbols.toml). u8 Scena10_Slot: the chapter's effect slot

// Callees nobody owns (above).
constexpr ::bof3::image::CodeVa kCellFind{bof3::addr::Field_CellTriggerAt};
constexpr ::bof3::image::CodeVa kPartyPlace{bof3::addr::Party_PlaceForBattle};
constexpr ::bof3::image::CodeVa kSetBit80{bof3::addr::Field_SetStatus80};
constexpr ::bof3::image::CodeVa kKeyItemAdd{bof3::addr::KeyItem_Add};
constexpr ::bof3::image::CodeVa kMusicStop{bof3::addr::Sound_StopMusic};
constexpr ::bof3::image::CodeVa kSpriteFindFree{bof3::addr::Sprite_FindFree};

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
