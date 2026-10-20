// Internal to area_w2f.cpp and area_w2f_fuzz.cpp: the cells world 2's areas
// 108 and 110..113 touch that symbols.toml has no name for, the areas' own
// .data tables, and the one callee nobody owns yet this wave (by its raw
// address, as round ten's rule for a function no group has taken). Every
// other call is to a named function through the area harness (AH_CALL).
// docs/area_w2f.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w2f {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (read signed by areas 108 and 112's
// choices) and the message word a choice handler leaves (0xFFFF: no new
// message).
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
constexpr std::uint32_t kCounter3 = 0x90384B;
// The byte after MoveScript_Var7: the step of a scene run (field_modes.cpp).
// kVar7Step: now MoveScript_PartyRecords[0x65] (symbols.toml).
// The story flags (Flags_Set / Flags_Test's bank) and Cond_Flags row 13
// (0x903F90 + 8 * 13), whose flag 0x10 area 112's choice and hook test.
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kCondRow13: now Cond_Flags[0x68] (symbols.toml).
// A byte area 113's choice 0 compares with 6 (the first byte after the
// inventory block 0x904098..0x9045F4, docs/scena_sx.md; no reader named).
// kByte9045F4: now FieldE1_Rank (symbols.toml).
// A byte areas 37, 50, 98 and 113's choices set to 6 beside the message they
// open; no reader read this round.
// kAnswerMark: now AreaW0c_AnswerMark (symbols.toml).
// The focus object: the object whose talk ran last (area_w1a_callees.h's
// kFocusObject); area 113's tail writes its word +0x88.
constexpr std::uint32_t kFocusObject = 0x903804;
// MoveScript_PartyRecords record 1's first byte: area 108's place scene sets
// its bit 0 (area_w1c_callees.h's kByte803490).
// kPartyRecords: now MoveScript_PartyRecords (symbols.toml).
constexpr std::uint32_t kByte803490 = 0x803490;
// The leader's record (ObjTrio record 0): its pose byte +8, the member id
// +0x89, x +0x34 (high word +0x36), z +0x38 (high word +0x3A).
// kLeader: now ObjTrio (symbols.toml).
// kLeaderPose: now ObjTrio[0].direction (symbols.toml).
// kLeaderMember: now ObjTrio[0].char_id (symbols.toml).
// kLeaderX: now ObjTrio[0].x (symbols.toml).
// kLeaderZ: now ObjTrio[0].z (symbols.toml).
// kLeaderXHigh: now ObjTrio[0].x_whole (symbols.toml).
// kLeaderZHigh: now ObjTrio[0].z_whole (symbols.toml).
constexpr std::uint32_t kPartyStride = 0x14C;
// The leader's zone counter (record 0 +0x134; area_w1f_callees.h's
// kLeaderZone): area 110's init sets Field_EdgeBits from it.
constexpr std::uint32_t kLeaderZone = 0x802E74;
// Sprite_ObjectsExtra's records (0xA4 bytes, four of them).
constexpr std::uint32_t kExtraStride = 0xA4;
constexpr unsigned kExtraCount = 4;
// Field_MoveSpeeds[3] and [4] (bytes): the kind-2 moves' divisors, << 3.
// kMoveSpeed3: now Field_MoveSpeeds[3] (symbols.toml).
// kMoveSpeed4: now Field_MoveSpeeds[4] (symbols.toml).
// Signed cell steps (x, z) per direction, two bytes apart
// (event_ops_callees.h's kCellDelta).
constexpr std::uint32_t kCellDelta = 0x66971C;
// Effect_Objects' stride and count (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;       // Sprite_Objects
// kSpriteObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
// kSpriteKind2: now Sprite_Kind2 (symbols.toml). Sprite_Kind2 (0xA4 bytes)
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kKind2Hold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold

// --- the areas' tables and cells (symbols.toml [[data]]) ---

