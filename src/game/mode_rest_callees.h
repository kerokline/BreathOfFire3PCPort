// The raw addresses mode_rest.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/mode-rest.md.
//
// Every function the group calls is named (symbols.toml) and ours but Rand,
// the C runtime's (called by name). The constants below are cells and the
// image's read-only tables, read in place (never copied).
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"

namespace mode_rest::at {

using U = std::uint32_t;

// --- mode 3 (the field menu): its two steps of ours ---------------------------------
constexpr U kMenuFile = 0x31E;             // the DAT GameMode3_Enter loads (the PSX loads 0x269)
// kMenuMode: now BattleE1_Mode (symbols.toml). the menu block's mode byte: 0 on entry
constexpr U kMenuTimer = 0x929F04;         // the menu block's byte +4: 3 on entry
// kPartyChanged: now MenuLists_PartyChanged (symbols.toml). u8: the menu changed the party - its set reloaded on leaving
// kPartyList: now Cond_Flags[0xd2] (symbols.toml). u8 x 3: the party list PartySet_Load is handed (Cond_Flags' row)
// kMusicWord: now ModeRest_MusicWord (symbols.toml). u16: 0x6E asks the music fade on entry and leaving (PSX 0x80143F20)
// kPartySet: now AreaW0b_PartySet (symbols.toml). u8: the party set (PartySet_Select's record); bit 7 set on leaving
constexpr U kPartySetFiles = 0x2C2;        // + the set's low seven bits: its mode-0 file (Snd_LoadBankFile)
constexpr U kCommuFile = 0x12A;            // the DAT loaded with Field_InputFlags bit 4 before CommuSim_RollOffers
// kTripAsked: now MenuLists_CampChosen (symbols.toml). u8: the menu asked for the two-way trip (cleared on leaving)
// kAreaAsked: now ModeRest_AreaAsked (symbols.toml). u8: the menu asked for an area by number (cleared on leaving)
// kTripOut: now AreaW3c_CampFlag (symbols.toml). u8: the trip is out (1) or home (0)
// kTripFacing: now AreaW3g_HealByte (symbols.toml). u8: the facing saved going out
// kTripX: now AreaW2d_ReturnPoint (symbols.toml). dword: the x saved going out
// kTripZ: now AreaW4d_ReturnZ (symbols.toml). dword: the z saved going out
// kTripArea: now AreaW4d_ReturnArea (symbols.toml). u16: the area saved going out
// kAreaWord: now Game_AreaNumber (symbols.toml). u16: Game_AreaNumber (scenario_harness.h's name)
constexpr U kTripAnyArea = 0xBD;           // the area the trip may start from without Field_InputFlags bit 0
// kLeaderX: now rv::As<records::PartyMember>(ObjTrio)->x (records.h). dword: ObjTrio record 0 +0x34, the leader's x
// kLeaderZ: now rv::As<records::PartyMember>(ObjTrio)->z (records.h). dword: ... +0x38, its z
// kLeaderFacing: now ObjTrio[8] (symbols.toml). u8: ObjTrio +8, the leader's facing
constexpr U kFacingJitter = 0x656A80;      // s8 x 4: the facing's jitter by Rand & 3 (image .data, after GameMode3_Steps)
// kPendingPlace: now AreaW0b_Place (symbols.toml). u16: the trip's destination area
inline auto& kPendingX = SharedScratch.d_10;          // dword: its x
// kPendingZ: now AreaEntry_PendingZ (symbols.toml). dword: its z
// kPendingFlags: now AreaEntry_PendingFlags (symbols.toml). u8: its Field_ChangeArea flags
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the story flag row Flags_Clear is handed
constexpr unsigned kTripFlag = 0x77;       // the story flag cleared coming home
constexpr U kAreaAskedX = 0x430000;        // the x and z an area asked by number is entered at
constexpr U kAreaAskedZ = 0x160000;

// --- mode 4 ------------------------------------------------------------------------
constexpr U kMessageBits = 0x7DEE44;       // u8: bit 1 ends mode 4 (the message box's cells, scenario_harness.h)
// kCameraTurnByte: now EventOps_BlockByte (symbols.toml). u8: 0x10 when mode 4 ends (CameraTurn_Steps' cells)

// --- mode 5 (the battle) -----------------------------------------------------------
// kBattleFlags: now EventBattle_Flags (symbols.toml). u8: EventBattle_Records +0's flags (Field_StartEventBattle)
// kBattleFlags2: now Battle_EndFlags (symbols.toml). u8: bit 3 keeps the field's music off after the battle
// kAreaTrack: now AreaW1f_MusicCurrent (symbols.toml). u8: the area's track Field_ChangeArea chose (mode_flow kAreaTrack); DIV-0092
// kBattleFlags3: now BattleMisc_BannerKinds (symbols.toml). u8: zeroed at steps 3 and 4
constexpr U kFormation = 0x904AAC;         // dword read, its low byte the formation (rest_3g_callees.h)
constexpr U kApproach = 0x656AA4;          // s8 pairs by the formation byte: x (+0) and z (+1) steps (image .data, after GameMode5_Steps)
// kElevationBase: now ModeRest_ElevationBase (symbols.toml). dword: less MapView_Elevation, sar 4, into MoveScript_FAWord
constexpr U kKind2ZHigh = 0x905E62;        // u16: Field_Kind2Z's upper half
constexpr U kKind2XHigh = 0x905E66;        // u16: Field_Kind2X's upper half
// kEventBattle: now EventBattle_Id (symbols.toml).
// kLeaderXHigh: now ObjTrio[0x36] (symbols.toml). u8: the leader's integer x, low byte (field_event_callees.h); DIV-0092's second amendment
// kLeaderZHigh: now ObjTrio[0x3a] (symbols.toml). u8: its z       // u8: the event battle (EventBattle_Records' index)
constexpr U kEventAsset = 0x64DDEF;        // EventBattle_Records + 3: each record's asset index (image .data)
constexpr U kEventFile = 0x64DECC;         // u16 pairs (file, track) by that index, stride 4 (image .data)
constexpr U kEventTrack = 0x64DECE;
constexpr U kBattleWord = 0x904AA8;        // u16: zeroed at step 3
constexpr U kBattleBytes = 0x904AA0;       // u8 x 5: the phase bytes, zeroed at steps 3 and 4
constexpr U kMemberCount = 0x904AB0;       // u8: Party_Count(1) at step 4
constexpr unsigned kChapterSplit = 8;      // Cond_ByteFA (s8) below it: the first battle file and track
constexpr U kBattleFileA = 0xD2, kBattleTrackA = 0x97;
constexpr U kBattleFileB = 0xD3, kBattleTrackB = 0x99;

}  // namespace mode_rest::at
