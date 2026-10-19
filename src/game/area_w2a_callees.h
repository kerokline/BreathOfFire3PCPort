// Internal to area_w2a.cpp and area_w2a_fuzz.cpp: the cells world 2's areas
// 76..84 touch that symbols.toml has no name for, the areas' own .data
// tables, and the two callees nobody owns yet (by their raw addresses, as
// round ten's rule for a function no group has taken: both are group SX2's
// this wave). Every other call is to a named function through the area
// harness (AH_CALL). docs/area_w2a.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w2a {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row), read as a signed
// byte (docs/item-use.md section 5), and the message word a choice handler
// leaves (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8, s8 0x9039F3), its state and a sub-kind;
// and the byte two before the mode phase 0x9039F2 that area 76's step hook
// clears with the kind (unnamed; no other reader read this round).
// kModeByte: now AreaW2a_ModeByte (symbols.toml).
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
// The movement script's four counters (MoveScript_CounterOps, ops A0..AF).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter2 = 0x90384A;
constexpr std::uint32_t kCounter3 = 0x90384B;
// The byte and the word after MoveScript_Var7 (the run step and its word,
// which area 78's scene reset clears beside Var7).
// kVar7Step: now MoveScript_PartyRecords[0x65] (symbols.toml).
// kVar7Word: now MoveScript_PartyRecords[0x66] (symbols.toml).
// The story flags (Flags_Set / Flags_Test's bank), Cond_Flags row 6 (area
// 77's cell hook tests its flag 0x2C: 0x903F90 + 8 * 6), and the row pointer
// the chapters' flag calls take (Cond_Flags + 8 * chapter; docs/field-modes.md).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kCondRow6: now Cond_Flags[0x30] (symbols.toml).
// kFlagRow: now AreaW0c_FlagRow (symbols.toml).
// The three party lists' first bytes (0x904062..0x904064).
// kPartyList0: now Cond_Flags[0xd2] (symbols.toml).
// The leader's record (ObjTrio): byte +8 (the pose the cell hook compares),
// byte +0x89 (area 84's handler tests it).
constexpr std::uint32_t kLeader = 0x802D40;
// kLeaderPose: now ObjTrio[8] (symbols.toml).
// kLeaderByte89: now ObjTrio[0x89] (symbols.toml).
constexpr std::uint32_t kPartyStride = 0x14C;
// Effect_Objects' stride (records of 0x80 bytes).
constexpr std::uint32_t kEffectStride = 0x80;
// Area_Descriptors (200 pointers): area 77's leap handler reads the running
// area's +0x10 script table through it.
// kDescriptors: now Area_Descriptors (symbols.toml).
constexpr unsigned kDescriptorCount = 200;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
// kSpriteObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kCameraShiftY: now Camera_ShiftY (symbols.toml). Camera_ShiftY
// kByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 77: the two cell switches of four bytes (x, z, pose, story flag), its
// choice 0's message words (six, by the answer), eight lists of effect kinds
// of 8 bytes by a party list byte (Effect_Spawn's third argument: list 0
// after area 76's descriptor, shared by three handlers; lists A..G one
// handler each), and the leap handler's (dx, dz) byte pairs by the script's
// operand byte.
// kArea77CellSwitches: now Area77_CellSwitches (symbols.toml). Area77_CellSwitches, 2 x 4 bytes
constexpr unsigned kArea77CellSwitchCount = 2;
// kArea77ChoiceMessages: now Area77_ChoiceMessages (symbols.toml). Area77_ChoiceMessages, 6 words
// kArea77EffectKinds0: now Area77_EffectKinds0 (symbols.toml). Area77_EffectKinds0, 8
// kArea77EffectKindsA: now Area77_EffectKindsA (symbols.toml). Area77_EffectKindsA..G, 8 each, consecutive
// kArea77EffectKindsB: now Area77_EffectKindsA[8] (symbols.toml).
// kArea77EffectKindsC: now Area77_EffectKindsA[0x10] (symbols.toml).
// kArea77EffectKindsD: now Area77_EffectKindsA[0x18] (symbols.toml).
// kArea77EffectKindsE: now Area77_EffectKindsA[0x20] (symbols.toml).
// kArea77EffectKindsF: now Area77_EffectKindsA[0x28] (symbols.toml).
// kArea77EffectKindsG: now Area77_EffectKindsA[0x30] (symbols.toml).
// kArea77Leaps: now Area77_Leaps (symbols.toml). Area77_Leaps, (dx, dz) byte pairs
// Area 78: its choice 7's message words (two, by the answer).
// kArea78ChoiceMessages: now Area78_ChoiceMessages (symbols.toml). Area78_ChoiceMessages, 2 words
// Area 79: its choice 3's message words (six), the object state table its
// handler 0 dispatches through by Sprite_Current +4 (two entries: 0x40D380,
// another block's, and Area79_StateSlide), and the sixteen signed steps the
// slide takes by Sprite_Current +0xA & 0xF.
// kArea79ChoiceMessages: now Area79_ChoiceMessages (symbols.toml). Area79_ChoiceMessages, 6 words
// kArea79States: now Area79_States (symbols.toml). Area79_States, 2
constexpr unsigned kArea79StateCount = 2;
// kArea79Steps: now Area79_SlideSteps (symbols.toml). Area79_SlideSteps, 16 signed bytes
// Area 80: its three choices' message words (two each, consecutive).
// kArea80ChoiceMessages: now Area80_ChoiceMessages (symbols.toml). Area80_ChoiceMessages, 3 x 2 words
// Area 81: its choice 1's message words (two).
// kArea81ChoiceMessages: now Area81_ChoiceMessages (symbols.toml). Area81_ChoiceMessages, 2 words

// --- the two callees nobody owns this wave (raw addresses; group SX2's) ---

// 0x57C160 (0x1F bytes): (bits, index) - bits[index >> 3] ^= 1 << (index & 7),
// the index a byte: Flags_Set / Flags_Clear's sibling that toggles
// (docs/area_w1c.md section 9). Engine.
constexpr std::uint32_t kFlagsToggle = bof3::addr::Flags_Toggle;
// 0x469FE0 (0x3C bytes): (v) - Effect_FindFree; a free slot gets +0 = 1,
// kind +5 = 4, +9 = v, and story flag 0x1C is set (docs/area_w1c.md
// section 9). Engine.
constexpr std::uint32_t kSpawnKind4 = bof3::addr::Effect_HoldFlag1C;

}  // namespace at
}  // namespace area_w2a
