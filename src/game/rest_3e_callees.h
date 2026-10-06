// The raw addresses rest_3e.cpp and its fuzz read or call that symbols.toml
// does not name - each a load-bearing constant (CLAUDE.md rule 3).
// docs/rest_3e.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_3e::at {

// --- callees called through the harness by address -----------------------------------
// R3F's EffectKind5F_DrawLineDisc (PSX twin 0x801F9F8C): (const long *point,
// unused, wobble, dy) - the second word is never read, the size is a constant
// 0x50 projected, the third the rows' wobble, the fourth the link's dy (capstone
// 2026-10-05: [esp+0x48] / +0x54 / +0x40 at 0x480333 / 0x48039F); a draw mode
// linked at the point, then a disc of lines round it; kind 0x5F's moving states.
// Ours since R3F merged, the value unchanged (round fourteen's rebinding,
// docs/round-14-cleanup.md).
constexpr std::uint32_t kR3FRing = bof3::addr::EffectKind5F_DrawLineDisc;
// The library layer's integer square root (eax read, through _ftol): the spiral's
// shade (an FX_RAW row of scenario_harness.cpp's kEffectStd).
constexpr std::uint32_t kSqrt = 0x5A7A90;

// --- the leader (ObjTrio record 0) ---------------------------------------------------
constexpr std::uint32_t kLeaderX = 0x802D74;        // ObjTrio +0x34, 16.16
constexpr std::uint32_t kLeaderZ = 0x802D78;        // +0x38
constexpr std::uint32_t kLeaderY = 0x802D7C;        // +0x3C, the height

// --- the shards' model copy (EffectShards_LoadModel) ----------------------------------
constexpr std::uint32_t kModelCopy = 0x8C5D80;      // the model bytes are copied here, +0x50 aimed at it
constexpr unsigned kModelFace = 0x28;               // bytes a face: the copy's count is the byte *(+0x54) * 0x28
constexpr std::uint32_t kNumberFormat = 0x5E10C0;   // Area08_MessageFormat, the Crt_sprintf format of the number
constexpr std::uint32_t kText = 0x904BA0;           // the text scratch Crt_sprintf writes and the draws read

// --- the glow sparks: 8 records of 0x1C at EffectKind30_Shards -------------------------
constexpr unsigned kSparks = 8;
constexpr unsigned kSparkStride = 0x1C;
constexpr std::uint32_t kRiseSounded = 0x6761C8;    // byte: sound 0x201 played (EffectGlowSparks_Rise)
constexpr std::uint32_t kGlowSounded = 0x6761C9;    // byte: sound 0x200 played (EffectGlowSparks_Glow)
constexpr unsigned kSoundGlow = 0x200;
constexpr unsigned kSoundRise = 0x201;

// --- the dust: 64 records of 0x20 ------------------------------------------------------
constexpr std::uint32_t kDust = 0x92D1DC;
constexpr unsigned kDustCount = 0x40;
constexpr unsigned kDustStride = 0x20;
constexpr std::uint32_t kHalfWidth = 0x5C41B8;      // a float constant of the image: the column's half width

// --- kinds 0x5D / 0x5E: the layout words and the counts --------------------------------
// Three windows of four words (x, y, w, h) from 0x654858; the board's frame
// 0x654868; the menu box 0x654870, the list box 0x654878, the panel 0x654880
// and its box 0x654888; the sprite's offsets 0x654890 / 0x654891; the hand
// 0x654894 (x the low byte, y the high).
constexpr std::uint32_t kWindows = 0x654858;
constexpr std::uint32_t kBoard = 0x654868;
constexpr std::uint32_t kMenuBox = 0x654870;
constexpr std::uint32_t kListBox = 0x654878;
constexpr std::uint32_t kPanel = 0x654880;
constexpr std::uint32_t kPanelBox = 0x654888;
constexpr std::uint32_t kSpriteDx = 0x654890;
constexpr std::uint32_t kSpriteDy = 0x654891;
constexpr std::uint32_t kHand = 0x654894;
constexpr std::uint32_t kRows = 0x6548BC;           // 8 rows of 3 mark indices (0xA none)
constexpr std::uint32_t kMarks = 0x6548D4;          // the marks' (dx, dy) words, 4 bytes each
constexpr unsigned kMarkCount = 11;                 // to the format string 0x654900
constexpr std::uint32_t kStringFormat = 0x654900;   // the format the item's name and the title go through
constexpr std::uint32_t kCountFormat = 0x65306C;    // the count's format (kind 0x5D)
constexpr std::uint32_t kListFormat = 0x653EC0;     // the count's format (kind 0x5E's list)
constexpr std::uint32_t kTitle = 0x66A098;          // the menu's title text
constexpr std::uint32_t kCounts = 0x903A10;         // 8 bytes: a count per item, held at 100
constexpr unsigned kItems = 8;
constexpr unsigned kFirstItem = 0x4E;               // the items 0x4E..0x55
constexpr std::uint32_t kStyle = 0x903A5A;          // the window style byte

}  // namespace rest_3e::at
