// The raw addresses mode_rest.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/mode-rest.md.
//
// Every function the group calls is named (symbols.toml) and ours but Rand,
// the C runtime's (called by name). The constants below are cells and the
// image's read-only tables, read in place (never copied).
#pragma once

#include <cstdint>

namespace mode_rest::at {

using U = std::uint32_t;

// --- mode 3 (the field menu): its two steps of ours ---------------------------------
constexpr U kMenuFile = 0x31E;             // the DAT GameMode3_Enter loads (the PSX loads 0x269)
constexpr U kMenuMode = 0x929F00;          // the menu block's mode byte: 0 on entry
constexpr U kMenuTimer = 0x929F04;         // the menu block's byte +4: 3 on entry
constexpr U kPartyChanged = 0x929F11;      // u8: the menu changed the party - its set reloaded on leaving
constexpr U kPartyList = 0x904062;         // u8 x 3: the party list PartySet_Load is handed (Cond_Flags' row)
constexpr U kMusicWord = 0x7E0678;         // u16: 0x6E asks the music fade on entry and leaving (PSX 0x80143F20)
constexpr U kPartySet = 0x90412C;          // u8: the party set (PartySet_Select's record); bit 7 set on leaving
constexpr U kPartySetFiles = 0x2C2;        // + the set's low seven bits: its mode-0 file (Snd_LoadBankFile)
constexpr U kCommuFile = 0x12A;            // the DAT loaded with Field_InputFlags bit 4 before CommuSim_RollOffers
constexpr U kTripAsked = 0x905B60;         // u8: the menu asked for the two-way trip (cleared on leaving)
constexpr U kAreaAsked = 0x905B61;         // u8: the menu asked for an area by number (cleared on leaving)
constexpr U kTripOut = 0x904152;           // u8: the trip is out (1) or home (0)
constexpr U kTripFacing = 0x904153;        // u8: the facing saved going out
constexpr U kTripX = 0x904148;             // dword: the x saved going out
constexpr U kTripZ = 0x90414C;             // dword: the z saved going out
constexpr U kTripArea = 0x904150;          // u16: the area saved going out
constexpr U kAreaWord = 0x904EFC;          // u16: Game_AreaNumber (scenario_harness.h's name)
constexpr U kTripAnyArea = 0xBD;           // the area the trip may start from without Field_InputFlags bit 0
constexpr U kLeaderX = 0x802D74;           // dword: ObjTrio record 0 +0x34, the leader's x
constexpr U kLeaderZ = 0x802D78;           // dword: ... +0x38, its z
constexpr U kLeaderFacing = 0x802D48;      // u8: ObjTrio +8, the leader's facing
constexpr U kFacingJitter = 0x656A80;      // s8 x 4: the facing's jitter by Rand & 3 (image .data, after GameMode3_Steps)
constexpr U kPendingPlace = 0x937F82;      // u16: the trip's destination area
constexpr U kPendingX = 0x903860;          // dword: its x
constexpr U kPendingZ = 0x90384C;          // dword: its z
constexpr U kPendingFlags = 0x905B88;      // u8: its Field_ChangeArea flags
constexpr U kStoryFlags = 0x904030;        // the story flag row Flags_Clear is handed
constexpr unsigned kTripFlag = 0x77;       // the story flag cleared coming home
constexpr U kAreaAskedX = 0x430000;        // the x and z an area asked by number is entered at
constexpr U kAreaAskedZ = 0x160000;

// --- mode 4 ------------------------------------------------------------------------
constexpr U kMessageBits = 0x7DEE44;       // u8: bit 1 ends mode 4 (the message box's cells, scenario_harness.h)
constexpr U kCameraTurnByte = 0x905B82;    // u8: 0x10 when mode 4 ends (CameraTurn_Steps' cells)

// --- mode 5 (the battle) -----------------------------------------------------------
constexpr U kBattleFlags = 0x904AE5;       // u8: EventBattle_Records +0's flags (Field_StartEventBattle)
constexpr U kBattleFlags2 = 0x904AE8;      // u8: bit 3 keeps the field's music off after the battle
constexpr U kBattleFlags3 = 0x904AE9;      // u8: zeroed at steps 3 and 4
constexpr U kFormation = 0x904AAC;         // dword read, its low byte the formation (rest_3g_callees.h)
constexpr U kApproach = 0x656AA4;          // s8 pairs by the formation byte: x (+0) and z (+1) steps (image .data, after GameMode5_Steps)
constexpr U kElevationBase = 0x939860;     // dword: less MapView_Elevation, sar 4, into MoveScript_FAWord
constexpr U kKind2ZHigh = 0x905E62;        // u16: Field_Kind2Z's upper half
constexpr U kKind2XHigh = 0x905E66;        // u16: Field_Kind2X's upper half
constexpr U kEventBattle = 0x904AAA;       // u8: the event battle (EventBattle_Records' index)
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
