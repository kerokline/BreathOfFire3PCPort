// Internal to magic_s24.cpp and magic_s24_fuzz.cpp: the addresses group S24's
// three overlays touch that are not the group's own functions, and the cells
// they read and write. docs/magic_s24.md.
//
// Calls into code the group does not own go through the raw addresses below,
// never bound here (the round's rule for calls across groups):
//   0x4FC0E0  the targeted side's centre: the mean position of every actor of
//             the side byte 0x904B44's 0x40 bit names that is not out, into
//             Sprite_Current +0x34 / +0x38 / +0x3E (the effect library, group L)
//   0x446770  a task's offset +0xC / +0x10 turned by its facing +8 (1, 2, 3:
//             a quarter, a half, three quarters): engine code outside the band,
//             in no group
#pragma once

#include <cstdint>

namespace magic_s24 {

constexpr std::uint32_t kCentreOnTargets = 0x4FC0E0;
constexpr std::uint32_t kTurnByFacing = 0x446770;

namespace cell {

constexpr std::uint32_t kActorRecord = 0x904B3C;   // unsigned char *: the acting actor's record (its +8 the facing)
constexpr std::uint32_t kSpriteBank = 0x9039D8;    // unsigned char *: the sprite bank Sprite_* read (0x8B3580 the default)
constexpr std::uint32_t kClutRow = 0x812980;       // Gfx_ClutStrip + 0x3400: strip row 26
constexpr std::uint32_t kClutSource = 0x80E980;    // kClutRow - 0x4000

// The scratch words the draws keep their radii, colours and vertices in.
constexpr std::uint32_t kScratch = 0x903850;       // DamageScratch: words +0 .. +0xE
constexpr std::uint32_t kVertices = 0x9037A0;      // Prim_VertexScratch: four SVECTORs, +0 +8 +0x10 +0x18

// MAGIC105's mote pool and MAGIC106's spark pool.
constexpr std::uint32_t kMotePool = 0x695C38;      // 64 records of 0x84, bit 0 of +0 "in use"
constexpr std::uint32_t kMoteStride = 0x84;
constexpr unsigned kMotes = 0x40;
constexpr std::uint32_t kSparkPool = 0x697D38;     // 96 records of 0x20, bit 0 of +0 "in use"
constexpr std::uint32_t kSparkStride = 0x20;
constexpr unsigned kSparks = 0x60;
constexpr std::uint32_t kSparkCurrent = 0x698938;  // unsigned char *: the spark being run

}  // namespace cell

// The overlays' .data (symbols.toml names them; the generated names are
// macros for the data, so the addresses are here).
namespace tbl {

constexpr std::uint32_t kChildOffsets = 0x65B828;     // Fx104_ChildOffsets: 8 (x, z) pairs of s32
constexpr std::uint32_t kChildPhases104 = 0x65B868;   // Fx104_ChildPhases: 2 handlers
constexpr std::uint32_t kWhirlPhases = 0x65B870;      // Fx104_WhirlPhases: 4
constexpr std::uint32_t kBurstPhases = 0x65B880;      // Fx104_BurstPhases: 2
constexpr std::uint32_t kChildPhases105 = 0x65B888;   // Fx105_ChildPhases: 1
constexpr std::uint32_t kOrbAngles = 0x65B88C;        // Fx105_OrbAngles: s16 by the orb index
constexpr std::uint32_t kOrbColours = 0x65B89C;       // Fx105_OrbColours: u8, rows of 3 read 6 wide
constexpr std::uint32_t kMotePhases = 0x65B8B4;       // Fx105_MotePhases: 1
constexpr std::uint32_t kMoteRunPhases = 0x65B8B8;    // Fx105_MoteRunPhases: 2
constexpr std::uint32_t kMoteSizes = 0x65B8C0;        // Fx105_MoteSizes: 16 (w, h) bytes
constexpr std::uint32_t kSparkPhases = 0x65B8E0;      // Fx106_SparkPhases: 1
constexpr std::uint32_t kSparkRunPhases = 0x65B8E4;   // Fx106_SparkRunPhases: 4
constexpr std::uint32_t kShardTables = 0x65B8F4;      // Fx106_ShardTables: s16 radii x2, s16 angles x2 (4 each), 4 u8 tilts

}  // namespace tbl

}  // namespace magic_s24
