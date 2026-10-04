// Group R1A's addresses (round fourteen, wave one): the .data state tables its
// dispatchers jump through and the cells its functions read by address. Every
// callee of the group is ours already (R0A's helpers among them, called by
// name through game/rest_0a.h) or Capcom's named Rand, so there is no raw
// callee here: these are data. docs/rest_1a.md.
//
// Each table is a run of code pointers read in place, the index unchecked
// (`jmp [table + index * 4]`); its count is the run up to the next table's
// start, read by hand (docs/rest_1a.md section 2). The tables lie back to back
// from 0x65F9A4 to 0x65FAC8, so an index past one reaches the next.
#pragma once

#include <cstdint>

namespace rest_1a::at {

// Member_States 0x65F960's entry 8 (Member_JumpState) jumps through this by +2:
// Field_JumpBegin, Field_JumpOut, Member_JumpAir, Field_JumpIn.
constexpr std::uint32_t kMemberJumpSteps = 0x65F9A4;            // 4
// Party set 0 (Field_FormActions[0], Field_ActionBySet[0]).
constexpr std::uint32_t kFormAction0Form0States = 0x65F9B4;     // 3, by +2
constexpr std::uint32_t kAction0Form0States = 0x65F9C0;         // 3, by +2 (PartyAction0_Form0States)
constexpr std::uint32_t kFormAction0Form1States = 0x65F9CC;     // 3, by +2
constexpr std::uint32_t kAction0Form1States = 0x65F9D8;         // 2, by +2
constexpr std::uint32_t kFormAction0Form2States = 0x65F9E0;     // 3, by +2
constexpr std::uint32_t kAction0Form2States = 0x65F9EC;         // 2, by +2
constexpr std::uint32_t kFormAction0Forms = 0x65F9F4;           // 3, by u16 +0x2C
constexpr std::uint32_t kAction0Forms = 0x65FA00;               // 3, by u16 +0x2C
// Party set 1.
constexpr std::uint32_t kFormAction1Form0States = 0x65FA0C;     // 3
constexpr std::uint32_t kAction1Form0States = 0x65FA18;         // 3 (PartyAction1_Form0States)
constexpr std::uint32_t kFormAction1Form1States = 0x65FA24;     // 3
constexpr std::uint32_t kAction1Form1States = 0x65FA30;         // 2
constexpr std::uint32_t kFormAction1Form2States = 0x65FA38;     // 3
constexpr std::uint32_t kAction1Form2States = 0x65FA44;         // 3
constexpr std::uint32_t kFormAction1Forms = 0x65FA50;           // 3, by u16 +0x2C
constexpr std::uint32_t kAction1Forms = 0x65FA5C;               // 3, by u16 +0x2C
// Party set 2 (its two by-form dispatchers, 0x51D710 and 0x51D730, are R1B's).
constexpr std::uint32_t kFormAction2Form0States = 0x65FA68;     // 3
constexpr std::uint32_t kAction2Form0States = 0x65FA74;         // 3 (PartyAction2_Form0States)
constexpr std::uint32_t kFormAction2Form1States = 0x65FA80;     // 3
constexpr std::uint32_t kAction2Form1States = 0x65FA8C;         // 2
constexpr std::uint32_t kFormAction2Form2States = 0x65FA94;     // 3
constexpr std::uint32_t kAction2Form2States = 0x65FAA0;         // 2
constexpr std::uint32_t kAction2Form2State0Steps = 0x65FAA8;    // 5, by +3
constexpr std::uint32_t kAction2Form2State1Steps = 0x65FABC;    // 3, by +3
// Field_FormActions: 19 entries by the party set 0x90412C & 0x7F (Member_FormActionState's call).
constexpr std::uint32_t kFormActions = 0x660A44;
constexpr unsigned kFormActionCount = 19;

// Cells read by address.
constexpr std::uint32_t kPartySet = 0x90412C;        // the loaded party set (& 0x7F)
constexpr std::uint32_t kLeaderKind = 0x802DC9;      // ObjTrio +0x89 (the leader's)
constexpr std::uint32_t kLeaderState = 0x802D41;     // ObjTrio +1
constexpr std::uint32_t kLeaderStep = 0x802D42;      // ObjTrio +2
constexpr std::uint32_t kCondRow1 = 0x903F98;        // Cond_Flags + 8: the flag row Flags_Test reads (bit 0x16)
constexpr std::uint32_t kScriptFlags3 = 0x9039A3;    // Field_ScriptFlags + 3
constexpr std::uint32_t kScratchFlag = 0x903850;     // DamageScratch's flag byte (MapView_SlopeAt's)
constexpr std::uint32_t kSteps = 0x6697B0;           // Field_DirectionSteps, 8 rows of two longs
constexpr std::uint32_t kSideStep3 = 0x6697C8;       // its row 3, read by address
constexpr std::uint32_t kSideStep5 = 0x6697D8;       // its row 5
constexpr std::uint32_t kObjectFlags = 0x7DEF00;     // Sprite_Objects + 0x80 (30 of 0xA4)
constexpr std::uint32_t kExtraFlags = 0x802080;      // Sprite_ObjectsExtra + 0x80 (4 of 0xA4)
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr std::uint32_t kPalettes = 0x80D380;        // Sprite_LoadPalette's destination: + Sprite_Current +5 * 0x40
constexpr std::uint32_t kMemberStride = 0x14C;       // ObjTrio's records

}  // namespace rest_1a::at
