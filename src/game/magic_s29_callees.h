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

namespace magic_s29 {

namespace cell {

// The effect scratch (DamageScratch, 0x903850..0x90385F): radii, colours, an
// angle, and a screen point in its last dword (Scratch_Swap).
constexpr std::uint32_t kScratch = 0x903850;
constexpr std::uint32_t kR = 0x903850;    // s16: the outer radius
constexpr std::uint32_t kR2 = 0x903852;   // s16: the inner radius, or a height
constexpr std::uint32_t kW4 = 0x903854;   // s16: an angle, or a shade
constexpr std::uint32_t kW6 = 0x903856;   // u16: red (its low byte is used)
constexpr std::uint32_t kW8 = 0x903858;   // u16: green
constexpr std::uint32_t kWA = 0x90385A;   // u16: blue
constexpr std::uint32_t kX = 0x90385C;    // s16: a screen x (Scratch_Swap)
constexpr std::uint32_t kY = 0x90385E;    // s16: a screen y
// Prim_VertexScratch: four SVECTORs, 8 bytes apart.
constexpr std::uint32_t kVertices = 0x9037A0;
constexpr std::uint32_t kV0 = 0x9037A0;
constexpr std::uint32_t kV1 = 0x9037A8;
constexpr std::uint32_t kV2 = 0x9037B0;
constexpr std::uint32_t kV3 = 0x9037B8;

constexpr std::uint32_t kPacketNext = 0x7E0670;   // Gfx_PacketNext
constexpr std::uint32_t kActorSprite = 0x904B3C;  // unsigned char *: the acting actor's sprite

// MAGIC125's pool (DivineMote_Pool): 64 task-shaped records of 0x84 (+0 bit 0
// in use, +0x80 the owner), run by DivineBreath_Task with Sprite_Current each.
constexpr std::uint32_t kMotePool = 0x69FC30;
constexpr unsigned kMotes = 64;
constexpr std::uint32_t kMoteStride = 0x84;
// MAGIC126's pool (ShadowMote_Pool): 128 records of 0x20 (+0 bit 0 in use,
// +0x1C the owner), run by ShadowBreath_Task with ShadowMote_Current each.
constexpr std::uint32_t kShadePool = 0x6A1D30;
constexpr unsigned kShades = 128;
constexpr std::uint32_t kShadeStride = 0x20;
constexpr std::uint32_t kShadeCurrent = 0x6A2D30;   // unsigned char *

// The CLUT strip row ShadowBreath_Start rewrites (row 26) and its source.
constexpr std::uint32_t kClutRow = 0x812980;
constexpr std::uint32_t kClutSource = 0x80E980;

}  // namespace cell

namespace tbl {

// The .data dispatch tables, read in place (symbols.toml [[data]]).
constexpr std::uint32_t kDivineKinds = 0x65BBA4;    // DivineBreathFx_Kinds, 2
constexpr std::uint32_t kBeamSteps = 0x65BBAC;      // DivineBeam_Steps, 3
constexpr std::uint32_t kBurstSteps = 0x65BBB8;     // DivineBurst_Steps, 3
constexpr std::uint32_t kMoteTask = 0x65BBC4;       // DivineMote_TaskTable, 1
constexpr std::uint32_t kMoteSteps = 0x65BBC8;      // DivineMote_Steps, 2
constexpr std::uint32_t kShadowKinds = 0x65BBD0;    // ShadowBreathFx_Kinds, 3
constexpr std::uint32_t kOrbSteps = 0x65BBDC;       // ShadowOrb_Steps, 4
constexpr std::uint32_t kGlowSteps = 0x65BBEC;      // ShadowGlow_Steps, 4
constexpr std::uint32_t kSeekerSteps = 0x65BBFC;    // ShadowSeeker_Steps, 4
constexpr std::uint32_t kSeekerColours = 0x65BC0C;  // ShadowSeeker_Colours, 8 pairs of bytes
constexpr std::uint32_t kShadeTask = 0x65BC1C;      // ShadowMote_TaskTable, 1
constexpr std::uint32_t kShadeSteps = 0x65BC20;     // ShadowMote_Steps, 4

}  // namespace tbl

// Other units' functions (above): the fuzz's keys for their stand-ins.
constexpr std::uint32_t kTurnByFacing = 0x446770;
constexpr std::uint32_t kFreeRecord = 0x4F6290;

}  // namespace magic_s29
