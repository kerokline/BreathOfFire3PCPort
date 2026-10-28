// The raw addresses effect_2c.cpp calls or reads - each a load-bearing
// constant (CLAUDE.md rule 3). The cells and tables this group named in
// symbols.toml ([[data]]) carry their names in the comment: a [[data]] name is
// a macro of symbols.gen.h, which bof3::addr:: cannot spell. docs/effect_2c.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/rdata_consts.h"

namespace effect_2c::at {

// Callees nobody owns this round (read 2026-09-29), called through the harness
// by address (SH_AT).
constexpr ::bof3::image::CodeVa kStoreImage{bof3::addr::Gfx_StoreImage};   // ours since group PH (d3d_rest.cpp); the rectangle (x, y, w, h, s16)
                                                   // of Gfx_VramShadow copied row by row to `to`, w * 2 bytes a row
constexpr ::bof3::image::CodeVa kWinding{bof3::addr::Screen_TriangleWinding};       // (const float *a, const float *b, const float *c): the cross
                                                   // product's z of a->b, b->c of three screen points, through _ftol
                                                   // (a tail jmp); the callers test ax (docs/effect_gte.md section 7)
constexpr ::bof3::image::CodeVa kSetPolyF3{bof3::addr::Gpu_SetPolyF3};     // (unsigned char *prim): a POLY_F3's code (0x20) and its three
                                                   // depth floats (libgpu SetPolyF3 by shape; docs/magic_s21.md)

// Data of the image, read in place.
// kBoltCells: now EffectKind3E_BoltCells (symbols.toml). EffectKind3E_BoltCells: four bolts of four cell bytes
// kFrameX: now EffectKind6B_Frame (symbols.toml). EffectKind6B_Frame: s16 x, y (where the sprite is
// kFrameY: now EffectKind6B_Frame[1] (symbols.toml). drawn), w, h (the rectangle read back
constexpr ::bof3::image::Va kFrameW{0x6544AC};          //   from 0x340, 0x100)
constexpr ::bof3::image::Va kFrameH{0x6544AE};
constexpr ::bof3::image::Va kSparkColours{0x6544EC};    // EffectKind44_SparkColours: eight dwords
constexpr rdata::Const kOne{0x5C41B8};           // float constants: 1.0,
constexpr rdata::Const kSixteen{0x5C41D0};       //   16.0,
constexpr rdata::Const kHalf{0x5C41D8};          //   0.5,
constexpr rdata::Const kZero{0x5C41DC};          //   0.0,
constexpr rdata::Const kSixteenth{0x5C41E0};     //   0.0625

// Cells the kinds keep outside their records.
constexpr ::bof3::image::Va kShardCursor{0x67610C};    // EffectKind40_ShardCursor: kind 0x40's shard being stepped
// kPixelCount: now EffectKind6B_PixelCount (symbols.toml). EffectKind6B_PixelCount: kind 0x6B's particles (u16)
constexpr ::bof3::image::Va kHeights{0x676114};    // EffectKind6B_Heights: 80 s16, the lowest pixel of
constexpr unsigned kHeightCount = 80;                                           //   each column (the 0xA0 bytes cleared)
// kSparkSide: now EffectKind43_SparkSide (symbols.toml). EffectKind43_SparkSide: kind 0x43's next side (0 / 1)
constexpr ::bof3::image::Va kSparkCursor{0x6761B8};    // EffectKind44_SparkCursor: kind 0x44's spark being stepped
constexpr ::bof3::image::Va kRingCell{0x6761C0};    // EffectKind44_RingCell: kind 0x44's ring (0x92BF80)

// The records those cells walk, inside and after EffectKind30_Shards 0x92BF80.
constexpr ::bof3::image::Va kShards{0x92BF80};   // EffectKind30_Shards: kind 0x40's 32 of 0x18; kind 0x43's 256
                                                                    // of 0x1C; kind 0x44's ring; kind 0x6B's pixels read
                                                                    // back (w * h u16)
constexpr std::uint32_t kShardStride = 0x18;
constexpr unsigned kShardCount = 32;
constexpr std::uint32_t kSparkStride43 = 0x1C;
constexpr unsigned kSparkCount43 = 256;
constexpr ::bof3::image::Va kDiscCentre{0x92C280};    // kind 0x40: the disc's screen centre (three floats)
// kDiscRim: now EffectKind30_Shards[0x30c] (symbols.toml). and its 32 rim points (three floats each)
// kSparks44: now EffectKind30_Shards[0xfc] (symbols.toml). kind 0x44: 8 spark records of 0xA0
constexpr std::uint32_t kSparkStride44 = 0xA0;
constexpr unsigned kSparkCount44 = 8;
constexpr ::bof3::image::Va kPixels{0x92EC80};        // kind 0x6B: the particles, 0x14 each
constexpr std::uint32_t kPixelStride = 0x14;

// Sprite_Objects records 1 and 2 (0x7DEE80 + 0xA4 k) and their points (+0x34).
// kSprite1Point: now Sprite_Objects[1].x (symbols.toml).
constexpr ::bof3::image::Va kSprite2{0x7DEFC8};
// kSprite2Point: now Sprite_Objects[2].x (symbols.toml).

constexpr ::bof3::image::Va kCounter{0x903848};       // the chapters' counter byte (scenario_harness at::kCounter)
constexpr std::uint32_t kEffectStride = 0x80;      // Effect_Objects' records
constexpr unsigned kEffects = 20;

}  // namespace effect_2c::at
