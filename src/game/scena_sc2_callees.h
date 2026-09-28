// Scenario chapter 2 (group SC2 of round ten's third wave): the addresses
// ours reads and the callees no group owns yet, called by address through
// the scenario harness (SH_AT). docs/scena_sc2.md.
//
// The raw callees are round ten's to rebind at the round's end: every one
// but 0x57C600 is group SX's this wave (the engine callees nobody owned);
// 0x57C600 is nobody's.
#pragma once

#include <cstdint>

namespace scena_sc2 {
namespace at {

// The scenario bytes (docs/field-modes.md section 2).
constexpr std::uint32_t kState = 0x8034E2;         // s8: the chapter's state (Scena02_States)
constexpr std::uint32_t kStep = 0x8034E5;          // u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;         // u16: cleared by Scena02_EnterArea
constexpr std::uint32_t kCounters = 0x903848;      // u8 x4: the script counters (MoveScript_CounterOps)
constexpr std::uint32_t kEffectSlot = 0x903850;    // u8: the effect slot just taken
constexpr std::uint32_t kFlagBank = 0x929ED0;      // unsigned char *: the chapter's flag row
constexpr std::uint32_t kRow2 = 0x903FA0;          // Cond_Flags + 8 * 2: chapter 2's row; Scena02_Start clears
                                                   // its first dword, the object hook passes it
constexpr std::uint32_t kAreaByte = 0x904CD0;      // u8 some scenes store after an area change
constexpr std::uint32_t kLeadMember = 0x904062;    // u8: the party's first member (0, 3, 4 tested)
constexpr std::uint32_t kFrameByte = 0x937F98;     // u8 Scena02_Scene04 steps 6 and 8 set to 1
constexpr std::uint32_t kExtraWord = 0x802290;     // u16 just past Sprite_ObjectsExtra: area 0x10's test (0x5A)
constexpr std::uint32_t kMemberSeven = 0x802FC3;   // u8: Scena02_Scene10 step 0xF waits while it is 7
constexpr std::uint32_t kChoiceBits = 0x7DEE44;    // u8, bit 1: the message box's choice made

// The chapter's tables (symbols.toml [[data]]).
constexpr std::uint32_t kStates = 0x660E64;        // Scena02_States: 3, on the s8 state
constexpr unsigned kStateCount = 3;
constexpr std::uint32_t kRuns = 0x660E70;          // Scena02_Runs: 28 scene handlers on MoveScript_Var7
constexpr unsigned kRunCount = 28;
constexpr std::uint32_t kObjects = 0x660EE0;       // Scena02_ObjectHandlers: 27, on the object's +0x86
constexpr unsigned kObjectCount = 27;
constexpr std::uint32_t kEffectX = 0x660E48;       // s8 by kLeadMember: the place helpers' +0x10

// The members' records (docs/scena_sc1.md section 6): eight of 0xA4 from
// 0x903A70; +0x7E ten bytes Scena02_StripMember hands back one by one.
constexpr std::uint32_t kMemberRecords = 0x903A70;
constexpr std::uint32_t kMemberStride = 0xA4;
constexpr std::uint32_t kMemberItems = 0x7E;

// Game_Mode 7 with the object 0xFE: the shop (docs/mode_states.md section 1),
// as Scena02_Scene0B and 1B open it.
constexpr std::uint32_t kShopObject = 0x929F0C;    // u8: 0xFE
constexpr std::uint32_t kShopByteA = 0x929F00;     // u8: 0
constexpr std::uint32_t kShopByteB = 0x929EC3;     // u8: 0
constexpr std::uint32_t kShopByteC = 0x929EC2;     // u8: 1

}  // namespace at

// Callees no group owns (SH_AT), with the types the originals call them by.
namespace callee {
constexpr std::uint32_t kPartyPlace = 0x532ED0;    // void (int x, int z, unsigned kind): the party placed for an event battle
constexpr std::uint32_t kPartyRestore = 0x533E50;  // void (void): the members' records rebuilt (Char_RecalcStats)
constexpr std::uint32_t kStatusBit80 = 0x56D6F0;   // void (void): Field_StatusBits |= 0x80
constexpr std::uint32_t kMusicStop = 0x587B80;     // void (void): a jmp to 0x5A6FF0, the sound layer
constexpr std::uint32_t kItemPut = 0x590C90;       // (u8 item, x, u8, y): an item handed back to the inventory
constexpr std::uint32_t kInventoryTake = 0x591B60; // (category, item, count): the item's count lowered
constexpr std::uint32_t kMoneyTake = 0x591BC0;     // unsigned char (amount, flag): al 0 when the party has too little
constexpr std::uint32_t kMoneyGive = 0x591BE0;     // (amount, flag): the party's money raised
constexpr std::uint32_t kTurnTest = 0x57C600;      // unsigned char (s16 a, s8 b): 0x57C650 on the two scaled, al tested - nobody's

using VoidFn = void (__cdecl*)();
using PartyPlaceFn = void (__cdecl*)(int, int, unsigned);
using ItemPutFn = void (__cdecl*)(unsigned, unsigned, unsigned, unsigned);
using InventoryTakeFn = void (__cdecl*)(unsigned, unsigned, unsigned, unsigned);
using MoneyTakeFn = unsigned char (__cdecl*)(unsigned, unsigned);
using MoneyGiveFn = void (__cdecl*)(unsigned, unsigned);
using TurnTestFn = unsigned char (__cdecl*)(int, int);
}  // namespace callee

}  // namespace scena_sc2
