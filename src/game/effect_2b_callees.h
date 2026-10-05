// The raw addresses effect_2b.cpp and its fuzz call or read: callees nobody
// owns, and the pools and cells the seven kinds keep their parts in
// (effect_2b.cpp reads the state tables by name; the fuzz lists them by
// address) - each a load-bearing constant (CLAUDE.md rule 3).
// docs/effect_2b.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace effect_2b::at {

// Callees nobody owns (the library layer), called through the harness by
// address (SH_AT).
constexpr std::uint32_t kSetPolyF3 = bof3::addr::Gpu_SetPolyF3;   // (unsigned char *prim): POLY_F3's code 0x20 at +7 and the three z
                                                 // at +0x10, +0x1C, +0x28 (0.01f); unnamed (magic_s04.cpp, magic_s21.cpp)
constexpr std::uint32_t kSqrt = bof3::addr::Gte_SquareRoot0;        // (long): an integer square root through _ftol, answered in eax
                                                 // (magic_c3.cpp's kSqrt)

// The pools. EffectKind30_Shards 0x92BF80 is shared by every kind that keeps
// parts: each of this group's kinds lays its own records over it.
constexpr std::uint32_t kPool = 0x92BF80;          // EffectKind30_Shards
constexpr std::uint32_t kTrails = 0x92BF80;        // kind 0x2F: three trail records of 0x198 - the head (+0 x, +4 z,
constexpr std::uint32_t kTrailStride = 0x198;      // +8 height, +0x10 radius, +0x14 an angle word) and 32 projected
constexpr std::uint32_t kTrailCount = 3;           // points of three floats from +0x18
constexpr std::uint32_t kShards35 = 0x92BF80;      // kind 0x35: sixteen shards of 0x28 (kind 0x1E's shape)
constexpr std::uint32_t kShard35Count = 16;
constexpr std::uint32_t kPieces35 = 0x92C200;      // kind 0x35: 55 piece records of 0x18 (the centre, its direction,
constexpr std::uint32_t kPiece35Count = 55;        // the velocity), after the shards
constexpr std::uint32_t kCopies35 = 0x92C728;      // kind 0x35: 55 face copies of 0x28, after the pieces, to 0x92CFC0
constexpr std::uint32_t kPool35End = 0x92CFC0;
constexpr std::uint32_t kSpirals = 0x92BF80;       // kind 0x3B: three spiral records of 0x4BC (+0 x, +4 z, +8 height,
constexpr std::uint32_t kSpiralStride = 0x4BC;     // +0x10 angle, +0x12 radius, +0x14 shade - words), to 0x92CDB4
constexpr std::uint32_t kSpiralCount = 3;
constexpr std::uint32_t kRings = 0x92BF80;         // kind 0x3D: sixteen ring records of 0x18 (+0 x, +4 z, +8 height,
constexpr std::uint32_t kRingStride = 0x18;        // +0x10 width, +0x12 half-thickness - words -, +0x14 in use, +0x15
constexpr std::uint32_t kRingCount = 16;           // state, +0x16 count, +0x17 shade)

// Cells.
constexpr std::uint32_t kExtra1Use = 0x8020A4;     // Sprite_ObjectsExtra[1] +0: kind 0x35's model record in use
constexpr std::uint32_t kExtra1Model = 0x8020F4;   // Sprite_ObjectsExtra[1] +0x50: kind 0x35's model (55 faces of 0x28)
constexpr std::uint32_t kLeaderPoint = 0x802D74;   // ObjTrio +0x34: the leader's x, z, height (kind 0x3D's glow and rings)

// The state tables (named in symbols.toml; the fuzz lists them by address).
constexpr std::uint32_t kKind2FStates = 0x6543D8;   // EffectKind2F_States, 2 (then kind 0x33's)
constexpr std::uint32_t kKind33States = 0x6543E0;   // EffectKind33_States, 4 (then EffectKind33_SignX)
constexpr std::uint32_t kKind33SignX = 0x6543F0;    // EffectKind33_SignX: four longs, 1 / -1
constexpr std::uint32_t kKind33SignZ = 0x654400;    // EffectKind33_SignZ: four longs, 1 / -1
constexpr std::uint32_t kKind35States = 0x654410;   // EffectKind35_States, 3 (then the shard states)
constexpr std::uint32_t kKind35ShardStates = 0x65441C;   // EffectKind35_ShardStates, 2, by a shard's +1
constexpr std::uint32_t kKind38States = 0x654424;   // EffectKind38_States, 5
constexpr std::uint32_t kKind39States = 0x654438;   // EffectKind39_States, 4
constexpr std::uint32_t kKind3BStates = 0x654448;   // EffectKind3B_States, 4
constexpr std::uint32_t kKind3DStates = 0x654458;   // EffectKind3D_States, 4 (then kind 0x3E's, 0x654468)
constexpr std::uint32_t kShardCursor35 = 0x676108;  // EffectKind35_ShardCursor

}  // namespace effect_2b::at
