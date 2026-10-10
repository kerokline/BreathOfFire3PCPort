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
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// A byte some choices set to 6 beside the message they open (areas 3, 37, 41
// and 50's choices do the same); no reader read this round.
constexpr std::uint32_t kAnswerMark = 0x9398CF;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind, and a word timer.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's counters 0 and 3 (MoveScript_CounterOps, ops A0..AF).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
// The scratch word 0x903850: an effect or sprite slot a handler keeps there.
constexpr std::uint32_t kScratch = 0x903850;
// The object pointer after Camera_ShiftY (area_w1a_callees.h's kFocusObject):
// area 68's object trigger 25 reads the object through it.
constexpr std::uint32_t kFocusObject = 0x903804;
// MoveScript_Var7 and the byte after it (the run step).
constexpr std::uint32_t kVar7 = 0x8034E4;
constexpr std::uint32_t kVar7Step = 0x8034E5;
// The story flags (Flags_Set's bank) and the Cond_Flags row 10 (0x903F90 +
// 8 * 10) area 75 sets, clears and tests.
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kCondRow10 = 0x903FE0;
// Cond_Flags row 7 (0x903F90 + 8 * 7): area 74's handler 0 toggles its flags
// 5 and 6.
constexpr std::uint32_t kCondRow7 = 0x903FC8;
// The story-flag byte 0x90409C (flags 0x360..0x367): area 74's choice tests
// its bit 0x20 (flag 0x365).
constexpr std::uint32_t kFlagByte9C = 0x90409C;
// The three party lists' first bytes (0x904062..0x904064).
constexpr std::uint32_t kPartyList0 = 0x904062;
// The current music track (a byte Music_Play returns at once on; the
// chapters' kMusicCurrent / kAreaTrack).
constexpr std::uint32_t kMusicCurrent = 0x904CD0;
// The leader's record (ObjTrio): byte +8 (its direction), +0x2C (a dword the
// push toggle plays a sound by), +0x4A, +0x89, the zone counter +0x134 (areas
// 72 / 73 set Field_EdgeBits to it less 5), the word +0x12E.
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kLeaderDir = 0x802D48;
constexpr std::uint32_t kLeaderSound = 0x802D6C;
constexpr std::uint32_t kLeader4A = 0x802D8A;
constexpr std::uint32_t kLeaderByte89 = 0x802DC9;
constexpr std::uint32_t kLeaderZone = 0x802E74;
constexpr std::uint32_t kLeaderWord12E = 0x802E6E;
constexpr std::uint32_t kPartyStride = 0x14C;
// Field objects (Sprite_Objects, 30 of 0xA4 bytes) and effect records
// (Effect_Objects, 0x80 bytes).
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr std::uint32_t kEffectStride = 0x80;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;     // Effect_Objects
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;     // Sprite_Objects
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;   // Sprite_ObjectsExtra
constexpr std::uint32_t kActiveMember = 0x9035A4;      // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;      // MoveScript_Object
constexpr std::uint32_t kWaitWord = 0x66C810;          // MoveScript_WaitWordDA
constexpr std::uint32_t kPassFlags = 0x7E0918;         // Draw_PassFlags
constexpr std::uint32_t kKind2Z = 0x905E60;            // Field_Kind2Z
constexpr std::uint32_t kInputPressed = 0x7E1BEC;      // Input_Pressed
constexpr std::uint32_t kPacketNext = 0x7E0670;        // Gfx_PacketNext
constexpr std::uint32_t kCondByteFA = 0x8034E0;        // Cond_ByteFA
// The text buffer area 75's counters are printed into (area 42's timer
// prints into the same, docs/area_w1b.md).
constexpr std::uint32_t kTextBuffer = 0x904BA0;

// --- area 68's tables (symbols.toml [[data]]) ---

