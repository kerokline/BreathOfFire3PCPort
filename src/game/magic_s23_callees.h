// Internal to magic_s23.cpp and magic_s23_fuzz.cpp: the addresses the four
// overlays of group S23 touch that have no name in symbols.toml, and the raw
// addresses of the callees they reach in other units. docs/magic_s23.md.
//
// Calls into code this group does not own:
//   0x446770  the record's (+0xC, +0x10) turned by its +8 (1, 2, 3 quarter
//             turns) - engine, in no queue group: by this raw address (MH_AT)
//   0x4FBB40  MagicFx_StepAround: Sprite_Current moved round a record by an
//             angle - the effect library (group L)
//   0x4FBC30  MagicFx_NearSprite: 1 when Sprite_Current is within a box of a
//             record - group L
//   0x4FC0E0  MagicFx_CenterOnSide: a side's centre to Sprite_Current - group L
// The three of group L are ours now and called by name (MH_CALL); their
// addresses stay below because the fuzz lists them by address (the harness
// maps the name to the same stand-in). The stack tables also name
// SpellFx_Countdown 0x4DA3B0 (MAGIC118's, group S27), MagicFx_DoneAndFree
// 0x43FE80 (group E's engine row 128), and SimoonFan_Phases 0x4AE0D0 /
// 0x4AE0F0 (MAGIC056's): they are called as phases, by the address the table
// holds.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace magic_s23 {

