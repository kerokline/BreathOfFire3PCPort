// Internal to area_w4e.cpp and area_w4e_fuzz.cpp: the cells world 4's areas
// 188..191 touch that symbols.toml has no name for, the areas' own .data
// tables, and the two callees nobody owns this wave (by their raw addresses,
// as round ten's rule for a function no group has taken). Every other call is
// to a named function through the area harness (AH_CALL). docs/area_w4e.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w4e {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row, read as s8 where
// it indexes) and the message word a choice handler leaves (0xFFFF: none).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state (s8), a sub-kind.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
// The movement script's counters 0 and 3 (MoveScript_CounterOps), and the
// byte after MoveScript_Var7 (the run's step, as the chapters use it).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
// kVar7Step: now MoveScript_PartyRecords[0x65] (symbols.toml).
// The story flags (Flags_Set / Flags_Test's bank), Cond_Flags row 14 (the row
// area 189's code tests bits 4, 9 and 0xA of: 0x903F90 + 8 * 14) and the row
// pointer the chapters' flag calls take (Cond_Flags + 8 * chapter).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kCondRow14: now Cond_Flags[0x70] (symbols.toml).
// kFlagRow: now AreaW0c_FlagRow (symbols.toml).
// The focus object: the object whose talk ran last (area_w3f_callees.h's
// kFocusObject); areas 188 and 191's choices store a byte pair at its +0x18
// and +0x1C.
constexpr std::uint32_t kFocusObject = 0x903804;
// A byte area 188's choice 3 clears on the yes of its one-answer path; no
// reader read this round.
// k929F0F: now AreaW3c_Load0F (symbols.toml).
// A mode byte area 188's init tests (1 or 4) and the byte +0x83 of
// Sprite_ObjectsExtra's first record it sets.
// kMode905E68: now AreaW1a_EntryZone (symbols.toml).
// kExtra0Byte83: now Sprite_ObjectsExtra[0x83] (symbols.toml).
// The word the step-count of area 189's walk is compared with: the leader
// record's +0x134 (ObjTrio 0x802D40 + 0x134), a pace the walk's start sets.
// kLeaderPace: now ObjTrio[0x134] (symbols.toml).
// Area 189's walk counters: a frame word (0x1E0 and 0x3C0 mark it), the
// byte after it counted up at 0x3C0, a byte the count adds 8 to and the
// arrival counts down, the facing the walk's start takes (low nibble), and
// the pending place the exit button saves (x, z dwords, the area word, a
// byte counted up) - all inside Cond_Flags' region to 0x904160.
// kWalkFrames: now Cond_Flags[0xcc] (symbols.toml).
constexpr std::uint32_t kWalkCount = 0x90405E;
// kWalkReserve: now Cond_Flags[0xcf] (symbols.toml).
// kSavedX: now AreaW2d_ReturnPoint (symbols.toml).
// kSavedZ: now AreaW4d_ReturnZ (symbols.toml).
// kSavedArea: now AreaW4d_ReturnArea (symbols.toml).
constexpr std::uint32_t kSavedCount = 0x904152;
// kWalkFacing: now AreaW3g_HealByte (symbols.toml).
// Two bytes area 189's arrival counts with: steps since the last event and
// events since the last big one.
// kStepsSince: now AreaW3g_Byte929EC1 (symbols.toml).
// kEventsSince: now AreaW3g_Byte9036D0 (symbols.toml).
// The pending area change Field_ChangeArea keeps: the area word, x and z
// dwords, the flags byte (docs/area_w3b.md's kPlace, kPendingX / Z,
// kChangeFlags).
constexpr std::uint32_t kPlace = 0x937F82;
constexpr std::uint32_t kPendingX = 0x903860;
// kPendingZ: now AreaEntry_PendingZ (symbols.toml).
constexpr std::uint32_t kChangeFlags = 0x905B88;
// The button words (a dword: two masks) area 189's arrival tests against
// Field_InputHeld with 0xB000 added.
// kButtonMap0: now Gfx_UnpackScratch[0x1f000] (symbols.toml).
// The second dword of MapView_ScreenXY (0x903820): the screen point's y that
// area 189's state 1 copies (a float, through the x87) after
// Gte_RotTransPers.
// kScreenY: now MapView_ScreenXY[1] (symbols.toml).
// A word area 189's state 1 sets to 0x3C (in the 0x8034E0 row).
// kWord8034E6: now MoveScript_PartyRecords[0x66] (symbols.toml).
// The party members' actor records (CharacterRecords 0x903A70, 0xA4 each):
// the byte +0x1E areas 189 and 191 read and raise, and the words +0x18 (the
// one area 189 lowers) and +0x40 (its bound); seven records are walked.
constexpr std::uint32_t kCharRecords = 0x903A70;
constexpr std::uint32_t kCharStride = 0xA4;
constexpr unsigned kCharWalked = 7;
// kCharByte1E: now Field_ActorStates[0xe] (symbols.toml).
// kCharWord18: now Field_ActorStates[8] (symbols.toml).
// The engine's direction table read by area 188's handler 0: 8 bytes a
// direction, its first dword read.
// kDirections: now Field_DirectionSteps[1] (symbols.toml).
// Effect_Objects' stride (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kCondByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
// kCondByteFF: now Cond_ByteFF (symbols.toml). Cond_ByteFF
constexpr std::uint32_t kInputHeld = 0x7E1BE8;         // Input_Held (Input_Pressed 0x7E1BEC)
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
// kCameraShiftY: now Camera_ShiftY (symbols.toml). Camera_ShiftY
// kVertexScratch: now Prim_VertexScratch (symbols.toml). Prim_VertexScratch (three shorts)
// kScreenXY: now MapView_ScreenXY (symbols.toml). MapView_ScreenXY (two floats)
// kCameraAngles: now Camera_Angles (symbols.toml). Camera_Angles; Cond_AngleFB 0x929ECC after
// kWaitWordDA: now MoveScript_WaitWordDA (symbols.toml). MoveScript_WaitWordDA
constexpr std::uint32_t kDamageScratch = 0x903850;     // DamageScratch (the slot word EventOp_0x reads)

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 188: the byte pairs its choice 3 stores at the focus object (by the
// answer, s8), the z limits of its handler 0 (by the tail's sub-kind),
// tail kind 43's byte and jump tables (inside its extent).
// kArea188FocusPairs: now Area188_FocusPairs (symbols.toml). Area188_FocusPairs, 6 x 2 bytes
// kArea188ZLimits: now Area188_ZLimits (symbols.toml). Area188_ZLimits, bytes
// Area 189: the leader's five states (Field_LeaderStates[13] jumps through
// them by Sprite_Current +2), the walk's sixteen step vectors (x, z dwords a
// facing), the exit button's two places (area, x, z bytes), the facing
// jitter (four s8).
// kArea189LeaderStates: now Area189_LeaderStates (symbols.toml). Area189_LeaderStates, 5
constexpr unsigned kArea189LeaderStateCount = 5;
// kArea189StepVectors: now Area189_StepVectors (symbols.toml). Area189_StepVectors, 16 x 8 bytes
// kArea189ExitPlaces: now Area189_ExitPlaces (symbols.toml). Area189_ExitPlaces, 2 x 3 bytes
// kArea189FacingJitter: now Area189_FacingJitter (symbols.toml). Area189_FacingJitter, 4 bytes
// Area 191: its choice 2's byte pairs, the scale's three states (by +4), the
// talk messages' tables (five member keys, two offset rows by the walk count,
// three 45-byte message rows), the three object lists its init places (six
// 17-byte event-op records each).
// kArea191FocusPairs: now Area191_FocusPairs (symbols.toml). Area191_FocusPairs, 6 x 2 bytes
// kArea191ScaleStates: now Area191_ScaleStates (symbols.toml). Area191_ScaleStates, 3
constexpr unsigned kArea191ScaleStateCount = 3;
// kArea191TalkMessagesA: now Area191_TalkMessagesA (symbols.toml). Area191_TalkMessagesA, 45 bytes (+ the next table's)
// kArea191TalkMessagesB: now Area191_TalkMessagesB (symbols.toml). Area191_TalkMessagesB, 45 bytes
// kArea191TalkKeys: now Area191_TalkKeys (symbols.toml). Area191_TalkKeys, 5 bytes
// kArea191TalkSteps: now Area191_TalkSteps (symbols.toml). Area191_TalkSteps, 9 bytes (by the walk count, at most 8)
// kArea191TalkMessagesC: now Area191_TalkMessagesC (symbols.toml). Area191_TalkMessagesC, 45 bytes
// kArea191TalkStepsC: now Area191_TalkStepsC (symbols.toml). Area191_TalkStepsC, 9 bytes
// kArea191ObjectLists: now Area191_ObjectLists (symbols.toml). Area191_ObjectLists, 3 pointers
constexpr std::uint32_t kEventOpRecord = 0x11;           // one EventOp_0x record

// --- the callees nobody owns (raw addresses) ---

// 0x511C10 (long x, long z): the map's height at (x, z) from the area block's
// corner table (AreaMap_Corners), answered in ax (the callers sign-extend it).
// Engine; no group's.
constexpr std::uint32_t kHeightAt = bof3::addr::AreaMap_CornerHeight;   // 0x511C10, E6C's
// 0x42C2D0 (no arguments): area 192's block (group AR4F's band this wave):
// each of eight actor records (CharacterRecords) with bit 0 of +0xB has its
// words +0x20 / +0x22 copied to +0x18 / +0x1A and its word +0x10 cleared;
// then each of Field_MemberCount's party lists' actors is copied whole (0xA4
// bytes) to its party record's +0x80. Called raw by area 191's tail kind 53,
// state 3.
constexpr std::uint32_t kRestoreRecords = bof3::addr::Area192_RestoreCharacters;

}  // namespace at
}  // namespace area_w4e
