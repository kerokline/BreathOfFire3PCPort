// Internal to magic_s24.cpp and magic_s24_fuzz.cpp: the addresses group S24's
// three overlays touch that are not the group's own functions, and the cells
// they read and write. docs/magic_s24.md.
//
// Calls into code the group does not own:
//   0x4FC0E0  MagicFx_CenterOnSide, the targeted side's centre: the mean
//             position of every actor of the side byte 0x904B44's 0x40 bit
//             names that is not out, into Sprite_Current +0x34 / +0x38 / +0x3E
//             (the effect library, group L). Ours now and called by name; the
//             address below stays as the fuzz's key for its stand-in.
//   0x446770  a task's offset +0xC / +0x10 turned by its facing +8 (1, 2, 3:
//             a quarter, a half, three quarters): engine code outside the band,
//             in no group
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace magic_s24 {

constexpr std::uint32_t kCentreOnTargets = bof3::addr::MagicFx_CenterOnSide;   // MagicFx_CenterOnSide (the fuzz's key)
constexpr std::uint32_t kTurnByFacing = bof3::addr::Battle_TurnVectorC;

namespace cell {

// kActorRecord: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's record (its +8 the facing)
// kSpriteBank: now BattleE2_AnimSet (symbols.toml). unsigned char *: the sprite bank Sprite_* read (0x8B3580 the default)
constexpr std::uint32_t kClutRow = 0x812980;       // Gfx_ClutStrip + 0x3400: strip row 26
constexpr std::uint32_t kClutSource = 0x80E980;    // kClutRow - 0x4000

// The scratch words the draws keep their radii, colours and vertices in.
constexpr std::uint32_t kScratch = 0x903850;       // SharedScratch: words +0 .. +0xE
// kVertices: now Prim_VertexScratch (symbols.toml). Prim_VertexScratch: four SVECTORs, +0 +8 +0x10 +0x18

// MAGIC105's mote pool and MAGIC106's spark pool.
constexpr std::uint32_t kMotePool = 0x695C38;      // 64 records of 0x84, bit 0 of +0 "in use"
constexpr std::uint32_t kMoteStride = 0x84;
constexpr unsigned kMotes = 0x40;
// kSparkPool: now Fx106_SparkPool (symbols.toml). 96 records of 0x20, bit 0 of +0 "in use"
constexpr std::uint32_t kSparkStride = 0x20;
constexpr unsigned kSparks = 0x60;
// kSparkCurrent: now Fx106_SparkCurrent (symbols.toml). unsigned char *: the spark being run

}  // namespace cell

// The overlays' .data (symbols.toml names them; the generated names are
// macros for the data, so the addresses are here).
namespace tbl {

// kChildOffsets: now Fx104_ChildOffsets (symbols.toml). Fx104_ChildOffsets: 8 (x, z) pairs of s32
// kChildPhases104: now Fx104_ChildPhases (symbols.toml). Fx104_ChildPhases: 2 handlers
// kWhirlPhases: now Fx104_WhirlPhases (symbols.toml). Fx104_WhirlPhases: 4
// kBurstPhases: now Fx104_BurstPhases (symbols.toml). Fx104_BurstPhases: 2
// kChildPhases105: now Fx105_ChildPhases (symbols.toml). Fx105_ChildPhases: 1
// kOrbAngles: now Fx105_OrbAngles (symbols.toml). Fx105_OrbAngles: s16 by the orb index
// kOrbColours: now Fx105_OrbColours (symbols.toml). Fx105_OrbColours: u8, rows of 3 read 6 wide
// kMotePhases: now Fx105_MotePhases (symbols.toml). Fx105_MotePhases: 1
// kMoteRunPhases: now Fx105_MoteRunPhases (symbols.toml). Fx105_MoteRunPhases: 2
// kMoteSizes: now Fx105_MoteSizes (symbols.toml). Fx105_MoteSizes: 16 (w, h) bytes
// kSparkPhases: now Fx106_SparkPhases (symbols.toml). Fx106_SparkPhases: 1
// kSparkRunPhases: now Fx106_SparkRunPhases (symbols.toml). Fx106_SparkRunPhases: 4
// kShardTables: now Fx106_ShardTables (symbols.toml). Fx106_ShardTables: s16 radii x2, s16 angles x2 (4 each), 4 u8 tilts

}  // namespace tbl

}  // namespace magic_s24
