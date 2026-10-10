// The raw addresses effect_4f.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_4f.md.
#pragma once

#include <cstdint>

#include "game/rdata_consts.h"
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_4f::at {

// --- callees nobody of ours names, called by address (SH_AT) ------------------
// Group E4E's (wave four, beside this one), raw until it merges:
constexpr std::uint32_t kGlowDraw = bof3::addr::EffectKindA0_DrawGlow;      // (const long *point, size word, colour byte): kind 0xB9's glow,
                                                   // a copy of EffectKind64_DrawGlow (a draw mode, 32 POLY_G3)
// Catalog part 6 (no group of round thirteen), read to its last instruction:
constexpr std::uint32_t kBarDraw = bof3::addr::EffectKindA8_DrawBar;       // (unsigned char *bar): one of kind 0xB0's sixteen bars of 6 drawn,
                                                   // two POLY_G4 from its +3 and +4; reads it to +5, writes nothing of it

// --- kind 0xAB: drops from eight sources (a copy of kind 0x81's, effect_3d) ---
constexpr std::uint32_t kDrops = 0x92BF80;         // 256 drops of 0x18 (EffectKind81's pool, over EffectKind30_Shards)
constexpr unsigned kDropCount = 256;
constexpr std::uint32_t kDropStride = 0x18;
constexpr std::uint32_t kSources = 0x92D780;       // 8 sources of 0x14, after the drops (kind 0x81 has 16 there)
constexpr unsigned kSourceCount = 8;
constexpr std::uint32_t kSourceStride = 0x14;
constexpr std::uint32_t kSourceCells = 0x6552AC;   // EffectKindAB_SourceCells: 8 pairs of s8, a source's cell (x, z)
constexpr rdata::Const kOne{0x5C41B8};           // float 1.0 (effect_3d_callees.h kOne; effect_3a's kHalf)
constexpr std::uint32_t kLeaderPoint = 0x802D74;   // ObjTrio + 0x34: the leader's x, z, height (three dwords)

// --- kind 0xB0: sixteen bars of 6 after kind 0xAB's sources --------------------
constexpr std::uint32_t kBars = 0x92D820;          // +0 in use, +1 / +3 4, +2 the bar's state, +4 a word
constexpr unsigned kBarCount = 16;
constexpr std::uint32_t kBarStride = 6;

// --- kind 0xB9: kind 0x64's pools (effect_3a_callees.h) ------------------------
constexpr std::uint32_t kSparks = 0x92BF80;        // 8 sparks of 0x18
constexpr unsigned kSparkCount = 8;
constexpr std::uint32_t kSparkStride = 0x18;
constexpr std::uint32_t kShards = 0x92C040;        // 16 shards of 0x2C
constexpr unsigned kShardCount = 0x10;
constexpr std::uint32_t kShardStride = 0x2C;
constexpr std::uint32_t kCounter = 0x903848;       // u8: the chapter's count (scenario_harness at::kCounter)

// --- kind 0xB1: the camera's angles (Camera_Angles + 2, the tilt) ---------------
constexpr std::uint32_t kTilt = 0x929ECA;          // s16: Camera_Angles[1]

// --- the pools' end (the fuzz's region) ------------------------------------------
constexpr std::uint32_t kPoolsEnd = kBars + kBarCount * kBarStride;   // 0x92D880

}  // namespace effect_4f::at
