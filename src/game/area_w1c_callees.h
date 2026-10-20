// Internal to area_w1c.cpp and area_w1c_fuzz.cpp: the cells world 1's areas
// 48..52 touch that symbols.toml has no name for, the areas' own .data
// tables, and the four callees nobody owns yet (by their raw addresses, as
// round ten's rule for a function no group has taken). Every other call is to
// a named function through the area harness (AH_CALL). docs/area_w1c.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w1c {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row), read as a signed
// byte by some handlers and as a byte by others (docs/item-use.md section 5),
// and the message word a choice handler leaves (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind, and a word timer.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's four counters (MoveScript_CounterOps, ops A0..AF).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter2 = 0x90384A;
constexpr std::uint32_t kCounter3 = 0x90384B;
// The byte after MoveScript_Var7 (the run step the scenes set beside it).
// kVar7Step: now MoveScript_PartyRecords[0x65] (symbols.toml).
// The high byte of Field_ScriptFlags (0x9039A2): Area49_EffectFrame ors 0x20
// into it as a byte.
constexpr std::uint32_t kScriptFlagsHigh = 0x9039A3;
// The story flags (Flags_Set / Flags_Test's bank), two rows of Cond_Flags
// the areas test (rows 5, 9 and 14: 0x903F90 + 8 * row), and the row pointer
// the chapters' flag calls take (Cond_Flags + 8 * chapter; docs/field-modes.md).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kCondRow5: now Cond_Flags[0x28] (symbols.toml).
// kCondRow9: now Cond_Flags[0x48] (symbols.toml).
// kCondRow14: now Cond_Flags[0x70] (symbols.toml).
// kFlagRow: now Chapter_FlagRow (symbols.toml).
// The three party lists' first bytes (0x904062..0x904064).
// kPartyList0: now Cond_Flags[0xd2] (symbols.toml).
// A byte area 50's choice compares with 0x1E; no other reader read this round.
// kByte9045FB: now AreaW1c_Byte9045FB (symbols.toml).
// A byte area 50's choice sets to 6 beside the message it opens (area 37's
// choices do the same, docs/area_w0c.md); no reader read this round.
// kAnswerMark: now AreaW0c_AnswerMark (symbols.toml).
// The pending area word the world map and Field_ChangeArea keep.
// kPendingArea: now AreaW0b_Place (symbols.toml).
// The leader's record (ObjTrio): byte +8 (the pose the hooks compare), byte
// +0x89 (Character_IndexById's index, op 8C).
constexpr std::uint32_t kLeader = 0x802D40;
// kLeaderPose: now ObjTrio[0].direction (symbols.toml).
// kLeaderByte89: now ObjTrio[0].char_id (symbols.toml).
constexpr std::uint32_t kPartyStride = 0x14C;
// Area 48's own cells: a count 0..8 its handlers step and read
// (0x92BEE7; MapView_ElevationOffset is 0x92BEE2, five bytes before), and a
// byte whose bit 0 its handler 5 sets (0x803490). Unnamed: no other reader
// read this round.
// kCount48: now AreaW1c_Count48 (symbols.toml).
constexpr std::uint32_t kByte803490 = 0x803490;
// Effect_Objects' stride (records of 0x80 bytes).
constexpr std::uint32_t kEffectStride = 0x80;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;     // Sprite_Objects
// kSpriteObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kCameraShiftY: now Camera_ShiftY (symbols.toml). Camera_ShiftY
// kEffectState: now Character_IndexById (symbols.toml). Character_IndexById (24 bytes)

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 49: the effect kinds by party member (Effect_Spawn's third argument),
// the seven zones of six bytes (x0, z0, x1, z1, direction, story flag) its
// effect frame tests each member against, the two choices' message words
// (two each, by the answer), the nine cell switches of four bytes (x, z,
// pose, story flag).
// kArea49EffectKinds: now Area49_EffectKinds (symbols.toml). Area49_EffectKinds, 8
// kArea49Zones: now Area49_Zones (symbols.toml). Area49_Zones, 7 x 6 bytes
constexpr unsigned kArea49ZoneCount = 7;
// kArea49ChoiceMessages: now Area49_ChoiceMessages (symbols.toml). Area49_ChoiceMessages, 4 words
// kArea49CellSwitches: now Area49_CellSwitches (symbols.toml). Area49_CellSwitches, 9 x 4 bytes
constexpr unsigned kArea49CellSwitchCount = 9;
// Area 52: its descriptor's +0x08 extra-object entries (two of 8 bytes; the
// tail writes entry 0's byte +1), the two rectangles of four bytes (x0, z0,
// width, depth) its cell hook tests the party against, the four cell
// switches of five bytes (x, z, pose in the low nibble, story flag, tail
// state).
// kArea52ExtraObjects: now Area52_ExtraObjects (symbols.toml). Area52_ExtraObjects, 16 bytes
// kArea52Rects: now Area52_Rects (symbols.toml). Area52_Rects, 2 x 4 bytes
// kArea52CellSwitches: now Area52_CellSwitches (symbols.toml). Area52_CellSwitches, 4 x 5 bytes
constexpr unsigned kArea52CellSwitchCount = 4;

// --- the four callees nobody owns this wave (raw addresses) ---

// 0x57C160 (0x1F bytes): (bits, index) - bits[index >> 3] ^= 1 << (index & 7),
// the index a byte: Flags_Set / Flags_Clear's sibling that toggles. Engine.
constexpr std::uint32_t kFlagsToggle = bof3::addr::Flags_Toggle;
// 0x572620 (0x2F bytes): (x, z, value) - AreaMap_Header[HeightBase * 4 +
// width * z + x] = value, x and z sign-extended words, value a byte: the
// area block's second byte layer, as AreaMap_SetByte writes the first. Engine.
constexpr std::uint32_t kSetLayerByte = bof3::addr::AreaMap_SetHeight;
// 0x57C8A0 (0x35 bytes): (member, v) - party record member's +1 = 2, +2 = 8,
// +3 = 0, +0xB = v; both arguments bytes. Engine.
constexpr std::uint32_t kMemberSetState = bof3::addr::Member_SetState2_8;
// 0x469FE0 (0x3C bytes): (v) - Effect_FindFree; a free slot gets +0 = 1,
// kind +5 = 4, +9 = v, and story flag 0x1C is set. Engine.
constexpr std::uint32_t kSpawnKind4 = bof3::addr::Effect_HoldFlag1C;

}  // namespace at
}  // namespace area_w1c
