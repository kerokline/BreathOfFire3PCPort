// Internal to area_w1d.cpp and area_w1d_fuzz.cpp: the cells world 1's areas
// 53..64 touch that symbols.toml has no name for, the areas' own .data
// tables, and the one callee nobody owns yet (by its raw address, as round
// ten's rule for a function no group has taken). Every other call is to a
// named function through the area harness (AH_CALL). docs/area_w1d.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w1d {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row), read as a signed
// byte by the handlers that index a table with it and as a byte by the rest
// (docs/item-use.md section 5), and the message word a choice handler leaves
// (0xFFFF: no new message).
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// A byte several areas' choices set to 6 beside the message they open (areas
// 37 and 50 do the same, docs/area_w0c.md, docs/area_w1c.md); no reader read.
constexpr std::uint32_t kAnswerMark = 0x9398CF;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state and a sub-kind.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
// The movement script's counters 0 and 3 (MoveScript_CounterOps, ops A0..AF).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
// A pointer to a field object (area_w1a's "focus object", object_kinds.cpp's
// "talker": the active member of the last talk); area 53's choice writes two
// dwords through it, area 59's trigger 24 three fields and divides it into
// an object index.
constexpr std::uint32_t kFocusObject = 0x903804;
// The high byte of Field_ScriptFlags (0x9039A2): area 60's init ors 0x20 in.
constexpr std::uint32_t kScriptFlagsHigh = 0x9039A3;
// The story flags (Flags_Set / Flags_Test's bank) and Cond_Flags row 3
// (0x903F90 + 8 * 3), whose flags 8..0xE area 56's choices set.
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kCondRow3 = 0x903FA8;
// The first party list's first byte (0x904062): area 57's effect kind index.
constexpr std::uint32_t kPartyList0 = 0x904062;
// Party_Zenny (a u32 symbols.toml names without a type): area 61's choice
// zeroes it.
constexpr std::uint32_t kZenny = 0x904058;
// Text_Records (typeless in symbols.toml): area 62's tail copies an item's
// 16-byte name record into its first 16 bytes.
constexpr std::uint32_t kTextRecords = 0x904CE0;
// The leader's record (ObjTrio record 0) and the fields the areas read or
// write in it directly: +1..+3 (the state bytes area 56 sets before
// MoveCmd_TestFB), +8 (the pose the step hook compares), +0x2E / +0x30 (the
// cell words Effect_Spawn takes), +0x36 / +0x3A (the high words of x and z),
// +0x89 (MoveScript_EffectState's index), the dword +0x134 (areas 63 / 64
// store it less 5 to Field_EdgeBits).
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kLeaderPose = 0x802D48;
constexpr std::uint32_t kLeaderByte89 = 0x802DC9;
constexpr std::uint32_t kLeaderDword134 = 0x802E74;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;   // Effect_Objects, records of 0x80
constexpr std::uint32_t kEffectStride = 0x80;
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;   // Sprite_Objects, records of 0xA4
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;   // Sprite_ObjectsExtra
constexpr std::uint32_t kActiveMember = 0x9035A4;    // Field_ActiveMember

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 53: byte pairs by the s8 answer, to the focus object's dwords +0x18
// and +0x1C (Area53_ChoicePairs).
constexpr std::uint32_t kArea53ChoicePairs = 0x5FEADC;
// Area 56: its handler 0's three fall states by the object's +4
// (Area56_FallStates); the next dword is data.
constexpr std::uint32_t kArea56FallStates = 0x600BE4;
constexpr unsigned kArea56FallStateCount = 3;
// Area 57: the choice's message words by the s8 answer
// (Area57_ChoiceMessages), and the effect kinds by the first party list byte
// for its two spawns (Area57_EffectKinds2 / 4, Effect_Spawn kinds 2 and 4).
constexpr std::uint32_t kArea57ChoiceMessages = 0x60296C;
constexpr std::uint32_t kArea57EffectKinds2 = 0x602978;
constexpr std::uint32_t kArea57EffectKinds4 = 0x602980;
// Area 59: its effect's two states by +1 (Area59_EffectStates: 0x40B4F0,
// shared by seven areas' tables, then Area59_EffectStep); the next dword is 0.
constexpr std::uint32_t kArea59EffectStates = 0x603CDC;
constexpr unsigned kArea59EffectStateCount = 2;
// Areas 63 and 64: eight (x, z) cell byte pairs, then eight weights out of
// 64 (the PSX copy's bytes in the same order), for the placement inits.
constexpr std::uint32_t kArea63Positions = 0x604298;
constexpr std::uint32_t kArea63Weights = 0x6042A8;
constexpr std::uint32_t kArea64Positions = 0x604368;
constexpr std::uint32_t kArea64Weights = 0x604378;

// --- the callee nobody owns this wave (raw address) ---

// 0x4220D0 (0x27A bytes, area band, group AR3F's block; area_w0c's
// kPositionHook): takes a pointer to three dwords (x, z, y) - area 59's
// effect step hands it the object's position on the stack.
constexpr std::uint32_t kPositionHook = bof3::addr::Area146_DrawGlowCylinder;

}  // namespace at
}  // namespace area_w1d
