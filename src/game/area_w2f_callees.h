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
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind, and a word timer.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's four counters (MoveScript_CounterOps, ops A0..AF).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter3 = 0x90384B;
// The byte after MoveScript_Var7: the step of a scene run (field_modes.cpp).
constexpr std::uint32_t kVar7Step = 0x8034E5;
// The story flags (Flags_Set / Flags_Test's bank) and Cond_Flags row 13
// (0x903F90 + 8 * 13), whose flag 0x10 area 112's choice and hook test.
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kCondRow13 = 0x903FF8;
// A byte area 113's choice 0 compares with 6 (the first byte after the
// inventory block 0x904098..0x9045F4, docs/scena_sx.md; no reader named).
constexpr std::uint32_t kByte9045F4 = 0x9045F4;
// A byte areas 37, 50, 98 and 113's choices set to 6 beside the message they
// open; no reader read this round.
constexpr std::uint32_t kAnswerMark = 0x9398CF;
// The focus object: the object whose talk ran last (area_w1a_callees.h's
// kFocusObject); area 113's tail writes its word +0x88.
constexpr std::uint32_t kFocusObject = 0x903804;
// MoveScript_PartyRecords record 1's first byte: area 108's place scene sets
// its bit 0 (area_w1c_callees.h's kByte803490).
constexpr std::uint32_t kPartyRecords = 0x803480;
constexpr std::uint32_t kByte803490 = 0x803490;
// The leader's record (ObjTrio record 0): its pose byte +8, the member id
// +0x89, x +0x34 (high word +0x36), z +0x38 (high word +0x3A).
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kLeaderPose = 0x802D48;
constexpr std::uint32_t kLeaderMember = 0x802DC9;
constexpr std::uint32_t kLeaderX = 0x802D74;
constexpr std::uint32_t kLeaderZ = 0x802D78;
constexpr std::uint32_t kLeaderXHigh = 0x802D76;
constexpr std::uint32_t kLeaderZHigh = 0x802D7A;
constexpr std::uint32_t kPartyStride = 0x14C;
// The leader's zone counter (record 0 +0x134; area_w1f_callees.h's
// kLeaderZone): area 110's init sets Field_EdgeBits from it.
constexpr std::uint32_t kLeaderZone = 0x802E74;
// Sprite_ObjectsExtra's records (0xA4 bytes, four of them).
constexpr std::uint32_t kExtraStride = 0xA4;
constexpr unsigned kExtraCount = 4;
// Field_MoveSpeeds[3] and [4] (bytes): the kind-2 moves' divisors, << 3.
constexpr std::uint32_t kMoveSpeed3 = 0x6697F3;
constexpr std::uint32_t kMoveSpeed4 = 0x6697F4;
// Signed cell steps (x, z) per direction, two bytes apart
// (event_ops_callees.h's kCellDelta).
constexpr std::uint32_t kCellDelta = 0x66971C;
// Effect_Objects' stride and count (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;       // Effect_Objects
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;       // Sprite_Objects
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;  // Sprite_ObjectsExtra
constexpr std::uint32_t kSpriteKind2 = 0x7E0940;         // Sprite_Kind2 (0xA4 bytes)
constexpr std::uint32_t kActiveMember = 0x9035A4;        // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;        // MoveScript_Object
constexpr std::uint32_t kKind2Hold = 0x929F12;           // Field_Kind2Hold

// --- the areas' tables and cells (symbols.toml [[data]]) ---

// Area 108: the descriptor's +0x08 record (0x61E508: a signed count, two
// bytes, a flags byte at +3, a pointer to count records of 0x28 bytes: four
// s16 corners (x, y, z) and a velocity (x, y, z) each); the tail's scene
// moves them.
constexpr std::uint32_t kArea108Model = 0x61E508;       // Area108_Model, 8 bytes
constexpr std::uint32_t kArea108ModelFlags = 0x61E50B;
constexpr std::uint32_t kArea108ModelPoints = 0x61E50C;
constexpr std::uint32_t kArea108ModelStride = 0x28;
// Area 108: the five places its handler 4 tests (x high word, z high word,
// the leader's direction: bytes, three a row), the two place tables handlers
// 5 and 6 read by extra object 1's +0x83 - 1 (x byte, x half flag, z byte, z
// half flag), the two fade states handler 9 jumps through, the four cells
// its cell hook tests (x byte, z byte, direction, flag), and the four cells
// its helper sets (x, z words).
constexpr std::uint32_t kArea108Places = 0x61E5DC;      // Area108_Places, 5 x 3
constexpr unsigned kArea108PlaceCount = 5;
constexpr std::uint32_t kArea108PlaceCellsA = 0x61E5EC;  // Area108_PlaceCellsA, 9 x 4
constexpr std::uint32_t kArea108PlaceCellsB = 0x61E610;  // Area108_PlaceCellsB, 9 x 4
constexpr std::uint32_t kArea108FadeStates = 0x61E634;  // Area108_FadeStates, 2
constexpr unsigned kArea108FadeStateCount = 2;
constexpr std::uint32_t kArea108HookCells = 0x61E63C;   // Area108_HookCells, 4 x 4
constexpr unsigned kArea108HookCellCount = 4;
constexpr std::uint32_t kArea108SetCells = 0x61E64C;    // Area108_SetCells, 4 x (x, z) words
constexpr unsigned kArea108SetCellCount = 4;
// Area 110: its init's eight cells (x, z bytes) and eight weights.
constexpr std::uint32_t kArea110Cells = 0x61E7F0;       // Area110_Cells, 8 x 2
constexpr std::uint32_t kArea110Weights = 0x61E800;     // Area110_Weights, 8
// Area 111: the 7 x 7 grid of nibbles (4 bytes a row; an odd column the low
// nibble) at the end of the image's initialized .data, its init's copy of it
// (28 bytes) and the 7 x 7 map bytes it stamps.
constexpr std::uint32_t kArea111Grid = 0x675C00;        // Area111_Grid, 28 bytes
constexpr unsigned kArea111GridBytes = 0x1C;
constexpr unsigned kArea111GridSide = 7;
constexpr std::uint32_t kArea111GridStart = 0x61FBB0;   // Area111_GridStart, 28 bytes
constexpr std::uint32_t kArea111CellBytes = 0x61FB7C;   // Area111_CellBytes, 49
// Area 112: the four member ids its handler 2 looks for and the four message
// words by them, the four cells its cell hook tests (x, z, flag bits, flag),
// the four flags those bits set or clear, the four boxes its helper tests
// (centre x, centre z, half widths), and the effect's two states.
constexpr std::uint32_t kArea112MemberKeys = 0x61FF74;  // Area112_MemberKeys, 4
constexpr std::uint32_t kArea112Messages = 0x61FF78;    // Area112_Messages, 4 words
constexpr unsigned kArea112KeyCount = 4;
constexpr std::uint32_t kArea112HookCells = 0x61FF80;   // Area112_HookCells, 4 x 4
constexpr unsigned kArea112HookCellCount = 4;
constexpr std::uint32_t kArea112HookFlags = 0x61FFA0;   // Area112_HookFlags, 4
constexpr std::uint32_t kArea112Boxes = 0x61FFA4;       // Area112_Boxes, 4 x 4
constexpr unsigned kArea112BoxCount = 4;
constexpr std::uint32_t kArea112EffectStates = 0x61FFB4;   // Area112_EffectStates, 2
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
