// Internal to area_w3f.cpp and area_w3f_fuzz.cpp: the cells world 3's areas
// 143..146 touch that symbols.toml has no name for, the areas' own .data
// tables, and the two callees nobody owns (Capcom's map-camera helpers, by
// their raw addresses, as round ten's rule for a function no group has
// taken). Every other call is to a named function through the area harness
// (AH_CALL). docs/area_w3f.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // EffectGte_* (round thirteen group EGT)

namespace area_w3f {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row) and the message
// word a choice handler leaves (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// A byte a choice sets to 6 beside the message it opens (areas 3, 37, 50,
// 98 do the same), and a second byte two further on the shared yes-handlers
// set to 3 or 5; no reader of either read this round.
// kAnswerMark: now AreaW0c_AnswerMark (symbols.toml).
// kAnswerMarkD1: now AreaW3f_AnswerMarkD1 (symbols.toml).
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state (s8), a sub-kind, a word timer.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's counters (MoveScript_CounterOps, ops A0..AF): 0 and
// 3; the byte after MoveScript_Var7 (the run's step, as the chapters use it).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
// kVar7Step: now MoveScript_PartyRecords[0x65] (symbols.toml).
// The slot byte area 145's effect spawns keep at SharedScratch (0x903850)
// and the byte after it, where area 145's handler 0 keeps a record index.
constexpr std::uint32_t kSlotScratch = SharedScratch_addr;
constexpr std::uint32_t kRecordIndex = 0x903851;
// The story flags (Flags_Set / Flags_Test's bank), Cond_Flags row 13 (the
// row area 145's cell hook tests: 0x903F90 + 8 * 13), the row pointer the
// chapters' flag calls take (Cond_Flags + 8 * chapter), and a dword of the
// second bit array whose value area 143's choice 0 compares with 0x3FFFF
// (eighteen flags set).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kCondRow13: now Cond_Flags[0x68] (symbols.toml).
// kFlagRow: now Chapter_FlagRow (symbols.toml).
// kFlags904650: now Gene_Bits (symbols.toml).
// The focus object: the object whose talk ran last (MoveScript_SetTurnTarget
// stores Field_ActiveMember there; area_w1a_callees.h's kFocusObject).
constexpr std::uint32_t kFocusObject = 0x903804;
// The party records (ObjTrio): the leader's byte +8 (the pose the hooks
// compare), its word x high +0x36, the byte +0x89 of the leader and of the
// third record.
// kLeaderPose: now ObjTrio[0].direction (symbols.toml).
// kLeaderX: now ObjTrio[0].x (symbols.toml).
// kLeaderXHigh: now ObjTrio[0].x_whole (symbols.toml).
// kLeaderZ: now ObjTrio[0].z (symbols.toml).
// kLeader89: now ObjTrio[0].char_id (symbols.toml).
// kMember3_89: now ObjTrio[2].char_id (symbols.toml).
constexpr std::uint32_t kPartyStride = 0x14C;
// The leader's record's bytes +1..+3 area 145's tail sets (0x802D41..43).
// kLeader1: now ObjTrio[0].state (symbols.toml).
// Sprite_ObjectsExtra's first record (0x802000): its x, z, y (+0x34, +0x38,
// +0x3C) and dword +0x6C (bits 9..10 a quarter turn); area 145's trails read
// records 0..3 at the stride 0xA4.
// kExtraObjects: now Sprite_ObjectsExtra (symbols.toml).
constexpr std::uint32_t kObjectStride = 0xA4;
// Field_ScriptFlags' low byte (area 144's handler 4 sets bit 8 in it).
// kScriptFlagsLow: now Field_ScriptFlags (symbols.toml).
// The kind-2 object (Sprite_Kind2 0x7E0940, 0xA4 bytes): x, z, y (+0x34,
// +0x38, +0x3C) and bytes +0x84, +0x87 area 144's handler 6 sets before
// MoveCmd_MoveKind2.
// kKind2: now Sprite_Kind2 (symbols.toml).
// Camera_Angles[1] (s16) and Transition_Busy (u16), which area 145's
// tail reads.
// kCameraAngle1: now Camera_Angles[1] (symbols.toml).
// kWaitWordDA: now Transition_Busy (symbols.toml).
// Effect_Objects' stride and count (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// The CLUT rows area 143's shift reads (Gfx_ClutStripSource + 0x600, eleven
// rows of 0x200 bytes) and writes (0x4000 further: Gfx_ClutStrip + 0x600).
// kClutSourceRows: now Gfx_ClutStripSource[0x300] (symbols.toml).
constexpr std::uint32_t kClutSourceEnd = 0x80D180;
constexpr std::uint32_t kClutToLive = 0x4000;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
// kCondByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
// kKind2Hold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
// kOtSlot: now Draw_OtSlot (symbols.toml). Draw_OtSlot
constexpr std::uint32_t kTextRecords = 0x904CE0;       // Text_Records
// kPartyRecords: now MoveScript_PartyRecords (symbols.toml). MoveScript_PartyRecords (records of 16 bytes)
constexpr std::uint32_t kClutDirty = 0x937F90;         // Gfx_ClutStripDirty

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 143: four member keys (bytes) and their four message words; effect
// kind 0xB3's two states.
// kArea143MemberKeys: now Area143_MemberKeys (symbols.toml). Area143_MemberKeys, 4 bytes
// kArea143MemberMessages: now Area143_MemberMessages (symbols.toml). Area143_MemberMessages, 4 words
// kArea143EffectStates: now Area143_EffectStates (symbols.toml). Area143_EffectStates, 2
// Area 145: the drop's two states; three key / message pairs (four bytes,
// four words, one pair a handler); the cell hook's seven records (x, z,
// pose in the low nibble, the tail state); the trails' legs (records of
// four bytes: a direction, a length, the next table's index, pad) - the
// first leg's four and the second and third legs' two banks each.
// kArea145DropStates: now Area145_DropStates (symbols.toml). Area145_DropStates, 2
// kArea145MemberKeys1: now Area145_MemberKeys1 (symbols.toml). Area145_MemberKeys1, 4 bytes
constexpr std::uint32_t kArea145MemberMessages1 = 0x6336F0;
// kArea145MemberKeys5: now Area145_MemberKeys5 (symbols.toml).
constexpr std::uint32_t kArea145MemberMessages5 = 0x6336FC;
// kArea145MemberKeysB: now Area145_MemberKeysB (symbols.toml).
constexpr std::uint32_t kArea145MemberMessagesB = 0x633708;
// kArea145CellRecords: now Area145_CellRecords (symbols.toml). Area145_CellRecords, 7 of 4 bytes
constexpr unsigned kArea145CellRecordCount = 7;
// kArea145Legs0: now Area145_TrailLegs0 (symbols.toml). Area145_TrailLegs0, 4 of 4 bytes
// kArea145Legs1A: now Area145_TrailLegs1A (symbols.toml). Area145_TrailLegs1A (record 1), 4 of 4
// kArea145Legs1B: now Area145_TrailLegs1B (symbols.toml). Area145_TrailLegs1B (other records), 4 of 4
// kArea145Legs2A: now Area145_TrailLegs2A (symbols.toml). Area145_TrailLegs2A (record 1), 4 of 4
// kArea145Legs2B: now Area145_TrailLegs2B (symbols.toml). Area145_TrailLegs2B (other records), 4 of 4
// Area 146: effect kind 0xB4's two states.
// kArea146EffectStates: now Area146_EffectStates (symbols.toml). Area146_EffectStates, 2
constexpr unsigned kStateCount = 2;
// Field_CellDeltas-style table: two signed bytes (x, z) a direction
// (event_ops_callees.h's kCellDelta).
constexpr std::uint32_t kCellDelta = 0x66971C;

// --- the callees nobody owns (raw addresses) ---

// 0x494060 (no arguments): sets the map camera up for a draw (magic_c2.cpp's
// kSetMapCamera). Engine; no group's.
constexpr std::uint32_t kSetMapCamera = bof3::addr::EffectGte_LoadMapCamera;
// 0x494110 (const long* point, float* vertex): projects a world point into a
// vertex of three dwords (two floats and a depth) at its second argument
// (magic_s32.cpp's kProjectPoint). Engine; no group's.
constexpr std::uint32_t kProjectPoint = bof3::addr::EffectGte_ProjectPoint;

}  // namespace at
}  // namespace area_w3f
