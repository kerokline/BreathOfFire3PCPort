// Internal to area_w3e.cpp and area_w3e_fuzz.cpp: the cells world 3's areas
// 136 and 139..142 touch that symbols.toml has no name for, and the areas' own
// .data tables. Every call of the band is to a named function (AH_CALL): the
// band calls no address nobody owns. docs/area_w3e.md.
#pragma once

#include <cstdint>

#include "common/image_base.h"

namespace area_w3e {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row; area 136's choices
// 2 and 3 read it signed) and the message word a choice handler leaves
// (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr ::bof3::image::Va kMessage{0x7DEE48};
// The message box's flag byte (msgbox_callees.h's kFlags): area 141's tail
// sets its bit 0x80.
constexpr ::bof3::image::Va kMsgFlags{0x7DEE44};
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state (read signed), a sub-kind, and a
// word timer.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
constexpr ::bof3::image::Va kTailTimer{0x9039F6};
// Field_ScriptFlags' low byte (area 141's tail ors 7 into it as a byte).
// kScriptFlagsLow: now Field_ScriptFlags (symbols.toml).
// The movement script's counters (MoveScript_CounterOps, ops A0..AF): 0 and 3.
constexpr ::bof3::image::Va kCounter0{0x903848};
constexpr ::bof3::image::Va kCounter3{0x90384B};
// A scratch word: the event ops' object index (EventOp_0x / EventOp_6x read
// it: scena_se.cpp), and where area 140's tail keeps Input_Held.
constexpr ::bof3::image::Va kScratch850{SharedScratch_addr};
// The story flags (Flags_Set / Flags_Test's bank) and the chapter's flag row
// pointer (Cond_Flags + 8 * chapter; docs/field-modes.md).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kFlagRow: now Chapter_FlagRow (symbols.toml).
// The party list: the character in slot 1 and slot 2 (0x904062 is slot 0).
// kPartyList1: now Cond_Flags[0xd3] (symbols.toml).
// kPartyList2: now Cond_Flags[0xd4] (symbols.toml).
// The leader's record (ObjTrio): its direction +8, +9 (a timed step's
// frames, non-zero while moving), +0x48, +0x89, +0x137; and the records'
// stride.
// kLeader: now ObjTrio (symbols.toml).
// kLeaderPose: now ObjTrio[0].direction (symbols.toml).
// kLeaderSteps: now ObjTrio[0].step_frames (symbols.toml).
constexpr ::bof3::image::Va kLeader48{0x802D88};
// kLeader89: now ObjTrio[0].char_id (symbols.toml).
// kLeader137: now ObjTrio_addr + 0x137 (symbols.toml).
constexpr std::uint32_t kPartyStride = 0x14C;
// The first of Sprite_ObjectsExtra's four, its +0x83 (area 136's init).
// kExtra0_83: now Sprite_ObjectsExtra[0].area_script (symbols.toml).
// The byte after Field_Kind2X (mode_flow_callees.h's kLastZone: Cond_ByteFD's
// value on entry); area 136's init tests it for 1 and 4.
constexpr ::bof3::image::Va kLastZone{0x905E68};
// The action button (the second of the button map's words; event_ops_callees.h's
// kButtonCheck): area 140's handler 1 tests Input_Pressed against it.
// kButtonMap6: now Gfx_UnpackScratch[0x1f00c] (symbols.toml).
// A u16 whose low byte area 136's handler 6 tests for 0x62..0x66
// (scena_sc12_callees.h's kTile).
constexpr ::bof3::image::Va kTile{0x939A00};
// The direction steps (Field_DirectionSteps: dword pairs x, z by direction &
// 7) and the cell steps (two signed bytes a direction; event_ops_callees.h's
// kCellDelta), read in place.
// kDirectionSteps: now Field_DirectionSteps (symbols.toml).
constexpr ::bof3::image::Va kCellDelta{0x66971C};
// Effect_Objects' stride (records of 0x80 bytes, 20 of them) and
// Sprite_Objects' (0xA4, 30).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr unsigned kObjectCount = 30;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
constexpr ::bof3::image::Va kSpriteObjects{0x7DEE80};     // Sprite_Objects
// kSpriteObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
// kSpriteKind2: now Sprite_Kind2 (symbols.toml). Sprite_Kind2 (0xA4 bytes)
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kCameraShiftY: now Camera_ShiftY (symbols.toml). Camera_ShiftY
// kInputHeld: now Input_Held (symbols.toml). Input_Held, then Input_Pressed at +4
// kKind2Hold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
// kCondByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 136: the message words its choices 2 and 3 open by the answer (two
// each), and the limits (bytes, << 16) its handler 0 compares the z ahead
// with, by the sub-kind; the effects by character its handlers 2..4, 7, 8
// spawn (bytes by the party list's character: two tables in .data before
// area 136's descriptor, read in place, the index unchecked).
// kArea136Messages2: now Area136_ChoiceMessages2 (symbols.toml). Area136_ChoiceMessages2, 2 words
// kArea136Messages3: now Area136_ChoiceMessages3 (symbols.toml). Area136_ChoiceMessages3, 2 words
// kArea136Limits: now Area136_ZLimits (symbols.toml). Area136_ZLimits, bytes by the sub-kind
// kEffectsByCharA: now Area136_EffectByCharA (symbols.toml). Area136_EffectByCharA
// kEffectsByCharB: now Area136_EffectByCharB (symbols.toml). Area136_EffectByCharB
// Area 139: its cell hook's two entries of 4 bytes (x, z, direction, flag).
// kArea139Cells: now Area139_CellEntries (symbols.toml). Area139_CellEntries
constexpr unsigned kArea139CellCount = 2;
// Area 140: its cell hook's eight entries of 6 bytes (x, z, direction, flag,
// effect, rectangle), the rectangles' cells (x, z words; entry 0 unused), the
// animation pairs by quadrant its handler 1 sets, the high-nibble remap its
// tail applies to Input_Held, and effect kind 0x71's three states.
// kArea140Cells: now Area140_CellEntries (symbols.toml). Area140_CellEntries
constexpr unsigned kArea140CellCount = 8;
constexpr unsigned kArea140CellStride = 6;
// kArea140Rects: now Area140_CellRects (symbols.toml). Area140_CellRects, 4 x (x, z) words
// kArea140Poses: now Area140_ActPoses (symbols.toml). Area140_ActPoses, 4 x (animation, +0x2A)
// kArea140Remap: now Area140_ButtonRemap (symbols.toml). Area140_ButtonRemap, 16 bytes
// kArea140EffectStates: now Area140_Effect71States (symbols.toml). Area140_Effect71States, 3
constexpr unsigned kArea140EffectStateCount = 3;
// Area 141: nine five-byte cell runs (x, z, count | 0x80 for a run along x,
// the value on, the value off) its paint takes, two two-state tables its
// handlers 2 and 3 jump through by Sprite_Current[4], its cell hook's two
// entries of 4 bytes (x, z, direction, tail state), and the event scripts its
// placers hand to EventOp_6x / EventOp_0x.
constexpr ::bof3::image::Va kArea141Runs{0x62F5E8};        // Area141_CellRuns, 9 x 5 bytes
// kArea141ShadeDownStates: now Area141_ShadeDownStates (symbols.toml). Area141_ShadeDownStates, 2
// kArea141ShadeUpStates: now Area141_ShadeUpStates (symbols.toml). Area141_ShadeUpStates, 2
constexpr unsigned kArea141StateCount = 2;
// The shade-down dispatcher's reach: its table is followed by the shade-up
// table, so its indices 2 and 3 run the shade-up states (code, not data); 4
// and on read Area141_CellEntries.
constexpr unsigned kArea141ShadeDownReach = 4;
// kArea141Cells: now Area141_CellEntries (symbols.toml). Area141_CellEntries
constexpr unsigned kArea141CellCount = 2;
constexpr ::bof3::image::Va kArea141Script630{0x62F630};
constexpr ::bof3::image::Va kArea141Script640{0x62F640};
constexpr ::bof3::image::Va kArea141Script650{0x62F650};
constexpr ::bof3::image::Va kArea141Script660{0x62F660};
constexpr ::bof3::image::Va kArea141Script670{0x62F670};
constexpr ::bof3::image::Va kArea141Script680{0x62F680};
constexpr ::bof3::image::Va kArea141Script691{0x62F691};
constexpr ::bof3::image::Va kArea141Script6A8{0x62F6A8};
// Area 142: its handler 0's two states (by Sprite_Current[4]).
// kArea142States: now Area142_States (symbols.toml). Area142_States, 2
constexpr unsigned kArea142StateCount = 2;

// The run records area 141 paints (kArea141Runs + 5 * i).
inline std::uint32_t Run(unsigned i) { return ::bof3::Img(kArea141Runs.v) + 5 * i; }   // a place

}  // namespace at
}  // namespace area_w3e
