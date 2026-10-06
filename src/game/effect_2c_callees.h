// The raw addresses effect_2c.cpp calls or reads - each a load-bearing
// constant (CLAUDE.md rule 3). The cells and tables this group named in
// symbols.toml ([[data]]) carry their names in the comment: a [[data]] name is
// a macro of symbols.gen.h, which bof3::addr:: cannot spell. docs/effect_2c.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace effect_2c::at {

// Callees nobody owns this round (read 2026-09-29), called through the harness
// by address (SH_AT).
constexpr std::uint32_t kStoreImage = bof3::addr::Gfx_StoreImage;   // ours since group PH (d3d_rest.cpp); the rectangle (x, y, w, h, s16)
                                                   // of Gfx_VramShadow copied row by row to `to`, w * 2 bytes a row
constexpr std::uint32_t kWinding = bof3::addr::Screen_TriangleWinding;       // (const float *a, const float *b, const float *c): the cross
                                                   // product's z of a->b, b->c of three screen points, through _ftol
                                                   // (a tail jmp); the callers test ax (docs/effect_gte.md section 7)
constexpr std::uint32_t kSetPolyF3 = bof3::addr::Gpu_SetPolyF3;     // (unsigned char *prim): a POLY_F3's code (0x20) and its three
                                                   // depth floats (libgpu SetPolyF3 by shape; docs/magic_s21.md)

// Data of the image, read in place.
constexpr std::uint32_t kBoltCells = 0x654470;    // EffectKind3E_BoltCells: four bolts of four cell bytes
constexpr std::uint32_t kFrameX = 0x6544A8;    // EffectKind6B_Frame: s16 x, y (where the sprite is
constexpr std::uint32_t kFrameY = 0x6544AA;          //   drawn), w, h (the rectangle read back
constexpr std::uint32_t kFrameW = 0x6544AC;          //   from 0x340, 0x100)
constexpr std::uint32_t kFrameH = 0x6544AE;
constexpr std::uint32_t kSparkColours = 0x6544EC;    // EffectKind44_SparkColours: eight dwords
constexpr std::uint32_t kOne = 0x5C41B8;           // float constants: 1.0,
constexpr std::uint32_t kSixteen = 0x5C41D0;       //   16.0,
constexpr std::uint32_t kHalf = 0x5C41D8;          //   0.5,
constexpr std::uint32_t kZero = 0x5C41DC;          //   0.0,
constexpr std::uint32_t kSixteenth = 0x5C41E0;     //   0.0625

// Cells the kinds keep outside their records.
constexpr std::uint32_t kShardCursor = 0x67610C;    // EffectKind40_ShardCursor: kind 0x40's shard being stepped
constexpr std::uint32_t kPixelCount = 0x676110;    // EffectKind6B_PixelCount: kind 0x6B's particles (u16)
constexpr std::uint32_t kHeights = 0x676114;    // EffectKind6B_Heights: 80 s16, the lowest pixel of
constexpr unsigned kHeightCount = 80;                                           //   each column (the 0xA0 bytes cleared)
constexpr std::uint32_t kSparkSide = 0x6761B4;    // EffectKind43_SparkSide: kind 0x43's next side (0 / 1)
constexpr std::uint32_t kSparkCursor = 0x6761B8;    // EffectKind44_SparkCursor: kind 0x44's spark being stepped
constexpr std::uint32_t kRingCell = 0x6761C0;    // EffectKind44_RingCell: kind 0x44's ring (0x92BF80)

// The records those cells walk, inside and after EffectKind30_Shards 0x92BF80.
constexpr std::uint32_t kShards = 0x92BF80;   // EffectKind30_Shards: kind 0x40's 32 of 0x18; kind 0x43's 256
                                                                    // of 0x1C; kind 0x44's ring; kind 0x6B's pixels read
                                                                    // back (w * h u16)
constexpr std::uint32_t kShardStride = 0x18;
constexpr unsigned kShardCount = 32;
constexpr std::uint32_t kSparkStride43 = 0x1C;
constexpr unsigned kSparkCount43 = 256;
constexpr std::uint32_t kDiscCentre = 0x92C280;    // kind 0x40: the disc's screen centre (three floats)
constexpr std::uint32_t kDiscRim = 0x92C28C;       //   and its 32 rim points (three floats each)
constexpr std::uint32_t kSparks44 = 0x92C07C;      // kind 0x44: 8 spark records of 0xA0
constexpr std::uint32_t kSparkStride44 = 0xA0;
constexpr unsigned kSparkCount44 = 8;
constexpr std::uint32_t kPixels = 0x92EC80;        // kind 0x6B: the particles, 0x14 each
constexpr std::uint32_t kPixelStride = 0x14;

// Sprite_Objects records 1 and 2 (0x7DEE80 + 0xA4 k) and their points (+0x34).
constexpr std::uint32_t kSprite1Point = 0x7DEF58;
constexpr std::uint32_t kSprite2 = 0x7DEFC8;
constexpr std::uint32_t kSprite2Point = 0x7DEFFC;

constexpr std::uint32_t kCounter = 0x903848;       // the chapters' counter byte (scenario_harness at::kCounter)
constexpr std::uint32_t kEffectStride = 0x80;      // Effect_Objects' records
constexpr unsigned kEffects = 20;

}  // namespace effect_2c::at
