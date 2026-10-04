// The raw addresses effect_2d.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_2d.md.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_2d::at {

// Callees nobody owns this round (in no group of analysis/round13_cut.tsv; the
// scenario harness's effect-standard rows stand in for them), called through
// the harness by address (SH_AT). Kind 0x49's variants 0 and 3 run on them;
// read 2026-09-29 for what they take and answer (docs/effect_2d.md section 8).
constexpr std::uint32_t kPuffsClear = bof3::addr::EffectGlowSparks_Clear;    // 0x4790C0, R3E's: (void): byte +0 of the 8 puffs of 0x1C at EffectKind30_Shards
                                                   // = 0, and the two bytes 0x6761C8 / 0x6761C9 = 0
constexpr std::uint32_t kPuffFindFree = bof3::addr::EffectSpark_FindFree;  // (void): the first of those 8 puffs whose +0 is 0, or null (eax)
constexpr std::uint32_t kPuffStart = bof3::addr::EffectGlowSparks_StartRise;     // 0x4790F0, R3E's: (unsigned char *puff): its +0..+0x17 set from Rand and
                                                   // Sprite_Current's point
constexpr std::uint32_t kPuffsStep = bof3::addr::EffectGlowSparks_Run;     // 0x479260, R3E's: (void): a draw mode, the map camera, each live puff stepped
                                                   // through 0x654660 by its +1 and drawn; al 1 when any was live
constexpr std::uint32_t kGlowStep = bof3::addr::EffectGlowTrail_Update;      // 0x4794D0, R3E's: (unsigned char *glow): the glow record 0x92C060 stepped
constexpr std::uint32_t kGlowDraw = bof3::addr::EffectGlowTrail_Draw;      // 0x4796B0, R3E's: (unsigned char *glow): the glow record 0x92C060 drawn
constexpr std::uint32_t kDustClear = bof3::addr::EffectDust_Clear;     // 0x47A110, R3E's: (void): byte +0 of the 64 dust records of 0x20 at 0x92D1DC = 0
constexpr std::uint32_t kDustFindFree = bof3::addr::EffectDust_FindFree;  // 0x47A130, R3E's: (void): the first of those 64 whose +0 is 0, or null (eax)
constexpr std::uint32_t kDustStart = bof3::addr::EffectDust_Start;     // 0x47A150, R3E's: (unsigned char *dust): its record set from Sprite_Current and Rand
constexpr std::uint32_t kDustStep = bof3::addr::EffectDust_Run;      // 0x47A200, R3E's: (void): the live dust drawn and stepped; al 1 when any was live

// Data.
constexpr std::uint32_t kFlagRow = 0x929ED0;       // unsigned char *: the chapter's flag row (scenario_harness at::kFlagRow)
constexpr std::uint32_t kCounter0 = 0x903848;      // the chapters' counter byte (scenario_harness at::kCounter)
constexpr std::uint32_t kPanelColour = 0x6761C4;  // three bytes: the blue, green and red of kind 0x45's lines and
                                                   // rectangles (committed at +6, +5, +4)
constexpr std::uint32_t kDustAnchor = 0x6761D0;    // a pointer (0x4789D0, kind 0x49's variant 4, sets it to 0x92D1C8):
                                                   // variant 3's burst adds to the word at +0x10 through it
constexpr std::uint32_t kDust = 0x92D1DC;          // variant 3's dust, 64 records of 0x20
constexpr std::uint32_t kGlow = 0x92C060;          // the glow record variant 2's states hand 0x4794D0 / 0x4796B0
constexpr std::uint32_t kGlowHeight = 0x92C49E;    // a word of that record's that variant 2's states move
constexpr std::uint32_t kTwinOps = 0x654540;       // kind 0x47's two EventOp_0x operands, 17 bytes each, by +0xB
constexpr std::uint32_t kTwinAnims = 0x654564;     // kind 0x47's five (animation byte, animation, start) triples
constexpr std::uint32_t kBurstMask = 0x6545EC;     // variant 3's per-step bit masks, a byte by +9
constexpr std::uint32_t kBurstCount = 0x6545F4;    // variant 3's per-step dust counts, a byte by +9
constexpr std::uint32_t kObject2 = 0x7DEFC8;       // Sprite_Objects record 2 (its +0 and +0x4B, as the leader's)
constexpr std::uint32_t kEffectStride = 0x80;      // Effect_Objects' records
constexpr unsigned kEffects = 20;                  // Effect_Objects' twenty
constexpr std::uint32_t kSpriteStride = 0xA4;      // Sprite_Objects' records
constexpr unsigned kSprites = 30;                  // Sprite_Objects' thirty
constexpr std::uint32_t kSparkStride = 0x18;       // kind 0x48's sparks at EffectKind30_Shards
constexpr unsigned kSparks = 16;
constexpr unsigned kHistory = 36;                  // kind 0x45's samples at EffectKind30_Shards (dwords)

}  // namespace effect_2d::at
