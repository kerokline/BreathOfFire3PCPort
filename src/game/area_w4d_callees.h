// Internal to area_w4d.cpp and area_w4d_fuzz.cpp: the cells world 4's areas
// 175..187 touch that symbols.toml has no name for, the areas' own .data
// tables, and the one callee nobody owns (by its raw address, as round ten's
// rule for a function no group has taken). Every other call is to a named
// function through the area harness (AH_CALL). docs/area_w4d.md.
#pragma once

#include <cstdint>

namespace area_w4d {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row; the choices that
// index by it read it signed) and the message word a choice handler leaves
// (0xFFFF: no new message).
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state (s8), a sub-kind, a word timer.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's counters 0 and 3 (MoveScript_CounterOps).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
// MoveScript_Var7's neighbour byte (0x8034E5), which three choices set.
constexpr std::uint32_t kVar8 = 0x8034E5;
// The story flags (Flags_Set / Flags_Test's bank).
constexpr std::uint32_t kStoryFlags = 0x904030;
// The return point Field_ChangeArea's callers keep (x, z dwords, the area
// word at +8; event_ops_callees.h's kReturnPoint), and the byte after it
// (0x904152) area 187's tail clears.
constexpr std::uint32_t kReturnPoint = 0x904148;
constexpr std::uint32_t kReturnX = 0x904148;
constexpr std::uint32_t kReturnZ = 0x90414C;
constexpr std::uint32_t kReturnArea = 0x904150;
constexpr std::uint32_t kReturnByte = 0x904152;
// The focus object: the object whose talk ran last (area_w1a_callees.h's
// kFocusObject); byte +0x86 is its trigger id.
constexpr std::uint32_t kFocusObject = 0x903804;
// Party_Zenny (a dword; two choices compare it with 500).
constexpr std::uint32_t kZenny = 0x904058;
// The leader's pose byte (ObjTrio +8), the cell hook's third test.
constexpr std::uint32_t kLeaderPose = 0x802D48;
// Six bytes of an engine facility's state (read and written by the engine
// code at 0x456AF0..0x460480; the name-entry commit of
// docs/save-interchange.md sets +2 and +4). The areas' choices set them;
// what they mean is the engine's, not read this round.
constexpr std::uint32_t kFacility = 0x939A3C;   // .. 0x939A41
constexpr unsigned kFacilityBytes = 6;
// Area 179's init clears it and area 185's step hook (shared by areas
// 175..185) arms on it: set while the leader is off the hook's square,
// cleared when the hook fires. No other reader (pe_xref, 2026-09-28).
constexpr std::uint32_t kHookArmed = 0x9046CF;
// The engine's Sprite_ObjectsExtra record 0 (the kind-2 object's twin):
// x +0x34, z +0x38, +0x83.
constexpr std::uint32_t kExtra0 = 0x802000;
// Sprite_Kind2 +0x38 (its z), and MapView_Origin's second word.
constexpr std::uint32_t kKind2Z = 0x7E0978;
constexpr std::uint32_t kOriginY = 0x7E068A;
// Camera_Angles (three words; the third is Cond_AngleFB's low word) and
// Camera_Distance.
constexpr std::uint32_t kCameraAngles = 0x929EC8;
constexpr std::uint32_t kCameraDistance = 0x903840;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;   // Effect_Objects
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;   // Sprite_ObjectsExtra
constexpr std::uint32_t kActiveMember = 0x9035A4;   // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;   // MoveScript_Object
constexpr std::uint32_t kPacketNext = 0x7E0670;     // Gfx_PacketNext
constexpr std::uint32_t kScreenXY = 0x903820;       // MapView_ScreenXY (two floats)
constexpr std::uint32_t kVertex = 0x9037A0;         // Prim_VertexScratch (three words)
constexpr std::uint32_t kFocusZ = 0x929F18;         // MapView_FocusZ
constexpr std::uint32_t kKind2X = 0x905E64;         // Field_Kind2X
constexpr std::uint32_t kKind2ZField = 0x905E60;    // Field_Kind2Z
constexpr std::uint32_t kKind2Mode = 0x905E68;      // the byte after Field_Kind2X
constexpr std::uint32_t kDescriptors = 0x667590;    // Area_Descriptors
constexpr unsigned kDescriptorCount = 200;

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 175: sixteen signed steps its handler 2 slides by (Frame_Counter &
// 0xF), its glide's two states (by Sprite_Current[4]) and sixteen signed
// steps the glide reads by +0xA & 0xF, and the four message words its
// handler 6 picks by the script's operand byte.
constexpr std::uint32_t kArea175SlideSteps = 0x642494;    // Area175_SlideSteps, 16 bytes
constexpr std::uint32_t kArea175GlideStates = 0x6424A4;   // Area175_GlideStates, 2
constexpr unsigned kArea175GlideStateCount = 2;
constexpr std::uint32_t kArea175GlideSteps = 0x6424AC;    // Area175_GlideSteps, 16 bytes
constexpr std::uint32_t kArea175ScriptMessages = 0x6424BC;   // Area175_ScriptMessages, 4 words
// The movement scripts area 175's descriptor +0x10 names (14; the fuzz keeps
// the script index inside them).
constexpr unsigned kArea175ScriptCount = 14;
// Area 187: four (x, y) byte pairs its choice 0 gives the focus object's
// dwords +0x18 / +0x1C by the answer.
constexpr std::uint32_t kArea187FocusPairs = 0x645A5C;    // Area187_FocusPairs, 4 pairs

// --- the one callee nobody owns (raw address) ---

// 0x455450 (void): engine code nobody owns - when Game_AreaNumber differs
// from the word 0x802290 it resets a block of field state (0x9039A0,
// 0x904A90, 0x937F80, 0x9046B0..0x9046CE) through 0x45E6B0 / 0x4560D0 /
// 0x455F40 / 0x4561A0 (read 2026-09-28 to 0x4554F8; the rest is its
// owner's). Area 179's init (shared by areas 175..185) calls it when
// Cond_ByteFA is above 7.
constexpr std::uint32_t kFieldReset = 0x455450;

}  // namespace at
}  // namespace area_w4d
