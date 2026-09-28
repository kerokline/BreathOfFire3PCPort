// Internal to area_w0c.cpp and area_w0c_fuzz.cpp: the cells world 0's areas
// 27..29 and 32..37 touch that symbols.toml has no name for, the areas' own
// .data tables, and the one callee nobody owns yet (by its raw address, as
// round ten's rule for a function another group owns). Every other call is to
// a named function, ours, through the area harness (AH_CALL).
// docs/area_w0c.md.
#pragma once

#include <cstdint>

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
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
// The message word a choice handler leaves (0xFFFF: no new message).
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind, and a word timer.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's four counters (MoveScript_CounterOps, ops A0..AF).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter2 = 0x90384A;
constexpr std::uint32_t kCounter3 = 0x90384B;
// The byte after MoveScript_Var7 (the run step the scenes set beside it).
constexpr std::uint32_t kVar7Step = 0x8034E5;
// The story flags (Flags_Set / Flags_Test's bank) and the row pointer the
// chapters' flag tests take (Cond_Flags + 8 * chapter; docs/field-modes.md).
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kFlagRow = 0x929ED0;
// The pending area kind (docs/mode-flow.md: 0xFE only Draw_PassFlags 0).
constexpr std::uint32_t kPendingKind = 0x937F98;
// The pending place word the world map keeps (docs/worldmap_area.md).
constexpr std::uint32_t kPlace = 0x937F82;
// A byte area 37's choices set to 6 beside the message they open; no reader
// read this round.
constexpr std::uint32_t kAnswerMark = 0x9398CF;
// CharacterRecords (0x903A70) record 0, byte +9: 0x404820 compares it with 9.
constexpr std::uint32_t kRecord0Byte9 = 0x903A79;
// The leader's record (ObjTrio): byte +8, the step words +0x34 / +0x38, the
// dword +0x134.
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kLeaderByte8 = 0x802D48;
constexpr std::uint32_t kLeaderX = 0x802D74;
constexpr std::uint32_t kLeaderZ = 0x802D78;
// Effect_Objects' stride (records of 0x80 bytes).
constexpr std::uint32_t kEffectStride = 0x80;
// Named cells, by address for the fuzz's regions and the one subtraction
// (symbols.gen.h spells each name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;     // Effect_Objects
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;     // Sprite_Objects
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;   // Sprite_ObjectsExtra
constexpr std::uint32_t kActiveMember = 0x9035A4;      // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;      // MoveScript_Object
constexpr std::uint32_t kWaitWord = 0x66C810;          // MoveScript_WaitWordDA
constexpr std::uint32_t kPassFlags = 0x7E0918;         // Draw_PassFlags

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 32's two two-state machines, by Sprite_Current[4].
constexpr std::uint32_t kArea32StatesA = 0x5EF30C;   // Area32_StatesA, 2
constexpr std::uint32_t kArea32StatesB = 0x5EF314;   // Area32_StatesB, 2
constexpr unsigned kArea32States = 2;
// Area 33's world-map record 1, slots +8 (effect kind 0x16) and +4 (kind
// 0xE): their state tables by Sprite_Current[1], and what their starts read.
constexpr std::uint32_t kRecord08States = 0x5EF688;  // WorldMap33_Record08States, 3
constexpr unsigned kRecord08StateCount = 3;
constexpr std::uint32_t kRecord08Steps = 0x5EF694;   // WorldMap33_Record08Steps: 4 x (word x, word z) by +8
constexpr std::uint32_t kRecord08Anims = 0x5EF6A4;   // WorldMap33_Record08Anims: 4 x (animation, +0x2A byte) by +8
constexpr std::uint32_t kRecord04States = 0x5EF6AC;  // WorldMap33_Record04States, 2
constexpr unsigned kRecord04StateCount = 2;
constexpr std::uint32_t kRecord04Cells = 0x5EF338;   // WorldMap33_Record04Cells: 3 x (x, z, two bytes) by +0xB
// Area 36's effect (Effect_KindHandlers entry 0xB5): its states by +1.
constexpr std::uint32_t kArea36EffectStates = 0x5F0584;   // Area36_EffectStates, 2
constexpr unsigned kArea36EffectStateCount = 2;

// --- the one callee nobody owns this wave ---

// 0x4220D0 (0x27A bytes, area band, group AR3F's): takes a pointer to three
// dwords (a position); called by area 36's effect state 1 and from engine
// code at 0x475CE2 / 0x47EF32. Unread here: a raw address until its group
// names it.
constexpr std::uint32_t kPositionHook = bof3::addr::Area146_DrawGlowCylinder;

}  // namespace at
}  // namespace area_w0c
