// Internal to area_w3e.cpp and area_w3e_fuzz.cpp: the cells world 3's areas
// 136 and 139..142 touch that symbols.toml has no name for, and the areas' own
// .data tables. Every call of the band is to a named function (AH_CALL): the
// band calls no address nobody owns. docs/area_w3e.md.
#pragma once

#include <cstdint>

namespace area_w3e {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row; area 136's choices
// 2 and 3 read it signed) and the message word a choice handler leaves
// (0xFFFF: no new message).
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// The message box's flag byte (msgbox_callees.h's kFlags): area 141's tail
// sets its bit 0x80.
constexpr std::uint32_t kMsgFlags = 0x7DEE44;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state (read signed), a sub-kind, and a
// word timer.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
constexpr std::uint32_t kTailTimer = 0x9039F6;
// Field_ScriptFlags' low byte (area 141's tail ors 7 into it as a byte).
constexpr std::uint32_t kScriptFlagsLow = 0x9039A2;
// The movement script's counters (MoveScript_CounterOps, ops A0..AF): 0 and 3.
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
// A scratch word: the event ops' object index (EventOp_0x / EventOp_6x read
// it: scena_se.cpp), and where area 140's tail keeps Input_Held.
constexpr std::uint32_t kScratch850 = 0x903850;
// The story flags (Flags_Set / Flags_Test's bank) and the chapter's flag row
// pointer (Cond_Flags + 8 * chapter; docs/field-modes.md).
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kFlagRow = 0x929ED0;
// The party list: the character in slot 1 and slot 2 (0x904062 is slot 0).
constexpr std::uint32_t kPartyList1 = 0x904063;
constexpr std::uint32_t kPartyList2 = 0x904064;
// The leader's record (ObjTrio): its direction +8, +9 (a timed step's
// frames, non-zero while moving), +0x48, +0x89, +0x137; and the records'
// stride.
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kLeaderPose = 0x802D48;
constexpr std::uint32_t kLeaderSteps = 0x802D49;
constexpr std::uint32_t kLeader48 = 0x802D88;
constexpr std::uint32_t kLeader89 = 0x802DC9;
constexpr std::uint32_t kLeader137 = 0x802E77;
constexpr std::uint32_t kPartyStride = 0x14C;
// The first of Sprite_ObjectsExtra's four, its +0x83 (area 136's init).
constexpr std::uint32_t kExtra0_83 = 0x802083;
// The byte after Field_Kind2X (mode_flow_callees.h's kLastZone: Cond_ByteFD's
// value on entry); area 136's init tests it for 1 and 4.
constexpr std::uint32_t kLastZone = 0x905E68;
// The action button (the second of the button map's words; event_ops_callees.h's
// kButtonCheck): area 140's handler 1 tests Input_Pressed against it.
constexpr std::uint32_t kButtonMap6 = 0x90358C;
// A u16 whose low byte area 136's handler 6 tests for 0x62..0x66
// (scena_sc12_callees.h's kTile).
constexpr std::uint32_t kTile = 0x939A00;
// The direction steps (Field_DirectionSteps: dword pairs x, z by direction &
// 7) and the cell steps (two signed bytes a direction; event_ops_callees.h's
// kCellDelta), read in place.
constexpr std::uint32_t kDirectionSteps = 0x6697B0;
constexpr std::uint32_t kCellDelta = 0x66971C;
// Effect_Objects' stride (records of 0x80 bytes, 20 of them) and
// Sprite_Objects' (0xA4, 30).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr unsigned kObjectCount = 30;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;     // Effect_Objects
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;     // Sprite_Objects
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;   // Sprite_ObjectsExtra
constexpr std::uint32_t kSpriteKind2 = 0x7E0940;       // Sprite_Kind2 (0xA4 bytes)
constexpr std::uint32_t kActiveMember = 0x9035A4;      // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;      // MoveScript_Object
constexpr std::uint32_t kCameraShiftY = 0x903802;      // Camera_ShiftY
constexpr std::uint32_t kInputHeld = 0x7E1BE8;         // Input_Held, then Input_Pressed at +4
constexpr std::uint32_t kKind2Hold = 0x929F12;         // Field_Kind2Hold
constexpr std::uint32_t kCondByteFE = 0x905E20;        // Cond_ByteFE

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 136: the message words its choices 2 and 3 open by the answer (two
// each), and the limits (bytes, << 16) its handler 0 compares the z ahead
// with, by the sub-kind; the effects by character its handlers 2..4, 7, 8
// spawn (bytes by the party list's character: two tables in .data before
// area 136's descriptor, read in place, the index unchecked).
constexpr std::uint32_t kArea136Messages2 = 0x62EA74;   // Area136_ChoiceMessages2, 2 words
constexpr std::uint32_t kArea136Messages3 = 0x62EA78;   // Area136_ChoiceMessages3, 2 words
constexpr std::uint32_t kArea136Limits = 0x62EA7C;      // Area136_ZLimits, bytes by the sub-kind
constexpr std::uint32_t kEffectsByCharA = 0x62DAA0;     // Area136_EffectByCharA
constexpr std::uint32_t kEffectsByCharB = 0x62DAAC;     // Area136_EffectByCharB
// Area 139: its cell hook's two entries of 4 bytes (x, z, direction, flag).
constexpr std::uint32_t kArea139Cells = 0x62EDBC;       // Area139_CellEntries
constexpr unsigned kArea139CellCount = 2;
// Area 140: its cell hook's eight entries of 6 bytes (x, z, direction, flag,
// effect, rectangle), the rectangles' cells (x, z words; entry 0 unused), the
// animation pairs by quadrant its handler 1 sets, the high-nibble remap its
// tail applies to Input_Held, and effect kind 0x71's three states.
constexpr std::uint32_t kArea140Cells = 0x62EFF8;       // Area140_CellEntries
constexpr unsigned kArea140CellCount = 8;
constexpr unsigned kArea140CellStride = 6;
constexpr std::uint32_t kArea140Rects = 0x62F028;       // Area140_CellRects, 4 x (x, z) words
constexpr std::uint32_t kArea140Poses = 0x62EFDC;       // Area140_ActPoses, 4 x (animation, +0x2A)
constexpr std::uint32_t kArea140Remap = 0x62EFE4;       // Area140_ButtonRemap, 16 bytes
constexpr std::uint32_t kArea140EffectStates = 0x62F038;   // Area140_Effect71States, 3
constexpr unsigned kArea140EffectStateCount = 3;
// Area 141: nine five-byte cell runs (x, z, count | 0x80 for a run along x,
// the value on, the value off) its paint takes, two two-state tables its
// handlers 2 and 3 jump through by Sprite_Current[4], its cell hook's two
// entries of 4 bytes (x, z, direction, tail state), and the event scripts its
// placers hand to EventOp_6x / EventOp_0x.
constexpr std::uint32_t kArea141Runs = 0x62F5E8;        // Area141_CellRuns, 9 x 5 bytes
constexpr std::uint32_t kArea141ShadeDownStates = 0x62F618;   // Area141_ShadeDownStates, 2
constexpr std::uint32_t kArea141ShadeUpStates = 0x62F620;     // Area141_ShadeUpStates, 2
constexpr unsigned kArea141StateCount = 2;
// The shade-down dispatcher's reach: its table is followed by the shade-up
// table, so its indices 2 and 3 run the shade-up states (code, not data); 4
// and on read Area141_CellEntries.
constexpr unsigned kArea141ShadeDownReach = 4;
constexpr std::uint32_t kArea141Cells = 0x62F628;       // Area141_CellEntries
constexpr unsigned kArea141CellCount = 2;
constexpr std::uint32_t kArea141Script630 = 0x62F630;
constexpr std::uint32_t kArea141Script640 = 0x62F640;
constexpr std::uint32_t kArea141Script650 = 0x62F650;
constexpr std::uint32_t kArea141Script660 = 0x62F660;
constexpr std::uint32_t kArea141Script670 = 0x62F670;
constexpr std::uint32_t kArea141Script680 = 0x62F680;
constexpr std::uint32_t kArea141Script691 = 0x62F691;
constexpr std::uint32_t kArea141Script6A8 = 0x62F6A8;
// Area 142: its handler 0's two states (by Sprite_Current[4]).
constexpr std::uint32_t kArea142States = 0x62FCDC;      // Area142_States, 2
constexpr unsigned kArea142StateCount = 2;

// The run records area 141 paints (kArea141Runs + 5 * i).
constexpr std::uint32_t Run(unsigned i) { return kArea141Runs + 5 * i; }

}  // namespace at
}  // namespace area_w3e
