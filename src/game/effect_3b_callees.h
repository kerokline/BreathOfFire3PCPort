// The raw addresses effect_3b.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_3b.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/rdata_consts.h"
// Rebound 2026-10-03 (round thirteen E4D): 0x48CA90 is ours, Effect_DrawScreenTint - the value
// unchanged, so the fuzz keys on it as before.

namespace effect_3b::at {

// --- callees nobody of this round's earlier waves owns, called by address (SH_AT) --
constexpr std::uint32_t kScreenTint = bof3::addr::Effect_DrawScreenTint;    // E4D's (wave four): void(void), a 320 x 240 semi-transparent
                                                   // TILE in Sprite_Current's +0x5D / +0x5E / +0x5F, committed to
                                                   // slot 5 after its draw mode (0xC + 0x1C of packet)
constexpr std::uint32_t kKind69Lines = bof3::addr::EffectKind69_DrawLines;   // in no group (catalog part 6, "Scenario effects"; PSX
                                                   // 0x801D218C call-disputed): void(void), Sprite_Current's
                                                   // +0xA - 1 LINE_G2s, writes 0x903850..0x90385F and 0x9037A0..

// --- the scratch cells (DamageScratch 0x903850 and after: the effect code's) --------
constexpr std::uint32_t kScale = 0x903850;         // dword: a radius / scale the states multiply by (re-read
                                                   // after each Math_* call); also kind 0x67's record byte
constexpr std::uint32_t kAngle = 0x903854;         // dword: the angle handed to Math_Sin (re-read for it)
constexpr std::uint32_t kShade = 0x903858;         // dword: kind 0x69's shade, 7 * +0xA (its low byte re-read)
constexpr std::uint32_t kCounter = 0x903848;       // byte: the chapter counter kind 0x63 raises and waits on
constexpr unsigned kCounterCue = 0x2B;             // kind 0x63's state 4 waits for it

// --- the vertex scratch kind 0x69's column builds its quads in ----------------------
constexpr std::uint32_t kV0 = 0x9037A0;            // Prim_VertexScratch: four SVECTORs of 8 bytes
constexpr std::uint32_t kV1 = 0x9037A8;
constexpr std::uint32_t kV2 = 0x9037B0;
constexpr std::uint32_t kV3 = 0x9037B8;
constexpr rdata::Const kCullY{0x5C41DC};         // a float constant (read in place) the column's quads are
                                                   // culled against: drawn while it is below the third corner's y

// --- kind 0x65 ------------------------------------------------------------------------
constexpr std::uint32_t kKind65Count = 0x904134;   // dword in the save block, tested for a multiple of 5
constexpr std::uint32_t kStoryFlags = 0x904030;    // Cond_Flags' story row (Flags_Toggle's bank)
constexpr unsigned kKind65Flag = 0x4F;             // the story flag kind 0x65 toggles
constexpr std::uint32_t kShakeSteps = 0x6549E8;    // EffectKind65_ShakeSteps: four signed bytes by +9 & 3

// --- kind 0x67 ------------------------------------------------------------------------
constexpr std::uint32_t kCameraAngle1 = 0x929ECA;  // Camera_Angles + 2 (s16)

// --- kind 0x69 ------------------------------------------------------------------------
constexpr std::uint32_t kParent = 0x676268;        // EffectKind69_Parent: the spawning record (a pointer)

// --- kind 0x6C: the sparks, 16 records of 0x28 at EffectKind30_Shards ------------------
constexpr std::uint32_t kSparkCursor = 0x67626C;   // the spark record the states act on (a pointer; E3C's
                                                   // kind 0x484050.. uses it too: left unnamed for E3C)
constexpr std::uint32_t kSparks = 0x92BF80;        // EffectKind30_Shards
constexpr std::uint32_t kSparkStride = 0x28;
constexpr unsigned kSparkCount = 16;

// --- pools ---------------------------------------------------------------------------
constexpr std::uint32_t kObjTrioStride = 0x14C;    // ObjTrio's three records
constexpr unsigned kMembers = 3;
constexpr std::uint32_t kSpriteStride = 0xA4;      // Sprite_Objects' thirty records
constexpr unsigned kSprites = 30;
constexpr std::uint32_t kEffectStride = 0x80;      // Effect_Objects' twenty records
constexpr unsigned kEffects = 20;

}  // namespace effect_3b::at
