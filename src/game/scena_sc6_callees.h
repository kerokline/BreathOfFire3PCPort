// Internal to scena_sc6.cpp and scena_sc6_fuzz.cpp: the cells chapter 6's bank
// (0x54A910..0x54F080, round ten group SC6) reads and writes, its tables, and
// the raw addresses of the callees it reaches that nobody owns yet.
// docs/scena_sc6.md.
//
// Calls into code this group does not own, by raw address (SH_AT); the
// rebinding pass at the round's end turns them into names:
//   0x533E50  a pass over the 8 records at 0x903A70 (stride 0xA4) and the
//             party - engine, nobody's (round10 doc section 3)
//   0x532ED0  (x, z, kind): an event battle's party placement - engine,
//             nobody's
//   0x56D6F0  Field_StatusBits (0x8034E1) |= 0x80 - engine, nobody's
//   0x56D800  (records, count, a, b): the index of the first record whose
//             area and cell match, or 0xFF - engine, nobody's
//   0x591900  (id): the id into the first free byte of the 32 at 0x904554,
//             al 1; none free, al 0 - engine, nobody's (round10 doc section 3)
//   0x591BE0  (amount, flag): the party's zenny 0x904058 += amount (and
//             0x904138 when flag is 0), capped - engine, nobody's (battle
//             code names it Zenny_Add as a raw address)
//   0x587B80  (): a jmp to 0x5A6FF0, the sound layer - engine, nobody's
// Scenario_CallB 0x5341C0 (named by SCH, not taken), MoveCmd_OpDB 0x57CD40
// and Sound_ResumeAll 0x587B90 (named, Capcom's) are called by name; so are
// the functions already ours (Field_StartEventBattle 0x4410B0 and
// Party_AddToLists 0x591CC0, group SE's).
//
// Calls from chapters 7 and 8 (group SC7's band 0x54F080..0x553B30) into this
// band, and from this band into it: none (an E8 / E9 scan of the whole image
// and the absolute references of every start; docs/scena_sc6.md section 6).
// The one caller outside the band is area 77's handler 0x40F090 (group
// AR2A's, a later wave): E8 at 0x40F0E6 into Scena06_Leap.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2):
// every constant here whose target has a name in symbols.toml reads
// bof3::addr::<Name>. The values are unchanged - the fuzz keys on them.

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sc6 {

namespace at {

// The scenario bytes (docs/field-modes.md section 2).
// kStatusBits: now Field_StatusBits (symbols.toml). Field_StatusBits (bit 0 toggled, bit 0x80 by 0x56D6F0)
// kState: now MoveScript_PartyRecords[0x62] (symbols.toml). s8: the chapter's state, 0..2
constexpr std::uint32_t kRun = 0x8034E4;        // s8: MoveScript_Var7, which scene
constexpr std::uint32_t kStep = 0x8034E5;       // u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;      // u16: run 1's count-down
// kCameraDistance: now Camera_Distance (symbols.toml). u16 Camera_Distance
constexpr std::uint32_t kCounters = 0x903848;   // four script counters (MoveScript_CounterOps)
constexpr std::uint32_t kEffectSlot = 0x903850; // u8: the effect slot just taken
// kFlagBits: now Chapter_FlagRow (symbols.toml). unsigned char *: the bits Flags_Test / Set / Clear take
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0xA0, handed to the flag calls as a literal
// kSelector: now AreaW0b_PartySet (symbols.toml). Cond_Flags + 0x19C: & 0x7F picks the call-table entries
constexpr std::uint32_t kPartyBytes = 0x904062; // Cond_Flags + 0xD2: the three members' ids
constexpr std::uint32_t kPartyList2 = 0x904065; // Cond_Flags + 0xD5: the second list, three bytes
// kArea: now Game_AreaNumber (symbols.toml). u16 Game_AreaNumber
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
constexpr std::uint32_t kScriptFlags = 0x9039A2;   // u16 Field_ScriptFlags
// kRequest: now Field_Request (symbols.toml). Field_Request
// kWait: now Transition_Busy (symbols.toml). u16 Transition_Busy
// kRedraw: now MapView_Redraw (symbols.toml). MapView_Redraw
// kMusicByte: now AreaW1f_MusicCurrent (symbols.toml). u8: set 0xFF / 0x3C / 0x42 after an area change
// kByte904C9F: now ScenaSc6_Byte904C9F (symbols.toml). u8: 0 or 1 with the request 7
// kCondFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
constexpr std::uint32_t kEffects = 0x7E11E0;    // Effect_Objects: records of 0x80
constexpr std::uint32_t kEffectStride = 0x80;
// kAngle0: now Camera_Angles (symbols.toml). s16 Camera_Angles +0
// kAngle1: now Camera_Angles[1] (symbols.toml). s16 Camera_Angles +2
constexpr std::uint32_t kAngle2 = 0x929ECC;     // s16 Cond_AngleFB
// kMemberCount: now Field_MemberCount (symbols.toml). Field_MemberCount
// kHold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
// kLoad0F: now AreaW3c_Load0F (symbols.toml). u8: run 12 waits for it after Party_AddToLists
// kLoad10: now ScenaSc6_Load10 (symbols.toml). u8: run 12 sets 1 and waits for 0
// kKind2Z: now Field_Kind2Z (symbols.toml). s32 Field_Kind2Z
// kKind2X: now Field_Kind2X (symbols.toml). s32 Field_Kind2X
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). u16 MoveScript_F3Divisor
// kObjTrio: now ObjTrio (symbols.toml). the party's three field objects, 0x14C each
constexpr std::uint32_t kObjStride = 0x14C;
// kLead34: now ObjTrio record 0 x (records::PartyMember).
// kLead38: now ObjTrio record 0 z (records::PartyMember).
constexpr std::uint32_t kBank7DEE44 = 0x7DEE44; // u8: run 12 step 8 waits for its bit 1
constexpr std::uint32_t kCharRecords = 0x903A70;   // CharacterRecords: 8 of 0xA4
constexpr std::uint32_t kCharStride = 0xA4;
constexpr std::uint32_t kEffectState = 0x66972C;   // Character_IndexById: the member -> record byte
constexpr std::uint32_t kRecord7 = 0x903A70 + 7 * 0xA4;   // character record 7 (0x903EEC)
constexpr std::uint32_t kTextRecords = 0x904CE0;   // Text_Records: an item's name copied in, 16 bytes
constexpr std::uint32_t kTempList = 0x939A10;   // run 8 copies the second party list here
// kTempCount: now BossSe_PartyCopyCount (symbols.toml). u8: and the member count here
constexpr std::uint32_t kMoveSpeeds = 0x6697F0; // Field_MoveSpeeds: 6 bytes, indexed unchecked
constexpr std::uint32_t kSpriteCurrent = 0x937F88;   // unsigned char *Sprite_Current
constexpr std::uint32_t kMoveScriptObject = 0x929E80;   // the movement-script object area 77 hands Scena06_Leap

// Chapter 6's tables (symbols.toml [[data]]), each read in place by its
// dispatcher. Back to back from 0x6610F4; an index past one reads the next.
constexpr std::uint32_t kMemberBytes = 0x6610D8;   // Scena06_MemberBytes, 8 s8
// kStates: now Scena06_States (symbols.toml). Scena06_States, 3
constexpr unsigned kStateCount = 3;
// kRuns: now Scena06_Runs (symbols.toml). Scena06_Runs, 18
constexpr unsigned kRunCount = 18;
// kObjects: now Scena06_Objects (symbols.toml). Scena06_Objects, 14
constexpr unsigned kObjectCount = 14;
// kGuestStats: now Scena06_GuestStats (symbols.toml). Scena06_GuestStats, 8 bytes
// kLeapPhases: now Scena06_LeapPhases (symbols.toml). Scena06_LeapPhases, 3
constexpr unsigned kLeapPhaseCount = 3;
// kCells: now Scena06_Cells (symbols.toml). Scena06_Cells, 4 x 5 bytes
constexpr unsigned kCellCount = 4;
// kCellHandlers: now Scena06_CellHandlers (symbols.toml). Scena06_CellHandlers, 4
constexpr unsigned kCellHandlerCount = 4;
constexpr std::uint32_t kBareRet = bof3::addr::BareRet;    // a bare ret: Runs[0], Objects[0], CellHandlers[1]

// Callees nobody owns (above).
constexpr std::uint32_t kPartyPass = bof3::addr::Party_HealJoined;
constexpr std::uint32_t kPartyPlace = bof3::addr::Party_PlaceForBattle;
constexpr std::uint32_t kSetBit80 = bof3::addr::Field_SetStatus80;
constexpr std::uint32_t kCellFind = bof3::addr::Field_CellTriggerAt;
constexpr std::uint32_t kKeyItemPut = bof3::addr::KeyItem_Add;
constexpr std::uint32_t kZennyAdd = bof3::addr::Zenny_Add;
constexpr std::uint32_t kSoundJmp = bof3::addr::Sound_StopMusic;

}  // namespace at

using VoidFn = void (__cdecl*)();
using PlaceFn = void (__cdecl*)(int x, int z, unsigned kind);
using CellFindFn = unsigned char (__cdecl*)(const void* records, unsigned count, int a, int b);
using ByteFn = unsigned char (__cdecl*)(unsigned id);
using ZennyFn = void (__cdecl*)(unsigned amount, unsigned flag);
// A table entry as the dispatchers call it (docs/scena_sc6.md section 2).
using ObjectEntry = void (__cdecl*)(unsigned char* object, std::uint32_t bits);
using CellEntry = unsigned char (__cdecl*)(int x, int z);
using LeapEntry = unsigned char (__cdecl*)(unsigned char* object, unsigned dx, unsigned dz, unsigned lift,
                                           unsigned gravity, unsigned animation, unsigned flag);

}  // namespace scena_sc6
