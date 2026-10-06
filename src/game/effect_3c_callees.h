// The raw addresses effect_3c.cpp and its fuzz call or read that symbols.toml
// does not name - each a load-bearing constant (CLAUDE.md rule 3).
// docs/effect_3c.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
// Rebound 2026-10-03 (round thirteen E4D): 0x48CA90 is ours, Effect_DrawScreenTint - the value
// unchanged, so the fuzz keys on it as before.

namespace effect_3c::at {

// --- callees another group of round thirteen owns (analysis/round13_cut.tsv),
// called through the harness by address (SH_AT) until the coordinator rebinds
// them ---------------------------------------------------------------------------
constexpr std::uint32_t kShardsSpread = bof3::addr::EffectKind6C_ScatterSparks;   // E3B's (void): the sixteen 0x28-byte shard records at
                                                    // EffectKind30_Shards set round Sprite_Current's point (+0x34 /
                                                    // +0x38 / +0x3C), Rand directions; steps the cursor kShardCursor
constexpr std::uint32_t kShardQuad = bof3::addr::EffectKind6C_DrawSpark;      // E3B's (unsigned char *shard): a shard's POLY_FT4 at Gfx_PacketNext,
                                                    // its point +4, its size +0x24 (EffectGte_ProjectSize), linked
constexpr std::uint32_t kScreenTile = bof3::addr::Effect_DrawScreenTint;     // E4D's (void): a full-screen TILE of Sprite_Current's +0x5D..+0x5F,
                                                    // semi-transparent, committed (Gfx_CommitPrim(5, ..))

// --- library layer (nobody's), the effect-standard set lists both --------------
constexpr std::uint32_t kMatrixVector = bof3::addr::Gte_ApplyMatrixSV;   // (matrix, in, out): an SVECTOR turned by the 3 x 3 (18 bytes read),
                                                    // 6 bytes written; in and out may be one (effect_1c_callees.h)
constexpr std::uint32_t kPolyF3 = bof3::addr::Gpu_SetPolyF3;         // (unsigned char *prim): a flat triangle's 0x2C bytes set up

// --- cells ------------------------------------------------------------------------
constexpr std::uint32_t kShardCursor = 0x67626C;    // unsigned char *: the 0x28-byte shard kinds 0x6C (E3B's) and 0x6E
                                                    // step (E3B's code writes it too: named by neither group yet)
constexpr std::uint32_t kSparkCursor = 0x676274;    // unsigned char *: the 0x18-byte spark kind 0x73 steps (only this
                                                    // band's code reads or writes it)
constexpr std::uint32_t kParticles = 0x92C780;      // kind 0x6D: 32 records of 0x28 per block, the block +6 * 0x500
constexpr std::uint32_t kParticleBlock = 0x500;
constexpr unsigned kParticleCount = 32;
constexpr std::uint32_t kParticleStride = 0x28;
constexpr unsigned kShardCount = 16;                // kind 0x6E: 16 records of 0x28 at EffectKind30_Shards
constexpr std::uint32_t kShardStride = 0x28;
constexpr unsigned kDebrisCount = 32;               // kind 0x72: 32 records of 0x2C at EffectKind30_Shards
constexpr std::uint32_t kDebrisStride = 0x2C;
constexpr unsigned kSparkCount = 32;                // kind 0x73: 32 records of 0x18 at EffectKind30_Shards
constexpr std::uint32_t kSparkStride = 0x18;

// --- .data read in place --------------------------------------------------------
constexpr std::uint32_t kParticleShapes = 0x654A5C; // kind 0x6D: eight rows of four s16 (two corner offsets), by +4
constexpr unsigned kParticleShapeCount = 8;
constexpr std::uint32_t kKind6FColour = 0x654A9C;   // kind 0x6F: one row of three colour bytes (r, g, b), by +6 * 3;
                                                    // the next row would run into EffectKind6F_States
constexpr unsigned kKind6FColourCount = 1;
constexpr std::uint32_t kKind6FCellsZ = 0x654AB0;   // kind 0x6F: four z cells (bytes) along x 0x5C, EffectKind6F_PushParty(0)
constexpr std::uint32_t kKind6FCellsX = 0x654AB4;   // kind 0x6F: four x cells (bytes) along z 0x44, EffectKind6F_PushParty(1)
constexpr std::uint32_t kStoryFlags = 0x904030;     // Cond_Flags' story row (Flags_Test's bank)

// --- ranges -----------------------------------------------------------------------
constexpr unsigned kObjTrioCount = 3;               // ObjTrio's records of 0x14C
constexpr std::uint32_t kObjTrioStride = 0x14C;
constexpr unsigned kSpriteCount = 30;               // Sprite_Objects' records of 0xA4
constexpr std::uint32_t kSpriteStride = 0xA4;

}  // namespace effect_3c::at
