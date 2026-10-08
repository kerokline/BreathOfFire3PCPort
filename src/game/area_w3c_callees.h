// Internal to area_w3c.cpp and area_w3c_fuzz.cpp: the cells world 3's areas
// 124..125, 127..128 and 130..134 touch that symbols.toml has no name for, the
// areas' own .data tables, and the one callee nobody owns (the CRT's strncpy,
// by its raw address, as round ten's rule for a function no group has taken).
// Every other call is to a named function through the area harness (AH_CALL).
// docs/area_w3c.md.
#pragma once

#include <cstdint>

namespace area_w3c {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row; areas 130 and 133
// read it signed) and the message word a choice handler leaves (0xFFFF: no
// new message).
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// The focus object: the object whose talk ran last (MoveScript_SetTurnTarget
// stores Field_ActiveMember there; area_w1a_callees.h's kFocusObject).
constexpr std::uint32_t kFocusObject = 0x903804;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state and a sub-kind.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
// The movement script's four counters (MoveScript_CounterOps, ops A0..AF).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter2 = 0x90384A;
constexpr std::uint32_t kCounter3 = 0x90384B;
// MoveScript_Var7 and the byte after it: the scene the choices start and its
// step (area_w2c_callees.h's kVar7 / kVar7Step); tail kind 63 keeps its state
// in the step byte.
constexpr std::uint32_t kVar7 = 0x8034E4;
constexpr std::uint32_t kVar7Step = 0x8034E5;
// The story flags (Flags_Set / Flags_Test's bank).
constexpr std::uint32_t kStoryFlags = 0x904030;
// The party lists' first bytes (a member's character id; the spawns index
// their tables by it).
constexpr std::uint32_t kPartyList0 = 0x904062;
constexpr std::uint32_t kPartyList1 = 0x904063;
constexpr std::uint32_t kPartyList2 = 0x904064;
// The party's records (ObjTrio: three of 0x14C); the leader's byte +0x89
// (MoveScript_EffectState's index, area_w2d's reading) and its dword +0x134
// (Field_EdgeBits is set from it less 5, area_w1f's kLeaderZone).
constexpr std::uint32_t kParty = 0x802D40;
constexpr std::uint32_t kPartyStride = 0x14C;
constexpr std::uint32_t kLeader89 = 0x802DC9;
constexpr std::uint32_t kLeaderZone = 0x802E74;
// The previous area (u16): the inits test it (area_w2c_callees.h's kLastArea).
constexpr std::uint32_t kLastArea = 0x802290;
// A byte before Cond_Flags tail kind 63 picks one of six items by (no reader
// of it is named in this repo).
constexpr std::uint32_t kItemPick = 0x903F6A;
// Text_Records + 0x2F: tail kind 63 zeroes it before its message.
constexpr std::uint32_t kTextRecords2F = 0x904D0F;
// A byte area 131's tail kind 34 zeroes before it changes area
// (menu_lists_callees.h's kCampFlag, a hypothesis there).
constexpr std::uint32_t kCampFlag = 0x904152;
// A byte area 133's choice zeroes (scena_sc6 / sc12 / shop_states read it as
// a load or save wait byte).
constexpr std::uint32_t kLoad0F = 0x929F0F;
// Effect_Objects' stride (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;     // Effect_Objects
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;     // Sprite_Objects
constexpr std::uint32_t kActiveMember = 0x9035A4;      // Field_ActiveMember
constexpr std::uint32_t kCameraShiftX = 0x903800;      // Camera_ShiftX, then Camera_ShiftY 0x903802
constexpr std::uint32_t kCameraShiftY = 0x903802;      // Camera_ShiftY
constexpr std::uint32_t kCondByteFE = 0x905E20;        // Cond_ByteFE
constexpr std::uint32_t kPassFlags = 0x7E0918;         // Draw_PassFlags
constexpr std::uint32_t kPacketNext = 0x7E0670;        // Gfx_PacketNext
constexpr std::uint32_t kTextRecords = 0x904CE0;       // Text_Records
constexpr std::uint32_t kMusicTrack = 0x904131;        // Music_Track
constexpr std::uint32_t kPatchBase = 0x8CB5A8;         // AreaMap_PatchBase
constexpr std::uint32_t kMapHeader = 0x8CB580;         // AreaMap_Header
constexpr std::uint32_t kScriptFlags = 0x9039A2;       // Field_ScriptFlags (u16)
constexpr std::uint32_t kEdgeBits = 0x905B80;          // Field_EdgeBits (u16)

// --- the areas' tables (symbols.toml [[data]]) ---

// Areas 124 and 125's inits: eight cells (x, z byte pairs) and eight weights
// (summing to 64 in the image) each, as areas 72 and 73's.
constexpr std::uint32_t kArea124Cells = 0x6265AC;     // Area124_Cells, 8 pairs
constexpr std::uint32_t kArea124Weights = 0x6265BC;   // Area124_Weights, 8
constexpr std::uint32_t kArea125Cells = 0x6266E8;     // Area125_Cells, 8 pairs
constexpr std::uint32_t kArea125Weights = 0x6266F8;   // Area125_Weights, 8
// Area 130: its choice's message words by the answer, and Effect_Spawn's third
// argument by a party list byte (three tables, twelve bytes apart).
constexpr std::uint32_t kArea130ChoiceMessages = 0x628CC4;   // Area130_ChoiceMessages, 2 words
constexpr std::uint32_t kArea130EffectArgsA = 0x627B50;      // Area130_EffectArgsA
constexpr std::uint32_t kArea130EffectArgsB = 0x627B5C;      // Area130_EffectArgsB
constexpr std::uint32_t kArea130EffectArgsC = 0x627B68;      // Area130_EffectArgsC
// Area 133: its handler 0's Effect_Spawn argument table (after area 132's
// descriptor), and its choice's byte pairs by the answer (six).
constexpr std::uint32_t kArea133EffectArgs = 0x62A5E0;       // Area133_EffectArgs
constexpr std::uint32_t kArea133ChoicePairs = 0x62B604;      // Area133_ChoicePairs, 6 byte pairs
// Area 134: two Effect_Spawn argument tables (after area 133's descriptor).
constexpr std::uint32_t kArea134EffectArgsA = 0x62B610;      // Area134_EffectArgsA
constexpr std::uint32_t kArea134EffectArgsB = 0x62B61C;      // Area134_EffectArgsB

// --- the callee nobody owns (raw address) ---

}  // namespace at
}  // namespace area_w3c
