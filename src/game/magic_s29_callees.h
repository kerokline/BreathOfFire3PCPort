// Internal to magic_s29.cpp and magic_s29_fuzz.cpp: the cells group S29's two
// overlays (MAGIC125, MAGIC126) keep, the .data tables they dispatch through,
// and the raw addresses of the callees they reach in units this group does not
// own. docs/magic_s29.md.
//
// Calls into code this group does not own (MH_AT, or a .data table read in
// place):
//   0x446770  the record's (+0xC, +0x10) turned by its +8 - engine, in no
//             queue group (as magic_s22 / magic_s23 / magic_s24 call it): by
//             this raw address
//   0x4F6290  MagicFx_FreeCurrentRecord: Sprite_Current's bytes 0..4 cleared
//             (a pool record freed) - MAGIC219, group S37: ours now and called
//             by name; the constant below stays as the fuzz's key
// and, as entries of ShadowOrb_Steps / ShadowGlow_Steps (called as phases, by
// the address the table holds):
//   0x4ADB50  the task put at its owner's point, +9 +0xA 0, +2 on - MAGIC056 (S10)
//   0x4B1740  +9 down; at 0 the owner's +0xB down and the task freed - MAGIC060 (S12)
//   0x4A5D20  +9 and +0xA up; +2 on at +0xA 0x10 - MAGIC040 (S07)
//   0x4A5D50  +9 up, +0xA down; at 0 the owner's +0xB down, freed - MAGIC040 (S07)
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace magic_s29 {

namespace cell {

// The effect scratch (SharedScratch, 0x903850..0x90385F): radii, colours, an
// angle, and a screen point in its last dword (Scratch_Swap).
constexpr std::uint32_t kScratch = 0x903850;
constexpr std::uint32_t kR = 0x903850;    // s16: the outer radius
constexpr std::uint32_t kR2 = 0x903852;   // s16: the inner radius, or a height
constexpr std::uint32_t kW4 = 0x903854;   // s16: an angle, or a shade
constexpr std::uint32_t kW6 = 0x903856;   // u16: red (its low byte is used)
constexpr std::uint32_t kW8 = 0x903858;   // u16: green
constexpr std::uint32_t kWA = 0x90385A;   // u16: blue
// kX: now Scratch_Swap (symbols.toml). s16: a screen x (Scratch_Swap)
constexpr std::uint32_t kY = 0x90385E;    // s16: a screen y
// Prim_VertexScratch: four SVECTORs, 8 bytes apart.
// kVertices: now Prim_VertexScratch (symbols.toml).
// kV0: now Prim_VertexScratch (symbols.toml).
constexpr std::uint32_t kV1 = 0x9037A8;
// kV2: now Prim_VertexScratch[8] (symbols.toml).
// kV3: now Prim_VertexScratch[0xc] (symbols.toml).

// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
// kActorSprite: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite

// MAGIC125's pool (DivineMote_Pool): 64 task-shaped records of 0x84 (+0 bit 0
// in use, +0x80 the owner), run by DivineBreath_Task with Sprite_Current each.
constexpr std::uint32_t kMotePool = 0x69FC30;
constexpr unsigned kMotes = 64;
constexpr std::uint32_t kMoteStride = 0x84;
// MAGIC126's pool (ShadowMote_Pool): 128 records of 0x20 (+0 bit 0 in use,
// +0x1C the owner), run by ShadowBreath_Task with ShadowMote_Current each.
// kShadePool: now ShadowMote_Pool (symbols.toml).
constexpr unsigned kShades = 128;
constexpr std::uint32_t kShadeStride = 0x20;
// kShadeCurrent: now ShadowMote_Current (symbols.toml). unsigned char *

// The CLUT strip row ShadowBreath_Start rewrites (row 26) and its source.
constexpr std::uint32_t kClutRow = 0x812980;
constexpr std::uint32_t kClutSource = 0x80E980;

}  // namespace cell

namespace tbl {

// The .data dispatch tables, read in place (symbols.toml [[data]]).
// kDivineKinds: now DivineBreathFx_Kinds (symbols.toml). DivineBreathFx_Kinds, 2
// kBeamSteps: now DivineBeam_Steps (symbols.toml). DivineBeam_Steps, 3
// kBurstSteps: now DivineBurst_Steps (symbols.toml). DivineBurst_Steps, 3
// kMoteTask: now DivineMote_TaskTable (symbols.toml). DivineMote_TaskTable, 1
// kMoteSteps: now DivineMote_Steps (symbols.toml). DivineMote_Steps, 2
// kShadowKinds: now ShadowBreathFx_Kinds (symbols.toml). ShadowBreathFx_Kinds, 3
// kOrbSteps: now ShadowOrb_Steps (symbols.toml). ShadowOrb_Steps, 4
// kGlowSteps: now ShadowGlow_Steps (symbols.toml). ShadowGlow_Steps, 4
// kSeekerSteps: now ShadowSeeker_Steps (symbols.toml). ShadowSeeker_Steps, 4
// kSeekerColours: now ShadowSeeker_Colours (symbols.toml). ShadowSeeker_Colours, 8 pairs of bytes
// kShadeTask: now ShadowMote_TaskTable (symbols.toml). ShadowMote_TaskTable, 1
// kShadeSteps: now ShadowMote_Steps (symbols.toml). ShadowMote_Steps, 4

}  // namespace tbl

// Other units' functions (above): the fuzz's keys for their stand-ins.
constexpr std::uint32_t kTurnByFacing = bof3::addr::Battle_TurnVectorC;
constexpr std::uint32_t kFreeRecord = bof3::addr::MagicFx_FreeCurrentRecord;

}  // namespace magic_s29
