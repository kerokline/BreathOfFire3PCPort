// Internal to scena_sc13.cpp and scena_sc13_fuzz.cpp: the cells chapters 13
// and 14 (0x561DB0..0x567DC0, group SC13 of round ten) read and write, their
// tables, and the raw addresses of the callees they reach that nobody owns
// yet. docs/scena_sc13.md.
//
// Calls into code this group does not own, by raw address (SH_AT)
// (Scenario_CallB 0x5341C0, named by SCH and not taken, is called by name;
// Field_StartEventBattle 0x4410B0 is SE's, ours, called by name):
//   0x533E50  a pass over the 8 records at 0x903A70 and the party - group SX's
//             this wave
//   0x532ED0  (x, z, kind): an event battle's party placement - group SX's
//   0x56D6F0  0x8034E1 |= 0x80 - group SX's
//   0x56FCA0  the view shift after a focus change - group SX's
//   0x57CD90  the first free Sprite_Objects record (+0 == 0) of 30, al; 0xFF
//             none - group SX's
//   0x587B80  a jmp to 0x5A6FF0 (the sound layer), no arguments - group SX's
//   0x591900  the index of the first 0 byte among the 32 at 0x904554, al; it
//             reads no argument (its callers here push one) - group SX's
//   0x587890  (id, level): a sound voice's level (the sound layer) - nobody's
//   0x591920  (byte): the index of the byte among the 32 at 0x904554, al -
//             nobody's
//   0x420A90, 0x4204D0, 0x420580, 0x4205D0, 0x420670, 0x420710  area code of
//             world 3 (areas 141 and 143; groups AR3E / AR3F, a later wave)
//   0x42BA90, 0x42C0A0  area code of world 4 (areas 191 / 192; AR4E / AR4F)
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc13 {

namespace at {

// The scenario bytes (docs/field-modes.md section 2).
// kStatusBits: now Field_StatusBits (symbols.toml). Field_StatusBits
// kState: now MoveScript_PartyRecords[0x62] (symbols.toml). s8: the chapter's state, 0..2
// kRun: now MoveScript_Var7 (symbols.toml). s8: MoveScript_Var7, which scene
// kStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;      // u16: the scenes' word timer
// kCondFD: now Cond_ByteFD (symbols.toml). Cond_ByteFD
constexpr std::uint32_t kCounters = 0x903848;   // four script counters (MoveScript_CounterOps)
constexpr std::uint32_t kSlotWord = 0x903850;   // u16: an object or effect slot, a caption's clock
constexpr std::uint32_t kCaptionTotal = 0x903852;  // u16: a caption's length in frames, less one
// kFlagBits: now Chapter_FlagRow (symbols.toml). unsigned char *: the bits Flags_Test / Set / Clear take
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0xA0, handed to Flags_Test / Set as a literal
// kArea: now Game_AreaNumber (symbols.toml). u16 Game_AreaNumber
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
constexpr std::uint32_t kScriptFlags = 0x9039A2;   // u16 Field_ScriptFlags
// kRequest: now Field_Request (symbols.toml). Field_Request
// kWait: now Transition_Busy (symbols.toml). u16 Transition_Busy
// kMusicCurrent: now AreaW1f_MusicCurrent (symbols.toml). u8: Music_Play returns at once while its track equals it
constexpr std::uint32_t kMusicTrack = 0x904131; // Music_Track (Cond_Flags + 0x1A1)
// kCondFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
constexpr std::uint32_t kEffects = 0x7E11E0;    // Effect_Objects: records of 0x80
constexpr std::uint32_t kEffectStride = 0x80;
constexpr std::uint32_t kSprites = 0x7DEE80;    // Sprite_Objects: records of 0xA4
constexpr std::uint32_t kSpriteStride = 0xA4;
// kAngleX: now Camera_Angles (symbols.toml). s16 Camera_Angles +0
// kAngleY: now Camera_Angles[1] (symbols.toml). s16 Camera_Angles +2
constexpr std::uint32_t kAngleFB = 0x929ECC;    // s16 Cond_AngleFB
// kCamDistance: now Camera_Distance (symbols.toml). u16 Camera_Distance
// kCamShiftY: now Camera_ShiftY (symbols.toml). u16 Camera_ShiftY
// kCamObject: now AreaW1a_FocusObject (symbols.toml). unsigned char *: an object whose +0x34 / +0x38 run 6 copies
// kElevOffset: now MapView_ElevationOffset (symbols.toml). u16 MapView_ElevationOffset
// kFocusX: now MapView_FocusX (symbols.toml). s32 MapView_FocusX
// kRedraw: now MapView_Redraw (symbols.toml). MapView_Redraw
// kOrigin: now MapView_Origin (symbols.toml). u16 MapView_Origin
// kKind2Z: now Field_Kind2Z (symbols.toml). s32 Field_Kind2Z
constexpr std::uint32_t kKind2X = 0x905E64;     // s32 Field_Kind2X
// kKind2: now Sprite_Kind2 on records::SpriteHead.
// kExtra0: now Sprite_ObjectsExtra record 0 on records::SpriteObject.
// kExtra1Angle: now Sprite_ObjectsExtra record 1 angle[2] (records::SpriteObject).
// kExtraWord: now AreaW2c_LastArea (symbols.toml). u16 just past Sprite_ObjectsExtra: tested 0x70 / 0x91
// kClut: now Gfx_ClutStrip (symbols.toml). Gfx_ClutStrip: 0x2000 16-bit colours
constexpr unsigned kClutWords = 0x2000;
// kClutDirty: now Gfx_ClutStripDirty (symbols.toml). Gfx_ClutStripDirty
// kFrame: now Frame_Counter (symbols.toml). Frame_Counter
// kSpriteMode: now Area_EnterTransition (symbols.toml). a byte the runs set 1 / 0xFF before an area change
// kEntryByte: now ModeFlow_AreaTransition (symbols.toml). a byte the runs set 0 / 0xFF before an area change
constexpr std::uint32_t kTrio = 0x802D40;       // ObjTrio: three records of 0x14C
constexpr std::uint32_t kTrioStride = 0x14C;
// kFacing: now ObjTrio[8] (symbols.toml). u8 ObjTrio record 0 + 8: the leader's facing
constexpr std::uint32_t kLeaderName = 0x802DC9; // u8 ObjTrio record 0 + 0x89: who leads
// kTrioX: now ObjTrio record 0 x (records::PartyMember).
// kTrioY: now ObjTrio record 0 z (records::PartyMember).
// kTrioZ: now ObjTrio record 0 height (records::PartyMember).
constexpr std::uint32_t kEffectArg = 0x669744;  // MoveScript_EffectArg, by the leader byte
constexpr std::uint32_t kPendingKind = 0x9039F3;   // three bytes an object handler arms: 0x2C, 0, 0x11
constexpr std::uint32_t kInputHeld = 0x7E1BE8;  // Input_Held (dword read)
constexpr std::uint32_t kInputPressed = 0x7E1BEC;  // Input_Pressed
constexpr std::uint32_t kCaptionText = 0x803580;   // u16 offsets, then the caption text from 0x803582
constexpr std::uint32_t kCaptionIndex = 0x7DEE48;  // u16: the caption Scena13_Caption draws
// kMemberCount: now Field_MemberCount (symbols.toml). Field_MemberCount
constexpr std::uint32_t kPartyList = 0x904062;  // the party list bytes (Cond_Flags + 0xD2)
constexpr std::uint32_t kEffectState = 0x66972C;   // Character_IndexById: a record index by member
constexpr std::uint32_t kRecords = 0x903A70;    // the eight records of 0xA4 (0x533E50's pass)
constexpr std::uint32_t kRecordStride = 0xA4;
// kRecordIndex: now Character_IndexById[8] (symbols.toml). u8: which of them run 2 / run 3 of chapter 14 touch
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). s16 MoveScript_F3Divisor
// kViewByte: now ScenaSc6_Load10 (symbols.toml). a byte chapter 14's run 7 sets 1
// kOtSlot: now Draw_OtSlot (symbols.toml). Draw_OtSlot
// kSortOnX: now Draw_SortOnX (symbols.toml). Draw_SortOnX

// The chapters' own effect-slot cells (symbols.toml [[data]]).
// kSlot13: now Scena13_Slot (symbols.toml). Scena13_Slot
// kSlot14: now Scena14_Slot (symbols.toml). Scena14_Slot

// Chapter 13's tables (symbols.toml [[data]]), each read in place. They lie
// back to back after the vtable Scena13_Hooks 0x661788.
// kStates13: now Scena13_States (symbols.toml). Scena13_States, 3
constexpr unsigned kStateCount13 = 3;
// kRuns13: now Scena13_Runs (symbols.toml). Scena13_Runs, 9
constexpr unsigned kRunCount13 = 9;
// kEventOps13: now Scena13_EventOps (symbols.toml). Scena13_EventOps: four records of 16 bytes
// kObjects13: now Scena13_Objects (symbols.toml). Scena13_Objects, 4
constexpr unsigned kObjectCount13 = 4;
// Chapter 14's, after Scena14_Hooks 0x661820.
// kStates14: now Scena14_States (symbols.toml). Scena14_States, 3
constexpr unsigned kStateCount14 = 3;
// kRuns14: now Scena14_Runs (symbols.toml). Scena14_Runs, 10
constexpr unsigned kRunCount14 = 10;
constexpr std::uint32_t kShake14 = 0x661868;    // Scena14_ShakeOffsets, 4 signed bytes
// kTalkWho14: now Scena14_TalkWho (symbols.toml). Scena14_TalkWho: two lists of 4 member bytes
// kTalkLines14: now Scena14_TalkLines (symbols.toml). Scena14_TalkLines: two lists of 4 message ids
// kObjects14: now Scena14_Objects (symbols.toml). Scena14_Objects, 8
constexpr unsigned kObjectCount14 = 8;

// Callees nobody owns (above).
constexpr std::uint32_t kPartyPass = bof3::addr::Party_HealJoined;
constexpr std::uint32_t kPartyPlace = bof3::addr::Party_PlaceForBattle;
constexpr std::uint32_t kSetBit80 = bof3::addr::Field_SetStatus80;
constexpr std::uint32_t kViewShift = bof3::addr::MapView_FillCells;
constexpr std::uint32_t kFreeSprite = bof3::addr::Sprite_FindFree;
constexpr std::uint32_t kSound587B80 = bof3::addr::Sound_StopMusic;
constexpr std::uint32_t kParty591900 = bof3::addr::KeyItem_Add;
constexpr std::uint32_t kVoiceLevel = bof3::addr::Sound_SetCueVolume;
constexpr std::uint32_t kFindByte = bof3::addr::KeyItem_Remove;
constexpr std::uint32_t kArea143 = bof3::addr::Area143_ClutShiftRight;
constexpr std::uint32_t kArea141a = bof3::addr::Area141_PlacePairAnimated;
constexpr std::uint32_t kArea141b = bof3::addr::Area141_PlaceOneAnimated;
constexpr std::uint32_t kArea141c = bof3::addr::Area141_PlacePair6x;
constexpr std::uint32_t kArea141d = bof3::addr::Area141_PlacePair0x;
constexpr std::uint32_t kArea141e = bof3::addr::Area141_PlaceOne0x;
constexpr std::uint32_t kArea191 = bof3::addr::Area191_TalkMessage;
constexpr std::uint32_t kArea192 = bof3::addr::Area192_TalkMessage;

}  // namespace at

using VoidFn = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
using ArgFn = void (__cdecl*)(unsigned a);
using PlaceFn = void (__cdecl*)(int x, int z, unsigned kind);
using LevelFn = void (__cdecl*)(unsigned id, unsigned level);
using FindByteFn = unsigned char (__cdecl*)(unsigned b);
using ScriptIdFn = unsigned (__cdecl*)(unsigned who);
// A table entry as the object triggers call it (docs/scena_sc13.md section 2).
using ObjectEntry = void (__cdecl*)(unsigned char* object, std::uint32_t bits);

}  // namespace scena_sc13
