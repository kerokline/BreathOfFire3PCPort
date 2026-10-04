// The raw addresses effect_4c.cpp and its fuzz call or read that symbols.toml
// does not name - each a load-bearing constant (CLAUDE.md rule 3).
// docs/effect_4c.md.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_4c::at {

// --- callees another group of round thirteen owns (analysis/round13_cut.tsv),
// called through the harness by address (SH_AT) until the coordinator rebinds
// them ---------------------------------------------------------------------------
constexpr std::uint32_t kScreenTile = bof3::addr::Effect_DrawScreenTint;     // E4D's (void): a full-screen TILE of Sprite_Current's +0x5D..+0x5F,
                                                    // semi-transparent, committed (Gfx_CommitPrim(5, ..)); kind 0x8D's
                                                    // states 1 and 2 jump to it

// --- library layer (nobody's) ---------------------------------------------------
constexpr std::uint32_t kPolyF3 = 0x5A7570;         // (unsigned char *prim): a flat triangle's code byte +7 0x20 and its
                                                    // three depth floats set (effect_3c_callees.h)

// --- cells ------------------------------------------------------------------------
constexpr std::uint32_t kVar7Step = 0x8034E5;       // the byte after MoveScript_Var7: kind 0x8D's last state steps it
                                                    // (area_w1a_callees.h's kVar7Step)
constexpr std::uint32_t kLeaderLift = 0x802D88;     // ObjTrio + 0x48 of each of the three party records (0x14C apart):
constexpr std::uint32_t kObjStride = 0x14C;         // kind 0x8D's start sets each byte 1 (area_w3e_callees.h's kLeader48)
constexpr std::uint32_t kStage = 0x676290;          // kind 0x8F: the byte its state 7 keeps (0..4, by the frame word +0x2E)
constexpr std::uint32_t kTag = 0x676294;            // kind 0x90: the dword its state 0 counts up; each record's +6 is its low
                                                    // byte, and the shards it emits carry it in +0
constexpr std::uint32_t kShardEnd = 0x92E580;       // the end of kind 0x8E's two pools (below)

// --- the pools at EffectKind30_Shards (0x92BF80) --------------------------------
constexpr unsigned kChipCount = 128;                // kind 0x8E: 128 chips of 0x28 from EffectKind30_Shards (to 0x92D380)
constexpr std::uint32_t kChipStride = 0x28;
constexpr std::uint32_t kDots = 0x92D380;           // kind 0x8E: 128 dots of 0x24 from here (to 0x92E580)
constexpr unsigned kDotCount = 128;
constexpr std::uint32_t kDotStride = 0x24;
constexpr unsigned kShard8FCount = 16;              // kind 0x8F: 16 shards of 0x28 (E3C's EffectKind6E_FindShard finds them)
constexpr unsigned kShard90Count = 32;              // kinds 0x90 / 0x93: 32 shards of 0x28
constexpr std::uint32_t kShardStride = 0x28;

// --- .data read in place --------------------------------------------------------
constexpr std::uint32_t kChipShapes = 0x654FC0;     // kind 0x8E: eight rows of four s16 (two corner offsets), by +4
constexpr unsigned kChipShapeCount = 8;
constexpr std::uint32_t kShardStarts = 0x655000;    // kind 0x8F: sixteen rows of two s16 (x, z offsets << 8), by index
constexpr std::uint32_t kShardSpeeds = 0x655040;    // kind 0x8F: sixteen rows of two s16 (x, z speeds << 5), by index
constexpr unsigned kShardRowCount = 16;
constexpr std::uint32_t kStageCounts = 0x6550A4;    // kind 0x8F: a byte per stage (0..4), how many shards it spawns
constexpr std::uint32_t kStageWaits = 0x6550AC;     // kind 0x8F: a byte per stage, the frames to the next spawn
constexpr unsigned kStageCount = 5;                 // the stages state 7 computes (0..4)
constexpr std::uint32_t kFanDx = 0x6550E0;          // kind 0x99: four dwords, each fan's link offset in x
constexpr std::uint32_t kFanDz = 0x6550F0;          // kind 0x99: four dwords, each fan's link offset in z
constexpr unsigned kFanCount = 4;

}  // namespace effect_4c::at