// Area 108: the descriptor's +0x08 record (0x61E508: a signed count, two
// bytes, a flags byte at +3, a pointer to count records of 0x28 bytes: four
// s16 corners (x, y, z) and a velocity (x, y, z) each); the tail's scene
// moves them.
// kArea108Model: now Area108_Model (symbols.toml). Area108_Model, 8 bytes
// kArea108ModelFlags: now Area108_Model[3] (symbols.toml).
// kArea108ModelPoints: now Area108_Model[4] (symbols.toml).
constexpr std::uint32_t kArea108ModelStride = 0x28;
// Area 108: the five places its handler 4 tests (x high word, z high word,
// the leader's direction: bytes, three a row), the two place tables handlers
// 5 and 6 read by extra object 1's +0x83 - 1 (x byte, x half flag, z byte, z
// half flag), the two fade states handler 9 jumps through, the four cells
// its cell hook tests (x byte, z byte, direction, flag), and the four cells
// its helper sets (x, z words).
// kArea108Places: now Area108_Places (symbols.toml). Area108_Places, 5 x 3
constexpr unsigned kArea108PlaceCount = 5;
// kArea108PlaceCellsA: now Area108_PlaceCellsA (symbols.toml). Area108_PlaceCellsA, 9 x 4
// kArea108PlaceCellsB: now Area108_PlaceCellsB (symbols.toml). Area108_PlaceCellsB, 9 x 4
// kArea108FadeStates: now Area108_FadeStates (symbols.toml). Area108_FadeStates, 2
constexpr unsigned kArea108FadeStateCount = 2;
// kArea108HookCells: now Area108_HookCells (symbols.toml). Area108_HookCells, 4 x 4
constexpr unsigned kArea108HookCellCount = 4;
// kArea108SetCells: now Area108_CellWords (symbols.toml). Area108_SetCells, 4 x (x, z) words
constexpr unsigned kArea108SetCellCount = 4;
// Area 110: its init's eight cells (x, z bytes) and eight weights.
// kArea110Cells: now Area110_Cells (symbols.toml). Area110_Cells, 8 x 2
// kArea110Weights: now Area110_Weights (symbols.toml). Area110_Weights, 8
// Area 111: the 7 x 7 grid of nibbles (4 bytes a row; an odd column the low
// nibble) at the end of the image's initialized .data, its init's copy of it
// (28 bytes) and the 7 x 7 map bytes it stamps.
// kArea111Grid: now Area111_Grid (symbols.toml). Area111_Grid, 28 bytes
constexpr unsigned kArea111GridBytes = 0x1C;
constexpr unsigned kArea111GridSide = 7;
// kArea111GridStart: now Area111_GridStart (symbols.toml). Area111_GridStart, 28 bytes
constexpr std::uint32_t kArea111CellBytes = 0x61FB7C;   // Area111_CellBytes, 49
// Area 112: the four member ids its handler 2 looks for and the four message
// words by them, the four cells its cell hook tests (x, z, flag bits, flag),
// the four flags those bits set or clear, the four boxes its helper tests
// (centre x, centre z, half widths), and the effect's two states.
// kArea112MemberKeys: now Area112_MemberKeys (symbols.toml). Area112_MemberKeys, 4
// kArea112Messages: now Area112_Messages (symbols.toml). Area112_Messages, 4 words
constexpr unsigned kArea112KeyCount = 4;
// kArea112HookCells: now Area112_HookCells (symbols.toml). Area112_HookCells, 4 x 4
constexpr unsigned kArea112HookCellCount = 4;
// kArea112HookFlags: now Area112_HookFlags (symbols.toml). Area112_HookFlags, 4
// kArea112Boxes: now Area112_Boxes (symbols.toml). Area112_Boxes, 4 x 4
constexpr unsigned kArea112BoxCount = 4;
// kArea112EffectStates: now Area112_EffectStates (symbols.toml). Area112_EffectStates, 2
constexpr unsigned kArea112EffectStateCount = 2;

// The image's .data (initialized and not): a read inside it cannot fault.
constexpr std::uint32_t kDataStart = 0x5DA000;
constexpr std::uint32_t kDataEnd = 0x93D6EC;

// --- the callee nobody owns this wave (raw address) ---

// 0x4220D0 (0x27A bytes): (const long* point) - reads the point's three
// dwords (x, z, y) and builds positions about it with Math_Cos / Math_Sin
// and the frame counter (docs/area_w2d.md section 9). World 3's area code
// (group AR3F's band this wave), called by the effect states of areas 36,
// 59, 100, 112, 116 and 146.
constexpr std::uint32_t kRingAt = bof3::addr::Area146_DrawGlowCylinder;

}  // namespace at
}  // namespace area_w2f
