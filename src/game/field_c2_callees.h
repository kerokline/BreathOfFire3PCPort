// The raw addresses field_c2.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/field_c2.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // EffectGte_* (round thirteen group EGT)

namespace field_c2::at {

// Callees nobody owns, or another group of round twelve's wave two owns: called
// through the harness by address (SH_AT) until the coordinator rebinds them.
constexpr std::uint32_t kCameraMatrices = bof3::addr::EffectGte_LoadMapCamera;   // (): the map camera's rotation and translation from Camera_Angles
                                                      // and the view focus (Gte_RotMatrix, Gte_ApplyMatrix, SetRotMatrix,
                                                      // SetTransMatrix). Nobody's (catalog part 2)
constexpr std::uint32_t kProjectPoint = bof3::addr::EffectGte_ProjectPoint;     // (const long *point, float *out): the point (x, z, height) turned
                                                      // into the camera's s16 vector and projected - Gte_RotTransPers to
                                                      // out[0..1] (screen x, y), Gte_StoreDepthF to out[2]. Nobody's
constexpr std::uint32_t kProjectSize = bof3::addr::EffectGte_ProjectSize;      // (const long *point, const short *size, short *out): the point's
                                                      // camera vector by Gte_RotTrans, then out[i] = size[i] * 1000 / its
                                                      // depth (idiv). Nobody's
constexpr std::uint32_t kDrawNumber = 0x46D5F0;       // (x s16, y s16, unused, clut byte): Sprite_Current +6 printed by
                                                      // Crt_sprintf into 0x904BA0 and drawn a SPRT per digit at the
                                                      // commit slot +0x29. Nobody's (just past this band)
constexpr std::uint32_t kClearCell = 0x5728D0;        // FE2's (x s16, z s16): the area byte of the cell cleared and the
                                                      // view's cell redrawn (scenario_harness.md 7.6)
constexpr std::uint32_t kZennyFind = 0x5307C0;        // FE1's (amount): Sound_PlayEffect(0x106), the amount printed into
                                                      // Text_Records, Msg_OpenSystem(5), Field_Request 2, Zenny_Add(amount, 0)

// Data.
constexpr std::uint32_t kStoryFlags = 0x904030;       // the story flags (Cond_Flags' row 0x14)
constexpr std::uint32_t kModelCopy = 0x8C5D80;        // the shared frame buffer the shattering object's model is copied into
                                                      // (also 0x46D710's; the battle and spell code's frame table 0x9039D8)
constexpr std::uint32_t kLeader = 0x802D40;           // ObjTrio record 0, the leader: +7 0x802D47, +8 the facing 0x802D48,
                                                      // +0x27, +0x29, +0x2C, +0x2E / +0x30 its screen words, +0x34 / +0x38
                                                      // x / z, +0x3E the height, +0x4C
constexpr std::uint32_t kMemberStride = 0x14C;        // ObjTrio's records
constexpr std::uint32_t kUnitSteps = 0x6696DC;        // (dx, dz) dwords per facing, 8 bytes a row: one pixel's step
constexpr std::uint32_t kAreaTileBase = 0x8CB582;     // AreaMap_Header's u16 offset word: the tile layer starts at 2 x it
constexpr std::uint32_t kObjectStride = 0xA4;         // Sprite_Objects' and Sprite_ObjectsExtra's records
constexpr unsigned kObjects = 0x1E;                   // Sprite_Objects' 30; an index from 0x1E names an extra
constexpr unsigned kExtras = 4;                       // Sprite_ObjectsExtra's four
constexpr std::uint32_t kEffectStride = 0x80;         // Effect_Objects' records
constexpr unsigned kEffects = 20;                     // Effect_Objects' twenty
constexpr std::uint32_t kShards = 0x92BF80;           // EffectKind30_Shards: 24 records of 0x38
constexpr std::uint32_t kShardStride = 0x38;
constexpr unsigned kShardCount = 24;
constexpr std::uint32_t kFaceStride = 0x28;           // the model's faces (+0x50's records)
constexpr std::uint32_t kSparkShade = 0x92C4C0;       // EffectKind30_Sparks: the shade byte, the size word +2, then
constexpr std::uint32_t kSparkSize = 0x92C4C2;        // eight records of 0x20 from +4
constexpr std::uint32_t kSparkRecords = 0x92C4C4;
constexpr std::uint32_t kSparkStride = 0x20;
constexpr unsigned kSparkCount = 8;

}  // namespace field_c2::at
