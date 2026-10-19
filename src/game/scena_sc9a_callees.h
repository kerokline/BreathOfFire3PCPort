// Internal to scena_sc9a.cpp and scena_sc9a_fuzz.cpp: the cells chapter 9's
// first block (0x553B30..0x557170, group SC9a of round ten) reads and writes,
// its tables, and the raw addresses of the callees it reaches that nobody owns
// yet. docs/scena_sc9a.md.
//
// Calls into code this group does not own, by raw address (SH_AT)
// (Scenario_CallB 0x5341C0, named by SCH and not taken, and
// Field_StartEventBattle 0x4410B0, group SE's, are called by name):
//   0x533E50  a pass over the 8 records at 0x903A70 and the party - engine,
//             nobody's
//   0x532ED0  (x, z, kind): a party placement before an event battle -
//             engine, nobody's
//   0x56D6F0  0x8034E1 |= 0x80 - engine, nobody's
// Chapter 9's closure beyond the band (group SC9b's block 0x557170..0x55C040,
// a later wave, and state 0 0x5646B0 in group SC13's) is reached only through
// the tables read in place: Scena09_States entry 0 and Scena09_Objects
// entries 5..15.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc9a {

namespace at {

// The scenario bytes (docs/field-modes.md section 2).
// kState: now MoveScript_PartyRecords[0x62] (symbols.toml). s8: the chapter's state, 0..2
constexpr std::uint32_t kRun = 0x8034E4;        // s8: MoveScript_Var7, which scene
// kStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;      // u16: run 11's countdown
// kCondFD: now Cond_ByteFD (symbols.toml). Cond_ByteFD
constexpr std::uint32_t kCounters = 0x903848;   // four script counters (MoveScript_CounterOps)
constexpr std::uint32_t kEffectSlot = 0x903850; // u8: the effect slot just taken
// kFlagBits: now AreaW0c_FlagRow (symbols.toml). unsigned char *: the bits Flags_Test / Set / Clear take
// kRow6: now Cond_Flags[0x30] (symbols.toml). Cond_Flags + 0x30 (row 6), handed to Flags_Set / Clear as a literal
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0xA0, handed to Flags_Set / Clear as a literal
constexpr std::uint32_t kSelector = 0x90412C;   // Cond_Flags + 0x19C: & 0x7F picks the call-table entries
// kArea: now Game_AreaNumber (symbols.toml). u16 Game_AreaNumber
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
constexpr std::uint32_t kScriptFlags = 0x9039A2;   // u16 Field_ScriptFlags
constexpr std::uint32_t kScriptFlags2 = 0x905BA4;  // u16 Field_ScriptFlags2: its low 3 bits tested (run 2 step 0)
// kRequest: now Field_Request (symbols.toml). Field_Request
// kWait: now MoveScript_WaitWordDA (symbols.toml). u16 MoveScript_WaitWordDA
// kRedraw: now MapView_Redraw (symbols.toml). MapView_Redraw
// kCamDist: now Camera_Distance (symbols.toml). u16 Camera_Distance
// kMusicCurrent: now AreaW1f_MusicCurrent (symbols.toml). u8: Music_Play returns at once while its track equals it
// kCondFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
constexpr std::uint32_t kEffects = 0x7E11E0;    // Effect_Objects: records of 0x80
constexpr std::uint32_t kEffectStride = 0x80;
// kAngleX: now Camera_Angles (symbols.toml). s16 Camera_Angles +0
// kAngleY: now Camera_Angles[1] (symbols.toml). s16 Camera_Angles +2
constexpr std::uint32_t kAngleFB = 0x929ECC;    // s16 Cond_AngleFB
// kHold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
constexpr std::uint32_t kObjTrioX = 0x802D74;   // s32 ObjTrio + 0x34
constexpr std::uint32_t kObjTrioZ = 0x802D78;   // s32 ObjTrio + 0x38
constexpr std::uint32_t kObjTrioY = 0x802D7C;   // s32 ObjTrio + 0x3C
// kMember1Byte: now Field_Members[0x13b] (symbols.toml). u8 ObjTrio record 1 + 0x137, tested against 3
// kMember2Byte: now ObjTrio[0x3cf] (symbols.toml). u8 ObjTrio record 2 + 0x137, tested against 2 / 3
// kKind2Z: now Field_Kind2Z (symbols.toml). s32 Field_Kind2Z
// kKind2X: now Field_Kind2X (symbols.toml). s32 Field_Kind2X
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). u16 MoveScript_F3Divisor
// kMenuButton: now Field_MenuButton (symbols.toml). u16 Field_MenuButton
// kInputPressed: now Input_Pressed (symbols.toml). u16 Input_Pressed
constexpr std::uint32_t kPartyList = 0x904062;  // three bytes: the party's members
// kSlot11: now Scena09_Slot11 (symbols.toml). u8: run 11's effect slot (its own cell, not 0x903850)

// Chapter 9's tables (symbols.toml [[data]]), each read in place by its
// dispatcher. They lie back to back after the vtable 0x6613E8; an index past
// one reads the next.
// kStates: now Scena09_States (symbols.toml). Scena09_States, 3
constexpr unsigned kStateCount = 3;
// kRuns: now Scena09_Runs (symbols.toml). Scena09_Runs, 17
constexpr unsigned kRunCount = 17;
// kObjects: now Scena09_Objects (symbols.toml). Scena09_Objects, 16
constexpr unsigned kObjectCount = 16;

// Callees nobody owns (above).
constexpr std::uint32_t kPartyPass = bof3::addr::Party_HealJoined;
constexpr std::uint32_t kPartyPlace = bof3::addr::Party_PlaceForBattle;
constexpr std::uint32_t kSetBit80 = bof3::addr::Field_SetStatus80;

}  // namespace at

using VoidFn = void (__cdecl*)();
using PlaceFn = void (__cdecl*)(int x, int z, unsigned kind);
// A Scena09_Objects entry as Scena09_ObjectTrigger calls it (the object, the
// flag bits' pointer).
using ObjectEntry = void (__cdecl*)(unsigned char* object, std::uint32_t bits);

}  // namespace scena_sc9a
