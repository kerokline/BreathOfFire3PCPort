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
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// A byte two of area 91's choices set to 6 beside the message they open (as
// areas 37 and 50 do; docs/area_w1c.md); no reader read this round.
// kAnswerMark: now AreaW0c_AnswerMark (symbols.toml).
// The object whose talk opened the message box: Field_ObjectTalk (0x517E90)
// stores Field_ActiveMember here (other groups' kFocusObject).
constexpr std::uint32_t kFocusObject = 0x903804;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state and its sub-kind.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
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
// kFlagRow: now AreaW0c_FlagRow (symbols.toml).
// The three party lists' first bytes (0x904062..0x904064).
constexpr std::uint32_t kPartyList0 = 0x904062;
constexpr std::uint32_t kPartyList1 = 0x904063;
constexpr std::uint32_t kPartyList2 = 0x904064;
// The party's records (ObjTrio, 0x14C bytes each): record 0 the leader.
// kParty: now ObjTrio (symbols.toml).
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
// kScreenX: now MapView_ScreenXY (symbols.toml).
constexpr std::uint32_t kScreenY = 0x903824;
// kVertex0: now Prim_VertexScratch (symbols.toml). Prim_VertexScratch
constexpr std::uint32_t kVertex1 = 0x9037A8;
constexpr std::uint32_t kVertex2 = 0x9037B0;
// The CLUT strips area 91's state 3 clears and state 4 restores: a row of
// 0x20 words per object (its byte +0x27), Gfx_ClutStripSource the copy the
// strip is restored from; Gfx_ClutStripDirty counts the writes.
// kClutStrip: now Gfx_ClutStrip (symbols.toml). Gfx_ClutStrip
// kClutStripSource: now Gfx_ClutStripSource (symbols.toml). Gfx_ClutStripSource
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kCondByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
// kKind2Hold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
constexpr std::uint32_t kKind2Z = 0x905E60;            // Field_Kind2Z
constexpr std::uint32_t kKind2X = 0x905E64;            // Field_Kind2X
// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
// kCameraMatrix: now Camera_Matrix (symbols.toml). Camera_Matrix
// kGteMatrix: now Gte_Matrix (symbols.toml). Gte_Matrix
// kGteNearZ: now Gte_NearZ (symbols.toml). Gte_NearZ
// kGteProjDistance: now Gte_ProjDistance (symbols.toml). Gte_ProjDistance
// kGteOffsetY: now Gte_OffsetY (symbols.toml). Gte_OffsetY, then Gte_OffsetX (0x7DE790)
// kPatchBase: now AreaMap_PatchBase (symbols.toml). AreaMap_PatchBase
// kMapHeader: now AreaMap_Header (symbols.toml). AreaMap_Header

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 90: the byte pairs its choice copies to the focus object's +0x18 and
// +0x1C, by the s8 answer (six pairs before 0x6145D8, which area 91's
// descriptor names).
// kArea90FocusPairs: now Area90_FocusPairs (symbols.toml).
// Area 91: handler 0's six states (by Sprite_Current[4]); the dword after
// them is the descriptor's +0x3C array, whose one entry is handler 0 itself.
// kArea91States: now Area91_States (symbols.toml).
constexpr unsigned kArea91StateCount = 6;
// Area 91: the rings' three turn directions (signed bytes, by the ring).
// kArea91RingTurns: now Area91_RingTurns (symbols.toml).
// Area 92: the four choices' message words, by the s8 answer (choice 0 six
// words, choices 1..3 two each), and its effect records' third bytes by a
// party list byte (eight bytes each): handler 0's, handlers 1..3', 4..6'.
// kArea92Messages0: now Area92_ChoiceMessages (symbols.toml).
constexpr std::uint32_t kArea92Messages1 = 0x614EC8;
constexpr std::uint32_t kArea92Messages2 = 0x614ECC;
constexpr std::uint32_t kArea92Messages3 = 0x614ED0;
// kArea92EffectArgs4: now Area92_EffectArgs4 (symbols.toml).
// kArea92EffectArgs3: now Area92_EffectArgs3 (symbols.toml).
// kArea92EffectArgs1: now Area92_EffectArgs1 (symbols.toml).
// Area 94: the five choices' message words, by the s8 answer, and the five
// tables of its Effect_Spawn third bytes, by a party list byte.
// kArea94Messages0: now Area94_ChoiceMessages (symbols.toml).
constexpr std::uint32_t kArea94Messages1 = 0x616DC0;
constexpr std::uint32_t kArea94Messages2 = 0x616DCC;
constexpr std::uint32_t kArea94Messages3 = 0x616DD0;
constexpr std::uint32_t kArea94Messages4 = 0x616DD4;
// kArea94EffectArgsA: now Area94_EffectArgsA (symbols.toml).
// kArea94EffectArgsB: now Area94_EffectArgsB (symbols.toml).
// kArea94EffectArgsC: now Area94_EffectArgsC (symbols.toml).
// kArea94EffectArgsD: now Area94_EffectArgsD (symbols.toml).
// kArea94EffectArgsE: now Area94_EffectArgsE (symbols.toml).

}  // namespace at
}  // namespace area_w2c
