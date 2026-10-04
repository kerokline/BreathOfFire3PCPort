// The raw addresses effect_1d.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_1d.md.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round fourteen's rebinding (R3E, docs/rest_3e.md section 8): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_1d::at {

// Callees nobody owns this round (catalog part 6, "Scenario event banks" by
// their PSX twin's section; read 2026-09-29 for what they read and write):
// called through the harness by address (SH_AT).
constexpr std::uint32_t kArmVertices = bof3::addr::EffectKind21_ArmPoints;   // 0x46F570, R3E's: (unsigned char *arm): arm = record + 0xC. Reads the words
                                                   // +0x4C (an angle), +0x4E, +0x50 (two turns), +0x52 (a length)
                                                   // and the dwords +0..+8 (the centre); writes four points of 0x10
                                                   // at +0xC..+0x4B. EffectGte_SetDiagonalOne, Math_*, 0x5A7F80,
                                                   // 0x5A7FF0, 0x5A7CF0
constexpr std::uint32_t kArmDraw = bof3::addr::EffectKind21_DrawArm;       // 0x46F690, R3E's: (unsigned char *arm): a draw mode, EffectGte_LoadMapCamera,
                                                   // then 0x46F6F0 twice - two Gouraud triangles of the four points
constexpr std::uint32_t kRayDraw = bof3::addr::EffectKind24_DrawRay;       // 0x46FAE0, R3E's: (const unsigned char *ray): ray = record + 0xC. Reads the words
                                                   // +0 (a length), +2 / +4 (two angles), +6 / +8 (two scales) and
                                                   // the bytes +0xA..+0xC (a colour); four Gouraud lines from the
                                                   // leader's point (ObjTrio +0x34..+0x3C). Writes nothing of it

// Data.
constexpr std::uint32_t kCounter0 = 0x903848;      // the chapters' counter byte (scenario_harness at::kCounter):
                                                   // EffectKind22_WaitArms adds 1 when its arms are done
constexpr std::uint32_t kLeaderX = 0x802D74;       // ObjTrio record 0 (the leader) +0x34, x (16.16)
constexpr std::uint32_t kLeaderZ = 0x802D78;       // +0x38, z
constexpr std::uint32_t kLeaderHeight = 0x802D7E;  // +0x3E, the height's integer word
constexpr std::uint32_t kEffectStride = 0x80;      // Effect_Objects' records
constexpr unsigned kEffects = 20;                  // Effect_Objects' twenty

}  // namespace effect_1d::at
