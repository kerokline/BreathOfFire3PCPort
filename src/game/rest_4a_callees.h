// The raw addresses rest_4a.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/rest_4a.md.
//
// Callees by address (owned by a group of this round's wave four; ours, named
// by symbol below, the value unchanged: round fourteen's rebinding,
// docs/round-14-cleanup.md):
//   0x45E6B0  R4D's: () -> al, the count of the 60 community records whose
//             byte +0 is not 0 (read 2026-10-05 for its answer only: al from
//             0, one per record; the rest of eax is the caller's).
// Everything else the group calls is ours, by name, or Capcom's C runtime
// (Rand, Crt_sprintf). The constants below are cells and the image's
// read-only tables, read in place (never copied).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_4a::at {

using U = std::uint32_t;

// --- callees ours, by address (through the harness) ----------------------------
constexpr U kCommuCount = bof3::addr::CommuName_CountSlots;      // R4D's: unsigned char(void), the records in use

// --- the battle (Battle_* of this group) ----------------------------------------
constexpr U kBattleFlags = 0x904AA8;       // dword: bit 14 the auto-target check's switch
constexpr U kAutoMode = 0x904B35;          // u8: the auto-target mode (4: by the enemy's ability)
constexpr U kTarget = 0x904B44;            // u8: the target the checks store
constexpr U kForcedActor = 0x904B8B;       // u8: the actor the auto-target check compares
constexpr U kPartyPick = 0x904AB1;         // u8: 1 keeps a member's pick off the party
constexpr U kMemberAction = 0x802E66;      // ObjTrio + 0x126 (u16): a member's action
constexpr U kMemberMode = 0x802E65;        // ObjTrio + 0x125 (u8): a member's auto mode
constexpr U kMemberOdds = 0x802E82;        // ObjTrio + 0x142 (s8): the odds of a party pick, by 20
constexpr U kMemberStride = 0x14C;
constexpr U kEnemyHp = 0x93BA1A;           // enemy record + 0xBA (u16), records from 0x93B960, 0x128 apart
constexpr U kEnemyStatus = 0x93B9F2;       // enemy record + 0x92 (u8): bit 5
constexpr U kEnemyFlags = 0x93BA74;        // enemy record + 0x114 (dword): bit 14
constexpr U kEnemyAbility = 0x93BA66;      // enemy record + 0x106 (u16): the ability
constexpr U kEnemyStride = 0x128;
constexpr U kAbilityFlags = 0x65C4D8;      // NameTable_Abilities: 24-byte records, byte +0 the flags (image .data)
constexpr U kAbilityStride = 24;

// --- the field slots (Field_RunSlot) ---------------------------------------------
constexpr U kSlotDivisors = 0x6528E4;      // u8 by object +0x28: the frames a CLUT row holds (image .data)
constexpr U kSlotSteps = 0x6528EC;         // u8 by object +0x28: the columns a frame steps (image .data)