// The two table choices' message words, two each, by the s8 answer.
constexpr std::uint32_t kArea68ChoiceMessages = 0x607D64;   // Area68_ChoiceMessages, 4 words
// The effect kinds by party member (Effect_Spawn's third argument): kind 4's
// spawns read the first, kinds 1 and 3's the second (12 bytes on).
constexpr std::uint32_t kArea68EffectKinds4 = 0x607060;   // Area68_EffectKinds4
constexpr std::uint32_t kArea68EffectKinds = 0x60706C;    // Area68_EffectKinds
// Handlers 2 and 3's two-state tables (by the running object's +4) and the
// 16 signed steps each glide reads by its count's low nibble.
constexpr std::uint32_t kArea68GlideStatesA = 0x607DCC;   // Area68_GlideStatesA, 2
constexpr std::uint32_t kArea68GlideStepsA = 0x607DD4;    // Area68_GlideStepsA, 16 bytes
constexpr std::uint32_t kArea68GlideStatesB = 0x607DE4;   // Area68_GlideStatesB, 2
constexpr std::uint32_t kArea68GlideStepsB = 0x607DEC;    // Area68_GlideStepsB, 16 bytes
// --- area 69's ---
constexpr std::uint32_t kArea69GlideStates = 0x608EDC;    // Area69_GlideStates, 2
constexpr std::uint32_t kArea69GlideSteps = 0x608EE4;     // Area69_GlideSteps, 16 bytes
// --- areas 72 and 73's: eight (x, z) cells of two bytes and eight weights
// (a Rand() & 0x3F walk) each ---
constexpr std::uint32_t kArea72Cells = 0x609410;          // Area72_Cells, 8 x 2 bytes
constexpr std::uint32_t kArea72Weights = 0x609420;        // Area72_Weights, 8
constexpr std::uint32_t kArea73Cells = 0x6094E0;          // Area73_Cells, 8 x 2 bytes
constexpr std::uint32_t kArea73Weights = 0x6094F0;        // Area73_Weights, 8
// --- area 75's ---
// The five event-op records of 0x11 bytes handler 6 spawns objects by.
constexpr std::uint32_t kArea75SpawnOps = 0x60AAE0;       // Area75_SpawnOps, 5 x 0x11 bytes
constexpr unsigned kArea75SpawnOpStride = 0x11;
// Handler 7's two-state table (by the running object's +4) and the byte its
// state 0 reads by the object's +0xB.
constexpr std::uint32_t kArea75FlyStates = 0x60AB38;      // Area75_FlyStates, 2
constexpr std::uint32_t kArea75FlyHeights = 0x60AB40;     // Area75_FlyHeights
// Handler 11's two-state table.
constexpr std::uint32_t kArea75FollowStates = 0x60AB48;   // Area75_FollowStates, 2
// The press phase table (by 0x93C340; entry 0 the shared `ret` 0x437CC0).
constexpr std::uint32_t kArea75Phases = 0x60AB50;         // Area75_Phases, 5
constexpr unsigned kArea75PhaseCount = 5;
// Three (list, count) records of 8 bytes: each list holds pairs (the move + 1,
// its frames); by the list index 0x93C352.
constexpr std::uint32_t kArea75Lists = 0x60AB88;          // Area75_Lists, 3 x 8 bytes
constexpr unsigned kArea75ListCount = 3;
// Sixteen bytes each: the rhythm row a deal draws (Rand() & 0xF), the next
// list a deal draws when one ends.
constexpr std::uint32_t kArea75Rows = 0x60ABA0;           // Area75_Rows, 16
constexpr std::uint32_t kArea75NextList = 0x60ABB0;       // Area75_NextList, 16
// Object B's animation by (the previous move, the new one): rows of 4.
constexpr std::uint32_t kArea75MoveAnims = 0x60ABC0;      // Area75_MoveAnims, rows of 4
// The other's press pattern: rows of 15 bytes by the rhythm row, read at the
// move's frame count mod 15.
constexpr std::uint32_t kArea75Rhythm = 0x60ABD0;         // Area75_Rhythm, rows of 15
// The counters' format for Crt_sprintf (two numbers).
constexpr std::uint32_t kArea75Format = 0x60ABF0;         // Area75_CounterFormat

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
constexpr std::uint32_t kMove = 0x93C343;
constexpr std::uint32_t kMoveFrames = 0x93C344;   // u16
constexpr std::uint32_t kPressFlags = 0x93C346;
constexpr std::uint32_t kEarlyPresses = 0x93C347;
constexpr std::uint32_t kIdleFrames = 0x93C348;
constexpr std::uint32_t kRow = 0x93C349;
constexpr std::uint32_t kPlayerCounter = 0x93C34A;   // u16
constexpr std::uint32_t kOtherCounter = 0x93C34C;    // u16
constexpr std::uint32_t kOtherEffect = 0x93C34E;
constexpr std::uint32_t kPlayerEffect = 0x93C34F;
constexpr std::uint32_t kObjectA = 0x93C350;
constexpr std::uint32_t kObjectB = 0x93C351;
constexpr std::uint32_t kList = 0x93C352;
constexpr std::uint32_t kListPos = 0x93C353;
constexpr std::uint32_t kArea75Cells = 0x93C340;
constexpr unsigned kArea75CellBytes = 0x14;

// Area 42's window draw (0x40E750, taken here as Area75_DrawWindow): the three
// float constants it adds and takes away (in .rdata).
constexpr rdata::Const kFloatOne{0x5C41B8};
constexpr rdata::Const kFloatThree{0x5C41BC};
constexpr rdata::Const kFloatTwo{0x5C41C0};

}  // namespace at
}  // namespace area_w1f
