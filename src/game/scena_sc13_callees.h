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
constexpr std::uint32_t kStatusBits = 0x8034E1;  // Field_StatusBits
constexpr std::uint32_t kState = 0x8034E2;      // s8: the chapter's state, 0..2
constexpr std::uint32_t kRun = 0x8034E4;        // s8: MoveScript_Var7, which scene
constexpr std::uint32_t kStep = 0x8034E5;       // u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;      // u16: the scenes' word timer
constexpr std::uint32_t kCondFD = 0x8034F1;     // Cond_ByteFD
constexpr std::uint32_t kCounters = 0x903848;   // four script counters (MoveScript_CounterOps)
constexpr std::uint32_t kSlotWord = 0x903850;   // u16: an object or effect slot, a caption's clock
constexpr std::uint32_t kCaptionTotal = 0x903852;  // u16: a caption's length in frames, less one
constexpr std::uint32_t kFlagBits = 0x929ED0;   // unsigned char *: the bits Flags_Test / Set / Clear take
constexpr std::uint32_t kStoryFlags = 0x904030; // Cond_Flags + 0xA0, handed to Flags_Test / Set as a literal
constexpr std::uint32_t kArea = 0x904EFC;       // u16 Game_AreaNumber
constexpr std::uint32_t kPassFlags = 0x7E0918;  // Draw_PassFlags
constexpr std::uint32_t kScriptFlags = 0x9039A2;   // u16 Field_ScriptFlags
constexpr std::uint32_t kRequest = 0x66C7D8;    // Field_Request
constexpr std::uint32_t kWait = 0x66C810;       // u16 MoveScript_WaitWordDA
constexpr std::uint32_t kMusicCurrent = 0x904CD0;  // u8: Music_Play returns at once while its track equals it
constexpr std::uint32_t kMusicTrack = 0x904131; // Music_Track (Cond_Flags + 0x1A1)
constexpr std::uint32_t kCondFE = 0x905E20;     // Cond_ByteFE
constexpr std::uint32_t kEffects = 0x7E11E0;    // Effect_Objects: records of 0x80
constexpr std::uint32_t kEffectStride = 0x80;
constexpr std::uint32_t kSprites = 0x7DEE80;    // Sprite_Objects: records of 0xA4
constexpr std::uint32_t kSpriteStride = 0xA4;
constexpr std::uint32_t kAngleX = 0x929EC8;     // s16 Camera_Angles +0
constexpr std::uint32_t kAngleY = 0x929ECA;     // s16 Camera_Angles +2
constexpr std::uint32_t kAngleFB = 0x929ECC;    // s16 Cond_AngleFB
constexpr std::uint32_t kCamDistance = 0x903840;   // u16 Camera_Distance
constexpr std::uint32_t kCamShiftY = 0x903802;  // u16 Camera_ShiftY
constexpr std::uint32_t kCamObject = 0x903804;  // unsigned char *: an object whose +0x34 / +0x38 run 6 copies
constexpr std::uint32_t kElevOffset = 0x92BEE2; // u16 MapView_ElevationOffset
constexpr std::uint32_t kFocusX = 0x929F14;     // s32 MapView_FocusX
constexpr std::uint32_t kRedraw = 0x905E69;     // MapView_Redraw
constexpr std::uint32_t kOrigin = 0x7E0688;     // u16 MapView_Origin
constexpr std::uint32_t kKind2Z = 0x905E60;     // s32 Field_Kind2Z
constexpr std::uint32_t kKind2X = 0x905E64;     // s32 Field_Kind2X
constexpr std::uint32_t kKind2 = 0x7E0940;      // Sprite_Kind2: +0x34 x, +0x38 z, +0x3E the elevation
constexpr std::uint32_t kExtra0 = 0x802000;     // Sprite_ObjectsExtra record 0: +0x34 x, +0x38 z, +0x3E
constexpr std::uint32_t kExtra1Angle = 0x802110;   // s32 Sprite_ObjectsExtra record 1 + 0x6C: run 5's dial
constexpr std::uint32_t kExtraWord = 0x802290;  // u16 just past Sprite_ObjectsExtra: tested 0x70 / 0x91
constexpr std::uint32_t kClut = 0x80F580;       // Gfx_ClutStrip: 0x2000 16-bit colours
constexpr unsigned kClutWords = 0x2000;
constexpr std::uint32_t kClutDirty = 0x937F90;  // Gfx_ClutStripDirty
constexpr std::uint32_t kFrame = 0x937F94;      // Frame_Counter
constexpr std::uint32_t kSpriteMode = 0x937F98; // a byte the runs set 1 / 0xFF before an area change
constexpr std::uint32_t kEntryByte = 0x904EE0;  // a byte the runs set 0 / 0xFF before an area change
constexpr std::uint32_t kTrio = 0x802D40;       // ObjTrio: three records of 0x14C
constexpr std::uint32_t kTrioStride = 0x14C;
constexpr std::uint32_t kFacing = 0x802D48;     // u8 ObjTrio record 0 + 8: the leader's facing
constexpr std::uint32_t kLeaderName = 0x802DC9; // u8 ObjTrio record 0 + 0x89: who leads
constexpr std::uint32_t kTrioX = 0x802D74;      // s32 ObjTrio record 0 + 0x34
constexpr std::uint32_t kTrioY = 0x802D78;      // s32 ObjTrio record 0 + 0x38
constexpr std::uint32_t kTrioZ = 0x802D7C;      // s32 ObjTrio record 0 + 0x3C
constexpr std::uint32_t kEffectArg = 0x669744;  // MoveScript_EffectArg, by the leader byte
constexpr std::uint32_t kPendingKind = 0x9039F3;   // three bytes an object handler arms: 0x2C, 0, 0x11
constexpr std::uint32_t kInputHeld = 0x7E1BE8;  // Input_Held (dword read)
constexpr std::uint32_t kInputPressed = 0x7E1BEC;  // Input_Pressed
constexpr std::uint32_t kCaptionText = 0x803580;   // u16 offsets, then the caption text from 0x803582
constexpr std::uint32_t kCaptionIndex = 0x7DEE48;  // u16: the caption Scena13_Caption draws
constexpr std::uint32_t kMemberCount = 0x929EC0;   // Field_MemberCount
constexpr std::uint32_t kPartyList = 0x904062;  // the party list bytes (Cond_Flags + 0xD2)
constexpr std::uint32_t kEffectState = 0x66972C;   // MoveScript_EffectState: a record index by member
constexpr std::uint32_t kRecords = 0x903A70;    // the eight records of 0xA4 (0x533E50's pass)
constexpr std::uint32_t kRecordStride = 0xA4;
constexpr std::uint32_t kRecordIndex = 0x669734;   // u8: which of them run 2 / run 3 of chapter 14 touch
constexpr std::uint32_t kF3Divisor = 0x937F8C;  // s16 MoveScript_F3Divisor
constexpr std::uint32_t kViewByte = 0x929F10;   // a byte chapter 14's run 7 sets 1
constexpr std::uint32_t kOtSlot = 0x92BF19;     // Draw_OtSlot
constexpr std::uint32_t kSortOnX = 0x905BA8;    // Draw_SortOnX