namespace at {

// The effect scratch (the PC's copy of the PSX scratchpad, DamageScratch):
// words the draws keep radii, colours and angle indices in.
// kR: now DamageScratch (symbols.toml). s16: the outer radius (the draws), or a size (Simoon)
constexpr std::uint32_t kR2 = 0x903852;      // s16: the inner radius
constexpr std::uint32_t kW4 = 0x903854;      // u16: a colour, or the next angle index
constexpr std::uint32_t kW6 = 0x903856;      // u16: a colour, or the angle
constexpr std::uint32_t kW8 = 0x903858;      // u16: a colour, or the angle index
constexpr std::uint32_t kWA = 0x90385A;      // u16: the angle, or a colour
// kWC: now Scratch_Swap (symbols.toml). s16: SimoonDust's screen x (Scratch_Swap)
constexpr std::uint32_t kWE = 0x90385E;      // s16: its screen y
// Prim_VertexScratch: four SVECTORs, 8 bytes apart.
// kV0: now Prim_VertexScratch (symbols.toml).
// kV1: now Prim_VertexScratch[4] (symbols.toml).
// kV2: now Prim_VertexScratch[8] (symbols.toml).
// kV3: now Prim_VertexScratch[0xc] (symbols.toml).

// kShiftY: now Camera_ShiftY (symbols.toml). Camera_ShiftY (s16)
// kActorSprite: now MagicS23_ActorSprite (symbols.toml). unsigned char *: its +8 is the side's facing
// kEventBattle: now EventBattle_Id (symbols.toml). u8

// Quake's own .bss (MAGIC102): the heights it keeps while it heaves the map.
constexpr std::uint32_t kQuakeEnemyZ = 0x695994;   // s16 x 8: an enemy's +0x3E before
constexpr std::uint32_t kQuakeEffectZ = 0x6959A4;  // s16 x 20: an effect object's +0x3E less the ground
// kQuakeFacing: now MagicS23_QuakeFacing (symbols.toml). u8: the facing 0..3 (bit 0 across, bit 1 the divisor row)
constexpr std::uint32_t kQuakeEnemyUp = 0x6959D0;  // s16 x 8: an enemy's +0x3E less the ground
constexpr std::uint32_t kQuakeSpriteZ = 0x6959E0;  // s16 x 30: Sprite_Objects' +0x3E less the ground
constexpr std::uint32_t kQuakeExtraZ = 0x695A1C;   // s16 x 4: Sprite_ObjectsExtra's
constexpr std::uint32_t kQuakePartyZ = 0x695A24;   // s16 x 3: the party's
constexpr std::uint32_t kQuakeLast = 0x695A2C;     // u8 x 16 x 15: a cell's lift one frame back
constexpr std::uint32_t kQuakeLift = 0x695B2C;     // u8 x 16 x 15: a cell's lift now
// kQuakeX: now MagicS23_QuakeX (symbols.toml). s16: the heaved block's first column
// kQuakeY: now MagicS23_QuakeY (symbols.toml). s16: its first row
// kQuakeHover: now MagicS23_QuakeHover (symbols.toml). u8: 1 for event battles 0x1C, 0x23, 0x2F, 0x34: an enemy above the ground keeps its height
constexpr std::uint32_t kCameraCellX = 0x905E66;   // s16: read as the column the view is on (beyond Camera_Matrix)
constexpr std::uint32_t kCameraCellY = 0x905E62;   // s16: the row

// The loaded area: AreaMap_Header's first dword (byte 0 the width, byte 1 the
// height), AreaMap_CellBase, and AreaMap_Corners (four bytes a cell).
// kArea: now AreaMap_Header (symbols.toml).
// kAreaCellBase: now AreaMap_CellBase (symbols.toml).
// kAreaCorners: now AreaMap_Corners (symbols.toml).
// kMapCells: now MapView_Cells (symbols.toml). MapView_Cells, 1568 words
constexpr std::uint32_t kMapCellsEnd = 0x905B60;

// The objects Quake keeps on the ground.
// kSpriteObjects: now Sprite_Objects (symbols.toml). 30 of 0xA4
// kSpriteExtra: now Sprite_ObjectsExtra (symbols.toml). 4 of 0xA4
// kEffectObjects: now Effect_Objects (symbols.toml). 20 of 0x80

// The CLUT strip rows Simoon makes semi-transparent (as FxDiscFan_Start's).
// kClutRow: now Gfx_ClutStrip[0x1a00] (symbols.toml).
// kClutSource: now Gfx_ClutStripSource[0x1a00] (symbols.toml).

// The overlays' .data, read in place.
// kFunnelTypes: now FxFunnel_Types (symbols.toml). FxFunnel_Types, 1
// kFunnelPhases: now FxFunnel_Phases (symbols.toml). FxFunnel_Phases, 4
// kSpiralTypes: now FxSpiral_Types (symbols.toml). FxSpiral_Types, 1
// kSpiralTurns: now FxSpiral_Turns (symbols.toml). FxSpiral_Turns, u16 x 16
// kSpiralTilts: now FxSpiral_Tilts (symbols.toml). FxSpiral_Tilts, a byte every 2
// kSpiralPhases: now FxSpiral_Phases (symbols.toml). FxSpiral_Phases, 5
// kQuakeOffsets: now Quake_FacingOffsets (symbols.toml). Quake_FacingOffsets, s8 pairs x 4
// kQuakeDivisors: now Quake_Divisors (symbols.toml). Quake_Divisors, u8 x 30
// kSimoonTypes: now SimoonFx_Types (symbols.toml). SimoonFx_Types, 3
// kDomePhases: now SimoonDome_Phases (symbols.toml). SimoonDome_Phases, 3
// kDustOffsets: now SimoonDust_Offsets (symbols.toml). SimoonDust_Offsets, 2 x 3 dwords
// kDustPhases: now SimoonDust_Phases (symbols.toml). SimoonDust_Phases, 4
// kFanPhases: now SimoonFan_Phases (symbols.toml). SimoonFan_Phases, 4

}  // namespace at

// Raw addresses of callees in other units (see the top): the engine's, called
// by it; group L's, the fuzz's keys for the stand-ins ours reaches by name.
constexpr std::uint32_t kTurnByFacing = bof3::addr::Battle_TurnVectorC;
constexpr std::uint32_t kOrbitRecord = bof3::addr::MagicFx_StepAround;    // MagicFx_StepAround
constexpr std::uint32_t kNearRecord = bof3::addr::MagicFx_NearSprite;     // MagicFx_NearSprite
constexpr std::uint32_t kSideCentre = bof3::addr::MagicFx_CenterOnSide;     // MagicFx_CenterOnSide

using TurnFn = void (__cdecl*)(unsigned char*);

}  // namespace magic_s23
