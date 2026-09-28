// Internal to scena_sc12.cpp and scena_sc12_fuzz.cpp: the cells chapter 12's
// first block (0x55E4E0..0x561DB0, group SC12 of round ten) reads and writes,
// its tables, and the raw addresses of the callees it reaches that nobody owns
// yet. docs/scena_sc12.md.
//
// Calls into code this group does not own, by raw address (SH_AT)
// (Scenario_CallB 0x5341C0, named by SCH and not taken, is called by name):
//   0x533E50  a pass over the 8 records at 0x903A70 (stride 0xA4) and the
//             party (0x929EC0 members from 0x802DC0) - engine, nobody's
//   0x532ED0  (x, z, kind): x and z to 0x903780 / 0x903784, a byte of the
//             table 0x64DDEC by kind to 0x904AAC, then a walk of the party -
//             engine, nobody's
//   0x56D6F0  0x8034E1 |= 0x80 (two instructions) - engine, nobody's
//   0x56D800  (records, count, a, b): the index of the first of `count`
//             5-byte records matching Game_AreaNumber and the cell (a, b), or
//             0xFF - engine, nobody's (the cell-hook search)
//   0x4410B0  (kind): a battle start's bytes - group SE's this wave
// Chapter 12's closure beyond the band (group SC13's, a later wave) is reached
// only through the tables read in place: state 0 (0x5646B0).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc12 {

namespace at {

// The scenario bytes (docs/field-modes.md section 2).
constexpr std::uint32_t kState = 0x8034E2;      // s8: the chapter's state, 0..2
constexpr std::uint32_t kRun = 0x8034E4;        // s8: MoveScript_Var7, which scene
constexpr std::uint32_t kStep = 0x8034E5;       // u8: the scene's step
constexpr std::uint32_t kCondFD = 0x8034F1;     // Cond_ByteFD
constexpr std::uint32_t kCounters = 0x903848;   // four script counters (MoveScript_CounterOps)
constexpr std::uint32_t kEffectSlot = 0x903850; // u8: the effect slot just taken
constexpr std::uint32_t kFlagBits = 0x929ED0;   // unsigned char *: the bits Flags_Test / Set / Clear take
constexpr std::uint32_t kStoryFlags = 0x904030; // Cond_Flags + 0xA0, handed to Flags_Set once as a literal
constexpr std::uint32_t kSelector = 0x90412C;   // Cond_Flags + 0x19C: & 0x7F picks a flag of a set of ten
constexpr std::uint32_t kArea = 0x904EFC;       // u16 Game_AreaNumber
constexpr std::uint32_t kPassFlags = 0x7E0918;  // Draw_PassFlags
constexpr std::uint32_t kScriptFlags = 0x9039A2;   // u16 Field_ScriptFlags
constexpr std::uint32_t kRequest = 0x66C7D8;    // Field_Request
constexpr std::uint32_t kWait = 0x66C810;       // u16 MoveScript_WaitWordDA
constexpr std::uint32_t kRedraw = 0x905E69;     // MapView_Redraw
constexpr std::uint32_t kMusicCurrent = 0x904CD0;  // u8: Music_Play returns at once while its track equals it
constexpr std::uint32_t kCondFE = 0x905E20;     // Cond_ByteFE
constexpr std::uint32_t kEffects = 0x7E11E0;    // Effect_Objects: records of 0x80
constexpr std::uint32_t kEffectStride = 0x80;
constexpr std::uint32_t kAngleX = 0x929EC8;     // s16 Camera_Angles +0
constexpr std::uint32_t kAngleY = 0x929ECA;     // s16 Camera_Angles +2
constexpr std::uint32_t kClut = 0x80F580;       // 0x2000 16-bit colours, greyed by run 4 step 0x17
constexpr unsigned kClutWords = 0x2000;
constexpr std::uint32_t kClutDirty = 0x937F90;  // Gfx_ClutStripDirty
constexpr std::uint32_t kTile = 0x939A00;       // u16: its low byte tested 0x62..0x66 (run 5 step 0x14)
constexpr std::uint32_t kHold = 0x929F12;       // Field_Kind2Hold
constexpr std::uint32_t kLoadByte = 0x929F0F;   // u8: tested for 1 before the ten-flag sets
constexpr std::uint32_t kLeaderName = 0x802DC9; // u8: ObjTrio record 0 + 0x89
constexpr std::uint32_t kMember1 = 0x80300C;    // s32: a member record's word (Field_Members + 0x184)
constexpr std::uint32_t kMemberState = 0x80310F; // u8: tested against 3 (run 9 step 0)
constexpr std::uint32_t kObjTrioZ = 0x802D74;   // s32 ObjTrio + 0x34
constexpr std::uint32_t kObjTrioY = 0x802D7C;   // s32 ObjTrio + 0x3C
constexpr std::uint32_t kPartyBytes = 0x904062; // three bytes, each tested against 2, 8, 5, 4
constexpr std::uint32_t kInputHeld = 0x7E1BE8;  // u16 Input_Held

// Chapter 12's tables (symbols.toml [[data]]), each read in place by its
// dispatcher. They lie back to back; an index past one reads the next.
constexpr std::uint32_t kStates = 0x6616F4;     // Scena12_States, 3
constexpr unsigned kStateCount = 3;
constexpr std::uint32_t kRuns = 0x661700;       // Scena12_Runs, 10
constexpr unsigned kRunCount = 10;
constexpr std::uint32_t kObjects = 0x661728;    // Scena12_Objects, 15
constexpr unsigned kObjectCount = 15;
constexpr std::uint32_t kCellRecords = 0x661764;   // Scena12_CellRecords, 4 x 5 bytes
constexpr unsigned kCellRecordCount = 4;
constexpr std::uint32_t kCellHooks = 0x661778;  // Scena12_CellHooks, 4
constexpr unsigned kCellHookCount = 4;

// Callees nobody owns (above).
constexpr std::uint32_t kPartyPass = bof3::addr::Party_HealJoined;
constexpr std::uint32_t kPartyPlace = bof3::addr::Party_PlaceForBattle;
constexpr std::uint32_t kSetBit80 = bof3::addr::Field_SetStatus80;
constexpr std::uint32_t kCellFind = bof3::addr::Field_CellTriggerAt;
constexpr std::uint32_t kBattleBytes = bof3::addr::Field_StartEventBattle;   // group SE

}  // namespace at

using VoidFn = void (__cdecl*)();
using PlaceFn = void (__cdecl*)(int x, int z, unsigned kind);
using CellFindFn = unsigned char (__cdecl*)(const void* records, unsigned count, unsigned a, unsigned b);
using KindFn = void (__cdecl*)(unsigned kind);
// A table entry as the dispatchers call it (docs/scena_sc12.md section 2).
using ObjectEntry = void (__cdecl*)(unsigned char* object, std::uint32_t bits);
using CellEntry = unsigned char (__cdecl*)(unsigned a, unsigned b);

}  // namespace scena_sc12
