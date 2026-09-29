// The raw addresses effect_1c.cpp and its fuzz call or read: callees nobody
// owns or another group of round thirteen owns, and the data (effect_1c.cpp
// reads the tables by name where symbols.toml has one; the fuzz lists them by
// address) - each a load-bearing constant (CLAUDE.md rule 3).
// docs/effect_1c.md.
#pragma once

#include <cstdint>

namespace effect_1c::at {

// Callees another group of round thirteen owns (analysis/round13_cut.tsv),
// called through the harness by address (SH_AT) until the coordinator rebinds
// them; or nobody's.
constexpr std::uint32_t kShardSpawn = 0x471D10;   // E2A's (void): a free record of EffectKind30_Shards' 0x80 (+0 == 0) set
                                                  // round Sprite_Current's point - +0 1, +1 0x40, +2 a word 0x1000 +
                                                  // Rand & 0xFFF, +4 / +8 x / z at a random angle and Rand % the radius
                                                  // +0x2E, +0xC the height + (+0x30 << 16); al the index, 0x80 none
constexpr std::uint32_t kShardTile = 0x471E20;    // E2A's (unsigned char *shard): a white or black TILE_1 at the shard's
                                                  // point (+4..+0xF, EffectGte_ProjectPoint), linked at its x, z
constexpr std::uint32_t kShardTile2 = 0x46E190;   // nobody's this round (catalog part 6, PSX twin 0x801F752C): the same
                                                  // TILE_1 committed by Gfx_CommitPrim(1, 0x14) instead of linked
constexpr std::uint32_t kDebrisDraw = 0x485030;   // E3C's (unsigned char *debris): a G3 of the 0x2C-byte debris record,
                                                  // its point +0, two edges +0x10 / +0x18 turned by +0x24 and scaled by
                                                  // +0x28, the shade +0x2A clamped to a byte; Gfx_CommitPrim(1, 0x34)
constexpr std::uint32_t kDebrisInit = 0x4851E0;   // E3C's (unsigned char *debris): EffectKind1E_DebrisInitOne with the
                                                  // edges' angle 0x20 and the scale 8 + Rand % 8 (0x2C bytes written)
constexpr std::uint32_t kCone = 0x493090;         // E4F's (const long *point, w, h, angle - three s16 -, shade byte,
                                                  // flag byte): G3 triangles round the point's screen position, sized
                                                  // by EffectGte_ProjectSize (w, h) and turned by the angle
constexpr std::uint32_t kMatrixVector = 0x5A7C70; // library layer (nobody's): (matrix, in, out) - an SVECTOR turned by
                                                  // the 3 x 3 (18 bytes read), 6 bytes written; in and out may be one

// Data.
constexpr std::uint32_t kKind1CStates = 0x654210;   // EffectKind1C_States, 5 code pointers (then kind 0x1D's)
constexpr std::uint32_t kKind1DStates = 0x654224;   // EffectKind1D_States, 5
constexpr std::uint32_t kKind1EStates = 0x654238;   // EffectKind1E_States, 5 (the last Effect_StateRelease)
constexpr std::uint32_t kKind1EShardStates = 0x65424C;   // EffectKind1E_ShardStates, 4, by a shard's +1
constexpr std::uint32_t kKind1FStates = 0x65425C;   // EffectKind1F_States, 3 (the last Effect_StateRelease)
constexpr std::uint32_t kKind20States = 0x654268;   // EffectKind20_States, 7 (then kind 0x21's, E1D's)
constexpr std::uint32_t kKind36Frames = 0x653F88;   // EffectKind36_Frames: four (s16 x, s16 y) VRAM origins
constexpr std::uint32_t kShards = 0x92BF80;         // EffectKind30_Shards: 0x80 records of 0x14 (kinds 0x1C / 0x1D), or
                                                    // 8 of 0x28 (kind 0x1E's shards), or 32 of 0x2C (kind 0x1F's debris)
constexpr std::uint32_t kShardCount = 0x80;
constexpr std::uint32_t kShardStride = 0x14;
constexpr std::uint32_t kPieces = 0x92C0C0;         // kind 0x1E: 27 piece records of 0x18 (the centre, its direction,
                                                    // the velocity), then at 0x92C348 27 face copies of 0x28
constexpr std::uint32_t kPieceCopies = 0x92C348;
constexpr std::uint32_t kPieceCount = 27;
constexpr std::uint32_t kDebris = 0x92C780;         // kind 0x1E: 16 debris records of 0x2C, to 0x92CA40
constexpr std::uint32_t kShardCursor = 0x675FDC;    // EffectKind1E_ShardCursor: the shard kind 0x1E's shard
                                                    // states step (a pointer)
constexpr std::uint32_t kModelFaces = 0x802050;     // Sprite_ObjectsExtra[0] +0x50: the model's faces (0x28 bytes each)
constexpr std::uint32_t kModelCount = 0x802054;     // Sprite_ObjectsExtra[0] +0x54: points at the face count (a byte)

}  // namespace effect_1c::at
