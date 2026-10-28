// Internal to area_w2d.cpp and area_w2d_fuzz.cpp: the cells world 2's areas
// 95..100 and 103 touch that symbols.toml has no name for, the areas' own
// .data tables, and the two callees nobody owns yet (by their raw addresses,
// as round ten's rule for a function no group has taken). Every other call is
// to a named function through the area harness (AH_CALL). docs/area_w2d.md.
#pragma once

#include <cstdint>

#include "common/image_base.h"

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w2d {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row; area 100's choice
// reads it signed) and the message word a choice handler leaves (0xFFFF: no
// new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr ::bof3::image::Va kMessage{0x7DEE48};
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind, and a word timer.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
constexpr ::bof3::image::Va kTailTimer{0x9039F6};
// The movement script's four counters (MoveScript_CounterOps, ops A0..AF).
constexpr ::bof3::image::Va kCounter0{0x903848};
constexpr ::bof3::image::Va kCounter1{0x903849};
constexpr ::bof3::image::Va kCounter3{0x90384B};
// The story flags (Flags_Set / Flags_Test's bank), the Cond_Flags row the
// area 99 init tests (row 3: 0x903F90 + 8 * 3), the row pointer the chapters'
// flag calls take (Cond_Flags + 8 * chapter; docs/field-modes.md), and a
// second bit array area 98's choice sets a flag in (the save menu's
// kFlagsA, save_menu_callees.h).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kCondRow3: now Cond_Flags[0x18] (symbols.toml).
// kFlagRow: now Chapter_FlagRow (symbols.toml).
constexpr ::bof3::image::Va kFlags904654{0x904654};
// The return point Field_ChangeArea's callers keep (x, z dwords, the area
// word at +8; event_ops_callees.h's kReturnPoint).
constexpr ::bof3::image::Va kReturnPoint{0x904148};
// A byte area 98's choice sets to 6 beside the message it opens (areas 37
// and 50 do the same); no reader read this round.
// kAnswerMark: now AreaW0c_AnswerMark (symbols.toml).
// The focus object: the object whose talk ran last (MoveScript_SetTurnTarget
// stores Field_ActiveMember there; area_w1a_callees.h's kFocusObject).
constexpr ::bof3::image::Va kFocusObject{0x903804};
// The leader's record (ObjTrio): x +0x34, z +0x38, byte +8 (the pose the
// hooks compare), byte +0x137 (area 95's handler 0 tests it;
// field_hidden_callees.h's kLeader137).
// kLeader: now ObjTrio (symbols.toml).
// kLeaderX: now ObjTrio[0].x (symbols.toml).
// kLeaderZ: now ObjTrio[0].z (symbols.toml).
// kLeaderPose: now ObjTrio[0].direction (symbols.toml).
constexpr ::bof3::image::Va kLeader137{0x802E77};
constexpr std::uint32_t kPartyStride = 0x14C;
// Field_MoveSpeeds + 3 (a byte): area 99's leap divides 16 by it.
constexpr ::bof3::image::Va kMoveSpeed3{0x6697F3};
// MoveScript_PartyRecords record 0 (16 bytes): area 95's arc writes its
// count +1 and its word +4.
// kPartyRecord0: now MoveScript_PartyRecords (symbols.toml).
// Effect_Objects' stride and count (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
// kSpriteObjects: now Sprite_Objects (symbols.toml). Sprite_Objects
// kSpriteObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kCameraShiftY: now Camera_ShiftY (symbols.toml). Camera_ShiftY

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 99: two two-state tables its handlers 3 and 4 jump through by
// Sprite_Current[4], and the sixteen signed steps its drift reads by
// +0xA & 0xF.
// kArea99DriftStates: now Area99_DriftStates (symbols.toml). Area99_DriftStates, 2
// kArea99DriftSteps: now Area99_DriftSteps (symbols.toml). Area99_DriftSteps, 16 bytes
// kArea99LeapStates: now Area99_LeapStates (symbols.toml). Area99_LeapStates, 2
constexpr unsigned kArea99StateCount = 2;
// Area 100: its choice's message words by the answer (five), and the two
// states of effect kind 0xB7's handler (by Sprite_Current[1]).
// kArea100ChoiceMessages: now Area100_ChoiceMessages (symbols.toml). Area100_ChoiceMessages, 5 words
// kArea100EffectStates: now Area100_EffectStates (symbols.toml). Area100_EffectStates, 2
constexpr unsigned kArea100EffectStateCount = 2;
// Area 103: three bytes its handler 0 looks for among the members' +0x89,
// and four signed steps its handler 3 moves the elevation by.
// kArea103MemberKeys: now Area103_MemberKeys (symbols.toml). Area103_MemberKeys, 3 bytes
constexpr unsigned kArea103MemberKeyCount = 3;
// kArea103Shake: now Area103_Shake (symbols.toml). Area103_Shake, 4 bytes

// --- the two callees nobody owns this wave (raw addresses) ---

// 0x57C160 (0x1F bytes): (bits, index) - bits[index >> 3] ^= 1 << (index & 7),
// the index a byte: Flags_Set / Flags_Clear's sibling that toggles. Engine;
// group SX2's this wave.
constexpr std::uint32_t kFlagsToggle = bof3::addr::Flags_Toggle;
// 0x4220D0 (0x27A bytes): (const long* point) - reads the point's three
// dwords (x, z, y) and builds positions about it with Math_Cos / Math_Sin
// and the frame counter (its first 0x100 bytes read here; the rest is its
// owner's). World 3's area code (the tool's AREA146, group AR3F), called by
// the effect states of areas 36, 59, 100, 112, 116 and 146.
constexpr std::uint32_t kRingAt = bof3::addr::Area146_DrawGlowCylinder;

}  // namespace at
}  // namespace area_w2d
