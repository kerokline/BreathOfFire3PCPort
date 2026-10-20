// Internal to area_w4d.cpp and area_w4d_fuzz.cpp: the cells world 4's areas
// 175..187 touch that symbols.toml has no name for, the areas' own .data
// tables, and the one callee nobody owns (by its raw address, as round ten's
// rule for a function no group has taken). Every other call is to a named
// function through the area harness (AH_CALL). docs/area_w4d.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // kFieldReset names its function since round 14 (R4A)

namespace area_w4d {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row; the choices that
// index by it read it signed) and the message word a choice handler leaves
// (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state (s8), a sub-kind, a word timer.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's counters 0 and 3 (MoveScript_CounterOps).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
// MoveScript_Var7's neighbour byte (0x8034E5), which three choices set.
// kVar8: now MoveScript_PartyRecords[0x65] (symbols.toml).
// The story flags (Flags_Set / Flags_Test's bank).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// The return point Field_ChangeArea's callers keep (x, z dwords, the area
// word at +8; event_ops_callees.h's kReturnPoint), and the byte after it
// (0x904152) area 187's tail clears.
constexpr std::uint32_t kReturnPoint = 0x904148;
// kReturnX: now AreaW2d_ReturnPoint (symbols.toml).
// kReturnZ: now AreaW4d_ReturnZ (symbols.toml).
// kReturnArea: now AreaW4d_ReturnArea (symbols.toml).
// kReturnByte: now AreaW3c_CampFlag (symbols.toml).
// The focus object: the object whose talk ran last (area_w1a_callees.h's
// kFocusObject); byte +0x86 is its trigger id.
constexpr std::uint32_t kFocusObject = 0x903804;
// Party_Zenny (a dword; two choices compare it with 500).
// kZenny: now Party_Zenny (symbols.toml).
// The leader's pose byte (ObjTrio +8), the cell hook's third test.
// kLeaderPose: now ObjTrio[0].direction (symbols.toml).
// Six bytes of an engine facility's state (read and written by the engine
// code at 0x456AF0..0x460480; the name-entry commit of
// docs/save-interchange.md sets +2 and +4). The areas' choices set them;
// what they mean is the engine's, not read this round.
// kFacility: now Rest4b_BoardFlag (symbols.toml). .. 0x939A41
constexpr unsigned kFacilityBytes = 6;
// Area 179's init clears it and area 185's step hook (shared by areas
// 175..185) arms on it: set while the leader is off the hook's square,
// cleared when the hook fires. No other reader (pe_xref, 2026-09-28).
// kHookArmed: now AreaW4d_HookArmed (symbols.toml).
// The engine's Sprite_ObjectsExtra record 0 (the kind-2 object's twin):
// x +0x34, z +0x38, +0x83.
// kExtra0: now Sprite_ObjectsExtra (symbols.toml).
// Sprite_Kind2 +0x38 (its z), and MapView_Origin's second word.
// kKind2Z: now Sprite_Kind2.z (symbols.toml).
// kOriginY: now MapView_Origin[1] (symbols.toml).
// Camera_Angles (three words; the third is Cond_AngleFB's low word) and
// Camera_Distance.
// kCameraAngles: now Camera_Angles (symbols.toml).
// kCameraDistance: now Camera_Distance (symbols.toml).
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// kSpriteObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
// kScreenXY: now MapView_ScreenXY (symbols.toml). MapView_ScreenXY (two floats)
// kVertex: now Prim_VertexScratch (symbols.toml). Prim_VertexScratch (three words)
// kFocusZ: now MapView_FocusZ (symbols.toml). MapView_FocusZ
constexpr std::uint32_t kKind2X = 0x905E64;         // Field_Kind2X
// kKind2ZField: now Field_Kind2Z (symbols.toml). Field_Kind2Z
// kKind2Mode: now AreaW1a_EntryZone (symbols.toml). the byte after Field_Kind2X
// kDescriptors: now Area_Descriptors (symbols.toml). Area_Descriptors
constexpr unsigned kDescriptorCount = 200;

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 175: sixteen signed steps its handler 2 slides by (Frame_Counter &
// 0xF), its glide's two states (by Sprite_Current[4]) and sixteen signed
// steps the glide reads by +0xA & 0xF, and the four message words its
// handler 6 picks by the script's operand byte.
// kArea175SlideSteps: now Area175_SlideSteps (symbols.toml). Area175_SlideSteps, 16 bytes
// kArea175GlideStates: now Area175_GlideStates (symbols.toml). Area175_GlideStates, 2
constexpr unsigned kArea175GlideStateCount = 2;
// kArea175GlideSteps: now Area175_GlideSteps (symbols.toml). Area175_GlideSteps, 16 bytes
// kArea175ScriptMessages: now Area175_ScriptMessages (symbols.toml). Area175_ScriptMessages, 4 words
// The movement scripts area 175's descriptor +0x10 names (14; the fuzz keeps
// the script index inside them).
constexpr unsigned kArea175ScriptCount = 14;
// Area 187: four (x, y) byte pairs its choice 0 gives the focus object's
// dwords +0x18 / +0x1C by the answer.
// kArea187FocusPairs: now Area187_FocusPairs (symbols.toml). Area187_FocusPairs, 4 pairs

// --- the one callee by address (R4A's CommuSim_AreaEnter since round 14) ---

// 0x455450 (void): CommuSim_AreaEnter, ours (docs/rest_4a.md) - when Game_AreaNumber differs
// from the word 0x802290 it resets a block of field state (0x9039A0,
// 0x904A90, 0x937F80, 0x9046B0..0x9046CE) through 0x45E6B0 / 0x4560D0 /
// 0x455F40 / 0x4561A0 (read 2026-09-28 to 0x4554F8; the rest is its
// owner's). Area 179's init (shared by areas 175..185) calls it when
// Cond_ByteFA is above 7.
constexpr std::uint32_t kFieldReset = bof3::addr::CommuSim_AreaEnter;   // the value unchanged, the fuzz keys stand

}  // namespace at
}  // namespace area_w4d
