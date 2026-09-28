// Internal to area_w3f.cpp and area_w3f_fuzz.cpp: the cells world 3's areas
// 143..146 touch that symbols.toml has no name for, the areas' own .data
// tables, and the two callees nobody owns (Capcom's map-camera helpers, by
// their raw addresses, as round ten's rule for a function no group has
// taken). Every other call is to a named function through the area harness
// (AH_CALL). docs/area_w3f.md.
#pragma once

#include <cstdint>

namespace area_w3f {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row) and the message
// word a choice handler leaves (0xFFFF: no new message).
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// A byte a choice sets to 6 beside the message it opens (areas 3, 37, 50,
// 98 do the same), and a second byte two further on the shared yes-handlers
// set to 3 or 5; no reader of either read this round.
constexpr std::uint32_t kAnswerMark = 0x9398CF;
constexpr std::uint32_t kAnswerMarkD1 = 0x9398D1;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state (s8), a sub-kind, a word timer.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's counters (MoveScript_CounterOps, ops A0..AF): 0 and
// 3; the byte after MoveScript_Var7 (the run's step, as the chapters use it).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
constexpr std::uint32_t kVar7Step = 0x8034E5;
// The slot byte area 145's effect spawns keep at DamageScratch (0x903850)
// and the byte after it, where area 145's handler 0 keeps a record index.
constexpr std::uint32_t kSlotScratch = 0x903850;
constexpr std::uint32_t kRecordIndex = 0x903851;
// The story flags (Flags_Set / Flags_Test's bank), Cond_Flags row 13 (the
// row area 145's cell hook tests: 0x903F90 + 8 * 13), the row pointer the
// chapters' flag calls take (Cond_Flags + 8 * chapter), and a dword of the
// second bit array whose value area 143's choice 0 compares with 0x3FFFF
// (eighteen flags set).
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kCondRow13 = 0x903FF8;
constexpr std::uint32_t kFlagRow = 0x929ED0;
constexpr std::uint32_t kFlags904650 = 0x904650;
// The focus object: the object whose talk ran last (MoveScript_SetTurnTarget
// stores Field_ActiveMember there; area_w1a_callees.h's kFocusObject).
constexpr std::uint32_t kFocusObject = 0x903804;
// The party records (ObjTrio): the leader's byte +8 (the pose the hooks
// compare), its word x high +0x36, the byte +0x89 of the leader and of the
// third record.
constexpr std::uint32_t kLeaderPose = 0x802D48;
constexpr std::uint32_t kLeaderX = 0x802D74;
constexpr std::uint32_t kLeaderXHigh = 0x802D76;
constexpr std::uint32_t kLeaderZ = 0x802D78;
constexpr std::uint32_t kLeader89 = 0x802DC9;
constexpr std::uint32_t kMember3_89 = 0x803061;
constexpr std::uint32_t kPartyStride = 0x14C;
// The leader's record's bytes +1..+3 area 145's tail sets (0x802D41..43).
constexpr std::uint32_t kLeader1 = 0x802D41;
// Sprite_ObjectsExtra's first record (0x802000): its x, z, y (+0x34, +0x38,
// +0x3C) and dword +0x6C (bits 9..10 a quarter turn); area 145's trails read
// records 0..3 at the stride 0xA4.
constexpr std::uint32_t kExtraObjects = 0x802000;
constexpr std::uint32_t kObjectStride = 0xA4;
// Field_ScriptFlags' low byte (area 144's handler 4 sets bit 8 in it).
constexpr std::uint32_t kScriptFlagsLow = 0x9039A2;
// The kind-2 object (Sprite_Kind2 0x7E0940, 0xA4 bytes): x, z, y (+0x34,
// +0x38, +0x3C) and bytes +0x84, +0x87 area 144's handler 6 sets before
// MoveCmd_MoveKind2.
constexpr std::uint32_t kKind2 = 0x7E0940;
// Camera_Angles[1] (s16) and MoveScript_WaitWordDA (u16), which area 145's
// tail reads.
constexpr std::uint32_t kCameraAngle1 = 0x929ECA;
constexpr std::uint32_t kWaitWordDA = 0x66C810;
// Effect_Objects' stride and count (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// The CLUT rows area 143's shift reads (Gfx_ClutStripSource + 0x600, eleven
// rows of 0x200 bytes) and writes (0x4000 further: Gfx_ClutStrip + 0x600).
constexpr std::uint32_t kClutSourceRows = 0x80BB80;
constexpr std::uint32_t kClutSourceEnd = 0x80D180;
constexpr std::uint32_t kClutToLive = 0x4000;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;     // Effect_Objects
constexpr std::uint32_t kActiveMember = 0x9035A4;      // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;      // MoveScript_Object
constexpr std::uint32_t kPacketNext = 0x7E0670;        // Gfx_PacketNext
constexpr std::uint32_t kCondByteFE = 0x905E20;        // Cond_ByteFE
constexpr std::uint32_t kKind2Hold = 0x929F12;         // Field_Kind2Hold
constexpr std::uint32_t kOtSlot = 0x92BF19;            // Draw_OtSlot
constexpr std::uint32_t kTextRecords = 0x904CE0;       // Text_Records
constexpr std::uint32_t kPartyRecords = 0x803480;      // MoveScript_PartyRecords (records of 16 bytes)
constexpr std::uint32_t kClutDirty = 0x937F90;         // Gfx_ClutStripDirty

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 143: four member keys (bytes) and their four message words; effect
// kind 0xB3's two states.
constexpr std::uint32_t kArea143MemberKeys = 0x630CC4;     // Area143_MemberKeys, 4 bytes
constexpr std::uint32_t kArea143MemberMessages = 0x630CC8; // Area143_MemberMessages, 4 words
constexpr std::uint32_t kArea143EffectStates = 0x630CD0;   // Area143_EffectStates, 2
// Area 145: the drop's two states; three key / message pairs (four bytes,
// four words, one pair a handler); the cell hook's seven records (x, z,
// pose in the low nibble, the tail state); the trails' legs (records of
// four bytes: a direction, a length, the next table's index, pad) - the
// first leg's four and the second and third legs' two banks each.
constexpr std::uint32_t kArea145DropStates = 0x6336E4;     // Area145_DropStates, 2
constexpr std::uint32_t kArea145MemberKeys1 = 0x6336EC;    // Area145_MemberKeys1, 4 bytes
constexpr std::uint32_t kArea145MemberMessages1 = 0x6336F0;
constexpr std::uint32_t kArea145MemberKeys5 = 0x6336F8;
constexpr std::uint32_t kArea145MemberMessages5 = 0x6336FC;
constexpr std::uint32_t kArea145MemberKeysB = 0x633704;
constexpr std::uint32_t kArea145MemberMessagesB = 0x633708;
constexpr std::uint32_t kArea145CellRecords = 0x633710;    // Area145_CellRecords, 7 of 4 bytes
constexpr unsigned kArea145CellRecordCount = 7;
constexpr std::uint32_t kArea145Legs0 = 0x63372C;          // Area145_TrailLegs0, 4 of 4 bytes
constexpr std::uint32_t kArea145Legs1A = 0x63373C;         // Area145_TrailLegs1A (record 1), 4 of 4
constexpr std::uint32_t kArea145Legs1B = 0x63374C;         // Area145_TrailLegs1B (other records), 4 of 4
constexpr std::uint32_t kArea145Legs2A = 0x63375C;         // Area145_TrailLegs2A (record 1), 4 of 4
constexpr std::uint32_t kArea145Legs2B = 0x63376C;         // Area145_TrailLegs2B (other records), 4 of 4
// Area 146: effect kind 0xB4's two states.
constexpr std::uint32_t kArea146EffectStates = 0x634114;   // Area146_EffectStates, 2
constexpr unsigned kStateCount = 2;
// Field_CellDeltas-style table: two signed bytes (x, z) a direction
// (event_ops_callees.h's kCellDelta).
constexpr std::uint32_t kCellDelta = 0x66971C;

// --- the callees nobody owns (raw addresses) ---

// 0x494060 (no arguments): sets the map camera up for a draw (magic_c2.cpp's
// kSetMapCamera). Engine; no group's.
constexpr std::uint32_t kSetMapCamera = 0x494060;
// 0x494110 (const long* point, float* vertex): projects a world point into a
// vertex of three dwords (two floats and a depth) at its second argument
// (magic_s32.cpp's kProjectPoint). Engine; no group's.
constexpr std::uint32_t kProjectPoint = 0x494110;

}  // namespace at
}  // namespace area_w3f
