// Internal to area_w3d.cpp and area_w3d_fuzz.cpp: the cells world 3's area 135
// touches that symbols.toml has no name for, the area's own .data tables, and
// the two engine callees nobody owns yet by their raw addresses.
// docs/area_w3d.md.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round fourteen's rebinding (R3E, docs/rest_3e.md section 8): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace area_w3d {

// --- engine callees nobody owns (raw; the rebinding pass names them) ---
// 0x46D710: copies the running object's script (+0x54 count, +0x50 source) to
// 0x8C5D80, calls EffectKind30_ShardsInit (0x46C200) and _SparksInit (0x46C430), sets +9 = 0x10 (read only as far as
// area 135's spawn state 0 calls it, void (void)).
constexpr std::uint32_t kEngine46D710 = bof3::addr::EffectShards_LoadModel;   // 0x46D710, R3E's
// 0x46D770: calls EffectKind30_ShardsStep (0x46C310) then tail-jumps to _SparksDraw (0x46C4B0) (void (void)).
constexpr std::uint32_t kEngine46D770 = bof3::addr::EffectShards_Step;   // 0x46D770, R3E's

namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (a byte; read signed by choice 2) and
// the message word a choice handler leaves (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub byte (area 135 keeps a
// count, then an object index in it), and a word timer.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's counter 3 (MoveScript_CounterOps) and the scratch
// word 0x903850 (a sprite slot the spawn state keeps there).
constexpr std::uint32_t kCounter3 = 0x90384B;
constexpr std::uint32_t kScratch = SharedScratch_addr;
// The story flags (Flags_Set's bank); party list 1's first byte.
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kPartyList1: now Cond_Flags[0xd3] (symbols.toml).
// The zone the field was entered from (Cond_ByteFD's value on entry,
// docs/mode-flow.md; area_w1a_callees.h's kEntryZone).
constexpr std::uint32_t kEntryZone = 0x905E68;
// Field_MoveSpeeds + 3 (area_w2d_callees.h's kMoveSpeed3): the tail's glide
// divisor is this byte << 3.
constexpr std::uint32_t kMoveSpeed3 = 0x6697F3;
// A byte the init sets bit 0 of in zone 4 (no reader read this round).
// kInitBit: now MoveScript_PartyRecords[0x30] (symbols.toml).
// The effect slot area 135's choice 2 keeps (Effect_FindFree's answer, 0xFF
// none; read back as a dword & 0xFF). Unnamed; no reader outside the band.
// kEffectSlot: now AreaW3d_EffectSlot (symbols.toml).
// Sprite_Kind2's +0x80 (0x7E0940 + 0x80): handler 3 sets its bit 3.
// kKind2Byte80: now Sprite_Kind2[0x80] (symbols.toml).
// The leader's record (ObjTrio): +8 its direction, words +0x2E / +0x30 (the
// spawn's x / z), dwords +0x34 / +0x38 (the tail's glide target).
// kLeader: now ObjTrio (symbols.toml).
// kLeaderDir: now ObjTrio[8] (symbols.toml).
// kLeaderX16: now ObjTrio[0x2e] (symbols.toml).
// kLeaderZ16: now ObjTrio[0x30] (symbols.toml).
// kLeaderX: now ObjTrio[0x34] (symbols.toml).
// kLeaderZ: now ObjTrio[0x38] (symbols.toml).
constexpr std::uint32_t kPartyStride = 0x14C;
// Party record words +0x36 / +0x3A (the cell x / z: the dwords' high words).
// kPartyCellX: now ObjTrio[0x36] (symbols.toml).
// kPartyCellZ: now ObjTrio[0x3a] (symbols.toml).
// Sprite_ObjectsExtra[0] (0x802000)'s x and z dwords: the tail's camera glide
// target.
// kExtra0X: now Sprite_ObjectsExtra[0x34] (symbols.toml).
// kExtra0Z: now Sprite_ObjectsExtra[0x38] (symbols.toml).
// Sprite_ObjectsExtra[3] (0x8021EC): the marker object. +0 (the init sets 1),
// +9, dwords +0xC / +0x10 (a step), +0x34 / +0x38 / +0x3C (x, z, y; the
// words +0x36 / +0x3A the cell), +0x70, +0x83 (choice 1 sets it).
// kMarker: now Sprite_ObjectsExtra[0x1ec] (symbols.toml).
// kMarker9: now Sprite_ObjectsExtra[0x1f5] (symbols.toml).
// kMarkerStepX: now Sprite_ObjectsExtra[0x1f8] (symbols.toml).
// kMarkerStepZ: now Sprite_ObjectsExtra[0x1fc] (symbols.toml).
// kMarkerX: now Sprite_ObjectsExtra[0x220] (symbols.toml).
// kMarkerCellX: now Sprite_ObjectsExtra[0x222] (symbols.toml).
// kMarkerZ: now Sprite_ObjectsExtra[0x224] (symbols.toml).
// kMarkerCellZ: now Sprite_ObjectsExtra[0x226] (symbols.toml).
// kMarkerY: now Sprite_ObjectsExtra[0x228] (symbols.toml).
// kMarker70: now Sprite_ObjectsExtra[0x25c] (symbols.toml).
// kMarker83: now Sprite_ObjectsExtra[0x26f] (symbols.toml).
// The window pool words the message helper resets (MessagePools 0x803160's
// +4 / +6), and the cell it points 0x905B84 at.
constexpr std::uint32_t kPoolWords = 0x803164;
// kWindowPoolPtr: now BattleWinStates_Current (symbols.toml).
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects, 20 of 0x80
constexpr unsigned kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// kSpriteObjects: now Sprite_Objects (symbols.toml). Sprite_Objects, 30 of 0xA4
constexpr unsigned kObjectStride = 0xA4;
constexpr unsigned kObjectCount = 30;
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;   // Sprite_ObjectsExtra
// kSpriteKind2: now Sprite_Kind2 (symbols.toml). Sprite_Kind2 (a record of 0xA4)
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kInputHeld: now Input_Held (symbols.toml). Input_Held
// kInputPressed: now Input_Pressed (symbols.toml). Input_Pressed
// kHeightScale: now MapView_HeightScale (symbols.toml). MapView_HeightScale
// kKind2Hold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
// kKind2X: now Field_Kind2X (symbols.toml). Field_Kind2X
// kKind2Z: now Field_Kind2Z (symbols.toml). Field_Kind2Z
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). MoveScript_F3Divisor
// kFAWord: now MoveScript_FAWord (symbols.toml). MoveScript_FAWord
// kElevation: now MapView_Elevation (symbols.toml). MapView_Elevation
// kRedraw: now MapView_Redraw (symbols.toml). MapView_Redraw
constexpr std::uint32_t kTextRecords = 0x904CE0;     // Text_Records
constexpr std::uint32_t kMessageFormat = 0x5E10C0;   // Area08_MessageFormat (area 8's, shared)
// kMessagePools: now WindowRecords (symbols.toml). MessagePools

