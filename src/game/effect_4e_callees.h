// The raw addresses effect_4e.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_4e.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"  // R3F's rebinding: the targets that are ours read bof3::addr::<Name>, the values unchanged

namespace effect_4e::at {

// --- callees nobody of ours names, called by address (SH_AT) ------------------
// Catalog part 6 rows ("Scenario effects"), in no group of round thirteen:
constexpr std::uint32_t kTrail9C = bof3::addr::EffectKind9C_DrawTrail;     // (const long *from, const long *to, unsigned char shade): two
                                                 // POLY_G4 and TILE_1 dots between the two points projected
                                                 // (the effect-standard row: both points 12 read, the byte)
constexpr std::uint32_t kGlowA7 = bof3::addr::EffectKindA7_DrawGlow;      // (const long *point, short size, unsigned char colour): a glow
                                                 // of 32 POLY_G3 (EffectKindA0_DrawGlow's form, slot 7)
constexpr std::uint32_t kDiscA9 = bof3::addr::EffectKindA9_DrawDisc;      // (short x, short y, unsigned char shade): a disc of POLY_G3 on
                                                 // screen (two words read, the third pushed 0x80)
// Capcom's library layer (the effect-standard set lists it):
constexpr std::uint32_t kMatrixVector = bof3::addr::Gte_ApplyMatrixSV;  // (matrix, in, out): an SVECTOR turned by the 3 x 3
                                                   // (effect_3a_callees.h kMatrixVector)

// --- the image's cells ---------------------------------------------------------
constexpr std::uint32_t kLeaderPoint = 0x802D74;   // ObjTrio + 0x34: the leader's x, z, height (three dwords)
constexpr std::uint32_t kCounter = 0x903848;       // u8: the chapter's count (scenario_harness at::kCounter)
constexpr std::uint32_t kHalf = 0x5C41B8;          // float (.rdata): the trail's half-width (effect_3a_callees.h kHalf)

// --- kind 0xA0's two pools (nothing else in the image reads them) --------------
constexpr std::uint32_t kSparks = 0x6762B0;        // EffectKindA0_Sparks: 16 sparks of 0x18 (EffectKind64's form)
constexpr unsigned kSparkCount = 0x10;
constexpr std::uint32_t kSparkStride = 0x18;
constexpr std::uint32_t kShards = 0x676430;        // EffectKindA0_Shards: 32 shards of 0x2C, two runs of 16
constexpr std::uint32_t kShardsB = 0x6766F0;       // the second run of 16 (kShards + 16 * 0x2C)
constexpr unsigned kShardCount = 0x20;
constexpr unsigned kShardRun = 0x10;
constexpr std::uint32_t kShardStride = 0x2C;
constexpr std::uint32_t kPoolsSize = 0x700;        // 0x6762B0..0x6769AF

}  // namespace effect_4e::at
