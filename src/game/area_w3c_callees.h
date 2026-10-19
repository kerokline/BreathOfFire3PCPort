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
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// The focus object: the object whose talk ran last (MoveScript_SetTurnTarget
// stores Field_ActiveMember there; area_w1a_callees.h's kFocusObject).
constexpr std::uint32_t kFocusObject = 0x903804;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state and a sub-kind.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
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
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// The party lists' first bytes (a member's character id; the spawns index
// their tables by it).
// kPartyList0: now Cond_Flags[0xd2] (symbols.toml).
// kPartyList1: now Cond_Flags[0xd3] (symbols.toml).
// kPartyList2: now Cond_Flags[0xd4] (symbols.toml).
// The party's records (ObjTrio: three of 0x14C); the leader's byte +0x89
// (MoveScript_EffectState's index, area_w2d's reading) and its dword +0x134
// (Field_EdgeBits is set from it less 5, area_w1f's kLeaderZone).
constexpr std::uint32_t kParty = 0x802D40;
constexpr std::uint32_t kPartyStride = 0x14C;
// kLeader89: now ObjTrio[0x89] (symbols.toml).
constexpr std::uint32_t kLeaderZone = 0x802E74;
// The previous area (u16): the inits test it (area_w2c_callees.h's kLastArea).
constexpr std::uint32_t kLastArea = 0x802290;
// A byte before Cond_Flags tail kind 63 picks one of six items by (no reader
// of it is named in this repo).
// kItemPick: now AreaW3c_ItemPick (symbols.toml).
// Text_Records + 0x2F: tail kind 63 zeroes it before its message.
constexpr std::uint32_t kTextRecords2F = 0x904D0F;
// A byte area 131's tail kind 34 zeroes before it changes area
// (menu_lists_callees.h's kCampFlag, a hypothesis there).
constexpr std::uint32_t kCampFlag = 0x904152;
// A byte area 133's choice zeroes (scena_sc6 / sc12 / shop_states read it as
// a load or save wait byte).
// kLoad0F: now AreaW3c_Load0F (symbols.toml).
// Effect_Objects' stride (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;     // Sprite_Objects
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kCameraShiftX: now Camera_ShiftX (symbols.toml). Camera_ShiftX, then Camera_ShiftY 0x903802
constexpr std::uint32_t kCameraShiftY = 0x903802;      // Camera_ShiftY
// kCondByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
constexpr std::uint32_t kTextRecords = 0x904CE0;       // Text_Records
constexpr std::uint32_t kMusicTrack = 0x904131;        // Music_Track
// kPatchBase: now AreaMap_PatchBase (symbols.toml). AreaMap_PatchBase
// kMapHeader: now AreaMap_Header (symbols.toml). AreaMap_Header
constexpr std::uint32_t kScriptFlags = 0x9039A2;       // Field_ScriptFlags (u16)
constexpr std::uint32_t kEdgeBits = 0x905B80;          // Field_EdgeBits (u16)

// --- the areas' tables (symbols.toml [[data]]) ---

// Areas 124 and 125's inits: eight cells (x, z byte pairs) and eight weights
// (summing to 64 in the image) each, as areas 72 and 73's.
// kArea124Cells: now Area124_Cells (symbols.toml). Area124_Cells, 8 pairs
// kArea124Weights: now Area124_Weights (symbols.toml). Area124_Weights, 8
// kArea125Cells: now Area125_Cells (symbols.toml). Area125_Cells, 8 pairs
// kArea125Weights: now Area125_Weights (symbols.toml). Area125_Weights, 8
// Area 130: its choice's message words by the answer, and Effect_Spawn's third
// argument by a party list byte (three tables, twelve bytes apart).
// kArea130ChoiceMessages: now Area130_ChoiceMessages (symbols.toml). Area130_ChoiceMessages, 2 words
// kArea130EffectArgsA: now Area130_EffectArgsA (symbols.toml). Area130_EffectArgsA
// kArea130EffectArgsB: now Area130_EffectArgsB (symbols.toml). Area130_EffectArgsB
// kArea130EffectArgsC: now Area130_EffectArgsC (symbols.toml). Area130_EffectArgsC
// Area 133: its handler 0's Effect_Spawn argument table (after area 132's
// descriptor), and its choice's byte pairs by the answer (six).
// kArea133EffectArgs: now Area133_EffectArgs (symbols.toml). Area133_EffectArgs
// kArea133ChoicePairs: now Area133_ChoicePairs (symbols.toml). Area133_ChoicePairs, 6 byte pairs
// Area 134: two Effect_Spawn argument tables (after area 133's descriptor).
// kArea134EffectArgsA: now Area134_EffectArgsA (symbols.toml). Area134_EffectArgsA
// kArea134EffectArgsB: now Area134_EffectArgsB (symbols.toml). Area134_EffectArgsB

// --- the callee nobody owns (raw address) ---

}  // namespace at
}  // namespace area_w3c
