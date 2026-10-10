// The raw addresses effect_3a.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_3a.md.
#pragma once

#include <cstdint>

#include "game/rdata_consts.h"

#include "bof3/symbols.gen.h"   // bof3::addr::<Name> for group E4F's two below

namespace effect_3a::at {

// --- callees nobody of ours names, called by address (SH_AT) ------------------
// Capcom's library layer and renderer (the effect-standard set lists both):
constexpr std::uint32_t kStoreImage = bof3::addr::Gfx_StoreImage;   // (const short *rect, void *to): the VRAM rectangle (x, y, w, h,
                                                   // s16) read back into `to` (effect_2c_callees.h kStoreImage)
constexpr std::uint32_t kMatrixVector = bof3::addr::Gte_ApplyMatrixSV;  // (matrix, in, out): an SVECTOR turned by the 3 x 3
                                                   // (effect_2e_callees.h kMatrixVector)
// Group E4F's (wave four), rebound to its names (the values unchanged):
constexpr std::uint32_t kSparkSpawn = bof3::addr::EffectKindB9_SpawnSpark;    // 0x493B50, (void): the first free of the eight sparks at 0x92BF80 in use at
                                                   // Sprite_Current's point, life 0x10, size 0x100, colours 0x40 / 0
constexpr std::uint32_t kShardDraw = bof3::addr::EffectKindB9_DrawShard;     // 0x493C60, (unsigned char *shard): one shard of 0x2C (EffectKind64_InitShard's)
                                                   // drawn, a POLY_G3; reads it to +0x2B, writes nothing of it

// --- the shared buffer at 0x92BF80 (EffectKind30_Shards and after) -----------
constexpr std::uint32_t kShards = 0x92BF80;        // EffectKind30_Shards: the pools below all start here
// kind 0x61: the read-back pixels (w x h u16) here, the particles after them
constexpr std::uint32_t kParticles = 0x92DF80;     // kind 0x61's particles, 0x14 each, w x h at most (no bound)
constexpr std::uint32_t kParticleStride = 0x14;
// kind 0x62: 24 rays of 0x38
constexpr unsigned kRays = 0x18;
constexpr std::uint32_t kRayStride = 0x38;
// kind 0x64: 8 sparks of 0x18 here; 16 shards of 0x2C at 0x92C040
constexpr unsigned kSparks = 8;
constexpr std::uint32_t kSparkStride = 0x18;
constexpr std::uint32_t kShards64 = 0x92C040;
constexpr unsigned kShardCount64 = 0x10;
constexpr std::uint32_t kShardStride64 = 0x2C;
// kind 0x68: 16 motes of 0x1C
constexpr unsigned kMotes = 0x10;
constexpr std::uint32_t kMoteStride = 0x1C;

// --- the kinds' cells (nothing else in the image reads them) -------------------
constexpr std::uint32_t kMoteFlagB = 0x676260;     // u8: set by kind 0x68's mote state 2 (EffectKind68_ClearMotes clears)
constexpr std::uint32_t kMoteFlagA = 0x676261;     // u8: set by kind 0x68's mote state 0
constexpr std::uint32_t kParticleCount = 0x676264; // u16: kind 0x61's particles made
constexpr std::uint32_t kVariant = 0x676266;       // u8: kind 0x61's frame, 0 or 1 (the member's +0x148 not 0)
constexpr std::uint32_t kCellsSize = 8;            // 0x676260..0x676267

// --- kind 0x61's frames: two records of 8 (s16 x, y, w, h) in .data -----------
constexpr std::uint32_t kFrames = 0x654948;        // EffectKind61_Frames
constexpr unsigned kFrameCount = 2;

// --- the leader and the cells the states read ------------------------------------
constexpr std::uint32_t kObjTrioStride = 0x14C;    // ObjTrio's three records
constexpr unsigned kObjTrioCount = 3;
constexpr std::uint32_t kMemberFlag = 0x148;       // ObjTrio record +0x148 (Field_Members' byte)
constexpr std::uint32_t kLeaderPoint = 0x802D74;   // ObjTrio + 0x34: the leader's x, z, height (three dwords)
constexpr std::uint32_t kMessageWord = 0x7DEE48;   // u16: the message on screen (effect_2g_callees.h kMessageWord)
constexpr std::uint32_t kStep = 0x8034E5;          // u8: the chapter's step (scenario_harness at::kStep)
constexpr std::uint32_t kCounter = 0x903848;       // u8: the chapter's count (scenario_harness at::kCounter)

// --- .rdata -----------------------------------------------------------------------
constexpr rdata::Const kHalf{0x5C41B8};          // float: kind 0x64's trail half-width (the fifteen groups' float)

}  // namespace effect_3a::at
