// The raw addresses effect_5a.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_5a.md.
// The image's tables are read in place (never copied: they are game data).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"  // R3G's rebinding: Area109_SwitchPattern, the value unchanged

namespace effect_5a::at {

// --- a callee no group of round thirteen holds, called by address (SH_AT) --
constexpr ::bof3::image::CodeVa kPattern{bof3::addr::Area109_SwitchPattern};      // (void) -> eax: the three story flags 0x65DE60 as bits, plus 1 (1..8)

// --- the flag rows ------------------------------------------------------------
// kFlagRow2: now Cond_Flags[0x10] (symbols.toml). Cond_Flags + 2 * 8 (sub-kind 4's flag 0x17)
// kFlagRow3: now Cond_Flags[0x18] (symbols.toml). Cond_Flags + 3 * 8 (sub-kind 8's flags 5, 6)
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0x14 * 8: the story flags (a dword for sub-kind 5)

// --- the map ------------------------------------------------------------------
constexpr ::bof3::image::Va kAreaHeader{0x8CB580};   // AreaMap_Header: its low byte the map's width
// kHeightBase: now AreaMap_HeightBase (symbols.toml). AreaMap_HeightBase (u16): the height bytes at kAreaHeader + 4 * it
// kAreaBytes: now AreaMap_Bytes (symbols.toml). AreaMap_Bytes (a pointer)

// --- the CLUT strip -------------------------------------------------------------
// kClutStrip: now Gfx_ClutStrip (symbols.toml). Gfx_ClutStrip: 32 rows of 0x200 bytes, a 16-colour slot 0x20
constexpr std::uint32_t kClutStripSize = 0x4000;

// --- sub-kind 4 (EffectKind18_04_States) ---------------------------------------
constexpr std::uint32_t kPanelX = 0x468000, kPanelZ = 0x478000, kPanelZ2 = 0x498000;   // the panels' map cells (link)
constexpr std::uint32_t kPanelTexture = 0x13500126, kPanelTexture2 = 0x13510125;

// --- sub-kind 5: the CLUT rows by +0x36 and the slot tables ----------------------
constexpr ::bof3::image::Va kRowsA{0x65DAF8};        // 5 bytes: row a by the word +0x36
constexpr ::bof3::image::Va kRowsB{0x65DAFD};        // 5 bytes: row b
constexpr unsigned kRows = 5;
constexpr ::bof3::image::Va kPulseSlots{0x65DB18};   // 6 bytes by (+9 >> 2) % 6
constexpr ::bof3::image::Va kFadeSlots{0x65DB20};    // 5 bytes by +0xA >> 1

// --- sub-kind 0x1F ----------------------------------------------------------------
constexpr ::bof3::image::Va kCycleA{0x65DB4C};       // 4 dwords by +9 & 3
constexpr ::bof3::image::Va kCycleB{0x65DB5C};       // 4 dwords by +9 & 3

// --- sub-kind 6 -----------------------------------------------------------------
constexpr ::bof3::image::Va kRects{0x65DB6C};        // 3 records of 4 bytes (x0, z0, x1, z1) by the word +0x36
constexpr ::bof3::image::Va kFlags6{0x65DB84};       // 3 bytes by +0x36: the story flag
constexpr ::bof3::image::Va kHeights6{0x65DB88};     // 4 words by the word +0x3A
constexpr ::bof3::image::Va kSteps6{0x65DB90};       // 4 s8 by +0x3A
constexpr ::bof3::image::Va kDy6{0x65DB94};          // bytes by the part's byte 0 * 2 + (+0x3E != 0xFE00)
constexpr ::bof3::image::Va kRuns6{0x65DB9C};        // 3 pairs (first, end) by +0x36
constexpr ::bof3::image::Va kShapes6{0x65DBA4};      // 12-byte vertex rows by the part's byte 1
constexpr ::bof3::image::Va kParts6{0x65DBE0};       // 8-byte parts: dword cell bytes, dword texture
constexpr unsigned kSets6 = 3, kHeightCount6 = 4;
constexpr std::uint16_t kLowered = 0xFE00;        // +0x3E at which the map bytes are 0

// --- sub-kind 7 -------------------------------------------------------------------
constexpr ::bof3::image::Va kRows7{0x65DDB0};        // 3 bytes by +0xB: the z row
constexpr ::bof3::image::Va kFlagRows7{0x65DDB4};    // 3 bytes: the Cond_Flags row
constexpr ::bof3::image::Va kFlagBits7{0x65DDB8};    // 3 bytes: the flag
constexpr ::bof3::image::Va kTextures7{0x65DDBC};    // 2 dwords
constexpr unsigned kDoors7 = 3;

// --- sub-kind 8 -------------------------------------------------------------------
constexpr ::bof3::image::Va kTopple8{0x65DDF0};      // 0x29 bytes by +9
constexpr unsigned kTopple8Count = 0x29;
constexpr ::bof3::image::Va kCrash8{0x65DE1C};       // 0xE bytes by +9
constexpr unsigned kCrash8Count = 0xE;
constexpr ::bof3::image::Va kPuffBase{0x65DE2C};     // 14 bytes (u8 pairs)
constexpr ::bof3::image::Va kPuffStep{0x65DE3C};     // 14 bytes (s8 pairs)
constexpr ::bof3::image::Va kTiles8{0x65DE4C};       // 6 cell pairs
constexpr ::bof3::image::Va kTileTex8{0x65DE58};     // 6 bytes

// --- sub-kind 9 -------------------------------------------------------------------
constexpr ::bof3::image::Va kHeights9{0x65DE62};     // words by +2 (1..8)
constexpr ::bof3::image::Va kFe9{0x65DE6F};          // bytes by +2: Cond_ByteFE
constexpr unsigned kPatterns = 9;                 // +2 0..8 (0x4FEE70 answers 1..8)

// --- sub-kind 0xA -----------------------------------------------------------------
constexpr ::bof3::image::Va kDoorShapes{0x65DE8C};   // 8 rows of 12 bytes (four vertices)
constexpr ::bof3::image::Va kDoorTextures{0x65DEEC}; // 8 records of two dwords
constexpr ::bof3::image::Va kDoorDy{0x65DF2C};       // 32 bytes

}  // namespace effect_5a::at
