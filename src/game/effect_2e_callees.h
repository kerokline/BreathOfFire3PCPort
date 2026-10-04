// The raw addresses effect_2e.cpp and its fuzz call or read: callees nobody
// owns this round or another group of round thirteen owns, and the data
// (effect_2e.cpp reads its state tables by name; the fuzz lists them by
// address) - each a load-bearing constant (CLAUDE.md rule 3).
// docs/effect_2e.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // E3C's EffectDebris_Draw below reads bof3::addr::<Name> (group E3C, 2026-10-03): the same value

namespace effect_2e::at {

// Callees in no group of the cut (catalog parts 6 and 7, read 2026-09-29 for
// their arguments and what they read and write), called through the harness by
// address (SH_AT). Each is cdecl.
constexpr std::uint32_t kRing = 0x479EE0;         // (unsigned char *ring): a ring of G4 quads from the 0x12-byte record
                                                  // kind 0x48's state 7 keeps at 0x92D1C8 (the pointer 0x6761D0)
constexpr std::uint32_t kSpiralInit = 0x4799C0;   // (unsigned char *spiral): the 0xD20-byte record at 0x92C4A4 (the
                                                  // pointer 0x6761CC) set up from its point +0..+8 and its turn word
                                                  // 0x92D1B4 (+0xD10)
constexpr std::uint32_t kSpiralDraw = 0x479B70;   // (unsigned char *spiral): that record stepped and drawn (read to
                                                  // +0xD20; PSX twin 0x801F9330)
constexpr std::uint32_t kSparksInit = 0x4790C0;   // (void): no arguments, no calls
constexpr std::uint32_t kSparkFree = bof3::addr::EffectSpark_FindFree;    // (void): the first of the 8 records of 0x1C at 0x92BF80 whose +0 is
                                                  // 0, or 0 when none is; eax
constexpr std::uint32_t kSparkSet = 0x479160;     // (unsigned char *spark): the record's +0..+0x17 set (in use, +2 = 8,
                                                  // a point round Sprite_Current's at a random angle); Rand, Math_*
constexpr std::uint32_t kSparksRun = 0x479260;    // (void): the 8 records stepped through the table 0x654660 by their
                                                  // +1 and drawn; al 1 when any is in use, else 0
constexpr std::uint32_t kBurstStep = 0x4794D0;    // (unsigned char *burst): the record at 0x92C060 (read to +0x440,
                                                  // written to +0x402) stepped; calls EffectAngle_Mean
constexpr std::uint32_t kBurstDraw = 0x4796B0;    // (unsigned char *burst): that record drawn as G4 quads (read to +0x400)
constexpr std::uint32_t kSqrt = 0x5A7A90;         // library layer: (long v) the square root through fsqrt and _ftol; eax
constexpr std::uint32_t kMatrixVector = 0x5A7C70; // library layer: (matrix, in, out) - an SVECTOR turned by the 3 x 3
                                                  // (18 bytes read), 6 bytes written; in and out may be one
// Callees another group of round thirteen owns (analysis/round13_cut.tsv),
// called by address until the coordinator rebinds them.
constexpr std::uint32_t kDebrisDraw = bof3::addr::EffectDebris_Draw;   // 0x485030, E3C's (unsigned char *debris): a G3 of a 0x2C-byte debris record
                                                  // (its point +0, edges +0x10 / +0x18 turned by +0x24, scaled by
                                                  // +0x28, shaded +0x2A); Gfx_CommitPrim(1, 0x34)
constexpr std::uint32_t kDiscDraw = bof3::addr::EffectKind4E_DrawDisc;     // E2F's (radius, centre, rim): a fan of 32 G3 triangles round the
                                                  // screen point Sprite_Current +0x74 / +0x78 (floats), the radius read
                                                  // as an s16 (movsx), the rim as a byte

// Data.
constexpr std::uint32_t kState7Steps = 0x65460C;  // two byte tables after kind 0x48's state-7 table (0x6545FC): the
constexpr std::uint32_t kState7Adds = 0x654614;   // mask and the add EffectKind48_State7_Swell indexes by +9
constexpr std::uint32_t kSpiral = 0x92C4A4;       // the 0xD20-byte record kind 0x48's states 8 and 10 use
constexpr std::uint32_t kSpiralTurn = 0x92D1B4;   // its turn word (+0xD10)
constexpr std::uint32_t kSpiralPtr = 0x6761CC;    // unsigned char *: points at kSpiral
constexpr std::uint32_t kRingRecord = 0x92D1C8;   // the ring record of state 7: +0..+8 its point, +0x10 a word
constexpr std::uint32_t kRingPtr = 0x6761D0;      // unsigned char *: points at kRingRecord
constexpr std::uint32_t kSoundSwitch = 0x6761D8;  // a byte: kind 0x4D's sound 0x209 when set, else 0x208
constexpr std::uint32_t kBurst = 0x92C060;        // state 12's record (0x440 bytes)
constexpr std::uint32_t kBurstSize = 0x92C49E;    // a word inside it: state 12's size
constexpr std::uint32_t kSparks = 0x92BF80;       // 8 records of 0x1C (state 9's sparks), or kind 0x4A's trail
constexpr std::uint32_t kTrail = 0x92BF80;        // kind 0x4A: 64 records of 0x34 (a point +0..+8, its two edge points
                                                  // +0x10 / +0x20, an angle word +0x30), to 0x92CC80
constexpr std::uint32_t kTrailStride = 0x34;
constexpr unsigned kTrailCount = 64;
constexpr std::uint32_t kTrailTurn = 0x92CC80;    // kind 0x4A: the angle word the newest point takes
constexpr std::uint32_t kTrailSpin = 0x92CC82;    // kind 0x4A: a byte, set: the angle word turns 0x40 a frame
constexpr std::uint32_t kDebris = 0x92CC84;       // kind 0x4B: 32 debris records of 0x2C, to 0x92D204
constexpr std::uint32_t kDebrisStride = 0x2C;
constexpr unsigned kDebrisCount = 32;
constexpr std::uint32_t kSphereVerts = 0x92D9DC;  // the sphere: 0x1E2 vertices of 8 bytes (x, y, z, pad) ...
constexpr std::uint32_t kSphereScreen = 0x92E8EC; // ... projected, 0x1E2 of 12 bytes (x, y, depth) ...
constexpr std::uint32_t kSphereQuads = 0x92FF84;  // ... 0x200 quads of four u16 vertex numbers ...
constexpr std::uint32_t kSphereShades = 0x930F84; // ... a shade byte a vertex ...
constexpr std::uint32_t kSphereLight = 0x931168;  // ... and the light, three longs, to 0x931174
constexpr unsigned kSphereVertCount = 0x1E2;
constexpr unsigned kSphereQuadCount = 0x200;
constexpr std::uint32_t kSprite1Point = 0x7DEF58; // Sprite_Objects[1] +0x34 / +0x38 / +0x3C: kind 0x4A follows it
constexpr std::uint32_t kLeaderX = 0x802D74;      // ObjTrio record 0 (the leader) +0x34, x (16.16)
constexpr std::uint32_t kLeaderZ = 0x802D78;      // +0x38, z
constexpr std::uint32_t kLeaderHeight = 0x802D7E; // +0x3E, the height's integer word
constexpr std::uint32_t kEffectStride = 0x80;     // Effect_Objects' records

}  // namespace effect_2e::at
