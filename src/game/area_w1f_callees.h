// Internal to area_w1f.cpp and area_w1f_fuzz.cpp: the cells world 1's areas
// 68..75 touch that symbols.toml has no name for, the areas' own .data tables,
// and the one callee of theirs nobody owns yet by its raw address (none: every
// callee of the band is named). docs/area_w1f.md.
#pragma once

#include <cstdint>

#include "game/rdata_consts.h"

namespace area_w1f {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row; read as a signed
// byte by the table choices, as a byte by the others) and the message word a
// choice handler leaves (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// A byte some choices set to 6 beside the message they open (areas 3, 37, 41
// and 50's choices do the same); no reader read this round.
// kAnswerMark: now AreaW0c_AnswerMark (symbols.toml).
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind, and a word timer.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's counters 0 and 3 (MoveScript_CounterOps, ops A0..AF).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
// The scratch word 0x903850: an effect or sprite slot a handler keeps there.
constexpr std::uint32_t kScratch = SharedScratch_addr;
// The object pointer after Camera_ShiftY (area_w1a_callees.h's kFocusObject):
// area 68's object trigger 25 reads the object through it.
constexpr std::uint32_t kFocusObject = 0x903804;
// MoveScript_Var7 and the byte after it (the run step).
constexpr std::uint32_t kVar7 = 0x8034E4;
// kVar7Step: now MoveScript_PartyRecords[0x65] (symbols.toml).
// The story flags (Flags_Set's bank) and the Cond_Flags row 10 (0x903F90 +
// 8 * 10) area 75 sets, clears and tests.
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kCondRow10: now Cond_Flags[0x50] (symbols.toml).
// Cond_Flags row 7 (0x903F90 + 8 * 7): area 74's handler 0 toggles its flags
// 5 and 6.
// kCondRow7: now Cond_Flags[0x38] (symbols.toml).
// The story-flag byte 0x90409C (flags 0x360..0x367): area 74's choice tests
// its bit 0x20 (flag 0x365).
// kFlagByte9C: now AreaW1f_FlagByte9C (symbols.toml).
// The three party lists' first bytes (0x904062..0x904064).
// kPartyList0: now Cond_Flags[0xd2] (symbols.toml).
// The current music track (a byte Music_Play returns at once on; the
// chapters' kMusicCurrent / kAreaTrack).
constexpr std::uint32_t kMusicCurrent = 0x904CD0;
// The leader's record (ObjTrio): byte +8 (its direction), +0x2C (a dword the
// push toggle plays a sound by), +0x4A, +0x89, the zone counter +0x134 (areas
// 72 / 73 set Field_EdgeBits to it less 5), the word +0x12E.
// kLeader: now ObjTrio (symbols.toml).
// kLeaderDir: now ObjTrio[0].direction (symbols.toml).
// kLeaderSound: now ObjTrio[0].column (symbols.toml).
// kLeader4A: now ObjTrio[0].script_hold (symbols.toml).
// kLeaderByte89: now ObjTrio[0].char_id (symbols.toml).
constexpr std::uint32_t kLeaderZone = 0x802E74;
// kLeaderWord12E: now ObjTrio_addr + 0x12e (symbols.toml).
constexpr std::uint32_t kPartyStride = 0x14C;
// Field objects (Sprite_Objects, 30 of 0xA4 bytes) and effect records
// (Effect_Objects, 0x80 bytes).
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr std::uint32_t kEffectStride = 0x80;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
// kSpriteObjects: now Sprite_Objects (symbols.toml). Sprite_Objects
// kSpriteObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kWaitWord: now Transition_Busy (symbols.toml). Transition_Busy
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
constexpr std::uint32_t kKind2Z = 0x905E60;            // Field_Kind2Z
// kInputPressed: now Input_Pressed (symbols.toml). Input_Pressed
// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
constexpr std::uint32_t kCondByteFA = 0x8034E0;        // Cond_ByteFA
// The text buffer area 75's counters are printed into (area 42's timer
// prints into the same, docs/area_w1b.md).
constexpr std::uint32_t kTextBuffer = 0x904BA0;

// --- area 68's tables (symbols.toml [[data]]) ---

// The two table choices' message words, two each, by the s8 answer.
// kArea68ChoiceMessages: now Area68_ChoiceMessages (symbols.toml). Area68_ChoiceMessages, 4 words
// The effect kinds by party member (Effect_Spawn's third argument): kind 4's
// spawns read the first, kinds 1 and 3's the second (12 bytes on).
// kArea68EffectKinds4: now Area68_EffectKinds4 (symbols.toml). Area68_EffectKinds4
// kArea68EffectKinds: now Area68_EffectKinds (symbols.toml). Area68_EffectKinds
// Handlers 2 and 3's two-state tables (by the running object's +4) and the
// 16 signed steps each glide reads by its count's low nibble.
// kArea68GlideStatesA: now Area68_GlideStatesA (symbols.toml). Area68_GlideStatesA, 2
// kArea68GlideStepsA: now Area68_GlideStepsA (symbols.toml). Area68_GlideStepsA, 16 bytes
// kArea68GlideStatesB: now Area68_GlideStatesB (symbols.toml). Area68_GlideStatesB, 2
// kArea68GlideStepsB: now Area68_GlideStepsB (symbols.toml). Area68_GlideStepsB, 16 bytes
// --- area 69's ---
// kArea69GlideStates: now Area69_GlideStates (symbols.toml). Area69_GlideStates, 2
// kArea69GlideSteps: now Area69_GlideSteps (symbols.toml). Area69_GlideSteps, 16 bytes
// --- areas 72 and 73's: eight (x, z) cells of two bytes and eight weights
// (a Rand() & 0x3F walk) each ---
// kArea72Cells: now Area72_Cells (symbols.toml). Area72_Cells, 8 x 2 bytes
// kArea72Weights: now Area72_Weights (symbols.toml). Area72_Weights, 8
// kArea73Cells: now Area73_Cells (symbols.toml). Area73_Cells, 8 x 2 bytes
// kArea73Weights: now Area73_Weights (symbols.toml). Area73_Weights, 8
// --- area 75's ---
// The five event-op records of 0x11 bytes handler 6 spawns objects by.
// kArea75SpawnOps: now Area75_SpawnOps (symbols.toml). Area75_SpawnOps, 5 x 0x11 bytes
constexpr unsigned kArea75SpawnOpStride = 0x11;
// Handler 7's two-state table (by the running object's +4) and the byte its
// state 0 reads by the object's +0xB.
// kArea75FlyStates: now Area75_FlyStates (symbols.toml). Area75_FlyStates, 2
// kArea75FlyHeights: now Area75_FlyHeights (symbols.toml). Area75_FlyHeights
// Handler 11's two-state table.
// kArea75FollowStates: now Area75_FollowStates (symbols.toml). Area75_FollowStates, 2
// The press phase table (by 0x93C340; entry 0 the shared `ret` 0x437CC0).
// kArea75Phases: now Area75_Phases (symbols.toml). Area75_Phases, 5
constexpr unsigned kArea75PhaseCount = 5;
// Three (list, count) records of 8 bytes: each list holds pairs (the move + 1,
// its frames); by the list index 0x93C352.
// kArea75Lists: now Area75_Lists (symbols.toml). Area75_Lists, 3 x 8 bytes
constexpr unsigned kArea75ListCount = 3;
// Sixteen bytes each: the rhythm row a deal draws (Rand() & 0xF), the next
// list a deal draws when one ends.
// kArea75Rows: now Area75_Rows (symbols.toml). Area75_Rows, 16
// kArea75NextList: now Area75_NextList (symbols.toml). Area75_NextList, 16
// Object B's animation by (the previous move, the new one): rows of 4.
// kArea75MoveAnims: now Area75_MoveAnims (symbols.toml). Area75_MoveAnims, rows of 4
// The other's press pattern: rows of 15 bytes by the rhythm row, read at the
// move's frame count mod 15.
// kArea75Rhythm: now Area75_Rhythm (symbols.toml). Area75_Rhythm, rows of 15
// The counters' format for Crt_sprintf (two numbers).
// kArea75Format: now Area75_CounterFormat (symbols.toml). Area75_CounterFormat

// Area 75's own cells (0x93C340..0x93C353; unnamed, no other reader read this
// round). Two counters (words, 0x5DC at a reset) come down by 4 or 5 a press:
// the player's by the button (Input_Pressed bit 0x20), the other's by the
// rhythm tables. The phase (Area75_Phases' index), the move (0 the other's
// turn, 1 the player's, 2 both) and its frames, a flags byte (bit 8: presses
// out of turn are not counted, 4: the counters blink, 1: not drawn), presses
// out of turn and frames without a press (the player's), the rhythm row, the
// two effect slots (the other's, the player's), the two object indexes (A the
// other's; B the one animated by the move), the list and its position.
constexpr std::uint32_t kPhase = 0x93C340;
// kMove: now AreaW1f_Move (symbols.toml).
// kMoveFrames: now AreaW1f_MoveFrames (symbols.toml). u16
// kPressFlags: now AreaW1f_PressFlags (symbols.toml).
// kEarlyPresses: now AreaW1f_EarlyPresses (symbols.toml).
// kIdleFrames: now AreaW1f_IdleFrames (symbols.toml).
// kRow: now AreaW1f_Row (symbols.toml).
// kPlayerCounter: now AreaW1f_PlayerCounter (symbols.toml). u16
// kOtherCounter: now AreaW1f_OtherCounter (symbols.toml). u16
constexpr std::uint32_t kOtherEffect = 0x93C34E;
constexpr std::uint32_t kPlayerEffect = 0x93C34F;
constexpr std::uint32_t kObjectA = 0x93C350;
// kObjectB: now AreaW1f_ObjectB (symbols.toml).
// kList: now AreaW1f_List (symbols.toml).
// kListPos: now AreaW1f_ListPos (symbols.toml).
// kArea75Cells: now AreaW1f_Phase (symbols.toml).
constexpr unsigned kArea75CellBytes = 0x14;

// Area 42's window draw (0x40E750, taken here as Area75_DrawWindow): the three
// float constants it adds and takes away (in .rdata).
constexpr rdata::Const kFloatOne{0x5C41B8};
constexpr rdata::Const kFloatThree{0x5C41BC};
constexpr rdata::Const kFloatTwo{0x5C41C0};

}  // namespace at
}  // namespace area_w1f
