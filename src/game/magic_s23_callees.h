// Internal to magic_s23.cpp and magic_s23_fuzz.cpp: the addresses the four
// overlays of group S23 touch that have no name in symbols.toml, and the raw
// addresses of the callees they reach in other units. docs/magic_s23.md.
//
// Calls into code this group does not own go through these raw addresses
// (MH_AT) and are never bound here:
//   0x446770  the record's (+0xC, +0x10) turned by its +8 (1, 2, 3 quarter
//             turns) - engine, in no queue group
//   0x4FBB40  Sprite_Current moved round a record by an angle - the effect
//             library (group L)
//   0x4FBC30  1 when Sprite_Current is within a box of a record - group L
//   0x4FC0E0  a side's centre to Sprite_Current - group L
// The stack tables also name 0x4DA3B0 (MAGIC118's) and 0x43FE80 (group E's
// engine row 128), and SimoonFan_Phases 0x4AE0D0 / 0x4AE0F0 (MAGIC056's):
// they are called as phases, by the address the table holds.
#pragma once

#include <cstdint>

namespace magic_s23 {

namespace at {

// The effect scratch (the PC's copy of the PSX scratchpad, DamageScratch):
// words the draws keep radii, colours and angle indices in.
constexpr std::uint32_t kR = 0x903850;       // s16: the outer radius (the draws), or a size (Simoon)
constexpr std::uint32_t kR2 = 0x903852;      // s16: the inner radius
constexpr std::uint32_t kW4 = 0x903854;      // u16: a colour, or the next angle index
constexpr std::uint32_t kW6 = 0x903856;      // u16: a colour, or the angle
constexpr std::uint32_t kW8 = 0x903858;      // u16: a colour, or the angle index
constexpr std::uint32_t kWA = 0x90385A;      // u16: the angle, or a colour
constexpr std::uint32_t kWC = 0x90385C;      // s16: SimoonDust's screen x (Scratch_Swap)
constexpr std::uint32_t kWE = 0x90385E;      // s16: its screen y
// Prim_VertexScratch: four SVECTORs, 8 bytes apart.
constexpr std::uint32_t kV0 = 0x9037A0;
constexpr std::uint32_t kV1 = 0x9037A8;
constexpr std::uint32_t kV2 = 0x9037B0;
constexpr std::uint32_t kV3 = 0x9037B8;

constexpr std::uint32_t kShiftY = 0x903802;        // Camera_ShiftY (s16)
constexpr std::uint32_t kActorSprite = 0x904B3C;   // unsigned char *: its +8 is the side's facing
constexpr std::uint32_t kEventBattle = 0x904AAA;   // u8

// Quake's own .bss (MAGIC102): the heights it keeps while it heaves the map.
constexpr std::uint32_t kQuakeEnemyZ = 0x695994;   // s16 x 8: an enemy's +0x3E before
constexpr std::uint32_t kQuakeEffectZ = 0x6959A4;  // s16 x 20: an effect object's +0x3E less the ground
constexpr std::uint32_t kQuakeFacing = 0x6959CC;   // u8: the facing 0..3 (bit 0 across, bit 1 the divisor row)
constexpr std::uint32_t kQuakeEnemyUp = 0x6959D0;  // s16 x 8: an enemy's +0x3E less the ground
constexpr std::uint32_t kQuakeSpriteZ = 0x6959E0;  // s16 x 30: Sprite_Objects' +0x3E less the ground
constexpr std::uint32_t kQuakeExtraZ = 0x695A1C;   // s16 x 4: Sprite_ObjectsExtra's
constexpr std::uint32_t kQuakePartyZ = 0x695A24;   // s16 x 3: the party's
constexpr std::uint32_t kQuakeLast = 0x695A2C;     // u8 x 16 x 15: a cell's lift one frame back
constexpr std::uint32_t kQuakeLift = 0x695B2C;     // u8 x 16 x 15: a cell's lift now
constexpr std::uint32_t kQuakeX = 0x695C2C;        // s16: the heaved block's first column
constexpr std::uint32_t kQuakeY = 0x695C2E;        // s16: its first row
constexpr std::uint32_t kQuakeHover = 0x695C30;    // u8: 1 for event battles 0x1C, 0x23, 0x2F, 0x34: an enemy above the ground keeps its height
constexpr std::uint32_t kCameraCellX = 0x905E66;   // s16: read as the column the view is on (beyond Camera_Matrix)
constexpr std::uint32_t kCameraCellY = 0x905E62;   // s16: the row

// The loaded area: AreaMap_Header's first dword (byte 0 the width, byte 1 the
// height), AreaMap_CellBase, and AreaMap_Corners (four bytes a cell).
constexpr std::uint32_t kArea = 0x8CB580;
constexpr std::uint32_t kAreaCellBase = 0x8CB5A4;
constexpr std::uint32_t kAreaCorners = 0x8CB5B0;
constexpr std::uint32_t kMapCells = 0x904F20;      // MapView_Cells, 1568 words
constexpr std::uint32_t kMapCellsEnd = 0x905B60;

// The objects Quake keeps on the ground.
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;   // 30 of 0xA4
constexpr std::uint32_t kSpriteExtra = 0x802000;     // 4 of 0xA4
constexpr std::uint32_t kEffectObjects = 0x7E11E0;   // 20 of 0x80

// The CLUT strip rows Simoon makes semi-transparent (as FxDiscFan_Start's).
constexpr std::uint32_t kClutRow = 0x812980;
constexpr std::uint32_t kClutSource = 0x80E980;

// The overlays' .data, read in place.
constexpr std::uint32_t kFunnelTypes = 0x65B75C;     // FxFunnel_Types, 1
constexpr std::uint32_t kFunnelPhases = 0x65B760;    // FxFunnel_Phases, 4
constexpr std::uint32_t kSpiralTypes = 0x65B770;     // FxSpiral_Types, 1
constexpr std::uint32_t kSpiralTurns = 0x65B774;     // FxSpiral_Turns, u16 x 16
constexpr std::uint32_t kSpiralTilts = 0x65B794;     // FxSpiral_Tilts, a byte every 2
constexpr std::uint32_t kSpiralPhases = 0x65B79C;    // FxSpiral_Phases, 5
constexpr std::uint32_t kQuakeOffsets = 0x65B7B0;    // Quake_FacingOffsets, s8 pairs x 4
constexpr std::uint32_t kQuakeDivisors = 0x65B7B8;   // Quake_Divisors, u8 x 30
constexpr std::uint32_t kSimoonTypes = 0x65B7D8;     // SimoonFx_Types, 3
constexpr std::uint32_t kDomePhases = 0x65B7E4;      // SimoonDome_Phases, 3
constexpr std::uint32_t kDustOffsets = 0x65B7F0;     // SimoonDust_Offsets, 2 x 3 dwords
constexpr std::uint32_t kDustPhases = 0x65B808;      // SimoonDust_Phases, 4
constexpr std::uint32_t kFanPhases = 0x65B818;       // SimoonFan_Phases, 4

}  // namespace at

// Raw addresses of callees and phases in other units (see the top).
constexpr std::uint32_t kTurnByFacing = 0x446770;
constexpr std::uint32_t kOrbitRecord = 0x4FBB40;
constexpr std::uint32_t kNearRecord = 0x4FBC30;
constexpr std::uint32_t kSideCentre = 0x4FC0E0;
constexpr std::uint32_t kTyphoonPhase2 = 0x4DA3B0;   // MAGIC118's
constexpr std::uint32_t kEnginePhase = 0x43FE80;     // group E's (Head Cracker's end)

using TurnFn = void (__cdecl*)(unsigned char*);
using OrbitFn = int (__cdecl*)(unsigned char*, int, int);
using NearFn = int (__cdecl*)(unsigned char*, int);
using VoidFn = void (__cdecl*)();

}  // namespace magic_s23
