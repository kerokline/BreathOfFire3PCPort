// The raw addresses effect_6a.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_6a.md.
// Every callee of the group is ours (by name) or the group's own; nothing is
// called by a raw address. The constants below are cells and the image's
// read-only tables the states index (read in place, never copied).
#pragma once

#include <cstdint>

namespace effect_6a::at {

// --- the leader (ObjTrio record 0) -------------------------------------------------
constexpr std::uint32_t kLeaderX = 0x802D74;        // ObjTrio +0x34: the leader's x, 16.16
constexpr std::uint32_t kLeaderZ = 0x802D78;        // ObjTrio +0x38: its z, 16.16

// --- the variant tables, indexed by the spawn's x cell (+0x36, movsx) ---------------
// Two bytes a variant: the cell x, z put into +0x36, +0x3A. The room is the
// space to the next table (sub-kinds 0x2E..0x32: to the two-byte sign pair
// that follows each, which no code of the band reads); ours aborts past it.
constexpr std::uint32_t kSub2DCells = 0x65EAC4;     // sub-kind 0x2D: room 16 (to its heights)
constexpr std::uint32_t kSub2DHeights = 0x65EAE4;   // sub-kind 0x2D: s16 each, +0x3E; room 16 (to its signs)
constexpr unsigned kSub2DVariants = 16;
constexpr std::uint32_t kSub2DSides = 0x65EB04;     // sub-kind 0x2D: s8 by +0xA, the slide's sign (room four,
                                                    // before EffectKind18Sub3E_States)
constexpr unsigned kSidesRoom = 4;
constexpr std::uint32_t kSub3ECells = 0x65EB1C;     // sub-kind 0x3E: room 10 (to EffectKind18Sub2E_States)
constexpr unsigned kSub3EVariants = 10;
constexpr std::uint32_t kSub2ECells = 0x65EB44;     // sub-kind 0x2E: room 4
constexpr unsigned kSub2EVariants = 4;
constexpr std::uint32_t kSub2FCells = 0x65EB64;     // sub-kind 0x2F: room 2
constexpr std::uint32_t kSub30Cells = 0x65EB80;     // sub-kind 0x30: room 2
constexpr std::uint32_t kSub31Cells = 0x65EB9C;     // sub-kind 0x31: room 2
constexpr std::uint32_t kSub32Cells = 0x65EBB8;     // sub-kind 0x32: room 2
constexpr unsigned kSmallVariants = 2;

// --- sound ids ------------------------------------------------------------------------
constexpr unsigned kSoundOpen = 0x200;              // the leader comes near (every sub-kind; 0x4A's flag)
constexpr unsigned kSoundShut = 0x201;              // closed again

// --- sub-kind 0x4A's state 1 ------------------------------------------------------------
constexpr unsigned kMapOpen = 0xA1;                 // the map byte AreaMap_SetByte writes at the cell and the next

}  // namespace effect_6a::at
