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
// kLeaderState: now ObjTrio[0].step (symbols.toml). ObjTrio +2: 3 runs Sprite_ScriptTick (sub-kind 0x44)
// kLeaderMoving: now ObjTrio[0].step_y (symbols.toml). ObjTrio +0x14, a dword: not 0 - the screen y is taken from the projection
// kLeaderDepth: now ObjTrio[0].screen_y (symbols.toml). ObjTrio +0x30, a word sub-kind 0x44 reads and writes
// kLeaderLift: now ObjTrio[0].height_whole (symbols.toml). ObjTrio +0x3E, an s16 (halved, negated into the vertex)
// kLeaderAnimation: now ObjTrio[0].animation (symbols.toml). ObjTrio +0x4B: the animation sub-kind 0x44 copies

// --- the field ------------------------------------------------------------------------
constexpr ::bof3::image::Va kKind2ZHigh{0x905E62};       // Field_Kind2Z's high word, an s16 cell
constexpr ::bof3::image::Va kKind2XHigh{0x905E66};       // Field_Kind2X's high word
// kWalkClock: now Cond_Flags[0xcc] (symbols.toml). area 189's frame word (Area189_StepArrive: 0..0x3BF, wraps)
// kFlagRow: now Cond_Flags[0x70] (symbols.toml). the flag row Flags_Test reads bits 4, 0xE, 0x14 of
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the story flags (bits 0x7A..0x7C, 0x80, 0x81)
constexpr ::bof3::image::Va kScreenY{0x903824};          // MapView_ScreenXY's second float
constexpr ::bof3::image::Va kScreenZ{0x903828};          // and the third (a centre's z, sub-kind 0x45)
constexpr ::bof3::image::Va kOffsetX{0x903828};          // sub-kind 0x5B's piece offsets: x ...
constexpr ::bof3::image::Va kOffsetY{0x90382C};          // ... and the lift
constexpr ::bof3::image::Va kGlowXY{0x903838};           // sub-kind 0x59's second projection, two floats
constexpr ::bof3::image::Va kGlowY{0x90383C};
constexpr ::bof3::image::Va kDrawList{0x9036E0};         // Sprite_DrawList: 40 pointers (sub-kind 0x44 adds its record)
constexpr unsigned kDrawListRoom = 0x28;

// --- the packet pool: sub-kind 0x44's own commit (Gfx_CommitPrim's test, another list) --
constexpr std::uint32_t kPoolLimit = 0x7F1BAC;        // Gfx_PacketPools 0x7E1C00 + 0x10000 - 0x54 (draw_emit.cpp's)
constexpr ::bof3::image::Va kLayerTails{0x802B34};       // DrawLayers + 0x894: the list's last pointer, 8 bytes a buffer

// --- the area map's cell records (AreaMap_Header's dword runs) ---------------------------
constexpr ::bof3::image::Va kAreaHeader{0x8CB580};       // AreaMap_Header: byte 0 the width, byte 1 the height
// kAreaCellBase: now AreaMap_Header[2] (symbols.toml). its word +2: the cells' offset
constexpr ::bof3::image::Va kAreaCorners{0x8CB5B0};      // AreaMap_Corners: four s8 heights a cell, 0x60 cells a row of blocks
constexpr std::uint32_t kDrawItemStride = 0x90;       // DrawItems' records (draw_pool::Items(), DIV-0062)
constexpr std::uint32_t kItemLink = 0x7E;             // a draw item's link word (& 0xFFF: another item)
constexpr std::uint32_t kItemHalf = 0x48;             // a draw item's half per buffer

// --- the image's tables, read in place (their bytes are not copied here) -------------------
constexpr ::bof3::image::Va kBlockMap{0x65ED78};         // 256 bytes: the corner table's block by (z >> 4, x >> 4) of a cell
constexpr ::bof3::image::Va kSkyXs{0x65EE80};            // sub-kind 0x44: five words, the bands' x edges
constexpr ::bof3::image::Va kSkyYs{0x65EE8C};            // and five bytes, their lower y
constexpr ::bof3::image::Va kSkyTints{0x65EE94};         // sixteen (red, green, blue) s8 triples, Gfx_ClutAdjust's
constexpr unsigned kSkyTintSteps = 16;
constexpr ::bof3::image::Va kTwinkle{0x65EEC4};          // four (dx, dy) s8 pairs around a star
constexpr ::bof3::image::Va kRingUv{0x65EED8};           // sub-kind 0x45: two (u, v) byte pairs by the ring's half
constexpr unsigned kRingHalves = 2;
constexpr ::bof3::image::Va kRect51{0x65EEDC};           // sub-kind 0x51: five (x0, z0, x1, z1) cell rectangles
constexpr unsigned kRect51Variants = 5;
constexpr ::bof3::image::Va kSpin53{0x65EEFC};           // sub-kind 0x53: two s8 spins (+1, -1)
constexpr ::bof3::image::Va kShades59{0x65EF0C};         // sub-kind 0x59: twelve shade bytes its fade WRITES (.data)
constexpr unsigned kShades59Size = 12;
constexpr ::bof3::image::Va kRadii59{0x65EF30};          // eight + one radius bytes
constexpr ::bof3::image::Va kHeights59{0x65EF3C};        // eight + one height bytes
constexpr ::bof3::image::Va kRows59{0x65EF48};           // sixteen depth-row bytes (MapView_LinkPrimAt's dy)
constexpr ::bof3::image::Va kSlide5A{0x65EF70};          // sub-kind 0x5A: four rect rows (0x5139F0) ...
constexpr ::bof3::image::Va kSlide5B{0x65EF74};          // ... and four more (0x513A60)
constexpr ::bof3::image::Va kPieces5B{0x65EF98};         // sub-kind 0x5B: 49 pieces of 0x14 bytes
constexpr unsigned kPieceStride = 0x14;
constexpr ::bof3::image::Va kParts5B{0x65F36C};          // three (first, end) piece bytes
constexpr unsigned kParts5BCount = 3;
constexpr rdata::Const kPieceScale{0x5C4250};       // a float the pieces' depth point scales the x offset by
constexpr rdata::Const kZero{0x5C41DC};             // the float constants sub-kind 0x44's stars compare with: 0.0f,
constexpr rdata::Const kEightyNine{0x5C4248};       // 89.0f
constexpr rdata::Const kNinety{0x5C424C};           // and 90.0f

// --- sound ids ---------------------------------------------------------------------------
constexpr unsigned kSoundOpen5B = 0x205;
constexpr unsigned kSoundStep5B = 0x206;

}  // namespace effect_6c::at
