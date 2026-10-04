// The raw addresses effect_2f.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_2f.md.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_2f::at {

// Callees nobody owns yet (read 2026-09-29 for what they read and write):
// called through the harness by address (SH_AT).
constexpr std::uint32_t kAngleMean = bof3::addr::EffectAngle_Mean;     // E2E (wave two): (a, b), each & 0xFFF; eax the mean angle of the
                                                   // two, the short way round (+0x800 when they are 0x800 or more apart)
constexpr std::uint32_t kSparkInit = 0x4790F0;     // nobody's (catalog part 7): (unsigned char *spark): +0 = 1, +1 = 0,
                                                   // +2 = 8, +3 = 0, +8 = 0, +4 = 0x40, +0xC / +0x10 Sprite_Current's
                                                   // +0x34 / +0x38 plus ((Rand & 0xFF) - 0x80) << 11, +0x14 its +0x3C
constexpr std::uint32_t kSparkDraw = 0x4792E0;     // nobody's (catalog part 7): (unsigned char *spark): sixteen Gouraud
                                                   // triangles round the spark's point +0xC (EffectGte_ProjectSize of a
                                                   // size 0x20, EffectGte_ProjectPoint), shaded +3 at the centre
constexpr std::uint32_t kSqrt = 0x5A7A90;          // library layer: (long v): fild, fsqrt, _ftol - eax the root

// Library layer (no name): (unsigned char *prim, const unsigned char *rect):
// prim +4 = 0xF0000000, prim +8 = rect's address.
constexpr std::uint32_t kPrimRect = 0x5A7840;

// Data.
constexpr std::uint32_t kCounter0 = 0x903848;      // the chapters' counter byte (scenario_harness at::kCounter)
constexpr std::uint32_t kCounter3 = 0x90384B;      // the counter byte three after it: kind 0x56 waits on 0xA and 0xE
constexpr std::uint32_t kTrail = 0x92C060;         // kind 0x52's trail: 32 points of 0x20 (+0 x, z, height; +0x10
                                                   // the screen x, y, depth floats; +0x1C an angle, +0x1E a width),
                                                   // then 31 angle words at +0x400 and the width word +0x43E - 0x440
                                                   // bytes inside EffectKind30_Shards' extent (not named: it overlaps)
constexpr std::uint32_t kTrailSize = 0x92C49E;     // the trail's width word (+0x43E)
constexpr std::uint32_t kExtra0Point = 0x802034;   // Sprite_ObjectsExtra[0] +0x34: x, z, height (three dwords)
constexpr std::uint32_t kEffectStride = 0x80;      // Effect_Objects' records
constexpr unsigned kEffects = 20;                  // Effect_Objects' twenty
constexpr std::uint32_t kSpriteStride = 0xA4;      // Sprite_Objects' and Sprite_ObjectsExtra's records
constexpr unsigned kSprites = 30;                  // Sprite_Objects' thirty
constexpr unsigned kExtras = 4;                    // Sprite_ObjectsExtra's four
constexpr std::uint32_t kSpeckStride = 0x28;       // kind 0x50's specks: 64 at EffectKind30_Shards
constexpr unsigned kSpecks = 64;
constexpr std::uint32_t kSparkStride = 0x1C;       // kind 0x52's sparks: 8 at EffectKind30_Shards
constexpr unsigned kSparks = 8;
constexpr unsigned kAngles = 0x41;                 // EffectKind51_Angles' words

}  // namespace effect_2f::at
