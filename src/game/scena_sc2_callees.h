// Scenario chapter 2 (group SC2 of round ten's third wave): the addresses
// ours reads and the callees no group owns yet, called by address through
// the scenario harness (SH_AT). docs/scena_sc2.md.
//
// The raw callees are round ten's to rebind at the round's end: every one
// but 0x57C600 is group SX's this wave (the engine callees nobody owned);
// 0x57C600 is nobody's.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc2 {
namespace at {

// The scenario bytes (docs/field-modes.md section 2).
// kState: now MoveScript_PartyRecords[0x62] (symbols.toml). s8: the chapter's state (Scena02_States)
// kStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8: the scene's step
// kTimer: now MoveScript_PartyRecords[0x66] (symbols.toml). u16: cleared by Scena02_EnterArea
constexpr std::uint32_t kCounters = 0x903848;      // u8 x4: the script counters (MoveScript_CounterOps)
inline auto& kEffectSlot = SharedScratch.b_00;  // u8: the effect slot just taken
// kFlagBank: now Chapter_FlagRow (symbols.toml). unsigned char *: the chapter's flag row
// kRow2: now Cond_Flags[0x10] (symbols.toml). Cond_Flags + 8 * 2: chapter 2's row; Scena02_Start clears
                                                   // its first dword, the object hook passes it
// kAreaByte: now AreaW1f_MusicCurrent (symbols.toml). u8 some scenes store after an area change
constexpr std::uint32_t kLeadMember = 0x904062;    // u8: the party's first member (0, 3, 4 tested)
// kFrameByte: now Area_EnterTransition (symbols.toml). u8 Scena02_Scene04 steps 6 and 8 set to 1
// kExtraWord: now AreaW2c_LastArea (symbols.toml). u16 just past Sprite_ObjectsExtra: area 0x10's test (0x5A)
// kMemberSeven: now Field_Members[0x13b] (symbols.toml). u8: Scena02_Scene10 step 0xF waits while it is 7
constexpr std::uint32_t kChoiceBits = 0x7DEE44;    // u8, bit 1: the message box's choice made

// The chapter's tables (symbols.toml [[data]]).
// kStates: now Scena02_States (symbols.toml). Scena02_States: 3, on the s8 state
constexpr unsigned kStateCount = 3;
// kRuns: now Scena02_Runs (symbols.toml). Scena02_Runs: 28 scene handlers on MoveScript_Var7
constexpr unsigned kRunCount = 28;
// kObjects: now Scena02_ObjectHandlers (symbols.toml). Scena02_ObjectHandlers: 27, on the object's +0x86
constexpr unsigned kObjectCount = 27;
// kEffectX: now Scena02_EffectX (symbols.toml). s8 by kLeadMember: the place helpers' +0x10

// The members' records (docs/scena_sc1.md section 6): eight of 0xA4 from
// 0x903A70; +0x7E ten bytes Scena02_StripMember hands back one by one.
// kMemberRecords: now CharacterRecords (symbols.toml).
constexpr std::uint32_t kMemberStride = 0xA4;
constexpr std::uint32_t kMemberItems = 0x7E;

// Game_Mode 7 with the object 0xFE: the shop (docs/mode_states.md section 1),
// as Scena02_Scene0B and 1B open it.
// kShopObject: now ModeStates_ShopObject (symbols.toml). u8: 0xFE
// kShopByteA: now BattleE1_Mode (symbols.toml). u8: 0
// kShopByteB: now ScenaSc1_ShopByteB (symbols.toml). u8: 0
constexpr std::uint32_t kShopByteC = 0x929EC2;     // u8: 1

}  // namespace at

// Callees no group owns (SH_AT), with the types the originals call them by.
namespace callee {
constexpr std::uint32_t kPartyPlace = bof3::addr::Party_PlaceForBattle;    // void (int x, int z, unsigned kind): the party placed for an event battle
constexpr std::uint32_t kPartyRestore = bof3::addr::Party_HealJoined;  // void (void): the members' records rebuilt (Char_RecalcStats)
constexpr std::uint32_t kStatusBit80 = bof3::addr::Field_SetStatus80;   // void (void): Field_StatusBits |= 0x80
constexpr std::uint32_t kMusicStop = bof3::addr::Sound_StopMusic;     // void (void): a jmp to 0x5A6FF0, the sound layer
constexpr std::uint32_t kItemPut = bof3::addr::AbilityList_Add;       // (u8 item, x, u8, y): an item handed back to the inventory
constexpr std::uint32_t kInventoryTake = bof3::addr::Inventory_Remove; // (category, item, count): the item's count lowered
constexpr std::uint32_t kMoneyTake = bof3::addr::Zenny_Sub;     // unsigned char (amount, flag): al 0 when the party has too little
constexpr std::uint32_t kMoneyGive = bof3::addr::Zenny_Add;     // (amount, flag): the party's money raised
constexpr std::uint32_t kTurnTest = bof3::addr::Camera_TurnFBToDegrees;      // unsigned char (s16 a, s8 b): 0x57C650 on the two scaled, al tested - nobody's

using VoidFn = void (__cdecl*)();
using PartyPlaceFn = void (__cdecl*)(int, int, unsigned);
using ItemPutFn = void (__cdecl*)(unsigned, unsigned, unsigned, unsigned);
using InventoryTakeFn = void (__cdecl*)(unsigned, unsigned, unsigned, unsigned);
using MoneyTakeFn = unsigned char (__cdecl*)(unsigned, unsigned);
using MoneyGiveFn = void (__cdecl*)(unsigned, unsigned);
using TurnTestFn = unsigned char (__cdecl*)(int, int);
}  // namespace callee

}  // namespace scena_sc2
