// Scenario chapter 1 (group SC1 of round ten): the addresses ours reads and
// the callees no group owns yet, called by address through the scenario
// harness (SH_AT). docs/scena_sc1.md.
//
// The raw callees are round ten's to rebind at the round's end: 0x4410B0 is
// group SE's (this wave); the rest are engine functions nobody has taken.
#pragma once

#include <cstdint>

namespace scena_sc1 {
namespace at {

// The scenario bytes (docs/field-modes.md section 2).
constexpr std::uint32_t kState = 0x8034E2;         // s8: the chapter's state (Scena01_States)
constexpr std::uint32_t kStep = 0x8034E5;          // u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;         // u16: the scene's timer
constexpr std::uint32_t kCounters = 0x903848;      // u8 x4: the script counters (MoveScript_CounterOps)
constexpr std::uint32_t kEffectSlot = 0x903850;    // u8: the effect slot just taken
constexpr std::uint32_t kFlagBank = 0x929ED0;      // unsigned char *: the chapter's flag bits
constexpr std::uint32_t kStartWord = 0x903F98;     // dword Scena01_Start clears; the object hook's second argument
constexpr std::uint32_t kStoryFlags = 0x90410C;    // the story flag bits Scena01_Scene06 step 0 tests (bit 4)
constexpr std::uint32_t kAreaByte = 0x904CD0;      // u8 some scenes store after an area change (0xB, 1, 0xFF)

// The chapter's tables (symbols.toml [[data]]).
constexpr std::uint32_t kRuns = 0x660D88;          // Scena01_Runs: 24 scene handlers on MoveScript_Var7
constexpr unsigned kRunCount = 24;
constexpr std::uint32_t kObjects = 0x660DE8;       // Scena01_ObjectHandlers: 18, on the object's +0x86
constexpr unsigned kObjectCount = 18;
constexpr std::uint32_t kCells = 0x660E30;         // Scena01_Cells: 2 records of 5 bytes for 0x56D800
constexpr std::uint32_t kCellHandlers = 0x660E3C;  // Scena01_CellHandlers: 2, on 0x56D800's answer
constexpr unsigned kCellCount = 2;
constexpr std::uint32_t kEffectX = 0x660D60;       // s8 by the byte 0x904062: Scena01_PlaceEffect's +0x10

// The message box's choice word and bits Scena01_Scene06 / 0D wait on.
constexpr std::uint32_t kChoiceWord = 0x7DEE48;    // u16
constexpr std::uint32_t kChoiceBits = 0x7DEE44;    // u8, bit 1

// Bytes of the party records beyond ObjTrio's first member, and elsewhere.
constexpr std::uint32_t kMemberOrder = 0x904062;   // u8: Scena01_PlaceEffect's index into kEffectX
constexpr std::uint32_t kScene08A = 0x803157;      // u8 set 0x7F by Scena01_Scene08 step 0
constexpr std::uint32_t kScene08B = 0x92BF17;      // u8 cleared by Scena01_Scene08 step 0
constexpr std::uint32_t kScene06Member = 0x669730; // u8: the member record Scena01_Scene06 step 0xF clears bit 0 of
constexpr std::uint32_t kMemberRecords = 0x903A7B; // + 0xA4 * index: the byte whose bit 0 goes
// Game_Mode 7 (the shop, docs/mode_states.md) as Scena01_Scene11 / 17 set it.
constexpr std::uint32_t kShopObject = 0x929F0C;    // u8: 0xFE
constexpr std::uint32_t kShopByteA = 0x929F00;     // u8: 0
constexpr std::uint32_t kShopByteB = 0x929EC3;     // u8: 0
constexpr std::uint32_t kShopByteC = 0x929EC2;     // u8: 1

}  // namespace at

// Callees no group owns (SH_AT), with the types the originals call them by.
namespace callee {
constexpr std::uint32_t kPartyPlace = 0x532ED0;    // void (int x, int z, unsigned facing): every member to (x, z)
constexpr std::uint32_t kPartyRestore = 0x533E50;  // void (void): the members' records rebuilt (Char_RecalcStats)
constexpr std::uint32_t kAngleTest = 0x57C550;     // unsigned char (s16 a, s8 b): 0x57C5A0 on the two scaled; al tested
constexpr std::uint32_t kStatusBit80 = 0x56D6F0;   // void (void): Field_StatusBits |= 0x80
constexpr std::uint32_t kCellFind = 0x56D800;      // unsigned char (const record *, n, x, z): the record's index or 0xFF
constexpr std::uint32_t kSeHelper = 0x4410B0;      // void (unsigned char n): group SE's (round ten wave one)

using PartyPlaceFn = void (__cdecl*)(int, int, unsigned);
using VoidFn = void (__cdecl*)();
using AngleTestFn = unsigned char (__cdecl*)(int, int);
using CellFindFn = unsigned char (__cdecl*)(const unsigned char*, unsigned, int, int);
using SeHelperFn = void (__cdecl*)(unsigned);
}  // namespace callee

}  // namespace scena_sc1
