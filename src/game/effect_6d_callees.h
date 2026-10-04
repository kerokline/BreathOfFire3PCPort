// The raw addresses effect_6d.cpp and its fuzz read or write that symbols.toml
// does not name - each a load-bearing constant (CLAUDE.md rule 3). Every one is
// data: the group calls no function it does not own or that is not ours
// already (the CRT's _ftol 0x5B9550 is done in place, as effect_5d does).
// The image's tables are read in place, never copied. docs/effect_6d.md.
#pragma once

#include <cstdint>

namespace effect_6d::at {

// --- cells -------------------------------------------------------------------------
constexpr std::uint32_t kLeaderX = 0x802D74;        // ObjTrio +0x34: the leader's x, 16.16
constexpr std::uint32_t kLeaderZ = 0x802D78;        // ObjTrio +0x38: its z, 16.16
constexpr std::uint32_t kStoryFlags = 0x904030;     // Cond_Flags + 0xA0: the bit row Flags_Set / _Clear take
constexpr std::uint32_t kKind2ZCell = 0x905E62;     // Field_Kind2Z's high word (s16): sub-kind 0x62's first row
constexpr std::uint32_t kKind2XCell = 0x905E66;     // Field_Kind2X's high word (s16): its first column
constexpr std::uint32_t kCounter48 = 0x903848;      // the counter byte sub-kind 0x65's wait compares with 0xD
constexpr std::uint32_t kBattleWord = 0x904AC8;     // a dword of the battle block sub-kind 0x64's watch scales
constexpr std::uint32_t kExtra0Point = 0x802034;    // Sprite_ObjectsExtra[0] +0x34 / +0x38 (16.16)
constexpr std::uint32_t kExtra0Height = 0x80203E;   // ... +0x3E: its height word, from the ground
constexpr std::uint32_t kExtra0Word14 = 0x802014;   // ... +0x14: a dword sub-kind 0x62 clears

// --- sub-kind 0x61's tracks ----------------------------------------------------------
// A ring of 32 entries of six bytes - two three-byte marks (cell x, cell z, a
// parity bit), one per follower - and the two followers' heads after it.
constexpr std::uint32_t kTrackRing = 0x6BC644;
constexpr unsigned kTrackEntries = 32;
constexpr unsigned kTrackStride = 6;
constexpr std::uint32_t kTrackHeads = 0x6BC704;     // a byte each (cleared as one word)
constexpr std::uint32_t kFollowers = 0x7DEF58;      // Sprite_Objects[1] +0x34; [2] at + 0xA4
constexpr unsigned kFollowerStride = 0xA4;
constexpr unsigned kFollowerCount = 2;
constexpr std::uint32_t kTrackTextures = 0x65F4E0;  // two bytes a follower: the two marks' texture bytes
constexpr std::uint32_t kTrackSteps = 0x65F4E4;     // s8 a follower: the second mark's z offset

// --- sub-kind 0x61's overlay -----------------------------------------------------------
constexpr std::uint32_t kOverlayPieces = 0x65F470;  // eight pieces of 14 bytes: s16 x, y, w, h; page byte; u, v, du, dv
constexpr unsigned kOverlayCount = 8;
constexpr unsigned kOverlayStride = 0xE;

// --- sub-kind 0x5C's panels ------------------------------------------------------------
constexpr std::uint32_t kSub5CCells = 0x65F388;     // two bytes a variant: the cell x, z (room two)
constexpr std::uint32_t kSub5CFlags = 0x65F38C;     // a byte a variant: the story flag (room four)
constexpr unsigned kSub5CVariants = 2;
constexpr std::uint32_t kSub5CSides = 0x65F390;     // s8 a side: the slide's sign (room four)
constexpr std::uint32_t kSub5CVertices = 0x65F394;  // three quads of four (s16 x, y, z) a call
constexpr std::uint32_t kSub5CTextures = 0x65F3DC;  // dwords: 6 a variant, 3 a side (room two variants)
constexpr unsigned kSub5CSidesUsed = 2;
constexpr unsigned kSub5CQuads = 3;

// --- sub-kind 0x5E's panels ------------------------------------------------------------
constexpr std::uint32_t kSub5ESides = 0x65F420;     // two s8: the panels' slide signs

// --- sub-kind 0x5D's panels ------------------------------------------------------------
constexpr std::uint32_t kSub5DLifts = 0x65F438;     // s16 a variant: +0x3E (room two)
constexpr std::uint32_t kSub5DHeights = 0x65F43C;   // s16 a variant: +0x2E (room two)
constexpr std::uint32_t kSub5DTextures = 0x65F440;  // dword a variant: +0x20 (room two)
constexpr std::uint32_t kSub5DCells = 0x65F448;     // two bytes a variant: the cell x, z (room two)
constexpr unsigned kSub5DVariants = 2;
constexpr std::uint32_t kSub5DSides = 0x65F44C;     // two s8: the panels' slide signs

// --- sub-kind 0x63's glow ---------------------------------------------------------------
constexpr std::uint32_t kGlowPoints = 0x65F4E8;     // nine of 8 bytes: s16 x, y, z, byte shade (+6)
constexpr std::uint32_t kGlowReach = 0x65F530;      // nine of 4 bytes: s16 reach, s16 z
constexpr std::uint32_t kGlowOrder = 0x65F554;      // six bytes: the first point of each quad
constexpr unsigned kGlowQuads = 6;

// --- sub-kind 0x64 ----------------------------------------------------------------------
constexpr std::uint32_t kSub64Cells = 0x65F578;     // two bytes a variant: the cell x, z (room two)
constexpr unsigned kSub64Variants = 2;
constexpr std::uint32_t kEnemyRecords = 0x93BA88;   // the enemy records from the second (0x93B960 + 0x128)
constexpr unsigned kEnemyStride = 0x128;

// --- sub-kind 0x65's CLUT rows ----------------------------------------------------------
constexpr std::uint32_t kClutRows = 0x80BD80;       // Gfx_ClutStripSource + 0x800: CLUT rows 4 and 5 as loaded
constexpr std::uint32_t kClutRowsLive = 0x80FD80;   // Gfx_ClutStrip + 0x800: the rows the game uploads
constexpr unsigned kClutWords = 0x200;

// --- sound ids ---------------------------------------------------------------------------
constexpr unsigned kSoundOpen = 0x200;
constexpr unsigned kSoundShut = 0x201;

}  // namespace effect_6d::at
