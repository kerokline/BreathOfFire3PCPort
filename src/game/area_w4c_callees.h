// Internal to area_w4c.cpp and area_w4c_fuzz.cpp: the cells world 4's areas
// 173 and 174 (and the six choices of area 175's in the band) touch that
// symbols.toml has no name for, the areas' own .data tables, and the two
// engine callees nobody owns yet by their raw addresses. docs/area_w4c.md.
#pragma once

#include <cstdint>

#include "common/image_base.h"

#include "bof3/symbols.gen.h"   // bof3::addr: the constants below naming group BE6's functions (docs/battle_e6.md section 5)

namespace area_w4c {

// --- engine callees nobody owns (raw; the rebinding pass names them) ---
// 0x454A80 (object): releases every Field_Slots record whose +0xC is the
// object (area_w2b_callees.h, read 2026-09-28 by AR2B, 0x454A80..0x454AAA).
constexpr ::bof3::image::CodeVa kSlotsReleaseFor{bof3::addr::Field_SlotsReleaseOwner};   // 0x454A80 (BE6, 2026-09-29)
// 0x455290 (object, script): the first free Field_Slots record of eight gets
// the object and the script; al its index, 0xFF none (area_w2b_callees.h,
// 0x455290..0x4552F5). Area 174's handler 0 does not read the answer.
constexpr ::bof3::image::CodeVa kSlotStart{bof3::addr::Field_SlotStart};                  // 0x455290 (BE6, 2026-09-29)

namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (a byte) and the message word a choice
// handler leaves (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr ::bof3::image::Va kMessage{0x7DEE48};
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, and a word timer.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
constexpr ::bof3::image::Va kTailTimer{0x9039F6};
// The movement script's counter 3 (MoveScript_CounterOps; area_w3d's
// kCounter3): area 173's tail waits on it.
constexpr ::bof3::image::Va kCounter3{0x90384B};
// A byte of the camera / scratch cells (0x903840 + 8) area 174's handler 11
// compares its script's byte with. No name.
constexpr ::bof3::image::Va kStepLimit{0x903848};
// The story flags (Flags_Set's bank).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// The leader's record (ObjTrio): +8 its direction.
// kLeaderDir: now ObjTrio[0].direction (symbols.toml).
// The party records: +0x89 a byte the member handlers match (area 173's
// handlers 1 and 2 against their id lists, handlers 3 and 4 on Field_State's).
constexpr ::bof3::image::Va kParty{0x802D40};
constexpr std::uint32_t kPartyStride = 0x14C;
// kParty89: now ObjTrio[0].char_id (symbols.toml).
// MoveScript_Var7's neighbour (0x8034E4 + 1): area 174's choice 0 stores 0x1E
// or 0xA in it. No name; no reader read this round.
// kChoiceByteE5: now MoveScript_PartyRecords[0x65] (symbols.toml).
// Two bytes area 175's choices 23 and 25 store (the answer; 0 or 4). No name;
// not in the harness's regions (the group's own); no reader read this round.
// kChoiceByte3C: now Rest4b_BoardFlag (symbols.toml).
constexpr ::bof3::image::Va kChoiceByte3E{0x939A3E};
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects, 20 of 0x80
constexpr unsigned kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// kTintRecords: now MoveScript_TintRecords (symbols.toml). MoveScript_TintRecords, 256 of 12 (0xC00 bytes)
constexpr unsigned kTintStride = 12;
// kSpriteObjects: now Sprite_Objects (symbols.toml). Sprite_Objects, 30 of 0xA4
constexpr unsigned kObjectStride = 0xA4;
// kSpriteObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra, four of 0xA4
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kClutSource: now Gfx_ClutStripSource[0xf00] (symbols.toml). a row of Gfx_ClutStripSource (0x80B580 + 0x1E00)
constexpr ::bof3::image::Va kDescriptors{0x667590};     // Area_Descriptors

// --- area 173's .data (descriptor 0x63FF68; symbols.toml [[data]]) ---

// Handlers 1 and 2's (member byte, message) lists: four member bytes, then
// four message words; the first member byte a party member's +0x89 matches
// opens its message.
// kArea173MembersA: now Area173_MembersA (symbols.toml). Area173_MembersA, 4 bytes
// kArea173MessagesA: now Area173_MessagesA (symbols.toml). Area173_MessagesA, 4 words
// kArea173MembersB: now Area173_MembersB (symbols.toml). Area173_MembersB, 4 bytes
// kArea173MessagesB: now Area173_MessagesB (symbols.toml). Area173_MessagesB, 4 words
constexpr unsigned kArea173ListCount = 4;

// --- area 174's .data (descriptor 0x641650) ---

// The movement script handler 0 hands 0x455290.
// kArea174SlotScript: now Area174_SlotScript (symbols.toml). Area174_SlotScript
// Handlers 13..15's pose tables by a script byte: three pointers to pose
// pairs (animation, +0x2A) by direction. Read in place, unchecked: a byte
// past 2 reads the step deltas after it as a pointer (handed to
// Area174_SetPose, which reads through it).
// kArea174PoseTables: now Area174_PoseTables (symbols.toml). Area174_PoseTables, 3
// Handler 11's steps by the object's +0xA & 0xF (s8; x += d << 11, z -= d << 11).
// kArea174StepDeltas: now Area174_StepDeltas (symbols.toml). Area174_StepDeltas, 16

// The descriptor's +0x10: a table of the area's movement scripts by the script
// object's +3 (areas 174 and 198 read it through Area_Descriptors by
// Game_AreaNumber; 37 and 15 script pointers).
constexpr unsigned kDescScripts = 0x10;
constexpr unsigned kArea174ScriptCount = 37;
constexpr unsigned kArea198ScriptCount = 15;

}  // namespace at
}  // namespace area_w4c
