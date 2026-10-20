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
// kMemberJumpSteps: now Member_JumpSteps (symbols.toml). 4
// Party set 0 (Field_FormActions[0], Field_ActionBySet[0]).
// kFormAction0Form0States: now PartyFormAction0_Form0States (symbols.toml). 3, by +2
// kAction0Form0States: now PartyAction0_Form0States (symbols.toml). 3, by +2 (PartyAction0_Form0States)
// kFormAction0Form1States: now PartyFormAction0_Form1States (symbols.toml). 3, by +2
// kAction0Form1States: now PartyAction0_Form1States (symbols.toml). 2, by +2
// kFormAction0Form2States: now PartyFormAction0_Form2States (symbols.toml). 3, by +2
// kAction0Form2States: now PartyAction0_Form2States (symbols.toml). 2, by +2
// kFormAction0Forms: now PartyFormAction0_Forms (symbols.toml). 3, by u16 +0x2C
// kAction0Forms: now PartyAction0_Forms (symbols.toml). 3, by u16 +0x2C
// Party set 1.
// kFormAction1Form0States: now PartyFormAction1_Form0States (symbols.toml). 3
// kAction1Form0States: now PartyAction1_Form0States (symbols.toml). 3 (PartyAction1_Form0States)
// kFormAction1Form1States: now PartyFormAction1_Form1States (symbols.toml). 3
// kAction1Form1States: now PartyAction1_Form1States (symbols.toml). 2
// kFormAction1Form2States: now PartyFormAction1_Form2States (symbols.toml). 3
// kAction1Form2States: now PartyAction1_Form2States (symbols.toml). 3
// kFormAction1Forms: now PartyFormAction1_Forms (symbols.toml). 3, by u16 +0x2C
// kAction1Forms: now PartyAction1_Forms (symbols.toml). 3, by u16 +0x2C
// Party set 2 (its two by-form dispatchers, 0x51D710 and 0x51D730, are R1B's).
// kFormAction2Form0States: now PartyFormAction2_Form0States (symbols.toml). 3
// kAction2Form0States: now PartyAction2_Form0States (symbols.toml). 3 (PartyAction2_Form0States)
// kFormAction2Form1States: now PartyFormAction2_Form1States (symbols.toml). 3
// kAction2Form1States: now PartyAction2_Form1States (symbols.toml). 2
// kFormAction2Form2States: now PartyFormAction2_Form2States (symbols.toml). 3
// kAction2Form2States: now PartyAction2_Form2States (symbols.toml). 2
// kAction2Form2State0Steps: now PartyAction2_Form2State0Steps (symbols.toml). 5, by +3
// kAction2Form2State1Steps: now PartyAction2_Form2State1Steps (symbols.toml). 3, by +3
// Field_FormActions: 19 entries by the party set 0x90412C & 0x7F (Member_FormActionState's call).
// kFormActions: now Field_FormActions (symbols.toml).
constexpr unsigned kFormActionCount = 19;

// Cells read by address.
// kPartySet: now AreaW0b_PartySet (symbols.toml). the loaded party set (& 0x7F)
// kLeaderKind: now ObjTrio[0x89] (symbols.toml). ObjTrio +0x89 (the leader's)
// kLeaderState: now ObjTrio[1] (symbols.toml). ObjTrio +1
// kLeaderStep: now ObjTrio[2] (symbols.toml). ObjTrio +2
// kCondRow1: now Cond_Flags[8] (symbols.toml). Cond_Flags + 8: the flag row Flags_Test reads (bit 0x16)
constexpr std::uint32_t kScriptFlags3 = 0x9039A3;    // Field_ScriptFlags + 1: the u16's high byte
// kScratchFlag: now SharedScratch (symbols.toml). SharedScratch's flag byte (MapView_SlopeAt's)
constexpr std::uint32_t kSteps = 0x6697B0;           // Field_DirectionSteps, 8 rows of two longs
// kSideStep3: now Field_DirectionSteps[6] (symbols.toml). its row 3, read by address
// kSideStep5: now Field_DirectionSteps[0xa] (symbols.toml). its row 5
constexpr std::uint32_t kObjectFlags = 0x7DEF00;     // Sprite_Objects + 0x80 (30 of 0xA4)
constexpr std::uint32_t kExtraFlags = 0x802080;      // Sprite_ObjectsExtra + 0x80 (4 of 0xA4)
constexpr std::uint32_t kObjectStride = 0xA4;
// kPalettes: now Gfx_ClutStripSource[0xf00] (symbols.toml). Sprite_LoadPalette's destination: + Sprite_Current +5 * 0x40
constexpr std::uint32_t kMemberStride = 0x14C;       // ObjTrio's records

}  // namespace rest_1a::at
