// Internal to area_w0c.cpp and area_w0c_fuzz.cpp: the cells world 0's areas
// 27..29 and 32..37 touch that symbols.toml has no name for, the areas' own
// .data tables, and the one callee nobody owns yet (by its raw address, as
// round ten's rule for a function another group owns). Every other call is to
// a named function, ours, through the area harness (AH_CALL).
// docs/area_w0c.md.
#pragma once

#include <cstdint>

#include "common/image_base.h"

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w0c {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice: the cursor's row, read as a signed
// byte by some handlers and as a byte by others (docs/item-use.md section 5).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
// The message word a choice handler leaves (0xFFFF: no new message).
constexpr ::bof3::image::Va kMessage{0x7DEE48};
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind, and a word timer.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
constexpr ::bof3::image::Va kTailTimer{0x9039F6};
// The movement script's four counters (MoveScript_CounterOps, ops A0..AF).
constexpr ::bof3::image::Va kCounter0{0x903848};
constexpr ::bof3::image::Va kCounter1{0x903849};
constexpr ::bof3::image::Va kCounter2{0x90384A};
constexpr ::bof3::image::Va kCounter3{0x90384B};
// The byte after MoveScript_Var7 (the run step the scenes set beside it).
// kVar7Step: now MoveScript_PartyRecords[0x65] (symbols.toml).
// The story flags (Flags_Set / Flags_Test's bank) and the row pointer the
// chapters' flag tests take (Cond_Flags + 8 * chapter; docs/field-modes.md).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kFlagRow: now Chapter_FlagRow (symbols.toml).
// The pending area kind (docs/mode-flow.md: 0xFE only Draw_PassFlags 0).
// kPendingKind: now Area_EnterTransition (symbols.toml).
// The pending place word the world map keeps (docs/worldmap_area.md).
constexpr ::bof3::image::Va kPlace{0x937F82};
// A byte area 37's choices set to 6 beside the message they open; no reader
// read this round.
// kAnswerMark: now AreaW0c_AnswerMark (symbols.toml).
// CharacterRecords (0x903A70) record 0, byte +9: 0x404820 compares it with 9.
constexpr ::bof3::image::Va kRecord0Byte9{0x903A79};
// The leader's record (ObjTrio): byte +8, the step words +0x34 / +0x38, the
// dword +0x134.
constexpr ::bof3::image::Va kLeader{0x802D40};
// kLeaderByte8: now ObjTrio[0].direction (symbols.toml).
// kLeaderX: now ObjTrio[0].x (symbols.toml).
// kLeaderZ: now ObjTrio[0].z (symbols.toml).
// Effect_Objects' stride (records of 0x80 bytes).
constexpr std::uint32_t kEffectStride = 0x80;
// Named cells, by address for the fuzz's regions and the one subtraction
// (symbols.gen.h spells each name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
// kSpriteObjects: now Sprite_Objects (symbols.toml). Sprite_Objects
// kSpriteObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kWaitWord: now Transition_Busy (symbols.toml). Transition_Busy
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 32's two two-state machines, by Sprite_Current[4].
// kArea32StatesA: now Area32_StatesA (symbols.toml). Area32_StatesA, 2
// kArea32StatesB: now Area32_StatesB (symbols.toml). Area32_StatesB, 2
constexpr unsigned kArea32States = 2;
// Area 33's world-map record 1, slots +8 (effect kind 0x16) and +4 (kind
// 0xE): their state tables by Sprite_Current[1], and what their starts read.
// kRecord08States: now WorldMap33_Record08States (symbols.toml). WorldMap33_Record08States, 3
constexpr unsigned kRecord08StateCount = 3;
// kRecord08Steps: now WorldMap33_Record08Steps (symbols.toml). WorldMap33_Record08Steps: 4 x (word x, word z) by +8
// kRecord08Anims: now WorldMap33_Record08Anims (symbols.toml). WorldMap33_Record08Anims: 4 x (animation, +0x2A byte) by +8
// kRecord04States: now WorldMap33_Record04States (symbols.toml). WorldMap33_Record04States, 2
constexpr unsigned kRecord04StateCount = 2;
// kRecord04Cells: now WorldMap33_Record04Cells (symbols.toml). WorldMap33_Record04Cells: 3 x (x, z, two bytes) by +0xB
// Area 36's effect (Effect_KindHandlers entry 0xB5): its states by +1.
// kArea36EffectStates: now Area36_EffectStates (symbols.toml). Area36_EffectStates, 2
constexpr unsigned kArea36EffectStateCount = 2;

// --- the one callee nobody owns this wave ---

// 0x4220D0 (0x27A bytes, area band, group AR3F's): takes a pointer to three
// dwords (a position); called by area 36's effect state 1 and from engine
// code at 0x475CE2 / 0x47EF32. Unread here: a raw address until its group
// names it.
constexpr std::uint32_t kPositionHook = bof3::addr::Area146_DrawGlowCylinder;

}  // namespace at
}  // namespace area_w0c
