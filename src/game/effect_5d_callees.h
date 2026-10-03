// The raw addresses effect_5d.cpp and its fuzz read or write that symbols.toml
// does not name - each a load-bearing constant (CLAUDE.md rule 3). Every one is
// data: the group calls no function it does not own or that is not ours
// already (Rand is Capcom's, called by name; the CRT's _ftol 0x5B9550 is done
// in place, as effect_2a does). docs/effect_5d.md.
#pragma once

#include <cstdint>

namespace effect_5d::at {

// --- cells ---------------------------------------------------------------------
constexpr std::uint32_t kScreenY = 0x903824;        // MapView_ScreenXY's second float
constexpr std::uint32_t kCameraX = 0x905E66;        // s16: the camera's map cell x (inside kCameraCells)
constexpr std::uint32_t kCameraZ = 0x905E62;        // s16: the camera's map cell z
constexpr std::uint32_t kCounter = 0x903848;        // u8: the chapters' counter (scenario_harness at::kCounter)
constexpr std::uint32_t kStoryFlags = 0x904030;     // the story flags (Cond_Flags + 0xA0): Flags_Test's / Flags_Clear's bank
constexpr std::uint32_t kAreaFlags = 0x903FD8;      // Cond_Flags + 0x48: the bank sub-kind 0x14's flags are tested in
constexpr std::uint32_t kClutStrip = 0x80BCC0;      // 16 u16 colours sub-kind 0x14 scales ...
constexpr std::uint32_t kClutStripOut = 0x80FCC0;   // ... into these 16 (+0x4000), Gfx_ClutStripDirty then set
constexpr unsigned kClutStripWords = 16;
constexpr unsigned kMemberStride = 0x14C;           // ObjTrio's records
constexpr unsigned kMemberCount = 3;
constexpr unsigned kActorStride = 0xA4;             // Field_ActorStates' records (by the member's +0x148)
constexpr unsigned kEffectStride = 0x80;            // Effect_Objects' records
constexpr unsigned kEffectCount = 20;

// --- the image's float constants (.rdata, read in place) ---------------------------
constexpr std::uint32_t kHalfCell = 0x5C41E4;       // 64.0: half of a 128-unit patch cell
constexpr std::uint32_t kRingDrop = 0x5C41CC;       // 8.0: the ring's screen-y offset
constexpr std::uint32_t kScreenHigh = 0x5C4228;     // 340.0: EffectKind18Sub1C_OnScreen's upper bound
constexpr std::uint32_t kScreenLow = 0x5C422C;      // -20.0: and its lower (DIV-0041's "0x5054E3 [-20, 340]")

// --- sub-kind 0x17 (E5C's dispatcher 0x503660; four of its helpers here) -----------
constexpr std::uint32_t kPatch = 0x65E1E8;          // 16 records of 8: four corner heights, a flag, three variant texture bytes
constexpr unsigned kPatchStride = 8, kPatchCount = 16, kPatchVariants = 3;
constexpr std::uint32_t kFrames = 0x65E268;         // 10 (u, v) byte pairs: the frames EffectKind18Sub17_CopyFrame copies
constexpr unsigned kFrameCount = 10;

// --- sub-kind 0x19 -------------------------------------------------------------------
constexpr std::uint32_t kSub19X = 0x65E28C;         // bytes by +0xB: the x cell
constexpr std::uint32_t kSub19Z = 0x65E290;         // bytes by +0xB: the z cell
constexpr std::uint32_t kSub19Height = 0x65E294;    // s16 by +0xB: the height
constexpr unsigned kSub19Count = 2;                 // the entries the three hold (0x65E298.. is data no code names)

// --- sub-kinds 0x1C / 0x1D: the waits by the cell's parity ----------------------------
constexpr std::uint32_t kSub1CWaits = 0x65E2DC;     // 4 bytes
constexpr std::uint32_t kSub1DWaits = 0x65E2F4;     // 4 bytes

// --- sub-kinds 0x14 / 0x1E: by +0x36 (the x cell's word), two places -----------------
constexpr std::uint32_t kSub14Spawn = 0x65E30C;     // 2 records of 8: the spawned kind 0x6E's x and z
constexpr std::uint32_t kSub14Flags = 0x65E31C;     // 2 bytes (of 4): the flag (in kAreaFlags) that starts the spawn
constexpr std::uint32_t kSub14Rect = 0x65E320;      // 2 records of 4: column from, row from, column to, row to
constexpr unsigned kSub14Count = 2;

// --- sub-kind 0x52 -----------------------------------------------------------------------
constexpr std::uint32_t kSub52Signs = 0x65E330;     // 2 signed bytes: the turn's direction by the ring's parity

// --- sub-kind 0x22: by +0x36, four places ---------------------------------------------------
constexpr std::uint32_t kSub22Flags = 0x65E348;     // 4 bytes: the story flag each place follows
constexpr std::uint32_t kSub22Heights = 0x65E34C;   // 4 s16: the height each place starts at
constexpr std::uint32_t kSub22Rect = 0x65E354;      // 4 records of 4: x from, z from, x to, z to (the map cells)
constexpr std::uint32_t kSub22Corners = 0x65E364;   // 2 rows of 11 bytes (by the flag): a corner byte per cell
constexpr unsigned kSub22CornerRow = 11;
constexpr std::uint32_t kSub22Rows = 0x65E37C;      // 8 pairs of signed bytes by a piece's kind: the row offset (sub-state 2 or not); <0 committed
constexpr unsigned kSub22RowKinds = 8;
constexpr std::uint32_t kSub22Pieces = 0x65E38C;    // 4 (from, to) byte pairs: each place's pieces
constexpr std::uint32_t kSub22Shapes = 0x65E394;    // 5 records of 12: four (dx, dz, dy) signed-byte vertices
constexpr unsigned kSub22ShapeCount = 5;
constexpr std::uint32_t kSub22Piece = 0x65E3D0;     // 89 records of 8: kind, shape, z cell, x cell, the texture dword
constexpr unsigned kSub22PieceCount = 89;
constexpr unsigned kSub22Count = 4;

}  // namespace effect_5d::at
