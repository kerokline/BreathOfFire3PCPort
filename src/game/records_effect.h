// The effect kinds' sub-pools, typed (docs/typed-records.md section 7;
// typed-data-stage-c.md section 3 item 5). Layout only, as records.h. Most
// lie over one shared scratch, EffectKind30_Shards 0x92BF80 (symbols.toml;
// 1,344 bytes), which each kind lays out its own way while it runs (D201,
// effect_2e.md: "the kinds share one scratch"): which struct a pointer into
// it is, is the running kind's, and the gate cannot see a wrong one. Where
// a pool is the kind's own (EffectKindA0_Sparks, EffectKindA0_Shards) it is
// said beside the struct. Each struct names only what its readers settle.
#pragma once

#include <cstddef>
#include <cstdint>

#include "game/records_gpu.h"

namespace records {

// A speck: 0x80 of 0x14 at EffectKind30_Shards (effect_2a.md 1.3;
// effect_2a_callees.h kSpeckStride). EffectSpecks_Spawn 0x471D10 (also E1C's
// kinds 0x1C and 0x1D), _Move 0x471DD0, _Draw 0x471E20.
struct EffectSpeck {
    std::uint8_t in_use;  // +0x00 1 when spawned; 0 free (EffectSpecks_Spawn takes the first 0)
    std::uint8_t count;  // +0x01 0x40 at the spawn (effect_2a.md 1.3)
    std::uint16_t fall;  // +0x02 the fall speed word, (Rand & 0xFFF) + 0x1000; the height goes down by it << 8 (EffectSpecks_Move)
    std::int32_t x;  // +0x04 the point (effect_2a.md 1.3: cos * d sar 4 + the record's +0x34)
    std::int32_t z;  // +0x08 (+ the record's +0x38)
    std::int32_t height;  // +0x0C (the record's +0x30 << 16 + +0x3C); freed once below Sprite_Current's +0x3C, signed
    unsigned char pad_10[0x4];  // +0x10..+0x13: unread
};

// A spark of the 0x28 pool: 0x80 at EffectKind30_Shards (effect_2a.md 1.4;
// effect_2a_callees.h / effect_3b_callees.h kSparkStride 0x28).
// EffectSparks_Move 0x4721A0, _Draw 0x472240, _Clear 0x4723E0, _FindFree
// 0x472400; kinds 0x2C and 0x2E (E2A) and E3B's.
struct EffectSpark {
    std::int32_t x;  // +0x00 the point (EffectSparks_Draw links at x, z: MapView_LinkPrimAt(UL(spark), UL(spark + 4)))
    std::int32_t z;  // +0x04
    std::int32_t height;  // +0x08 the ground holds it (EffectSparks_Move)
    unsigned char pad_0C[0x4];  // +0xC..+0xF: a fourth dword copied with the point (effect_2a.md 1.4); not named
    std::int32_t vx;  // +0x10 the velocity, (x, z, height), normalised in place by Gte_VectorNormal (effect_2a.md 1.4)
    std::int32_t vz;  // +0x14
    std::int32_t vheight;  // +0x18 zeroed when the ground stops it
    unsigned char pad_1C[0x4];  // +0x1C..+0x1F: unread
    std::uint32_t size;  // +0x20 up 4 a frame; the quad's half size from it >> 6 (UL, effect_2a.cpp EffectSparks_Draw)
    std::uint8_t shade;  // +0x24 down one a frame; the quad's r, g, b
    std::uint8_t frames;  // +0x25 frames left, 0 free (EffectSparks_Clear / _FindFree)
    unsigned char pad_26[0x2];  // +0x26, +0x27: unread
};

// A drop: 0x80 of 0x18 at EffectKind30_Shards (effect_2a.md 1.5, kind 0x2D;
// effect_2a_callees.h kDropStride). EffectDrops_Clear 0x472D70, _Move
// 0x472D90, _Draw 0x472E00.
struct EffectDrop {
    float x;  // +0x00 the screen point, floats (effect_2a.md 1.5)
    float y;  // +0x04 down by the speed each frame
    float z;  // +0x08 the drop's depth, copied to the quad's four z (effect_2a.cpp EffectDrops_Draw)
    std::int16_t half_length;  // +0x0C up one a frame; read signed (SW)
    std::int16_t speed;  // +0x0E
    std::int16_t top;  // +0x10 the top bound (SW)
    std::int16_t bottom;  // +0x12 the bottom bound (SW)
    std::uint16_t frames;  // +0x14 frames left, 0 free; a word (EffectDrops_Clear)
    unsigned char pad_16[0x2];  // +0x16, +0x17: unread
};

// A glow spark of 0x18: kind 0x64's eight at EffectKind30_Shards
// (effect_3a_callees.h kSparks / kSparkStride; effect_3a.md 1.4), kind
// 0xA0's sixteen at EffectKindA0_Sparks 0x6762B0 (effect_4e.md), and E4F's
// (effect_4f_callees.h). EffectKind64_StepSparks 0x482070, _DrawSpark
// 0x4820C0, EffectKindA0_SpawnSpark 0x490650.
struct EffectGlowSpark {
    std::uint8_t in_use;  // +0x00 (EffectKind64_ClearSparks clears it; StepSparks skips a 0)
    unsigned char pad_01[0x1];  // +1: unread
    std::uint8_t life;  // +0x02 down one a step, out of use at 0 (StepSparks); 0x10 at a spawn (EffectKindA0_SpawnSpark)
    unsigned char pad_03[0x1];  // +3: unread
    std::int32_t x;  // +0x04 the point, projected (EffectKind64_DrawSpark: Point(spark + 4))
    std::int32_t z;  // +0x08
    std::int32_t height;  // +0x0C
    unsigned char pad_10[0x4];  // +0x10..+0x13: unread
    std::uint16_t size;  // +0x14 down 0x10 a step (StepSparks); the disc's radius projected (DrawSpark)
    std::uint8_t centre;  // +0x16 the disc's centre shade (DrawSpark)
    std::uint8_t rim;  // +0x17 the rim's
};

// A shard (debris) of 0x2C: kind 0x64's sixteen at EffectKind30_Shards +
// 0xC0 (0x92C040), E1C's debris at 0x92C780, kind 0x72's at
// EffectKind30_Shards (effect_3c.md), kind 0xA0's 32 at EffectKindA0_Shards
// 0x676430 (effect_4e.md). EffectKind64_InitShard 0x482240 (effect_3a.cpp)
// writes every field below; EffectKindA0_DrawShard 0x490880 and E4F's
// 0x493C60 draw it (effect_4e.md: the point plus each edge turned by +0x24,
// times the speed +0x28).
struct EffectShard {
    std::int32_t x;  // +0x00 the point: the record's x (InitShard)
    std::int32_t z;  // +0x04 the record's z
    std::int32_t height;  // +0x08 the record's height
    unsigned char pad_0C[0x4];  // +0xC..+0xF: unwritten by InitShard
    SVector edge0;  // +0x10 the first edge (cos, sin of 0x10, 0), turned in place by a random matrix (InitShard)
    SVector edge1;  // +0x18 the second (cos, sin of -0x10, 0)
    std::uint16_t angle[3];  // +0x20 zeroed by InitShard; +0x24 the spin, up 0x10 a frame (EffectKind64_DrawShards; effect_3a.md 1.4)
    unsigned char pad_26[0x2];  // +0x26, +0x27: unwritten
    std::uint16_t speed;  // +0x28 (Rand & 2) + 3 (InitShard); the edges' scale in the draw (effect_4e.md); E1C's debris a scale 10 + Rand % 4
    std::uint16_t shade;  // +0x2A 0x20 (InitShard; 0x40 for kind 0xA0's); E1C's debris shade up 0x20 / down 2 (effect_1c.md 1.3)
};

// A trail point of 0x34: kind 0x4A's 64 at EffectKind30_Shards
// (effect_2e_callees.h kTrail / kTrailStride; effect_2e.md;
// EffectKind4A_TrailInit / _TrailStep / _TrailDraw, effect_2e.cpp).
struct EffectTrailPoint {
    std::int32_t x;  // +0x00 the point
    std::int32_t z;  // +0x04
    std::int32_t height;  // +0x08
    unsigned char pad_0C[0x4];  // +0xC..+0xF: unread
    std::int32_t edge0_x;  // +0x10 the first edge point (x; z + cos << 3; height + sin << 11: TrailStep), projected by TrailDraw
    std::int32_t edge0_z;  // +0x14
    std::int32_t edge0_height;  // +0x18
    unsigned char pad_1C[0x4];  // +0x1C..+0x1F: unread
    std::int32_t edge1_x;  // +0x20 the second (x; z - cos << 3; height - sin << 11)
    std::int32_t edge1_z;  // +0x24
    std::int32_t edge1_height;  // +0x28
    unsigned char pad_2C[0x4];  // +0x2C..+0x2F: unread
    std::uint16_t angle;  // +0x30 the angle word: Effect2e_TrailTurn for the head (TrailStep), 0 at the start (TrailInit)
    unsigned char pad_32[0x2];  // +0x32, +0x33: unread
};

// A ray of 0x38: kind 0x62's 24 at EffectKind30_Shards (effect_3a_callees.h
// kRays / kRayStride; effect_3a.md 1.2: EffectKind62_SpawnRay 0x4812D0,
// _StepRays 0x4813B0, _DrawRay 0x4814B0).
struct EffectRay {
    std::uint8_t in_use;  // +0x00
    std::uint8_t phase;  // +0x01 0 the first end moves, 1 the second
    std::uint8_t count;  // +0x02 8 a phase
    std::uint8_t shade0;  // +0x03 the first end's shade, down 10 a step in phase 0
    std::uint8_t shade1;  // +0x04 the second's, in phase 1
    unsigned char pad_05[0x3];  // +5..+7: unread
    std::int32_t end0_x;  // +0x08 the first end, the record's point + (cos, sin) * length sar 12
    std::int32_t end0_z;  // +0x0C
    std::int32_t end0_height;  // +0x10
    unsigned char pad_14[0x4];  // +0x14..+0x17: unread
    std::int32_t end1_x;  // +0x18 the second end
    std::int32_t end1_z;  // +0x1C
    std::int32_t end1_height;  // +0x20
    unsigned char pad_24[0x4];  // +0x24..+0x27: unread
    std::int32_t speed_x;  // +0x28 the speed, -(cos, sin) << 1, added to the moving end
    std::int32_t speed_z;  // +0x2C
    std::int32_t speed_height;  // +0x30
    unsigned char pad_34[0x4];  // +0x34..+0x37: unread
};

// A mote of 0x1C: kind 0x68's sixteen at EffectKind30_Shards (effect_3a.md
// 1.5: EffectKind68_InitMote 0x481BD0, _StepMotes 0x481C40, _DrawMote
// 0x481CC0, the three MoteStates).
struct EffectMote {
    std::uint8_t in_use;  // +0x00
    std::uint8_t state;  // +0x01 EffectKind68_MoteStates by it, unbounded
    std::uint8_t count;  // +0x02 8 at the start; reloaded from +4 (MoteGlow) or 0x10 (MoteHold)
    std::uint8_t shade;  // +0x03 up 0x10 (MoteGlow), down 8 (MoteRise); the disc's centre
    std::uint8_t reload;  // +0x04 0x20 (InitMote)
    unsigned char pad_05[0x3];  // +5..+7: unread
    std::uint32_t speed;  // +0x08 up 0x400 a step, added to +0x10 (MoteRise)
    std::int32_t x;  // +0x0C the point: the record's x moved ((Rand & 0xFF) - 0x80) << 10 (InitMote); the disc round it (DrawMote)
    std::int32_t z;  // +0x10 the record's z; up by the speed (MoteRise)
    std::int32_t height;  // +0x14 the record's height, up (Rand % 0x300) << 16
    unsigned char pad_18[0x4];  // +0x18..+0x1B: unread
};

static_assert(offsetof(EffectSpeck, in_use) == 0x0, "EffectSpeck::in_use");
static_assert(offsetof(EffectSpeck, count) == 0x1, "EffectSpeck::count");
static_assert(offsetof(EffectSpeck, fall) == 0x2, "EffectSpeck::fall");
static_assert(offsetof(EffectSpeck, x) == 0x4, "EffectSpeck::x");
static_assert(offsetof(EffectSpeck, z) == 0x8, "EffectSpeck::z");
static_assert(offsetof(EffectSpeck, height) == 0xC, "EffectSpeck::height");
static_assert(sizeof(EffectSpeck) == 0x14, "EffectSpeck stride");

static_assert(offsetof(EffectSpark, x) == 0x0, "EffectSpark::x");
static_assert(offsetof(EffectSpark, z) == 0x4, "EffectSpark::z");
static_assert(offsetof(EffectSpark, height) == 0x8, "EffectSpark::height");
static_assert(offsetof(EffectSpark, vx) == 0x10, "EffectSpark::vx");
static_assert(offsetof(EffectSpark, vz) == 0x14, "EffectSpark::vz");
static_assert(offsetof(EffectSpark, vheight) == 0x18, "EffectSpark::vheight");
static_assert(offsetof(EffectSpark, size) == 0x20, "EffectSpark::size");
static_assert(offsetof(EffectSpark, shade) == 0x24, "EffectSpark::shade");
static_assert(offsetof(EffectSpark, frames) == 0x25, "EffectSpark::frames");
static_assert(sizeof(EffectSpark) == 0x28, "EffectSpark stride");

static_assert(offsetof(EffectDrop, x) == 0x0, "EffectDrop::x");
static_assert(offsetof(EffectDrop, y) == 0x4, "EffectDrop::y");
static_assert(offsetof(EffectDrop, z) == 0x8, "EffectDrop::z");
static_assert(offsetof(EffectDrop, half_length) == 0xC, "EffectDrop::half_length");
static_assert(offsetof(EffectDrop, speed) == 0xE, "EffectDrop::speed");
static_assert(offsetof(EffectDrop, top) == 0x10, "EffectDrop::top");
static_assert(offsetof(EffectDrop, bottom) == 0x12, "EffectDrop::bottom");
static_assert(offsetof(EffectDrop, frames) == 0x14, "EffectDrop::frames");
static_assert(sizeof(EffectDrop) == 0x18, "EffectDrop stride");

static_assert(offsetof(EffectGlowSpark, in_use) == 0x0, "EffectGlowSpark::in_use");
static_assert(offsetof(EffectGlowSpark, life) == 0x2, "EffectGlowSpark::life");
static_assert(offsetof(EffectGlowSpark, x) == 0x4, "EffectGlowSpark::x");
static_assert(offsetof(EffectGlowSpark, z) == 0x8, "EffectGlowSpark::z");
static_assert(offsetof(EffectGlowSpark, height) == 0xC, "EffectGlowSpark::height");
static_assert(offsetof(EffectGlowSpark, size) == 0x14, "EffectGlowSpark::size");
static_assert(offsetof(EffectGlowSpark, centre) == 0x16, "EffectGlowSpark::centre");
static_assert(offsetof(EffectGlowSpark, rim) == 0x17, "EffectGlowSpark::rim");
static_assert(sizeof(EffectGlowSpark) == 0x18, "EffectGlowSpark stride");

static_assert(offsetof(EffectShard, x) == 0x0, "EffectShard::x");
static_assert(offsetof(EffectShard, z) == 0x4, "EffectShard::z");
static_assert(offsetof(EffectShard, height) == 0x8, "EffectShard::height");
static_assert(offsetof(EffectShard, edge0) == 0x10, "EffectShard::edge0");
static_assert(offsetof(EffectShard, edge1) == 0x18, "EffectShard::edge1");
static_assert(offsetof(EffectShard, angle) == 0x20, "EffectShard::angle");
static_assert(offsetof(EffectShard, speed) == 0x28, "EffectShard::speed");
static_assert(offsetof(EffectShard, shade) == 0x2A, "EffectShard::shade");
static_assert(sizeof(EffectShard) == 0x2C, "EffectShard stride");

static_assert(offsetof(EffectTrailPoint, x) == 0x0, "EffectTrailPoint::x");
static_assert(offsetof(EffectTrailPoint, z) == 0x4, "EffectTrailPoint::z");
static_assert(offsetof(EffectTrailPoint, height) == 0x8, "EffectTrailPoint::height");
static_assert(offsetof(EffectTrailPoint, edge0_x) == 0x10, "EffectTrailPoint::edge0_x");
static_assert(offsetof(EffectTrailPoint, edge0_z) == 0x14, "EffectTrailPoint::edge0_z");
static_assert(offsetof(EffectTrailPoint, edge0_height) == 0x18, "EffectTrailPoint::edge0_height");
static_assert(offsetof(EffectTrailPoint, edge1_x) == 0x20, "EffectTrailPoint::edge1_x");
static_assert(offsetof(EffectTrailPoint, edge1_z) == 0x24, "EffectTrailPoint::edge1_z");
static_assert(offsetof(EffectTrailPoint, edge1_height) == 0x28, "EffectTrailPoint::edge1_height");
static_assert(offsetof(EffectTrailPoint, angle) == 0x30, "EffectTrailPoint::angle");
static_assert(sizeof(EffectTrailPoint) == 0x34, "EffectTrailPoint stride");

static_assert(offsetof(EffectRay, in_use) == 0x0, "EffectRay::in_use");
static_assert(offsetof(EffectRay, phase) == 0x1, "EffectRay::phase");
static_assert(offsetof(EffectRay, count) == 0x2, "EffectRay::count");
static_assert(offsetof(EffectRay, shade0) == 0x3, "EffectRay::shade0");
static_assert(offsetof(EffectRay, shade1) == 0x4, "EffectRay::shade1");
static_assert(offsetof(EffectRay, end0_x) == 0x8, "EffectRay::end0_x");
static_assert(offsetof(EffectRay, end0_z) == 0xC, "EffectRay::end0_z");
static_assert(offsetof(EffectRay, end0_height) == 0x10, "EffectRay::end0_height");
static_assert(offsetof(EffectRay, end1_x) == 0x18, "EffectRay::end1_x");
static_assert(offsetof(EffectRay, end1_z) == 0x1C, "EffectRay::end1_z");
static_assert(offsetof(EffectRay, end1_height) == 0x20, "EffectRay::end1_height");
static_assert(offsetof(EffectRay, speed_x) == 0x28, "EffectRay::speed_x");
static_assert(offsetof(EffectRay, speed_z) == 0x2C, "EffectRay::speed_z");
static_assert(offsetof(EffectRay, speed_height) == 0x30, "EffectRay::speed_height");
static_assert(sizeof(EffectRay) == 0x38, "EffectRay stride");

static_assert(offsetof(EffectMote, in_use) == 0x0, "EffectMote::in_use");
static_assert(offsetof(EffectMote, state) == 0x1, "EffectMote::state");
static_assert(offsetof(EffectMote, count) == 0x2, "EffectMote::count");
static_assert(offsetof(EffectMote, shade) == 0x3, "EffectMote::shade");
static_assert(offsetof(EffectMote, reload) == 0x4, "EffectMote::reload");
static_assert(offsetof(EffectMote, speed) == 0x8, "EffectMote::speed");
static_assert(offsetof(EffectMote, x) == 0xC, "EffectMote::x");
static_assert(offsetof(EffectMote, z) == 0x10, "EffectMote::z");
static_assert(offsetof(EffectMote, height) == 0x14, "EffectMote::height");
static_assert(sizeof(EffectMote) == 0x1C, "EffectMote stride");

}  // namespace records
