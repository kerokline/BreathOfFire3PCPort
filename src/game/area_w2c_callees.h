// Internal to area_w2c.cpp and area_w2c_fuzz.cpp: the cells world 2's areas
// 90..92 and 94 touch that symbols.toml has no name for, and the areas' own
// .data tables. Every call is to a named function through the area harness
// (AH_CALL); this group has no raw-address callee. docs/area_w2c.md.
#pragma once

#include <cstdint>

namespace area_w2c {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row), read as a signed
// byte by some choices and as a byte by others (docs/item-use.md section 5),
// and the message word a choice leaves (0xFFFF: no new message).
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// A byte two of area 91's choices set to 6 beside the message they open (as
// areas 37 and 50 do; docs/area_w1c.md); no reader read this round.
constexpr std::uint32_t kAnswerMark = 0x9398CF;
// The object whose talk opened the message box: Field_ObjectTalk (0x517E90)
// stores Field_ActiveMember here (other groups' kFocusObject).
constexpr std::uint32_t kFocusObject = 0x903804;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state and its sub-kind.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
// The movement script's four counters (MoveScript_CounterOps, ops A0..AF).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter2 = 0x90384A;
constexpr std::uint32_t kCounter3 = 0x90384B;
// MoveScript_Var7 and the byte after it (the run step the scenes set beside it).
constexpr std::uint32_t kVar7 = 0x8034E4;
constexpr std::uint32_t kVar7Step = 0x8034E5;
// The story flags (Flags_Set / Flags_Test's bank) and the pointer to the
// chapter's flag row the chapters' flag calls take (docs/field-modes.md).
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kFlagRow = 0x929ED0;
// The three party lists' first bytes (0x904062..0x904064).
constexpr std::uint32_t kPartyList0 = 0x904062;
constexpr std::uint32_t kPartyList1 = 0x904063;
constexpr std::uint32_t kPartyList2 = 0x904064;
// The party's records (ObjTrio, 0x14C bytes each): record 0 the leader.
constexpr std::uint32_t kParty = 0x802D40;
constexpr std::uint32_t kPartyStride = 0x14C;
// The leader's byte +8 (the pose the hooks compare) and +0x137 (a byte of
// each member record; mode_flow_callees.h's kMemberWalk).
constexpr std::uint32_t kLeaderPose = 0x802D48;
constexpr std::uint32_t kLeader137 = 0x802E77;
// The previous area (u16; mode_flow_callees.h's kLastArea): area 94's init
// compares it with 0x79.
constexpr std::uint32_t kLastArea = 0x802290;
// Effect_Objects' stride (records of 0x80 bytes).
constexpr std::uint32_t kEffectStride = 0x80;
// The glow and the rings build their primitives from MapView_ScreenXY's two
// floats (x, then y at +4) and Prim_VertexScratch's words.
constexpr std::uint32_t kScreenX = 0x903820;
constexpr std::uint32_t kScreenY = 0x903824;
constexpr std::uint32_t kVertex0 = 0x9037A0;   // Prim_VertexScratch
constexpr std::uint32_t kVertex1 = 0x9037A8;
constexpr std::uint32_t kVertex2 = 0x9037B0;
// The CLUT strips area 91's state 3 clears and state 4 restores: a row of
// 0x20 words per object (its byte +0x27), Gfx_ClutStripSource the copy the
// strip is restored from; Gfx_ClutStripDirty counts the writes.
constexpr std::uint32_t kClutStrip = 0x80F580;         // Gfx_ClutStrip
constexpr std::uint32_t kClutStripSource = 0x80B580;   // Gfx_ClutStripSource
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;     // Effect_Objects
constexpr std::uint32_t kActiveMember = 0x9035A4;      // Field_ActiveMember
constexpr std::uint32_t kCondByteFE = 0x905E20;        // Cond_ByteFE
constexpr std::uint32_t kKind2Hold = 0x929F12;         // Field_Kind2Hold
constexpr std::uint32_t kKind2Z = 0x905E60;            // Field_Kind2Z
constexpr std::uint32_t kKind2X = 0x905E64;            // Field_Kind2X
constexpr std::uint32_t kPacketNext = 0x7E0670;        // Gfx_PacketNext
constexpr std::uint32_t kCameraMatrix = 0x905E40;      // Camera_Matrix
constexpr std::uint32_t kGteMatrix = 0x7DE4A0;         // Gte_Matrix
constexpr std::uint32_t kGteNearZ = 0x7DE498;          // Gte_NearZ
constexpr std::uint32_t kGteProjDistance = 0x7DE780;   // Gte_ProjDistance
constexpr std::uint32_t kGteOffsetY = 0x7DE78C;        // Gte_OffsetY, then Gte_OffsetX (0x7DE790)
constexpr std::uint32_t kPatchBase = 0x8CB5A8;         // AreaMap_PatchBase
constexpr std::uint32_t kMapHeader = 0x8CB580;         // AreaMap_Header

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 90: the byte pairs its choice copies to the focus object's +0x18 and
// +0x1C, by the s8 answer (six pairs before 0x6145D8, which area 91's
// descriptor names).
constexpr std::uint32_t kArea90FocusPairs = 0x6145CC;
// Area 91: handler 0's six states (by Sprite_Current[4]); the dword after
// them is the descriptor's +0x3C array, whose one entry is handler 0 itself.
constexpr std::uint32_t kArea91States = 0x614728;
constexpr unsigned kArea91StateCount = 6;
// Area 91: the rings' three turn directions (signed bytes, by the ring).
constexpr std::uint32_t kArea91RingTurns = 0x61478C;
// Area 92: the four choices' message words, by the s8 answer (choice 0 six
// words, choices 1..3 two each), and its effect records' third bytes by a
// party list byte (eight bytes each): handler 0's, handlers 1..3', 4..6'.
constexpr std::uint32_t kArea92Messages0 = 0x614EBC;
constexpr std::uint32_t kArea92Messages1 = 0x614EC8;
constexpr std::uint32_t kArea92Messages2 = 0x614ECC;
constexpr std::uint32_t kArea92Messages3 = 0x614ED0;
constexpr std::uint32_t kArea92EffectArgs4 = 0x614ED4;
constexpr std::uint32_t kArea92EffectArgs3 = 0x614790;
constexpr std::uint32_t kArea92EffectArgs1 = 0x614798;
// Area 94: the five choices' message words, by the s8 answer, and the five
// tables of its Effect_Spawn third bytes, by a party list byte.
constexpr std::uint32_t kArea94Messages0 = 0x616DB4;
constexpr std::uint32_t kArea94Messages1 = 0x616DC0;
constexpr std::uint32_t kArea94Messages2 = 0x616DCC;
constexpr std::uint32_t kArea94Messages3 = 0x616DD0;
constexpr std::uint32_t kArea94Messages4 = 0x616DD4;
constexpr std::uint32_t kArea94EffectArgsA = 0x614F78;
constexpr std::uint32_t kArea94EffectArgsB = 0x614F80;
constexpr std::uint32_t kArea94EffectArgsC = 0x614F88;
constexpr std::uint32_t kArea94EffectArgsD = 0x616DDC;
constexpr std::uint32_t kArea94EffectArgsE = 0x616DE4;

}  // namespace at
}  // namespace area_w2c
