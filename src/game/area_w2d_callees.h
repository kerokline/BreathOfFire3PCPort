// Internal to area_w2d.cpp and area_w2d_fuzz.cpp: the cells world 2's areas
// 95..100 and 103 touch that symbols.toml has no name for, the areas' own
// .data tables, and the two callees nobody owns yet (by their raw addresses,
// as round ten's rule for a function no group has taken). Every other call is
// to a named function through the area harness (AH_CALL). docs/area_w2d.md.
#pragma once

#include <cstdint>

namespace area_w2d {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row; area 100's choice
// reads it signed) and the message word a choice handler leaves (0xFFFF: no
// new message).
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
// The story flags (Flags_Set / Flags_Test's bank), the Cond_Flags row the
// area 99 init tests (row 3: 0x903F90 + 8 * 3), the row pointer the chapters'
// flag calls take (Cond_Flags + 8 * chapter; docs/field-modes.md), and a
// second bit array area 98's choice sets a flag in (the save menu's
// kFlagsA, save_menu_callees.h).
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kCondRow3 = 0x903FA8;
constexpr std::uint32_t kFlagRow = 0x929ED0;
constexpr std::uint32_t kFlags904654 = 0x904654;
// The return point Field_ChangeArea's callers keep (x, z dwords, the area
// word at +8; event_ops_callees.h's kReturnPoint).
constexpr std::uint32_t kReturnPoint = 0x904148;
// A byte area 98's choice sets to 6 beside the message it opens (areas 37
// and 50 do the same); no reader read this round.
constexpr std::uint32_t kAnswerMark = 0x9398CF;
// The focus object: the object whose talk ran last (MoveScript_SetTurnTarget
// stores Field_ActiveMember there; area_w1a_callees.h's kFocusObject).
constexpr std::uint32_t kFocusObject = 0x903804;
// The leader's record (ObjTrio): x +0x34, z +0x38, byte +8 (the pose the
// hooks compare), byte +0x137 (area 95's handler 0 tests it;
// field_hidden_callees.h's kLeader137).
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kLeaderX = 0x802D74;
constexpr std::uint32_t kLeaderZ = 0x802D78;
constexpr std::uint32_t kLeaderPose = 0x802D48;
constexpr std::uint32_t kLeader137 = 0x802E77;
constexpr std::uint32_t kPartyStride = 0x14C;
// Field_MoveSpeeds + 3 (a byte): area 99's leap divides 16 by it.
constexpr std::uint32_t kMoveSpeed3 = 0x6697F3;
// MoveScript_PartyRecords record 0 (16 bytes): area 95's arc writes its
// count +1 and its word +4.
constexpr std::uint32_t kPartyRecord0 = 0x803480;
// Effect_Objects' stride and count (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;     // Effect_Objects
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;     // Sprite_Objects
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;   // Sprite_ObjectsExtra
constexpr std::uint32_t kActiveMember = 0x9035A4;      // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;      // MoveScript_Object
constexpr std::uint32_t kCameraShiftY = 0x903802;      // Camera_ShiftY

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 99: two two-state tables its handlers 3 and 4 jump through by
// Sprite_Current[4], and the sixteen signed steps its drift reads by
// +0xA & 0xF.
constexpr std::uint32_t kArea99DriftStates = 0x619C3C;   // Area99_DriftStates, 2
constexpr std::uint32_t kArea99DriftSteps = 0x619C44;    // Area99_DriftSteps, 16 bytes
constexpr std::uint32_t kArea99LeapStates = 0x619C54;    // Area99_LeapStates, 2
constexpr unsigned kArea99StateCount = 2;
// Area 100: its choice's message words by the answer (five), and the two
// states of effect kind 0xB7's handler (by Sprite_Current[1]).
constexpr std::uint32_t kArea100ChoiceMessages = 0x61AB34;   // Area100_ChoiceMessages, 5 words
constexpr std::uint32_t kArea100EffectStates = 0x61AB40;     // Area100_EffectStates, 2
constexpr unsigned kArea100EffectStateCount = 2;
// Area 103: three bytes its handler 0 looks for among the members' +0x89,
// and four signed steps its handler 3 moves the elevation by.
constexpr std::uint32_t kArea103MemberKeys = 0x61B4E4;   // Area103_MemberKeys, 3 bytes
constexpr unsigned kArea103MemberKeyCount = 3;
constexpr std::uint32_t kArea103Shake = 0x61B4E8;        // Area103_Shake, 4 bytes

// --- the two callees nobody owns this wave (raw addresses) ---

// 0x57C160 (0x1F bytes): (bits, index) - bits[index >> 3] ^= 1 << (index & 7),
// the index a byte: Flags_Set / Flags_Clear's sibling that toggles. Engine;
// group SX2's this wave.
constexpr std::uint32_t kFlagsToggle = 0x57C160;
// 0x4220D0 (0x27A bytes): (const long* point) - reads the point's three
// dwords (x, z, y) and builds positions about it with Math_Cos / Math_Sin
// and the frame counter (its first 0x100 bytes read here; the rest is its
// owner's). World 3's area code (the tool's AREA146, group AR3F), called by
// the effect states of areas 36, 59, 100, 112, 116 and 146.
constexpr std::uint32_t kRingAt = 0x4220D0;

}  // namespace at
}  // namespace area_w2d
