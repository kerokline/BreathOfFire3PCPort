// The raw addresses effect_6c.cpp and its fuzz read or write that symbols.toml
// does not name - each a load-bearing constant (CLAUDE.md rule 3).
// docs/effect_6c.md. Every callee of the group is ours (by name: Rand, the
// Gpu_* / Gte_* / Math_* rows, Flags_*, MoveCmd_TestFB, MapView_*,
// Prim_SetTexture, Gfx_*, Sprite_*, Sound_PlayEffect, Effect_Release) or the
// group's own; nothing is called by a raw address. The CRT's _ftol 0x5B9550 is
// done in place, as effect_2a does. The constants below are cells and the
// image's tables the functions read in place (never copied).
#pragma once

#include <cstdint>

#include "game/rdata_consts.h"

namespace effect_6c::at {

// --- the leader (ObjTrio record 0) ------------------------------------------------------
constexpr std::uint32_t kLeaderState = 0x802D42;      // ObjTrio +2: 3 runs Sprite_ScriptTick (sub-kind 0x44)
constexpr std::uint32_t kLeaderMoving = 0x802D54;     // ObjTrio +0x14, a dword: not 0 - the screen y is taken from the projection
constexpr std::uint32_t kLeaderDepth = 0x802D70;      // ObjTrio +0x30, a word sub-kind 0x44 reads and writes
constexpr std::uint32_t kLeaderLift = 0x802D7E;       // ObjTrio +0x3E, an s16 (halved, negated into the vertex)
constexpr std::uint32_t kLeaderAnimation = 0x802D8B;  // ObjTrio +0x4B: the animation sub-kind 0x44 copies

// --- the field ------------------------------------------------------------------------
constexpr std::uint32_t kKind2ZHigh = 0x905E62;       // Field_Kind2Z's high word, an s16 cell
constexpr std::uint32_t kKind2XHigh = 0x905E66;       // Field_Kind2X's high word
constexpr std::uint32_t kWalkClock = 0x90405C;        // area 189's frame word (Area189_StepArrive: 0..0x3BF, wraps)
constexpr std::uint32_t kFlagRow = 0x904000;          // the flag row Flags_Test reads bits 4, 0xE, 0x14 of
constexpr std::uint32_t kStoryFlags = 0x904030;       // the story flags (bits 0x7A..0x7C, 0x80, 0x81)
constexpr std::uint32_t kScreenY = 0x903824;          // MapView_ScreenXY's second float
constexpr std::uint32_t kScreenZ = 0x903828;          // and the third (a centre's z, sub-kind 0x45)
constexpr std::uint32_t kOffsetX = 0x903828;          // sub-kind 0x5B's piece offsets: x ...
constexpr std::uint32_t kOffsetY = 0x90382C;          // ... and the lift
constexpr std::uint32_t kGlowXY = 0x903838;           // sub-kind 0x59's second projection, two floats
constexpr std::uint32_t kGlowY = 0x90383C;
constexpr std::uint32_t kDrawList = 0x9036E0;         // Sprite_DrawList: 40 pointers (sub-kind 0x44 adds its record)
constexpr unsigned kDrawListRoom = 0x28;

// --- the packet pool: sub-kind 0x44's own commit (Gfx_CommitPrim's test, another list) --
constexpr std::uint32_t kPoolLimit = 0x7F1BAC;        // Gfx_PacketPools 0x7E1C00 + 0x10000 - 0x54 (draw_emit.cpp's)
constexpr std::uint32_t kLayerTails = 0x802B34;       // DrawLayers + 0x894: the list's last pointer, 8 bytes a buffer

// --- the area map's cell records (AreaMap_Header's dword runs) ---------------------------
constexpr std::uint32_t kAreaHeader = 0x8CB580;       // AreaMap_Header: byte 0 the width, byte 1 the height
constexpr std::uint32_t kAreaCellBase = 0x8CB582;     // its word +2: the cells' offset
constexpr std::uint32_t kAreaCorners = 0x8CB5B0;      // AreaMap_Corners: four s8 heights a cell, 0x60 cells a row of blocks
constexpr std::uint32_t kDrawItemStride = 0x90;       // DrawItems' records (draw_pool::Items(), DIV-0062)
constexpr std::uint32_t kItemLink = 0x7E;             // a draw item's link word (& 0xFFF: another item)
constexpr std::uint32_t kItemHalf = 0x48;             // a draw item's half per buffer

// --- the image's tables, read in place (their bytes are not copied here) -------------------
constexpr std::uint32_t kBlockMap = 0x65ED78;         // 256 bytes: the corner table's block by (z >> 4, x >> 4) of a cell
constexpr std::uint32_t kSkyXs = 0x65EE80;            // sub-kind 0x44: five words, the bands' x edges
constexpr std::uint32_t kSkyYs = 0x65EE8C;            // and five bytes, their lower y
constexpr std::uint32_t kSkyTints = 0x65EE94;         // sixteen (red, green, blue) s8 triples, Gfx_ClutAdjust's
constexpr unsigned kSkyTintSteps = 16;
constexpr std::uint32_t kTwinkle = 0x65EEC4;          // four (dx, dy) s8 pairs around a star
constexpr std::uint32_t kRingUv = 0x65EED8;           // sub-kind 0x45: two (u, v) byte pairs by the ring's half
constexpr unsigned kRingHalves = 2;
constexpr std::uint32_t kRect51 = 0x65EEDC;           // sub-kind 0x51: five (x0, z0, x1, z1) cell rectangles
constexpr unsigned kRect51Variants = 5;
constexpr std::uint32_t kSpin53 = 0x65EEFC;           // sub-kind 0x53: two s8 spins (+1, -1)
constexpr std::uint32_t kShades59 = 0x65EF0C;         // sub-kind 0x59: twelve shade bytes its fade WRITES (.data)
constexpr unsigned kShades59Size = 12;
constexpr std::uint32_t kRadii59 = 0x65EF30;          // eight + one radius bytes
constexpr std::uint32_t kHeights59 = 0x65EF3C;        // eight + one height bytes
constexpr std::uint32_t kRows59 = 0x65EF48;           // sixteen depth-row bytes (MapView_LinkPrimAt's dy)
constexpr std::uint32_t kSlide5A = 0x65EF70;          // sub-kind 0x5A: four rect rows (0x5139F0) ...
constexpr std::uint32_t kSlide5B = 0x65EF74;          // ... and four more (0x513A60)
constexpr std::uint32_t kPieces5B = 0x65EF98;         // sub-kind 0x5B: 49 pieces of 0x14 bytes
constexpr unsigned kPieceStride = 0x14;
constexpr std::uint32_t kParts5B = 0x65F36C;          // three (first, end) piece bytes
constexpr unsigned kParts5BCount = 3;
constexpr rdata::Const kPieceScale{0x5C4250};       // a float the pieces' depth point scales the x offset by
constexpr rdata::Const kZero{0x5C41DC};             // the float constants sub-kind 0x44's stars compare with: 0.0f,
constexpr rdata::Const kEightyNine{0x5C4248};       // 89.0f
constexpr rdata::Const kNinety{0x5C424C};           // and 90.0f

// --- sound ids ---------------------------------------------------------------------------
constexpr unsigned kSoundOpen5B = 0x205;
constexpr unsigned kSoundStep5B = 0x206;

}  // namespace effect_6c::at
