// The raw addresses effect_4d.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_4d.md.
// Every callee of the group is ours (by name) or the group's own, but one: the
// library layer's 0x5A7C70, which nobody owns.
#pragma once

#include <cstdint>

namespace effect_4d::at {

// --- library layer (nobody's), the effect-standard set lists it ------------------
constexpr std::uint32_t kMatrixVector = 0x5A7C70;   // (matrix, in, out): an SVECTOR turned by the 3 x 3 (18 bytes read),
                                                    // 6 bytes written; in and out may be one (effect_1c_callees.h)

// --- cells ------------------------------------------------------------------------
constexpr std::uint32_t kStep = 0x8034E5;           // the chapter's step byte (scenario_harness kStep): kind 0x91
                                                    // raises it, kinds 0x95 and 0x97 set it
constexpr std::uint32_t kCounter = 0x903848;        // the chapter counter byte kinds 0x97 and 0x9A wait on
constexpr unsigned kKind97Cue = 0xA;                // kind 0x97's state 2 waits for it
constexpr unsigned kKind9AEnd = 0x28;               // kind 0x9A ends at it
constexpr std::uint32_t kTrio0Mode = 0x802E77;      // ObjTrio record 0's +0x137 (kind 0x95's finish waits while 2)
constexpr std::uint32_t kTrio2Mode = 0x80310F;      // ObjTrio record 2's +0x137 by its address (kind 0x95's clock
                                                    // waits while 2)
constexpr std::uint32_t kTrioBusy = 0x89;           // ObjTrio +0x89: kind 0x94 redraws a member whose byte is 7
constexpr unsigned kTrioCount = 3;                  // ObjTrio's records of 0x14C
constexpr std::uint32_t kTrioStride = 0x14C;
constexpr std::uint32_t kKind96Shade = 0x676298;    // byte: kind 0x96's colour level (r and b of its quad)
constexpr std::uint32_t kKind96Index = 0x676299;    // byte: kind 0x96's index into EffectKind96_Shades (0..7)
constexpr std::uint32_t kKind95Clock = 0x67629A;    // u16: kind 0x95's clock - low byte the ticks, high byte bit 7
                                                    // a mode (40 frames a tick, else 30), bits 0..6 the frames
constexpr std::uint32_t kSprite1 = 0x7DEF24;        // Sprite_Objects record 1, which kind 0x9A nudges

// --- the pools in EffectKind30_Shards kind 0x97 uses ---------------------------------
constexpr unsigned kSparkCount = 8;                 // eight 0x18-byte sparks at EffectKind30_Shards
constexpr std::uint32_t kSparkStride = 0x18;
constexpr std::uint32_t kDebris = 0x92C040;         // 32 debris records of 0x2C after the sparks (Shards + 0xC0)
constexpr unsigned kDebrisCount = 32;
constexpr std::uint32_t kDebrisStride = 0x2C;

}  // namespace effect_4d::at
