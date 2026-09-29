// Internal to area_w3d.cpp and area_w3d_fuzz.cpp: the cells world 3's area 135
// touches that symbols.toml has no name for, the area's own .data tables, and
// the two engine callees nobody owns yet by their raw addresses.
// docs/area_w3d.md.
#pragma once

#include <cstdint>

namespace area_w3d {

// --- engine callees nobody owns (raw; the rebinding pass names them) ---
// 0x46D710: copies the running object's script (+0x54 count, +0x50 source) to
// 0x8C5D80, calls EffectKind30_ShardsInit (0x46C200) and _SparksInit (0x46C430), sets +9 = 0x10 (read only as far as
// area 135's spawn state 0 calls it, void (void)).
constexpr std::uint32_t kEngine46D710 = 0x46D710;
// 0x46D770: calls EffectKind30_ShardsStep (0x46C310) then tail-jumps to _SparksDraw (0x46C4B0) (void (void)).
constexpr std::uint32_t kEngine46D770 = 0x46D770;

namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (a byte; read signed by choice 2) and
// the message word a choice handler leaves (0xFFFF: no new message).
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub byte (area 135 keeps a
// count, then an object index in it), and a word timer.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's counter 3 (MoveScript_CounterOps) and the scratch
// word 0x903850 (a sprite slot the spawn state keeps there).
constexpr std::uint32_t kCounter3 = 0x90384B;
constexpr std::uint32_t kScratch = 0x903850;
// The story flags (Flags_Set's bank); party list 1's first byte.
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kPartyList1 = 0x904063;
// The zone the field was entered from (Cond_ByteFD's value on entry,
// docs/mode-flow.md; area_w1a_callees.h's kEntryZone).
constexpr std::uint32_t kEntryZone = 0x905E68;
// Field_MoveSpeeds + 3 (area_w2d_callees.h's kMoveSpeed3): the tail's glide
// divisor is this byte << 3.
constexpr std::uint32_t kMoveSpeed3 = 0x6697F3;
// A byte the init sets bit 0 of in zone 4 (no reader read this round).
constexpr std::uint32_t kInitBit = 0x8034B0;
// The effect slot area 135's choice 2 keeps (Effect_FindFree's answer, 0xFF
// none; read back as a dword & 0xFF). Unnamed; no reader outside the band.
constexpr std::uint32_t kEffectSlot = 0x675CC0;
// Sprite_Kind2's +0x80 (0x7E0940 + 0x80): handler 3 sets its bit 3.
constexpr std::uint32_t kKind2Byte80 = 0x7E09C0;
// The leader's record (ObjTrio): +8 its direction, words +0x2E / +0x30 (the
// spawn's x / z), dwords +0x34 / +0x38 (the tail's glide target).
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kLeaderDir = 0x802D48;
constexpr std::uint32_t kLeaderX16 = 0x802D6E;
constexpr std::uint32_t kLeaderZ16 = 0x802D70;
constexpr std::uint32_t kLeaderX = 0x802D74;
constexpr std::uint32_t kLeaderZ = 0x802D78;
constexpr std::uint32_t kPartyStride = 0x14C;
// Party record words +0x36 / +0x3A (the cell x / z: the dwords' high words).
constexpr std::uint32_t kPartyCellX = 0x802D76;
constexpr std::uint32_t kPartyCellZ = 0x802D7A;
// Sprite_ObjectsExtra[0] (0x802000)'s x and z dwords: the tail's camera glide
// target.
constexpr std::uint32_t kExtra0X = 0x802034;
constexpr std::uint32_t kExtra0Z = 0x802038;
// Sprite_ObjectsExtra[3] (0x8021EC): the marker object. +0 (the init sets 1),
// +9, dwords +0xC / +0x10 (a step), +0x34 / +0x38 / +0x3C (x, z, y; the
// words +0x36 / +0x3A the cell), +0x70, +0x83 (choice 1 sets it).
constexpr std::uint32_t kMarker = 0x8021EC;
constexpr std::uint32_t kMarker9 = 0x8021F5;
constexpr std::uint32_t kMarkerStepX = 0x8021F8;
constexpr std::uint32_t kMarkerStepZ = 0x8021FC;
constexpr std::uint32_t kMarkerX = 0x802220;
constexpr std::uint32_t kMarkerCellX = 0x802222;
constexpr std::uint32_t kMarkerZ = 0x802224;
constexpr std::uint32_t kMarkerCellZ = 0x802226;
constexpr std::uint32_t kMarkerY = 0x802228;
constexpr std::uint32_t kMarker70 = 0x80225C;
constexpr std::uint32_t kMarker83 = 0x80226F;
// The window pool words the message helper resets (MessagePools 0x803160's
// +4 / +6), and the cell it points 0x905B84 at.
constexpr std::uint32_t kPoolWords = 0x803164;
constexpr std::uint32_t kWindowPoolPtr = 0x905B84;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;   // Effect_Objects, 20 of 0x80
constexpr unsigned kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;   // Sprite_Objects, 30 of 0xA4
constexpr unsigned kObjectStride = 0xA4;
constexpr unsigned kObjectCount = 30;
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;   // Sprite_ObjectsExtra
constexpr std::uint32_t kSpriteKind2 = 0x7E0940;     // Sprite_Kind2 (a record of 0xA4)
constexpr std::uint32_t kScriptObject = 0x929E80;    // MoveScript_Object
constexpr std::uint32_t kInputHeld = 0x7E1BE8;       // Input_Held
constexpr std::uint32_t kInputPressed = 0x7E1BEC;    // Input_Pressed
constexpr std::uint32_t kHeightScale = 0x929F22;     // MapView_HeightScale
constexpr std::uint32_t kKind2Hold = 0x929F12;       // Field_Kind2Hold
constexpr std::uint32_t kKind2X = 0x905E64;          // Field_Kind2X
constexpr std::uint32_t kKind2Z = 0x905E60;          // Field_Kind2Z
constexpr std::uint32_t kF3Divisor = 0x937F8C;       // MoveScript_F3Divisor
constexpr std::uint32_t kFAWord = 0x904EFE;          // MoveScript_FAWord
constexpr std::uint32_t kElevation = 0x929F1C;       // MapView_Elevation
constexpr std::uint32_t kRedraw = 0x905E69;          // MapView_Redraw
constexpr std::uint32_t kTextRecords = 0x904CE0;     // Text_Records
constexpr std::uint32_t kMessageFormat = 0x5E10C0;   // Area08_MessageFormat (area 8's, shared)
constexpr std::uint32_t kMessagePools = 0x803160;    // MessagePools

// --- area 135's .data (descriptor 0x62D940; symbols.toml [[data]]) ---

// A byte of the struct the descriptor's +8 points at (0x62D880 + 3): the tail
// sets and clears its bit 4.
constexpr std::uint32_t kArea135MapFlags = 0x62D883;
// The exits handler 3 leaves by: (x, z) byte pairs by the running script
// object's +3 less 6, the first table in zone 4, the second otherwise.
constexpr std::uint32_t kArea135ExitsZone4 = 0x62D984;   // Area135_ExitsZone4
constexpr std::uint32_t kArea135Exits = 0x62D98C;        // Area135_Exits
// Handler 4's sixteen 7-byte routes: the marker's cell (x, z), then a target
// x (half flag, cell), z (half flag, cell) and a direction; a cell of 0xFF
// is "no target".
constexpr std::uint32_t kArea135Routes = 0x62D994;       // Area135_Routes, 16 x 7
constexpr unsigned kArea135RouteCount = 16;
constexpr unsigned kArea135RouteStride = 7;
// Handler 14's four (x, z) cells (bytes; the positions are cell << 16 |
// 0x8000).
constexpr std::uint32_t kArea135JumpCells = 0x62DA04;    // Area135_JumpCells, 4 x 2
// The dispatchers' state tables, read in place by the running object's +4.
// They are contiguous and overlap as their dispatchers read them: the walk's
// 0x62DA18 runs on through 0x62DA20's and 0x62DA44's entries (13 code
// pointers to the zero at 0x62DA4C), the lift's 0x62DA20 through 0x62DA44's
// (11). Each dispatcher here reads as far as the original would reach code;
// past that ours aborts.
constexpr std::uint32_t kArea135QueueStates = 0x62DA0C;   // Area135_QueueStates, 2
constexpr unsigned kArea135QueueReach = 2;
// The queue's four row cells (bytes, << 16 | 0x8000), by the queue's count.
constexpr std::uint32_t kArea135QueueCells = 0x62DA14;    // Area135_QueueCells, 4
constexpr std::uint32_t kArea135WalkStates = 0x62DA18;    // Area135_WalkStates, 2 (13 reached)
constexpr unsigned kArea135WalkReach = 13;
constexpr std::uint32_t kArea135LiftStates = 0x62DA20;    // Area135_LiftStates, 9 (11 reached)
constexpr unsigned kArea135LiftReach = 11;
constexpr std::uint32_t kArea135SpawnStates = 0x62DA44;   // Area135_SpawnStates, 2
constexpr unsigned kArea135SpawnReach = 2;
// The spawn's event-op records (EventOp_9x, 13 bytes each) by the tail's sub
// byte.
constexpr std::uint32_t kArea135SpawnOps = 0x62DA50;      // Area135_SpawnOps, 13-byte records
constexpr unsigned kArea135SpawnOpStride = 13;
// The cell hook's two 4-byte entries: x, z, a direction (low nibble), the
// tail state it arms.
constexpr std::uint32_t kArea135CellEntries = 0x62DA84;   // Area135_CellEntries, 2 x 4
constexpr std::uint32_t kArea135CellEntriesEnd = 0x62DA8D;
// The init's EventOp_Bx record (zone 4, entered from zone 1).
constexpr std::uint32_t kArea135InitOp = 0x62DA8C;        // Area135_InitOp
// Handler 19's effect kinds by party list 1's byte (a table before the
// descriptor, 0x62C168; read in place, unchecked).
constexpr std::uint32_t kArea135EffectKinds = 0x62C168;   // Area135_EffectKinds

}  // namespace at
}  // namespace area_w3d
