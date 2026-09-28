// Internal to scena_sc5.cpp and scena_sc5_fuzz.cpp: the cells chapter 5's
// bank (0x546390..0x54A910, group SC5 of round ten's second wave) reads and
// writes, its tables, and the raw addresses of the callees it reaches that
// nobody owns yet. docs/scena_sc5.md.
//
// Calls into code this group does not own, by raw address (SH_AT)
// (Scenario_CallB 0x5341C0, named by SCH and not taken, is called by name;
// Field_StartEventBattle 0x4410B0, group SE's, is ours and called by name):
//   0x532ED0  (x, z, kind): an event battle's party placement - engine, nobody's
//   0x533E50  a pass over the 8 records at 0x903A70 and the party - engine,
//             nobody's
//   0x56D6F0  0x8034E1 |= 0x80 (two instructions) - engine, nobody's
//   0x56D800  (records, count, a, b): the index of the first of `count` 5-byte
//             records matching Game_AreaNumber and the cell (a, b), or 0xFF -
//             engine, nobody's (the cell-hook search)
// Chapter 5's closure beyond the band is reached only through its state
// table read in place: state 0 (0x5646B0, group SC13's block, a later wave).
// The call tables' entries (Scena05_CallA / _CallB, 0x519890..0x51AC50, group
// CALLS this wave) are reached only through Scenario_CallA / Scenario_CallB.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc5 {

namespace at {

// The scenario bytes (docs/field-modes.md section 2).
constexpr std::uint32_t kState = 0x8034E2;      // s8: the chapter's state, 0..2
constexpr std::uint32_t kRun = 0x8034E4;        // s8: MoveScript_Var7, which scene
constexpr std::uint32_t kStep = 0x8034E5;       // u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;      // u16: a scene's frame count
constexpr std::uint32_t kCounters = 0x903848;   // four script counters (MoveScript_CounterOps)
constexpr std::uint32_t kFlagBits = 0x929ED0;   // unsigned char *: the bits Flags_Test / Set / Clear take
constexpr std::uint32_t kStoryFlags = 0x904030; // Cond_Flags + 0xA0, handed to Flags_Set / Clear as a literal
constexpr std::uint32_t kSelector = 0x90412C;   // Cond_Flags + 0x19C: & 0x7F, 1 or 2 picks a branch
constexpr std::uint32_t kArea = 0x904EFC;       // u16 Game_AreaNumber
constexpr std::uint32_t kPassFlags = 0x7E0918;  // Draw_PassFlags
constexpr std::uint32_t kScriptFlags = 0x9039A2;   // u16 Field_ScriptFlags
constexpr std::uint32_t kRequest = 0x66C7D8;    // Field_Request
constexpr std::uint32_t kWait = 0x66C810;       // u16 MoveScript_WaitWordDA
constexpr std::uint32_t kMusicCurrent = 0x904CD0;  // u8: Music_Play returns at once while its track equals it
constexpr std::uint32_t kMsgFlags = 0x7DEE44;   // u8 (MsgBoxState +4): bit 1 set once a message is done
constexpr std::uint32_t kMemberCount = 0x929EC0;   // Field_MemberCount
constexpr std::uint32_t kSpriteCurrent = 0x937F88; // unsigned char * Sprite_Current
constexpr std::uint32_t kObjTrio = 0x802D40;    // ObjTrio record 0
constexpr std::uint32_t kLeaderX = 0x802D74;    // s32 ObjTrio + 0x34
constexpr std::uint32_t kLeaderZ = 0x802D78;    // s32 ObjTrio + 0x38
// The party lists (Cond_Flags + 0xD2..): 0x904062 the first member, read as a
// case 0..6 and as an index; 0x904065 / 0x904066 read as cases 0, 1, 5, 6.
constexpr std::uint32_t kParty = 0x904062;
constexpr std::uint32_t kPartyA = 0x904065;
constexpr std::uint32_t kPartyB = 0x904066;
// Three bytes Scena05_EnterArea and Scena05_Run20 set bit 0 of (and clear, in
// 0x903DAF / 0x903E53's case, by the branch): what they mean is not read here.
constexpr std::uint32_t kBitsB1F = 0x903B1F;
constexpr std::uint32_t kBitsDAF = 0x903DAF;
constexpr std::uint32_t kBitsE53 = 0x903E53;
// Two bytes Scena05_Run17 step 8 sets (0x7F, 0) and step 0xB tests (s8 at
// most 8): read by nothing else in the band.
constexpr std::uint32_t kLevel = 0x803150;
constexpr std::uint32_t kLevelB = 0x92BF10;
constexpr std::uint32_t kEffects = 0x7E11E0;    // Effect_Objects: records of 0x80
constexpr std::uint32_t kEffectStride = 0x80;

// Chapter 5's tables (symbols.toml [[data]]), each read in place by its
// dispatcher. They lie back to back after the vtable Scena05_Hooks 0x661020;
// an index past one reads the next.
constexpr std::uint32_t kMemberBytes = 0x661018;   // Scena05_MemberBytes, 8 s8 by the first member
constexpr unsigned kMemberByteCount = 8;
constexpr std::uint32_t kStates = 0x661034;     // Scena05_States, 3
constexpr unsigned kStateCount = 3;
constexpr std::uint32_t kRuns = 0x661040;       // Scena05_Runs, 23
constexpr unsigned kRunCount = 23;
constexpr std::uint32_t kObjects = 0x66109C;    // Scena05_Objects, 9
constexpr unsigned kObjectCount = 9;
constexpr std::uint32_t kCellRecords = 0x6610C0;   // Scena05_CellRecords, 2 x 5 bytes
constexpr unsigned kCellRecordCount = 2;
constexpr std::uint32_t kCellHooks = 0x6610CC;  // Scena05_CellHooks, 2
constexpr unsigned kCellHookCount = 2;

// Callees nobody owns (above).
constexpr std::uint32_t kPartyPlace = bof3::addr::Party_PlaceForBattle;
constexpr std::uint32_t kPartyPass = bof3::addr::Party_HealJoined;
constexpr std::uint32_t kSetBit80 = bof3::addr::Field_SetStatus80;
constexpr std::uint32_t kCellFind = bof3::addr::Field_CellTriggerAt;

}  // namespace at

using VoidFn = void (__cdecl*)();
using PlaceFn = void (__cdecl*)(int x, int z, unsigned kind);
using CellFindFn = unsigned char (__cdecl*)(const void* records, unsigned count, unsigned a, unsigned b);
// A table entry as the dispatchers call it (docs/scena_sc5.md section 2).
using ObjectEntry = void (__cdecl*)(unsigned char* object, std::uint32_t bits);
using CellEntry = unsigned char (__cdecl*)(unsigned a, unsigned b);

}  // namespace scena_sc5
