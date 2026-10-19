// Internal to area_w1a.cpp and area_w1a_fuzz.cpp: the cells world 1's areas
// 38..41 touch that symbols.toml has no name for, the areas' own .data tables,
// and the three callees nobody owns yet (group SX's this wave: raw addresses,
// as round ten's rule for a function another group owns). Every other call is
// to a named function, ours, through the area harness (AH_CALL).
// docs/area_w1a.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w1a {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (docs/item-use.md section 5) and the
// message word a choice handler leaves (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// A byte some choices set to 6 beside the message they open (area 37's too).
// kAnswerMark: now AreaW0c_AnswerMark (symbols.toml).
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
// Field_ScriptFlags' high byte (the original ors 0x10 into it as a byte).
constexpr std::uint32_t kScriptFlagsHigh = 0x9039A3;
// The movement script's counters 0 and 1 (MoveScript_CounterOps).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
// The scratch word the field's event ops take an object index from
// (docs/event-ops.md); area 40 stores a free index and a button word there.
constexpr std::uint32_t kScratchWord = 0x903850;
// The byte after MoveScript_Var7 (area 40's choice 0 sets it).
constexpr std::uint32_t kVar7Step = 0x8034E5;
// The story flags (Flags_Set / Flags_Test's bank), the lever flags' row
// (Cond_Flags + 0x20, flags 8..0xB are the low nibble of 0x903FB1), and the
// row pointer the chapters' flag tests take (docs/field-modes.md).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kLeverRow: now Cond_Flags[0x20] (symbols.toml).
// kLeverNibble: now Cond_Flags[0x21] (symbols.toml).
// kFlagRow: now AreaW0c_FlagRow (symbols.toml).
// The party lists' first bytes (Cond_Flags' tail): the members in the party.
// kPartyList0: now Cond_Flags[0xd2] (symbols.toml).
// kPartyList1: now Cond_Flags[0xd3] (symbols.toml).
// kPartyList2: now Cond_Flags[0xd4] (symbols.toml).
// The 32 key-item bytes (the key-item list, docs/char-stats.md).
constexpr std::uint32_t kKeyItems = 0x904554;
constexpr unsigned kKeyItemCount = 0x20;
// A sprite pointer after Camera_ShiftY (scena_sc7_callees.h's kFocusObject).
constexpr std::uint32_t kFocusObject = 0x903804;
// Cond_ByteFD's value on entry (mode_flow_callees.h's kLastZone).
constexpr std::uint32_t kEntryZone = 0x905E68;
// The leader's record (ObjTrio): its +8 (direction), +0x89; records 1, 2.
constexpr std::uint32_t kLeader = 0x802D40;
// kLeaderByte8: now ObjTrio[8] (symbols.toml).
// kLeaderByte89: now ObjTrio[0x89] (symbols.toml).
// kPartyRecord1: now Field_Members[4] (symbols.toml).
// kPartyRecord2: now ObjTrio[0x298] (symbols.toml).
// The cell step per direction: two signed bytes (x, z), 2 bytes apart
// (event_ops_callees.h's kCellDelta).
constexpr std::uint32_t kCellDelta = 0x66971C;
// Effect_Objects' stride (records of 0x80 bytes); DrawItems' half (0x48).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr std::uint32_t kDrawItemStride = 0x48;
// Named cells by address, for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kInputHeld: now Input_Held (symbols.toml). Input_Held (read as a dword)
// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
// kOrigin: now MapView_Origin (symbols.toml). MapView_Origin: x, z words
// kViewColumn: now MapView_Column (symbols.toml). MapView_Column (s16), MapView_HeightScale, MapView_Row (s16)
// kViewCells: now MapView_Cells (symbols.toml). MapView_Cells: 56 rows of 28 words
constexpr unsigned kViewCellCount = 56 * 28;
// kDrawItems: now DrawItems (symbols.toml). DrawItems
// kTintRecords: now MoveScript_TintRecords (symbols.toml). MoveScript_TintRecords: 12 bytes a record
// kMapHeader: now AreaMap_Header (symbols.toml). AreaMap_Header: the loaded area block
// kCellBase: now AreaMap_CellBase (symbols.toml). AreaMap_CellBase (the low word used)
// kPatchBase: now AreaMap_PatchBase (symbols.toml). AreaMap_PatchBase (the low word used)

// --- the areas' tables (symbols.toml [[data]]) ---

constexpr std::uint32_t kArea38EffectBytes = 0x5F14E8;  // Area38_EffectBytes: s8 by a party-list byte
// kArea39States: now Area39_States (symbols.toml). Area39_States, 2
constexpr unsigned kArea39StateCount = 2;
// kArea39DriftSteps: now Area39_DriftSteps (symbols.toml). Area39_DriftSteps: 16 s8
// kArea40Pattern: now Area40_Pattern (symbols.toml). Area40_Pattern: 4 x 4 bytes, row by z
// kArea40PlaceOp: now Area40_PlaceOp (symbols.toml). Area40_PlaceOp: EventOp_0x's op
// kArea40InputMap: now Area40_InputMap (symbols.toml). Area40_InputMap: 16 bytes
// kArea41GiveMessages: now Area41_GiveMessages (symbols.toml). Area41_GiveMessages: 2 words by the s8 answer
// kArea41States: now Area41_States (symbols.toml). Area41_States, 4
constexpr unsigned kArea41StateCount = 4;
constexpr std::uint32_t kArea41TriggerBase = 0x5F5F2D;  // + object +0x86: Area41_TriggerFlags 0x5F5F68 at ids 59, 60

// Area 40's grid: cells x 0x94..0x97 by z 0x20..0x23; its gate x 0x41..0x44
// by z 3..9.
constexpr unsigned kGridX = 0x94, kGridZ = 0x20, kGridSide = 4;

// --- the callees nobody owns this wave (group SX's) ---

// 0x57CD90: a free Sprite_Objects index 0..0x1D in al, 0xFF none
// (docs/scena_sc3.md).
constexpr std::uint32_t kFreeObject = bof3::addr::Sprite_FindFree;
// 0x591900 (u8 id): the id into the first free of the 32 key-item bytes
// (docs/scena_sc6.md).
constexpr std::uint32_t kKeyItemAdd = bof3::addr::KeyItem_Add;
// 0x591B60 (category, item, count, 0): the inventory take (round10 doc
// section 7).
constexpr std::uint32_t kInventoryTake = bof3::addr::Inventory_Remove;

}  // namespace at
}  // namespace area_w1a
