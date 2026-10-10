// The raw addresses effect_4b.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_4b.md.
// Group E4B calls no function another group of round thirteen owns: every
// callee is ours already (named through symbols.gen.h) or the group's own.
#pragma once

#include <cstdint>

#include "game/rdata_consts.h"

namespace effect_4b::at {

// --- cells ------------------------------------------------------------------------
constexpr std::uint32_t kStep = 0x8034E5;           // the chapter's step byte (scenario_harness::at::kStep): kinds
                                                    // 0x89, 0xA4 and 0x8C move it
constexpr std::uint32_t kCounter = 0x903848;        // the chapters' counter byte (scenario_harness::at::kCounter):
                                                    // kinds 0x9F and 0x8C
constexpr std::uint32_t kCounter2 = 0x903849;       // the byte after it: kind 0x8B waits on it, kind 0x8C counts it
constexpr std::uint32_t kFlagRow = 0x929ED0;        // unsigned char *: the chapter's flag row (Flags_Test's bank)
constexpr std::uint32_t kStoryFlags = 0x904030;     // Cond_Flags' story row
constexpr std::uint32_t kExtraX = 0x802034;         // Sprite_ObjectsExtra record 0's +0x34 (x): kind 0x88's goal
constexpr std::uint32_t kSoundIndex = 0x676280;     // kind 0x87: the byte 0..2 that picks kKind87Sounds' next word
// kind 0x8C's cells, 0x676284..0x67628E (only this band's code reads or writes them)
constexpr std::uint32_t kPresses = 0x676284;        // a frame count, 0..0x1C
constexpr std::uint32_t kDirection = 0x676286;      // u16: the pad word state 0xB took
constexpr std::uint32_t kTimerB = 0x676288;         // the second count-down (states 0x10, 0x12)
constexpr std::uint32_t kSteps = 0x676289;          // state 0xD's step count, 0..8
constexpr std::uint32_t kTarget = 0x67628A;         // u16: the count kept when the first count-down ran out
constexpr std::uint32_t kCount = 0x67628C;          // u16: the presses counted
constexpr std::uint32_t kTimerA = 0x67628E;         // the first count-down (states 1, 4)
constexpr std::uint32_t kKind8CCells = 0x676280;    // the run of kind 0x87's and 0x8C's cells, 0x10 bytes
constexpr std::uint32_t kKind8CCellsSize = 0x10;

// --- the pools at EffectKind30_Shards ---------------------------------------------
constexpr std::uint32_t kPanes = 0x92BF80;          // kind 0x87: six records of 0x1C (five point pointers, a colour,
constexpr unsigned kPaneCount = 6;                  // the case +0x17, live +0x18, a timer +0x19, an animation +0x1A,
constexpr std::uint32_t kPaneStride = 0x1C;         // the packet slot +0x1B)
constexpr std::uint32_t kPanePoints = 0x92C028;     // kind 0x87: fourteen screen points of three floats, kKind87Points
constexpr unsigned kPanePointCount = 14;            // projected
constexpr std::uint32_t kPanePointStride = 0xC;
constexpr std::uint32_t kParticles = 0x92BF80;      // kind 0x88: 32 records of 0x2C (+3 live, +5 life, +6 angle,
constexpr unsigned kParticleCount = 32;             // +8 / +0xA the size, +0xC.. the point, +0x1C.. the velocity)
constexpr std::uint32_t kParticleStride = 0x2C;

// --- the image's tables, read in place ---------------------------------------------
constexpr std::uint32_t kKind87Points = 0x654DA0;   // kind 0x87: fourteen rows of 0x10 (three s32 and a pad); rows 8..13
constexpr unsigned kKind87PointCount = 14;          // are written by EffectKind87_Midpoint - the image's .data
constexpr std::uint32_t kKind87PointStride = 0x10;
constexpr std::uint32_t kKind87Sounds = 0x654E80;   // kind 0x87: three u16 sound ids by kSoundIndex
constexpr unsigned kKind87SoundCount = 3;
constexpr std::uint32_t kKind8AColour = 0x654EF8;   // kind 0x8A: one row of three colour bytes by +6 * 3; the next
constexpr unsigned kKind8AColourCount = 1;          // row would run into kKind8ACells
constexpr std::uint32_t kKind8ACells = 0x654EFC;    // kind 0x8A: six z cells (bytes) along x 0x10
constexpr unsigned kKind8ACellCount = 6;
constexpr std::uint32_t kKind8ALines = 0x654F10;    // kind 0x8A: three pairs of s32, the z of a line's two ends
constexpr unsigned kKind8ALineCount = 3;
constexpr std::uint32_t kKind8CCounts = 0x654F28;   // kind 0x8C: sixteen bytes, the count-downs' start, by Rand & 0xF
constexpr rdata::Const kQuadH{0x5C41EC};          // float constants (.rdata) kind 0x9D's quads add to y,
constexpr rdata::Const kQuadW{0x5C41F0};          // to x,
constexpr rdata::Const kCentreY{0x5C41F4};        // and its projection takes from y,
constexpr rdata::Const kCentreX{0x5C41F8};        // from x

// --- ranges -----------------------------------------------------------------------
constexpr unsigned kObjTrioCount = 3;               // ObjTrio's records of 0x14C
constexpr std::uint32_t kObjTrioStride = 0x14C;
constexpr unsigned kSpriteCount = 30;               // Sprite_Objects' records of 0xA4
constexpr std::uint32_t kSpriteStride = 0xA4;

}  // namespace effect_4b::at