// The chapters' own effect-slot cells (symbols.toml [[data]]).
constexpr std::uint32_t kSlot13 = 0x6BC738;     // Scena13_Slot
constexpr std::uint32_t kSlot14 = 0x6BC73C;     // Scena14_Slot

// Chapter 13's tables (symbols.toml [[data]]), each read in place. They lie
// back to back after the vtable Scena13_Hooks 0x661788.
constexpr std::uint32_t kStates13 = 0x66179C;   // Scena13_States, 3
constexpr unsigned kStateCount13 = 3;
constexpr std::uint32_t kRuns13 = 0x6617A8;     // Scena13_Runs, 9
constexpr unsigned kRunCount13 = 9;
constexpr std::uint32_t kEventOps13 = 0x6617D0; // Scena13_EventOps: four records of 16 bytes
constexpr std::uint32_t kObjects13 = 0x661810;  // Scena13_Objects, 4
constexpr unsigned kObjectCount13 = 4;
// Chapter 14's, after Scena14_Hooks 0x661820.
constexpr std::uint32_t kStates14 = 0x661834;   // Scena14_States, 3
constexpr unsigned kStateCount14 = 3;
constexpr std::uint32_t kRuns14 = 0x661840;     // Scena14_Runs, 10
constexpr unsigned kRunCount14 = 10;
constexpr std::uint32_t kShake14 = 0x661868;    // Scena14_ShakeOffsets, 4 signed bytes
constexpr std::uint32_t kTalkWho14 = 0x66186C;  // Scena14_TalkWho: two lists of 4 member bytes
constexpr std::uint32_t kTalkLines14 = 0x661874;   // Scena14_TalkLines: two lists of 4 message ids
constexpr std::uint32_t kObjects14 = 0x661884;  // Scena14_Objects, 8
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
