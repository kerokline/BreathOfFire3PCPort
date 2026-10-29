// The raw addresses effect_2a.cpp and its fuzz call or read that symbols.toml
// does not name - each a load-bearing constant (CLAUDE.md rule 3). Every one
// is data: the group calls no function it does not own or that is not ours
// already, but for the library layer's 0x5A7A90 and the CRT's _ftol 0x5B9550.
// docs/effect_2a.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/rdata_consts.h"

namespace effect_2a::at {

// Library-layer callees nobody owns (catalog part 2), called through the
// harness by address (SH_AT).
constexpr ::bof3::image::CodeVa kRootWord{bof3::addr::Gte_SquareRoot0};       // (long n): fild, fsqrt, a tail jmp to _ftol - the square root of n
                                                    // truncated, in eax; the kind-0x2D disc takes its low word as a row's
                                                    // half width (the effect-standard row stands in for it)
constexpr ::bof3::image::CodeVa kFtol{0x5B9550};           // the CRT's _ftol: pops st(0), answers edx:eax (the harness calls it
                                                    // for real on both sides, kThrough)

// Data the functions read or write in place.
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the story flags (Cond_Flags + 0xA0): Flags_Test's bank
constexpr ::bof3::image::Va kCounter0{0x903848};       // the chapters' counter byte (scenario_harness at::kCounter)
constexpr ::bof3::image::Va kMessageBits{0x7DEE44};    // a byte of the message cells (kind 0x2D's end ORs 2 into it)
// kObject0Point: now Sprite_Objects[0].x (symbols.toml). Sprite_Objects[0] +0x34..+0x3C: x, z, height (16.16)
// kObject1Point: now Sprite_Objects[1].x (symbols.toml). Sprite_Objects[1] +0x34..+0x3C
// kExtra0Point: now Sprite_ObjectsExtra[0].x (symbols.toml). Sprite_ObjectsExtra[0] +0x34..+0x3C
constexpr ::bof3::image::Va kBeamColours28{0x6542EC};  // kind 0x28's colours: three bytes (r, g, b) a +6 value, read in place
constexpr ::bof3::image::Va kPushCells28{0x654308};    // kind 0x28's eight x cells (bytes) of the row it pushes the party from
constexpr ::bof3::image::Va kBeamColours2A{0x654328};  // kind 0x2A's colours, as kind 0x28's
constexpr ::bof3::image::Va kPushRows2A{0x654344};     // kind 0x2A's rows: two bytes a +6 value, the z cell and the state
                                                    // Member_SetState2_8 is handed
constexpr ::bof3::image::Va kSparkOffsetX{0x65436C};   // kind 0x2C's eight x offsets (dwords) round its centre
constexpr ::bof3::image::Va kSparkOffsetZ{0x65438C};   // and the eight z offsets
constexpr rdata::Const kDropWidth{0x5C41C0};      // a float: the kind-0x2D drop quad's width
constexpr rdata::Const kScreenScale{0x5C41D8};    // a float: kind 0x2D's start scales its screen point by it
// kSegments: now Effect2a_Segments (symbols.toml). kind 0x29's five segments of 0x20: two points (x, z, height) at
                                                    // +0 and +0x10, the dwords +0xC / +0x1C never written
constexpr std::uint32_t kSegmentStride = 0x20;
constexpr unsigned kSegmentCount = 5;
constexpr ::bof3::image::Va kBoxPoints{0x676080};      // kind 0x29's eight box corners of 0x10 (x, z, height, one unused)
constexpr unsigned kBoxPointCount = 8;
// kLinkX: now Effect2a_LinkX (symbols.toml). kind 0x2D's link cell: the x and z MapView_LinkPrimAt sorts its
// kLinkZ: now Effect2a_LinkZ (symbols.toml). primitives at (its start writes both)
constexpr ::bof3::image::Va kSparkCentre{0x92D380};    // kinds 0x2C / 0x2E: the centre (x, z, height >> 8, one more dword)
constexpr ::bof3::image::Va kSparkPoints{0x92D390};    // kind 0x2C: eight ground points of 0x10 round the centre

// The particle pool at EffectKind30_Shards (0x92BF80), read with three strides.
// kPool: now EffectKind30_Shards (symbols.toml).
constexpr unsigned kPoolCount = 0x80;
constexpr std::uint32_t kSpeckStride = 0x14;        // EffectSpecks_*: +0 in use, +1 a count, +2 a fall speed word,
                                                    // +4 / +8 / +0xC x, z, height
constexpr std::uint32_t kSparkStride = 0x28;        // EffectSparks_*: +0..+8 a point, +0xC one more dword, +0x10..+0x18 a
                                                    // velocity, +0x20 a size, +0x24 a shade, +0x25 frames left (0 free)
constexpr std::uint32_t kDropStride = 0x18;         // EffectDrops_*: +0..+8 a screen point (floats), +0xC / +0xE / +0x10
                                                    // / +0x12 words, +0x14 frames left (0 free)
constexpr ::bof3::image::Va kPoolEnd{0x92D410};        // the pool (0x80 of 0x28), the spark centre and points

// Effect_Objects.
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffects = 20;
// Party objects (ObjTrio), for kind 0x2A's push.
// kObjTrio: now ObjTrio (symbols.toml).
constexpr std::uint32_t kObjStride = 0x14C;
constexpr unsigned kMembers = 3;

}  // namespace effect_2a::at
