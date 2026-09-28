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
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// A byte some choices set to 6 beside the message they open (area 37's too).
constexpr std::uint32_t kAnswerMark = 0x9398CF;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
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
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kLeverRow = 0x903FB0;
constexpr std::uint32_t kLeverNibble = 0x903FB1;
constexpr std::uint32_t kFlagRow = 0x929ED0;
// The party lists' first bytes (Cond_Flags' tail): the members in the party.
constexpr std::uint32_t kPartyList0 = 0x904062;
constexpr std::uint32_t kPartyList1 = 0x904063;
constexpr std::uint32_t kPartyList2 = 0x904064;
// The 32 key-item bytes (the key-item list, docs/char-stats.md).
constexpr std::uint32_t kKeyItems = 0x904554;
constexpr unsigned kKeyItemCount = 0x20;
// A sprite pointer after Camera_ShiftY (scena_sc7_callees.h's kFocusObject).
constexpr std::uint32_t kFocusObject = 0x903804;
// Cond_ByteFD's value on entry (mode_flow_callees.h's kLastZone).
constexpr std::uint32_t kEntryZone = 0x905E68;
// The leader's record (ObjTrio): its +8 (direction), +0x89; records 1, 2.
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kLeaderByte8 = 0x802D48;
constexpr std::uint32_t kLeaderByte89 = 0x802DC9;
constexpr std::uint32_t kPartyRecord1 = 0x802E8C;
constexpr std::uint32_t kPartyRecord2 = 0x802FD8;
// The cell step per direction: two signed bytes (x, z), 2 bytes apart
// (event_ops_callees.h's kCellDelta).
constexpr std::uint32_t kCellDelta = 0x66971C;
// Effect_Objects' stride (records of 0x80 bytes); DrawItems' half (0x48).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr std::uint32_t kDrawItemStride = 0x48;
// Named cells by address, for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;     // Effect_Objects
constexpr std::uint32_t kActiveMember = 0x9035A4;      // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;      // MoveScript_Object
constexpr std::uint32_t kInputHeld = 0x7E1BE8;         // Input_Held (read as a dword)
constexpr std::uint32_t kPacketNext = 0x7E0670;        // Gfx_PacketNext
constexpr std::uint32_t kOrigin = 0x7E0688;            // MapView_Origin: x, z words
constexpr std::uint32_t kViewColumn = 0x929F20;        // MapView_Column (s16), MapView_HeightScale, MapView_Row (s16)
constexpr std::uint32_t kViewCells = 0x904F20;         // MapView_Cells: 56 rows of 28 words
constexpr unsigned kViewCellCount = 56 * 28;
constexpr std::uint32_t kDrawItems = 0x905E80;         // DrawItems
constexpr std::uint32_t kTintRecords = 0x7E0700;       // MoveScript_TintRecords: 12 bytes a record
constexpr std::uint32_t kMapHeader = 0x8CB580;         // AreaMap_Header: the loaded area block
constexpr std::uint32_t kCellBase = 0x8CB5A4;          // AreaMap_CellBase (the low word used)
constexpr std::uint32_t kPatchBase = 0x8CB5A8;         // AreaMap_PatchBase (the low word used)

// --- the areas' tables (symbols.toml [[data]]) ---

constexpr std::uint32_t kArea38EffectBytes = 0x5F14E8;  // Area38_EffectBytes: s8 by a party-list byte
constexpr std::uint32_t kArea39States = 0x5F3EA4;       // Area39_States, 2
constexpr unsigned kArea39StateCount = 2;
constexpr std::uint32_t kArea39DriftSteps = 0x5F3EAC;   // Area39_DriftSteps: 16 s8
constexpr std::uint32_t kArea40Pattern = 0x5F4C1C;      // Area40_Pattern: 4 x 4 bytes, row by z
constexpr std::uint32_t kArea40PlaceOp = 0x5F4C2C;      // Area40_PlaceOp: EventOp_0x's op
constexpr std::uint32_t kArea40InputMap = 0x5F4C40;     // Area40_InputMap: 16 bytes
constexpr std::uint32_t kArea41GiveMessages = 0x5F5F54; // Area41_GiveMessages: 2 words by the s8 answer
constexpr std::uint32_t kArea41States = 0x5F5F58;       // Area41_States, 4
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
