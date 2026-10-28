// The raw addresses effect_5d.cpp and its fuzz read or write that symbols.toml
// does not name - each a load-bearing constant (CLAUDE.md rule 3). Every one is
// data: the group calls no function it does not own or that is not ours
// already (Rand is Capcom's, called by name; the CRT's _ftol 0x5B9550 is done
// in place, as effect_2a does). docs/effect_5d.md.
#pragma once

#include <cstdint>

#include "game/rdata_consts.h"

namespace effect_5d::at {

// --- cells ---------------------------------------------------------------------
constexpr ::bof3::image::Va kScreenY{0x903824};        // MapView_ScreenXY's second float
constexpr ::bof3::image::Va kCameraX{0x905E66};        // s16: the camera's map cell x (inside kCameraCells)
constexpr ::bof3::image::Va kCameraZ{0x905E62};        // s16: the camera's map cell z
constexpr ::bof3::image::Va kCounter{0x903848};        // u8: the chapters' counter (scenario_harness at::kCounter)
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the story flags (Cond_Flags + 0xA0): Flags_Test's / Flags_Clear's bank
constexpr ::bof3::image::Va kAreaFlags{0x903FD8};      // Cond_Flags + 0x48: the bank sub-kind 0x14's flags are tested in
constexpr ::bof3::image::Va kClutStrip{0x80BCC0};      // 16 u16 colours sub-kind 0x14 scales ...
// kClutStripOut: now Gfx_ClutStrip[0x3a0] (symbols.toml). ... into these 16 (+0x4000), Gfx_ClutStripDirty then set
constexpr unsigned kClutStripWords = 16;
constexpr unsigned kMemberStride = 0x14C;           // ObjTrio's records
constexpr unsigned kMemberCount = 3;
constexpr unsigned kActorStride = 0xA4;             // Field_ActorStates' records (by the member's +0x148)
constexpr unsigned kEffectStride = 0x80;            // Effect_Objects' records
constexpr unsigned kEffectCount = 20;

// --- the image's float constants (.rdata, read in place) ---------------------------
constexpr rdata::Const kHalfCell{0x5C41E4};       // 64.0: half of a 128-unit patch cell
constexpr rdata::Const kRingDrop{0x5C41CC};       // 8.0: the ring's screen-y offset
constexpr rdata::Const kScreenHigh{0x5C4228};     // 340.0: EffectKind18Sub1C_OnScreen's upper bound
constexpr rdata::Const kScreenLow{0x5C422C};      // -20.0: and its lower (DIV-0041's "0x5054E3 [-20, 340]")

// --- sub-kind 0x17 (E5C's dispatcher 0x503660; four of its helpers here) -----------
constexpr ::bof3::image::Va kPatch{0x65E1E8};          // 16 records of 8: four corner heights, a flag, three variant texture bytes
constexpr unsigned kPatchStride = 8, kPatchCount = 16, kPatchVariants = 3;
constexpr ::bof3::image::Va kFrames{0x65E268};         // 10 (u, v) byte pairs: the frames EffectKind18Sub17_CopyFrame copies
constexpr unsigned kFrameCount = 10;

// --- sub-kind 0x19 -------------------------------------------------------------------
constexpr ::bof3::image::Va kSub19X{0x65E28C};         // bytes by +0xB: the x cell
constexpr ::bof3::image::Va kSub19Z{0x65E290};         // bytes by +0xB: the z cell
constexpr ::bof3::image::Va kSub19Height{0x65E294};    // s16 by +0xB: the height
constexpr unsigned kSub19Count = 2;                 // the entries the three hold (0x65E298.. is data no code names)

// --- sub-kinds 0x1C / 0x1D: the waits by the cell's parity ----------------------------
constexpr ::bof3::image::Va kSub1CWaits{0x65E2DC};     // 4 bytes
constexpr ::bof3::image::Va kSub1DWaits{0x65E2F4};     // 4 bytes

// --- sub-kinds 0x14 / 0x1E: by +0x36 (the x cell's word), two places -----------------
constexpr ::bof3::image::Va kSub14Spawn{0x65E30C};     // 2 records of 8: the spawned kind 0x6E's x and z
constexpr ::bof3::image::Va kSub14Flags{0x65E31C};     // 2 bytes (of 4): the flag (in kAreaFlags) that starts the spawn
constexpr ::bof3::image::Va kSub14Rect{0x65E320};      // 2 records of 4: column from, row from, column to, row to
constexpr unsigned kSub14Count = 2;

// --- sub-kind 0x52 -----------------------------------------------------------------------
constexpr ::bof3::image::Va kSub52Signs{0x65E330};     // 2 signed bytes: the turn's direction by the ring's parity

// --- sub-kind 0x22: by +0x36, four places ---------------------------------------------------
constexpr ::bof3::image::Va kSub22Flags{0x65E348};     // 4 bytes: the story flag each place follows
constexpr ::bof3::image::Va kSub22Heights{0x65E34C};   // 4 s16: the height each place starts at
constexpr ::bof3::image::Va kSub22Rect{0x65E354};      // 4 records of 4: x from, z from, x to, z to (the map cells)
constexpr ::bof3::image::Va kSub22Corners{0x65E364};   // 2 rows of 11 bytes (by the flag): a corner byte per cell
constexpr unsigned kSub22CornerRow = 11;
constexpr ::bof3::image::Va kSub22Rows{0x65E37C};      // 8 pairs of signed bytes by a piece's kind: the row offset (sub-state 2 or not); <0 committed
constexpr unsigned kSub22RowKinds = 8;
constexpr ::bof3::image::Va kSub22Pieces{0x65E38C};    // 4 (from, to) byte pairs: each place's pieces
constexpr ::bof3::image::Va kSub22Shapes{0x65E394};    // 5 records of 12: four (dx, dz, dy) signed-byte vertices
constexpr unsigned kSub22ShapeCount = 5;
constexpr ::bof3::image::Va kSub22Piece{0x65E3D0};     // 89 records of 8: kind, shape, z cell, x cell, the texture dword
constexpr unsigned kSub22PieceCount = 89;
constexpr unsigned kSub22Count = 4;

}  // namespace effect_5d::at
