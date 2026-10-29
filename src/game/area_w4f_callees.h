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

#include "common/image_base.h"

#include "bof3/symbols.gen.h"   // bof3::addr: the constants below naming group BE6's functions (docs/battle_e6.md section 5)

namespace area_w4f {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row, s8) and the message
// word a choice handler leaves (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr ::bof3::image::Va kMessage{0x7DEE48};
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8) and its state (s8).
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// The movement script's counters (MoveScript_CounterOps, ops A0..AF): 0, 1
// and 3; the byte after MoveScript_Var7 (the run's step, as the chapters use
// it); the scratch word the event ops keep the new object's index in
// (Sprite_FindFree's answer, zero-extended).
constexpr ::bof3::image::Va kCounter0{0x903848};
constexpr ::bof3::image::Va kCounter1{0x903849};
constexpr ::bof3::image::Va kCounter3{0x90384B};
// kVar7Step: now MoveScript_PartyRecords[0x65] (symbols.toml).
constexpr ::bof3::image::Va kObjectIndex{SharedScratch_addr};
// The focus object: the object whose talk ran last (MoveScript_SetTurnTarget
// stores Field_ActiveMember there; area_w1a_callees.h's kFocusObject). Area
// 192's and 193's pair choices write its dwords +0x18 / +0x1C.
constexpr ::bof3::image::Va kFocusObject{0x903804};
// The story flags (Flags_Set / Flags_Test's bank, Cond_Flags row 20) and
// Cond_Flags row 14, whose flags 3 and 0xA areas 192 and 193 test.
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kCondRow14: now Cond_Flags[0x70] (symbols.toml).
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
constexpr ::bof3::image::Va kFlagRow{0x929ED0};
// The party records (ObjTrio, 0x14C each): the leader's +0x89 (0x802DC9;
// the record k's is k * 0x14C on), and the +0x80 block (0xA4 bytes from
// 0x802DC0) area 192's pass copies each member's character record into.
// kLeader89: now ObjTrio[0].char_id (symbols.toml).
constexpr ::bof3::image::Va kParty80{0x802DC0};
constexpr std::uint32_t kPartyStride = 0x14C;
// The party list (a member id a byte, Field_MemberCount of them) and
// Character_IndexById 0x66972C, which by this use maps a member id to its
// character record (docs/area-entry.md).
// kPartyList: now Cond_Flags[0xd2] (symbols.toml).
// kMemberToRecord: now Character_IndexById (symbols.toml).
// The eight character records (0xA4 bytes from 0x903A70): +0xB bit 0
// ("joined", docs/field-event.md), the status word +0x10, HP +0x18, AP +0x1A,
// max HP +0x20, max AP +0x22, and the byte +0x1E (docs/item-use.md section 2
// calls it the max-HP scale) that area 192's talk and init compare.
// kCharRecords: now CharacterRecords (symbols.toml).
constexpr std::uint32_t kCharStride = 0xA4;
constexpr unsigned kCharCount = 8;
// kChar0Byte1E: now Field_ActorStates[0xe] (symbols.toml).
// A block of four bytes after Party_Zenny areas 192 and 193 set together: a
// word (0x1E0 or 0), a byte 0x90405E (area 192 compares it with 5 and caps it
// at 8), and 0x90405F (item_use_callees.h's kWaterJug, set to 0xF0).
// kWord90405C: now Cond_Flags[0xcc] (symbols.toml).
// kByte90405E: now Cond_Flags[0xce] (symbols.toml).
// kByte90405F: now Cond_Flags[0xcf] (symbols.toml).
// Two bytes cleared with them (no reader read this round).
// kByte929EC1: now Area189_StepsSinceEvent (symbols.toml).
// kByte9036D0: now Area189_EventsInRow (symbols.toml).
// The return point (event_ops_callees.h's kReturnPoint): x and z dwords, the
// area word at +8; the byte +0xA cleared on the way back, the byte +0xB area
// 193's tail sets to 2 (menu_lists_callees.h calls +0xA kCampFlag).
constexpr ::bof3::image::Va kReturnPoint{0x904148};
// kReturnArea: now AreaW4d_ReturnArea (symbols.toml).
// kReturnByteA: now AreaW3c_CampFlag (symbols.toml).
// kReturnByteB: now AreaW3g_HealByte (symbols.toml).
// The pending-area byte (scena_sc3_callees.h's kPendingKind): area 193's init
// sets it to 1.
constexpr ::bof3::image::Va kPendingKind{0x937F98};
// Effect_Objects' stride and count (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
// kSpriteObjects: now Sprite_Objects (symbols.toml). Sprite_Objects (30 of 0xA4)
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr ::bof3::image::Va kActiveMember{0x9035A4};      // Field_ActiveMember
constexpr ::bof3::image::Va kScriptObject{0x929E80};      // MoveScript_Object
// kCondByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
// kDrawPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
constexpr ::bof3::image::Va kWaitWordDA{0x66C810};        // Transition_Busy
constexpr ::bof3::image::Va kStatusBits{0x8034E1};        // Field_StatusBits
// kScriptFlags2: now Field_ScriptFlags2 (symbols.toml). Field_ScriptFlags2

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 192: the focus pairs its choice 2 reads by the answer (6 pairs of
// bytes); the talk tables 0x42C0A0 / 0x42C1C0 read (the five member ids, two
// rows of 5 x 9 message bytes, the rank steps, a third row of message bytes
// and its rank steps); the three blocks of six placement ops its init hands
// EventOp_0x (0x11 bytes an op).
// kArea192FocusPairs: now Area192_FocusPairs (symbols.toml).
// kArea192TalkMessages: now Area192_TalkMessages (symbols.toml). 0x30, flag 2 clear
// kArea192TalkMessagesF: now Area192_TalkMessagesF (symbols.toml). 0x30, flag 2 set
// kArea192TalkWho: now Area192_TalkWho (symbols.toml). 5 member ids
constexpr unsigned kArea192TalkWhoCount = 5;
// kArea192TalkSteps: now Area192_TalkSteps (symbols.toml). 9 bytes, by the rank
// kArea192TalkMessagesB: now Area192_TalkMessagesB (symbols.toml). 0x30, the flagged talk's
// kArea192TalkStepsB: now Area192_TalkStepsB (symbols.toml). 9 bytes, by the rank
// kArea192PlaceOps: now Area192_PlaceOps (symbols.toml). 3 pointers to six ops each
constexpr std::uint32_t kPlaceOpStride = 0x11;
constexpr unsigned kPlaceOpCount = 6;
// Area 193: the message words its choices 0..2 read by the answer; its
// choice 3's focus pairs.
// kArea193Messages0: now Area193_Messages0 (symbols.toml).
// kArea193Messages1: now Area193_Messages1 (symbols.toml).
// kArea193Messages2: now Area193_Messages2 (symbols.toml).
// kArea193FocusPairs: now Area193_FocusPairs (symbols.toml).
// Areas 196 and 197: the member searches' four keys (bytes) and four message
// words, one pair of tables a handler (0xC bytes from one pair to the next).
// kArea196Keys0: now Area196_Keys0 (symbols.toml).
// kArea196Keys1: now Area196_Keys1 (symbols.toml).
// kArea197Keys0: now Area197_Keys (symbols.toml).
constexpr std::uint32_t kKeysToMessages = 4;
constexpr std::uint32_t kKeysStride = 0xC;
// Area 197: its handler 7's two states (by the running object's +4); the
// shake's steps by the running object's +0xA (low nibble; s8).
// kArea197ShakeStates: now Area197_ShakeStates (symbols.toml).
// kArea197ShakeSteps: now Area197_ShakeSteps (symbols.toml).
// Area 198: the two slot scripts its handlers 0 and 11 hand 0x455290; the
// sink's four animations; the shake's steps (low nibble; s8); the drop's
// placement op; effect kind 0xA6's two states (by the record's +1) and its
// three records of 8 bytes (s16 x and z scale, u16 bank, u8 animation) by
// the record's +6.
// kArea198SlotScript0: now Area198_SlotScript0 (symbols.toml).
// kArea198SlotScript11: now Area198_SlotScript11 (symbols.toml).
// kArea198SinkAnims: now Area198_SinkAnims (symbols.toml).
// kArea198ShakeSteps: now Area198_ShakeSteps (symbols.toml).
// kArea198DropOp: now Area198_DropOp (symbols.toml).
// kArea198EffectA6Records: now Area198_EffectA6Records (symbols.toml).
// kArea198EffectA6States: now Area198_EffectA6States (symbols.toml).
constexpr unsigned kStateCount = 2;

// --- the callees nobody owns (raw addresses) ---

constexpr ::bof3::image::CodeVa kSlotsReleaseFor{bof3::addr::Field_SlotsReleaseOwner};   // 0x454A80 (BE6, 2026-09-29)
constexpr ::bof3::image::CodeVa kSlotStart{bof3::addr::Field_SlotStart};                  // 0x455290 (BE6, 2026-09-29)
constexpr ::bof3::image::CodeVa kRoundHigh{bof3::addr::Fixed_HighRoundUp};  // BE3's since round twelve (battle_e3.cpp): the same value, so the fuzz keys stand

}  // namespace at
}  // namespace area_w4f
