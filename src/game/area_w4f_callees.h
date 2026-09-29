// Internal to area_w4f.cpp and area_w4f_fuzz.cpp: the cells world 4's areas
// 192..199 touch that symbols.toml has no name for, the areas' own .data
// tables, and the three callees nobody owns (engine code, by their raw
// addresses, as round ten's rule for a function no group has taken). Every
// other call is to a named function through the area harness (AH_CALL).
// docs/area_w4f.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x454A80  (object): releases every Field_Slots record whose +0xC is the
//             object (area_w2b_callees.h; engine, nobody's).
//   0x455290  (object, script): starts a Field_Slots script for the object;
//             al its index, 0xFF none (area_w2b_callees.h; engine, nobody's).
//   0x441090  (value, sign): the high word of a 16.16 value, one more when
//             sign is not negative and the low word is not 0 - a round-up of
//             the integer part; ax the answer (read 2026-09-28,
//             0x441090..0x4410AD; engine, nobody's; chapter 1's and 3's runs
//             call its neighbour 0x4410B0).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace area_w4f {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row, s8) and the message
// word a choice handler leaves (0xFFFF: no new message).
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8) and its state (s8).
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
// The movement script's counters (MoveScript_CounterOps, ops A0..AF): 0, 1
// and 3; the byte after MoveScript_Var7 (the run's step, as the chapters use
// it); the scratch word the event ops keep the new object's index in
// (Sprite_FindFree's answer, zero-extended).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter3 = 0x90384B;
constexpr std::uint32_t kVar7Step = 0x8034E5;
constexpr std::uint32_t kObjectIndex = 0x903850;
// The focus object: the object whose talk ran last (MoveScript_SetTurnTarget
// stores Field_ActiveMember there; area_w1a_callees.h's kFocusObject). Area
// 192's and 193's pair choices write its dwords +0x18 / +0x1C.
constexpr std::uint32_t kFocusObject = 0x903804;
// The story flags (Flags_Set / Flags_Test's bank, Cond_Flags row 20) and
// Cond_Flags row 14, whose flags 3 and 0xA areas 192 and 193 test.
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kCondRow14 = 0x904000;
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
constexpr std::uint32_t kFlagRow = 0x929ED0;
// The party records (ObjTrio, 0x14C each): the leader's +0x89 (0x802DC9;
// the record k's is k * 0x14C on), and the +0x80 block (0xA4 bytes from
// 0x802DC0) area 192's pass copies each member's character record into.
constexpr std::uint32_t kLeader89 = 0x802DC9;
constexpr std::uint32_t kParty80 = 0x802DC0;
constexpr std::uint32_t kPartyStride = 0x14C;
// The party list (a member id a byte, Field_MemberCount of them) and
// MoveScript_EffectState 0x66972C, which by this use maps a member id to its
// character record (docs/area-entry.md).
constexpr std::uint32_t kPartyList = 0x904062;
constexpr std::uint32_t kMemberToRecord = 0x66972C;
// The eight character records (0xA4 bytes from 0x903A70): +0xB bit 0
// ("joined", docs/field-event.md), the status word +0x10, HP +0x18, AP +0x1A,
// max HP +0x20, max AP +0x22, and the byte +0x1E (docs/item-use.md section 2
// calls it the max-HP scale) that area 192's talk and init compare.
constexpr std::uint32_t kCharRecords = 0x903A70;
constexpr std::uint32_t kCharStride = 0xA4;
constexpr unsigned kCharCount = 8;
constexpr std::uint32_t kChar0Byte1E = 0x903A8E;
// A block of four bytes after Party_Zenny areas 192 and 193 set together: a
// word (0x1E0 or 0), a byte 0x90405E (area 192 compares it with 5 and caps it
// at 8), and 0x90405F (item_use_callees.h's kWaterJug, set to 0xF0).
constexpr std::uint32_t kWord90405C = 0x90405C;
constexpr std::uint32_t kByte90405E = 0x90405E;
constexpr std::uint32_t kByte90405F = 0x90405F;
// Two bytes cleared with them (no reader read this round).
constexpr std::uint32_t kByte929EC1 = 0x929EC1;
constexpr std::uint32_t kByte9036D0 = 0x9036D0;
// The return point (event_ops_callees.h's kReturnPoint): x and z dwords, the
// area word at +8; the byte +0xA cleared on the way back, the byte +0xB area
// 193's tail sets to 2 (menu_lists_callees.h calls +0xA kCampFlag).
constexpr std::uint32_t kReturnPoint = 0x904148;
constexpr std::uint32_t kReturnArea = 0x904150;
constexpr std::uint32_t kReturnByteA = 0x904152;
constexpr std::uint32_t kReturnByteB = 0x904153;
// The pending-area byte (scena_sc3_callees.h's kPendingKind): area 193's init
// sets it to 1.
constexpr std::uint32_t kPendingKind = 0x937F98;
// Effect_Objects' stride and count (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;     // Effect_Objects
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;     // Sprite_Objects (30 of 0xA4)
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr std::uint32_t kActiveMember = 0x9035A4;      // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;      // MoveScript_Object
constexpr std::uint32_t kCondByteFE = 0x905E20;        // Cond_ByteFE
constexpr std::uint32_t kDrawPassFlags = 0x7E0918;     // Draw_PassFlags
constexpr std::uint32_t kWaitWordDA = 0x66C810;        // MoveScript_WaitWordDA
constexpr std::uint32_t kStatusBits = 0x8034E1;        // Field_StatusBits
constexpr std::uint32_t kScriptFlags2 = 0x905BA4;      // Field_ScriptFlags2

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 192: the focus pairs its choice 2 reads by the answer (6 pairs of
// bytes); the talk tables 0x42C0A0 / 0x42C1C0 read (the five member ids, two
// rows of 5 x 9 message bytes, the rank steps, a third row of message bytes
// and its rank steps); the three blocks of six placement ops its init hands
// EventOp_0x (0x11 bytes an op).
constexpr std::uint32_t kArea192FocusPairs = 0x647F5C;
constexpr std::uint32_t kArea192TalkMessages = 0x647F68;   // 0x30, flag 2 clear
constexpr std::uint32_t kArea192TalkMessagesF = 0x647F98;  // 0x30, flag 2 set
constexpr std::uint32_t kArea192TalkWho = 0x647FC8;        // 5 member ids
constexpr unsigned kArea192TalkWhoCount = 5;
constexpr std::uint32_t kArea192TalkSteps = 0x647FD0;      // 9 bytes, by the rank
constexpr std::uint32_t kArea192TalkMessagesB = 0x647FE0;  // 0x30, the flagged talk's
constexpr std::uint32_t kArea192TalkStepsB = 0x648010;     // 9 bytes, by the rank
constexpr std::uint32_t kArea192PlaceOps = 0x648158;       // 3 pointers to six ops each
constexpr std::uint32_t kPlaceOpStride = 0x11;
constexpr unsigned kPlaceOpCount = 6;
// Area 193: the message words its choices 0..2 read by the answer; its
// choice 3's focus pairs.
constexpr std::uint32_t kArea193Messages0 = 0x64894C;
constexpr std::uint32_t kArea193Messages1 = 0x648958;
constexpr std::uint32_t kArea193Messages2 = 0x648960;
constexpr std::uint32_t kArea193FocusPairs = 0x648968;
// Areas 196 and 197: the member searches' four keys (bytes) and four message
// words, one pair of tables a handler (0xC bytes from one pair to the next).
constexpr std::uint32_t kArea196Keys0 = 0x6490EC;
constexpr std::uint32_t kArea196Keys1 = 0x6490F8;
constexpr std::uint32_t kArea197Keys0 = 0x6497C4;
constexpr std::uint32_t kKeysToMessages = 4;
constexpr std::uint32_t kKeysStride = 0xC;
// Area 197: its handler 7's two states (by the running object's +4); the
// shake's steps by the running object's +0xA (low nibble; s8).
constexpr std::uint32_t kArea197ShakeStates = 0x649818;
constexpr std::uint32_t kArea197ShakeSteps = 0x649820;
// Area 198: the two slot scripts its handlers 0 and 11 hand 0x455290; the
// sink's four animations; the shake's steps (low nibble; s8); the drop's
// placement op; effect kind 0xA6's two states (by the record's +1) and its
// three records of 8 bytes (s16 x and z scale, u16 bank, u8 animation) by
// the record's +6.
constexpr std::uint32_t kArea198SlotScript0 = 0x649D4C;
constexpr std::uint32_t kArea198SlotScript11 = 0x649D90;
constexpr std::uint32_t kArea198SinkAnims = 0x649DD4;
constexpr std::uint32_t kArea198ShakeSteps = 0x649DD8;
constexpr std::uint32_t kArea198DropOp = 0x649DE8;
constexpr std::uint32_t kArea198EffectA6Records = 0x649E00;
constexpr std::uint32_t kArea198EffectA6States = 0x649E18;
constexpr unsigned kStateCount = 2;

// --- the callees nobody owns (raw addresses) ---

constexpr std::uint32_t kSlotsReleaseFor = 0x454A80;
constexpr std::uint32_t kSlotStart = 0x455290;
constexpr std::uint32_t kRoundHigh = bof3::addr::Fixed_HighRoundUp;  // BE3's since round twelve (battle_e3.cpp): the same value, so the fuzz keys stand

}  // namespace at
}  // namespace area_w4f
