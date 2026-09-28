// Internal to area_w2a.cpp and area_w2a_fuzz.cpp: the cells world 2's areas
// 76..84 touch that symbols.toml has no name for, the areas' own .data
// tables, and the two callees nobody owns yet (by their raw addresses, as
// round ten's rule for a function no group has taken: both are group SX2's
// this wave). Every other call is to a named function through the area
// harness (AH_CALL). docs/area_w2a.md.
#pragma once

#include <cstdint>

namespace area_w2a {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row), read as a signed
// byte (docs/item-use.md section 5), and the message word a choice handler
// leaves (0xFFFF: no new message).
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8, s8 0x9039F3), its state and a sub-kind;
// and the byte two before the mode phase 0x9039F2 that area 76's step hook
// clears with the kind (unnamed; no other reader read this round).
constexpr std::uint32_t kModeByte = 0x9039F1;
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
// The movement script's four counters (MoveScript_CounterOps, ops A0..AF).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter2 = 0x90384A;
constexpr std::uint32_t kCounter3 = 0x90384B;
// The byte and the word after MoveScript_Var7 (the run step and its word,
// which area 78's scene reset clears beside Var7).
constexpr std::uint32_t kVar7Step = 0x8034E5;
constexpr std::uint32_t kVar7Word = 0x8034E6;
// The story flags (Flags_Set / Flags_Test's bank), Cond_Flags row 6 (area
// 77's cell hook tests its flag 0x2C: 0x903F90 + 8 * 6), and the row pointer
// the chapters' flag calls take (Cond_Flags + 8 * chapter; docs/field-modes.md).
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kCondRow6 = 0x903FC0;
constexpr std::uint32_t kFlagRow = 0x929ED0;
// The three party lists' first bytes (0x904062..0x904064).
constexpr std::uint32_t kPartyList0 = 0x904062;
// The leader's record (ObjTrio): byte +8 (the pose the cell hook compares),
// byte +0x89 (area 84's handler tests it).
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kLeaderPose = 0x802D48;
constexpr std::uint32_t kLeaderByte89 = 0x802DC9;
constexpr std::uint32_t kPartyStride = 0x14C;
// Effect_Objects' stride (records of 0x80 bytes).
constexpr std::uint32_t kEffectStride = 0x80;
// Area_Descriptors (200 pointers): area 77's leap handler reads the running
// area's +0x10 script table through it.
constexpr std::uint32_t kDescriptors = 0x667590;
constexpr unsigned kDescriptorCount = 200;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;     // Effect_Objects
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;   // Sprite_ObjectsExtra
constexpr std::uint32_t kActiveMember = 0x9035A4;      // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;      // MoveScript_Object
constexpr std::uint32_t kCameraShiftY = 0x903802;      // Camera_ShiftY
constexpr std::uint32_t kByteFE = 0x905E20;            // Cond_ByteFE

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 77: the two cell switches of four bytes (x, z, pose, story flag), its
// choice 0's message words (six, by the answer), eight lists of effect kinds
// of 8 bytes by a party list byte (Effect_Spawn's third argument: list 0
// after area 76's descriptor, shared by three handlers; lists A..G one
// handler each), and the leap handler's (dx, dz) byte pairs by the script's
// operand byte.
constexpr std::uint32_t kArea77CellSwitches = 0x60BBCC;  // Area77_CellSwitches, 2 x 4 bytes
constexpr unsigned kArea77CellSwitchCount = 2;
constexpr std::uint32_t kArea77ChoiceMessages = 0x60BBD4;   // Area77_ChoiceMessages, 6 words
constexpr std::uint32_t kArea77EffectKinds0 = 0x60ACC0;  // Area77_EffectKinds0, 8
constexpr std::uint32_t kArea77EffectKindsA = 0x60BBE0;  // Area77_EffectKindsA..G, 8 each, consecutive
constexpr std::uint32_t kArea77EffectKindsB = 0x60BBE8;
constexpr std::uint32_t kArea77EffectKindsC = 0x60BBF0;
constexpr std::uint32_t kArea77EffectKindsD = 0x60BBF8;
constexpr std::uint32_t kArea77EffectKindsE = 0x60BC00;
constexpr std::uint32_t kArea77EffectKindsF = 0x60BC08;
constexpr std::uint32_t kArea77EffectKindsG = 0x60BC10;
constexpr std::uint32_t kArea77Leaps = 0x60BC18;         // Area77_Leaps, (dx, dz) byte pairs
// Area 78: its choice 7's message words (two, by the answer).
constexpr std::uint32_t kArea78ChoiceMessages = 0x60CF34;   // Area78_ChoiceMessages, 2 words
// Area 79: its choice 3's message words (six), the object state table its
// handler 0 dispatches through by Sprite_Current +4 (two entries: 0x40D380,
// another block's, and Area79_StateSlide), and the sixteen signed steps the
// slide takes by Sprite_Current +0xA & 0xF.
constexpr std::uint32_t kArea79ChoiceMessages = 0x60EC74;   // Area79_ChoiceMessages, 6 words
constexpr std::uint32_t kArea79States = 0x60EC80;        // Area79_States, 2
constexpr unsigned kArea79StateCount = 2;
constexpr std::uint32_t kArea79Steps = 0x60EC88;         // Area79_SlideSteps, 16 signed bytes
// Area 80: its three choices' message words (two each, consecutive).
constexpr std::uint32_t kArea80ChoiceMessages = 0x60F34C;   // Area80_ChoiceMessages, 3 x 2 words
// Area 81: its choice 1's message words (two).
constexpr std::uint32_t kArea81ChoiceMessages = 0x60FDD4;   // Area81_ChoiceMessages, 2 words

// --- the two callees nobody owns this wave (raw addresses; group SX2's) ---

// 0x57C160 (0x1F bytes): (bits, index) - bits[index >> 3] ^= 1 << (index & 7),
// the index a byte: Flags_Set / Flags_Clear's sibling that toggles
// (docs/area_w1c.md section 9). Engine.
constexpr std::uint32_t kFlagsToggle = 0x57C160;
// 0x469FE0 (0x3C bytes): (v) - Effect_FindFree; a free slot gets +0 = 1,
// kind +5 = 4, +9 = v, and story flag 0x1C is set (docs/area_w1c.md
// section 9). Engine.
constexpr std::uint32_t kSpawnKind4 = 0x469FE0;

}  // namespace at
}  // namespace area_w2a