// --- the community's simulation (CommuSim_*) -------------------------------------
constexpr U kClock = 0x904134;             // dword: the clock the simulation compares (GameMode_Field counts it on Field_Request 3)
constexpr U kClockA = 0x9046B0;            // dword: cleared at the first entry, not read here
constexpr U kStampGrow = 0x9046B4;         // dword: the clock at the last growth
constexpr U kStampShrink = 0x9046B8;       // dword: the clock at the last shrink
constexpr U kStampMood = 0x9046BC;         // dword: the clock at the last mood update
constexpr U kStampLevelLit = 0x9046C0;     // dword: the clock at the last kind-4 level (lit buildings)
constexpr U kStampLevelDark = 0x9046C4;    // dword: the clock at the last kind-4 level (dark buildings)
constexpr U kAreaSeen = 0x9046C8;          // u16: the highest area the queue has seen
constexpr U kMood = 0x9046CA;              // s8: 0..99
constexpr U kLevelDark = 0x9046CB;         // u8: below 10
constexpr U kLevelLit = 0x9046CC;          // u8: below 7
constexpr U kSumA = 0x9046CD;              // u8: kind 0xA's sum
constexpr U kSumB = 0x9046CE;              // u8: kind 0xB's sum
constexpr U kRecords = 0x9046D0;           // 60 records of 8: +0 in use, +1 building (1-based) or a kind, +2, +3, +4 a clock stamp
constexpr U kRecordsEnd = 0x9048B0;
constexpr unsigned kRecordCount = 60;
constexpr U kBuildings = 0x9048B0;         // 8 records of 8: +0 the kind, +1, +2, +3 a level, +4 a clock stamp
constexpr unsigned kBuildingCount = 8;
constexpr U kRecordCopies = 0x9048F0;      // 60 x 5 bytes copied from kRecordTraits at a spawn
constexpr U kSpawnCount = 0x904A90;        // u8: kSpawned's count
constexpr U kSpawned = 0x904F00;           // u8 list: the records spawned
constexpr U kRemovedCount = 0x9039A0;      // u8: kRemoved's count
constexpr U kRemoved = 0x9039C0;           // u8 list: the records removed (bit 7: removed at a kind-9 roll)
constexpr U kEventCount = 0x937F80;        // u8: kEvents' count
constexpr U kEvents = 0x904CA0;            // 2-byte events (kind, record)
constexpr U kPrevArea = 0x802290;          // u16: the area the field came from
constexpr U kAreaKinds = 0x652850;         // u8 by area number (0x652900 - 0xB0): kinds 4 and 5 queue an event (image .data)
constexpr U kLevelLitCosts = 0x65290C;     // u16 by kLevelLit (image .data)
constexpr U kLevelDarkNeeds = 0x65291B;    // u8 by kLevelDark: the kLevelLit it needs (image .data)
constexpr U kPriceTiers = 0x652928;        // 8 records of 6: u16 price bound, u16 clock span, u16 odds (image .data)
constexpr U kPriceTiersEnd = 0x652958;
constexpr U kKindBCaps = 0x652958;         // u8 by kLevelLit: kind 0xB's cap (image .data)
constexpr U kObjectPlaces = 0x652960;      // 3 bytes by object: x, z, a flag (image .data)
constexpr U kResidentPlaces = 0x65299C;    // 3 bytes by (count + building * 3): x, z, facing (image .data)
constexpr U kBanks = 0x6528F4;             // u16 by (record % 6): the animation bank (image .data)
constexpr U kRecordTraits = 0x653200;      // 60 records of 0x14 (image .data); +0x10..+0x13 read by the ticks
constexpr unsigned kTraitStride = 0x14;
constexpr U kConsumables = 0x656B38;       // NameTable_Consumables + 0x10: 22-byte records (byte +0, u16 +4)
constexpr U kWeapons = 0x657461;           // NameTable_Weapons + 0x11: 28-byte records (byte +0, u16 +9)
constexpr U kArmour = 0x657D79;            // NameTable_Armour + 0x11: 26-byte records (byte +0, u16 +7)
constexpr U kAccessories = 0x658461;       // NameTable_Accessories + 0x11: 24-byte records (byte +0, u16 +5)
constexpr U kResidentCounts = 0x675F58;    // u8 by building: the residents placed so far (.data, written)
constexpr U kOfferWords = 0x675F60;        // 3 x 3 u16 (.data, written)
constexpr U kOfferWordsEnd = 0x675F72;
constexpr U kSpriteBase = 0x7DEE80;        // Sprite_Objects
constexpr U kSpriteStride = 0xA4;
constexpr U kSpritePose = 0x7DEF08;        // Sprite_Objects + 0x88 (u16)
constexpr U kSpriteKind = 0x7DEE86;        // Sprite_Objects + 6
constexpr U kSpriteHold = 0x7DEF04;        // Sprite_Objects + 0x84
constexpr U kStoryFlags = 0x904030;        // the story flag row Flags_* are handed

// --- the object triggers (Field_ObjectTriggers 1..11 and 61) ---------------------
constexpr U kTailKind = 0x9039F3;          // u8: the tail kind the event engine runs next
constexpr U kTailSub = 0x9039F5;           // u8: its sub-kind
constexpr U kTriggerObject = 0x939A38;     // dword: the object the trigger ran for
constexpr U kShopRecords = 0x658930;       // Shop_Records: 23-byte records (.data, written)
constexpr U kShopStride = 23;
constexpr U kBuildingLevels = 0x9048AB;    // kBuildings - 8 + 3: +3 of building (b - 1)
constexpr U kNumberFormat = 0x5E10C0;      // Area08_MessageFormat (image .data)
constexpr U kTextRow0 = 0x904CE0;          // Text_Records rows 0..3
constexpr U kTextRow1 = 0x904D00;
constexpr U kTextRow2 = 0x904D20;
constexpr U kTextRow3 = 0x904D40;
constexpr U kCountA = 0x9040C8;            // u8
constexpr U kCountB = 0x9040C9;            // u8
constexpr U kCountC = 0x904138;            // dword
constexpr U kCountD = 0x90413C;            // dword
constexpr U kCountE = 0x904140;            // dword
constexpr U kCountF = 0x904144;            // dword

}  // namespace rest_4a::at