// --- area 135's .data (descriptor 0x62D940; symbols.toml [[data]]) ---

// A byte of the struct the descriptor's +8 points at (0x62D880 + 3): the tail
// sets and clears its bit 4.
// kArea135MapFlags: now AreaW3d_Area135MapFlags (symbols.toml).
// The exits handler 3 leaves by: (x, z) byte pairs by the running script
// object's +3 less 6, the first table in zone 4, the second otherwise.
// kArea135ExitsZone4: now Area135_ExitsZone4 (symbols.toml). Area135_ExitsZone4
// kArea135Exits: now Area135_Exits (symbols.toml). Area135_Exits
// Handler 4's sixteen 7-byte routes: the marker's cell (x, z), then a target
// x (half flag, cell), z (half flag, cell) and a direction; a cell of 0xFF
// is "no target".
// kArea135Routes: now Area135_Routes (symbols.toml). Area135_Routes, 16 x 7
constexpr unsigned kArea135RouteCount = 16;
constexpr unsigned kArea135RouteStride = 7;
// Handler 14's four (x, z) cells (bytes; the positions are cell << 16 |
// 0x8000).
// kArea135JumpCells: now Area135_JumpCells (symbols.toml). Area135_JumpCells, 4 x 2
// The dispatchers' state tables, read in place by the running object's +4.
// They are contiguous and overlap as their dispatchers read them: the walk's
// 0x62DA18 runs on through 0x62DA20's and 0x62DA44's entries (13 code
// pointers to the zero at 0x62DA4C), the lift's 0x62DA20 through 0x62DA44's
// (11). Each dispatcher here reads as far as the original would reach code;
// past that ours aborts.
// kArea135QueueStates: now Area135_QueueStates (symbols.toml). Area135_QueueStates, 2
constexpr unsigned kArea135QueueReach = 2;
// The queue's four row cells (bytes, << 16 | 0x8000), by the queue's count.
// kArea135QueueCells: now Area135_QueueCells (symbols.toml). Area135_QueueCells, 4
// kArea135WalkStates: now Area135_WalkStates (symbols.toml). Area135_WalkStates, 2 (13 reached)
constexpr unsigned kArea135WalkReach = 13;
// kArea135LiftStates: now Area135_LiftStates (symbols.toml). Area135_LiftStates, 9 (11 reached)
constexpr unsigned kArea135LiftReach = 11;
// kArea135SpawnStates: now Area135_SpawnStates (symbols.toml). Area135_SpawnStates, 2
constexpr unsigned kArea135SpawnReach = 2;
// The spawn's event-op records (EventOp_9x, 13 bytes each) by the tail's sub
// byte.
// kArea135SpawnOps: now Area135_SpawnOps (symbols.toml). Area135_SpawnOps, 13-byte records
constexpr unsigned kArea135SpawnOpStride = 13;
// The cell hook's two 4-byte entries: x, z, a direction (low nibble), the
// tail state it arms.
// kArea135CellEntries: now Area135_CellEntries (symbols.toml). Area135_CellEntries, 2 x 4
constexpr std::uint32_t kArea135CellEntriesEnd = 0x62DA8D;
// The init's EventOp_Bx record (zone 4, entered from zone 1).
// kArea135InitOp: now Area135_InitOp (symbols.toml). Area135_InitOp
// Handler 19's effect kinds by party list 1's byte (a table before the
// descriptor, 0x62C168; read in place, unchecked).
// kArea135EffectKinds: now Area135_EffectKinds (symbols.toml). Area135_EffectKinds

}  // namespace at
}  // namespace area_w3d
