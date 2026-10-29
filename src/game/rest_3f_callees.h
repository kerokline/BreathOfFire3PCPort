// The raw addresses rest_3f.cpp and its fuzz read or call that symbols.toml
// does not name - each a load-bearing constant (CLAUDE.md rule 3).
// docs/rest_3f.md. Two callees are Capcom's and unnamed (the square root and
// the VRAM read-back); the rest are cells and the image's read-only tables the
// functions read in place (never copied).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/rdata_consts.h"

namespace rest_3f::at {

// --- Capcom's callees, not ours -----------------------------------------------------
constexpr ::bof3::image::CodeVa kSqrt{bof3::addr::Gte_SquareRoot0};           // int (int v): fild; fsqrt; _ftol (library layer)
constexpr ::bof3::image::CodeVa kStoreImage{bof3::addr::Gfx_StoreImage}; // ours since group PH, the value unchanged; (const short *rect, unsigned short *to): Gfx_VramShadow's
                                                    // rect copied out (renderer)

// --- read-only floats of .rdata -----------------------------------------------------
constexpr rdata::Const kTrailHalfX{0x5C41B8};     // EffectKind9C_DrawTrail's half-width across x (float)
constexpr rdata::Const kTrailHalfY{0x5C41C0};     // ... and across y (float)

// --- cells -------------------------------------------------------------------------------
constexpr ::bof3::image::Va kCounter{0x903848};        // the chapter's count (a byte compared)
// EffectKind69_DrawLines' working cells: the relocated PSX scratchpad
// (SharedScratch) +0, +4, +8 and Scratch_Swap (+0xC)
inline auto& kLineAmp = SharedScratch.d_00;  // the wave's amplitude, a dword
inline auto& kLineAngle = SharedScratch.d_04;    // the angle, a dword
inline auto& kLineShade = SharedScratch.b_08;    // the next shade, a dword (its byte read)
// Kind 0x9E's wall vector (three longs): its state 2 writes the negated
// half-extent, its state 3 bounces the third between +/- the record's +0x14.
// k9EWall: now Rest3f_9EWall (symbols.toml).
constexpr unsigned k9EWallSize = 0xC;

// --- kinds 0xA1 / 0xA3: a sprite broken into pixels ---------------------------------
// The capture records: 8 bytes each (s16 dx, s16 dy, s16 width, s16 height),
// indexed by the byte kPixIndex; three records before EffectKindA1_States
// (0x655220), so ours aborts past 3.
constexpr ::bof3::image::Va kPixTable{0x655208};
constexpr unsigned kPixRecords = 3;
// kPixCursor: now Rest3f_PixCursor (symbols.toml). unsigned short *: the next pixel of the read-back
// kPartCursor: now Rest3f_PartCursor (symbols.toml). unsigned char *: the next free particle
// kPartCount: now Rest3f_PartCount (symbols.toml). unsigned short: the particles made
constexpr ::bof3::image::Va kPixIndex{0x6769BA};       // unsigned char: the capture record (both captures write 1)
constexpr std::uint32_t kPixCellsSize = 0xC;
// The read-back buffer is EffectKind30_Shards (0x92BF80, shared scratch); the
// particles 0x14 bytes each from kParticles, unbounded (docs/rest_3f.md
// section 7): +0 drawn this frame (flips every frame), +1 a delay, +2 the
// pixel (15-bit), +4 / +8 x and y (floats), +0xC the depth (a float copied),
// +0x10 / +0x11 the step (s8 each).
constexpr ::bof3::image::Va kParticles{0x931980};
constexpr std::uint32_t kPartStride = 0x14;
// The VRAM rectangle the capture draws into and reads back: (0x340, 0x100).
constexpr int kCaptureX = 0x340;
constexpr int kCaptureY = 0x100;

// --- kind 0xA8's bars: 16 records of 6 in EffectKind30_Shards ------------------------
// +0 in use, +1 / +3 bytes (4), +2 the step (0, 1, 2), +4 the s16 position.
constexpr unsigned kBarCount = 16;
constexpr unsigned kBarStride = 6;

// --- sound ids ---------------------------------------------------------------------------
constexpr unsigned kSound9EOpenA = 0x207, kSound9EOpenB = 0x202;    // kind 0x9E's start, by +6 bit 2
constexpr unsigned kSound9EShutA = 0x208, kSound9EShutB = 0x203;    // its close, likewise
constexpr unsigned kSoundPixels = 0x20C;            // kinds 0xA1 / 0xA3: the pixels aimed
constexpr unsigned kSoundRing = 0x208;              // kind 0xA2's two starts

}  // namespace rest_3f::at
