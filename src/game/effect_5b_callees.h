// The raw addresses effect_5b.cpp reads that symbols.toml does not name - each a
// load-bearing constant (CLAUDE.md rule 3). docs/effect_5b.md. Every callee of
// the group is ours or named already (Flags_Test .. Rand); nothing is called by
// address.
#pragma once

#include <cstdint>

#include "game/rdata_consts.h"

namespace effect_5b::at {

// --- the flag rows, cells and the leader --------------------------------------
constexpr std::uint32_t kStoryFlags = 0x904030;    // Cond_Flags + 0xA0: the story flags (Flags_Test's bank)
constexpr std::uint32_t kCondRow9 = 0x903FD8;      // Cond_Flags + 8 * 9: the row sub-kinds 0x0E, 0x0F, 0x13, 0x4F test
constexpr std::uint32_t kCount48 = 0x92BEE7;       // u8: area 48's count (area_w1c_callees.h kCount48)
constexpr std::uint32_t kTailState = 0x9039F4;     // s8: the field hook's / armed tail's state (area_w1a_callees.h)
constexpr std::uint32_t kLeaderX = 0x802D74;       // ObjTrio + 0x34: the leader's x (16.16)
constexpr std::uint32_t kLeaderZ = 0x802D78;       // ObjTrio + 0x38: the leader's z (16.16)

// --- the screen cull's floats in .rdata (MapCell_Handlers 0x27's too) ----------
constexpr rdata::Const kCullLeft{0x5C4210};      // -60.0
constexpr rdata::Const kCullRight{0x5C420C};     // 380.0
constexpr rdata::Const kCullTop{0x5C4200};       // -150.0
constexpr rdata::Const kCullBottom{0x5C41FC};    // 300.0

// --- sub-kind 0x0D's byte arrays (back to back; each read by a record byte) -----
constexpr std::uint32_t kSub0DOpenFrames = 0x65DF64;   // 4: _Open's frame by +9 >> 1
constexpr std::uint32_t kSub0DShutFrames = 0x65DF68;   // 4: _Shut's frame by +9 / 3
constexpr std::uint32_t kSub0DFlashFrames = 0x65DF6C;  // 4: _Flash's frame by +9 >> 1
constexpr std::uint32_t kSub0DGlowX = 0x65DF70;        // 4: the glow's cell x by the column +0x36
constexpr std::uint32_t kSub0DGlowZ = 0x65DF74;        // 4: its cell z
constexpr std::uint32_t kSub0DGlowY = 0x65DF78;        // 4: its height (eighths)
constexpr std::uint32_t kSub0DGlowSize = 0x65DF7C;     // 4: its radius
constexpr unsigned kSub0DArray = 4;

// --- sub-kind 0x0B's eighteen tiles: (cell x, cell z, row dy) ------------------
constexpr std::uint32_t kSub0BTiles = 0x65DF8C;
constexpr std::uint32_t kSub0BTilesEnd = 0x65DFC3;     // the loop's bound on its middle byte
constexpr unsigned kSub0BTileCount = 18;

// --- sub-kind 0x0C's byte arrays ------------------------------------------------
constexpr std::uint32_t kSub0CFlags = 0x65DFC4;        // 8: the story flag by the column +0x36
constexpr unsigned kSub0CFlagCount = 8;
constexpr std::uint32_t kSub0COpenFrames = 0x65DFE4;   // 4
constexpr std::uint32_t kSub0CShutFrames = 0x65DFE8;   // 4
constexpr std::uint32_t kSub0CFlashFrames = 0x65DFEC;  // 4
constexpr std::uint32_t kSub0CColumns = 0x65DFF0;      // 8: the VRAM column by +0x36
constexpr unsigned kSub0CColumnCount = 8;

// --- sub-kind 0x13's arrays, by +0xB --------------------------------------------
constexpr std::uint32_t kSub13Flags = 0x65E010;        // 4: row 9's flag that shows the panel
constexpr std::uint32_t kSub13Faces = 0x65E014;        // 4: row 9's flag that picks the face
constexpr std::uint32_t kSub13Cells = 0x65E02C;        // 4 pairs (cell x, cell z)
constexpr unsigned kSub13Count = 4;

// --- the gates' arrays (sub-kinds 0x0F and 0x4F), by the variant +0x36 ---------
constexpr std::uint32_t kGateFlags = 0x65E034;         // 4, by +0x3A: row 9's flag
constexpr unsigned kGateFlagCount = 4;
constexpr std::uint32_t kGateHeights = 0x65E038;       // 8 words
constexpr std::uint32_t kGateTextures = 0x65E048;      // 8 words
constexpr std::uint32_t kGateCells = 0x65E058;         // 8 pairs (cell x, cell z)
constexpr unsigned kGateCount = 8;

}  // namespace effect_5b::at
