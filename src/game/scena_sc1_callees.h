// Scenario chapter 1 (group SC1 of round ten): the addresses ours reads and
// the callees no group owns yet, called by address through the scenario
// harness (SH_AT). docs/scena_sc1.md.
//
// The raw callees are round ten's to rebind at the round's end: 0x4410B0 is
// group SE's (this wave); the rest are engine functions nobody has taken.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc1 {
namespace at {

// The scenario bytes (docs/field-modes.md section 2).
// kState: now MoveScript_PartyRecords[0x62] (symbols.toml). s8: the chapter's state (Scena01_States)
// kStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the scene's step
// kTimer: now MoveScript_PartyRecords[0x66] (symbols.toml). u16: the scene's timer
constexpr std::uint32_t kCounters = 0x903848;      // u8 x4: the script counters (MoveScript_CounterOps)
inline auto& kEffectSlot = SharedScratch.b_00;  // u8: the effect slot just taken
constexpr std::uint32_t kEffectSlot_addr = SharedScratch_addr; // kEffectSlot's address, for the raw accessors that take one
// kFlagBank: now Chapter_FlagRow (symbols.toml). unsigned char *: the chapter's flag bits
// kStartWord: now Cond_Flags[8] (symbols.toml). dword Scena01_Start clears; the object hook's second argument
constexpr std::uint32_t kStoryFlags = 0x90410C;    // the story flag bits Scena01_Scene06 step 0 tests (bit 4)
// kAreaByte: now AreaW1f_MusicCurrent (symbols.toml). u8 some scenes store after an area change (0xB, 1, 0xFF)

// The chapter's tables (symbols.toml [[data]]).
// kRuns: now Scena01_Runs (symbols.toml). Scena01_Runs: 24 scene handlers on MoveScript_Var7
constexpr unsigned kRunCount = 24;
// kObjects: now Scena01_ObjectHandlers (symbols.toml). Scena01_ObjectHandlers: 18, on the object's +0x86
constexpr unsigned kObjectCount = 18;
// kCells: now Scena01_Cells (symbols.toml). Scena01_Cells: 2 records of 5 bytes for 0x56D800
// kCellHandlers: now Scena01_CellHandlers (symbols.toml). Scena01_CellHandlers: 2, on 0x56D800's answer
constexpr unsigned kCellCount = 2;
constexpr std::uint32_t kEffectX = 0x660D60;       // s8 by the byte 0x904062: Scena01_PlaceEffect's +0x10

// The message box's choice word and bits Scena01_Scene06 / 0D wait on.
constexpr std::uint32_t kChoiceWord = 0x7DEE48;    // u16
constexpr std::uint32_t kChoiceBits = 0x7DEE44;    // u8, bit 1

// Bytes of the party records beyond ObjTrio's first member, and elsewhere.
// kMemberOrder: now Cond_Flags[0xd2] (symbols.toml). u8: Scena01_PlaceEffect's index into kEffectX
// kScene08A: now ScenaSc1_Scene08A (symbols.toml). u8 set 0x7F by Scena01_Scene08 step 0
// kScene08B: now ScenaSc1_Scene08B (symbols.toml). u8 cleared by Scena01_Scene08 step 0
// kScene06Member: now Character_IndexById[4] (symbols.toml). u8: the member record Scena01_Scene06 step 0xF clears bit 0 of
constexpr std::uint32_t kMemberRecords = 0x903A7B; // + 0xA4 * index: the byte whose bit 0 goes
// Game_Mode 7 (the shop, docs/mode_states.md) as Scena01_Scene11 / 17 set it.
// kShopObject: now ModeStates_ShopObject (symbols.toml). u8: 0xFE
// kShopByteA: now BattleE1_Mode (symbols.toml). u8: 0
// kShopByteB: now ScenaSc1_ShopByteB (symbols.toml). u8: 0
constexpr std::uint32_t kShopByteC = 0x929EC2;     // u8: 1

}  // namespace at

// Callees no group owns (SH_AT), with the types the originals call them by.
namespace callee {
constexpr std::uint32_t kPartyPlace = bof3::addr::Party_PlaceForBattle;    // void (int x, int z, unsigned facing): every member to (x, z)
constexpr std::uint32_t kPartyRestore = bof3::addr::Party_HealJoined;  // void (void): the members' records rebuilt (Char_RecalcStats)
constexpr std::uint32_t kAngleTest = bof3::addr::Camera_TurnToDegrees;     // unsigned char (s16 a, s8 b): 0x57C5A0 on the two scaled; al tested
constexpr std::uint32_t kStatusBit80 = bof3::addr::Field_SetStatus80;   // void (void): Field_StatusBits |= 0x80
constexpr std::uint32_t kCellFind = bof3::addr::Field_CellTriggerAt;      // unsigned char (const record *, n, x, z): the record's index or 0xFF
constexpr std::uint32_t kSeHelper = bof3::addr::Field_StartEventBattle;      // void (unsigned char n): group SE's (round ten wave one)

using PartyPlaceFn = void (__cdecl*)(int, int, unsigned);
using VoidFn = void (__cdecl*)();
using AngleTestFn = unsigned char (__cdecl*)(int, int);
using CellFindFn = unsigned char (__cdecl*)(const unsigned char*, unsigned, int, int);
using SeHelperFn = void (__cdecl*)(unsigned);
}  // namespace callee

}  // namespace scena_sc1
