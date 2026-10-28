// The raw addresses field_c2.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/field_c2.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // EffectGte_* (round thirteen group EGT); the constants below name their functions since 2026-10-01 (round twelve's debt 2): the same values, so the fuzz keys stand

namespace field_c2::at {

// Callees nobody owns, or another group of round twelve's wave two owns: called
// through the harness by address (SH_AT) until the coordinator rebinds them.
constexpr ::bof3::image::CodeVa kCameraMatrices{bof3::addr::EffectGte_LoadMapCamera};   // (): the map camera's rotation and translation from Camera_Angles
                                                      // and the view focus (Gte_RotMatrix, Gte_ApplyMatrix, SetRotMatrix,
                                                      // SetTransMatrix). Nobody's (catalog part 2)
constexpr ::bof3::image::CodeVa kProjectPoint{bof3::addr::EffectGte_ProjectPoint};     // (const long *point, float *out): the point (x, z, height) turned
                                                      // into the camera's s16 vector and projected - Gte_RotTransPers to
                                                      // out[0..1] (screen x, y), Gte_StoreDepthF to out[2]. Nobody's
constexpr ::bof3::image::CodeVa kProjectSize{bof3::addr::EffectGte_ProjectSize};      // (const long *point, const short *size, short *out): the point's
                                                      // camera vector by Gte_RotTrans, then out[i] = size[i] * 1000 / its
                                                      // depth (idiv). Nobody's
constexpr ::bof3::image::CodeVa kDrawNumber{bof3::addr::EffectKind41_DrawNumber};       // 0x46D5F0, R3E's: (x s16, y s16, unused, clut byte): Sprite_Current +6 printed by
                                                      // Crt_sprintf into 0x904BA0 and drawn a SPRT per digit at the
                                                      // commit slot +0x29. Nobody's (just past this band)
constexpr ::bof3::image::CodeVa kClearCell{bof3::addr::AreaMap_ClearCell};        // FE2's (x s16, z s16): the area byte of the cell cleared and the
                                                      // view's cell redrawn (scenario_harness.md 7.6)
constexpr ::bof3::image::CodeVa kZennyFind{bof3::addr::Field_GiveZenny};        // FE1's (amount): Sound_PlayEffect(0x106), the amount printed into
                                                      // Text_Records, Msg_OpenSystem(5), Field_Request 2, Zenny_Add(amount, 0)

// Data.
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the story flags (Cond_Flags' row 0x14)
constexpr std::uint32_t kModelCopy = 0x8C5D80;        // the shared frame buffer the shattering object's model is copied into
                                                      // (also 0x46D710's; the battle and spell code's frame table 0x9039D8)
// kLeader: now ObjTrio (symbols.toml). ObjTrio record 0, the leader: +7 0x802D47, +8 the facing 0x802D48,
                                                      // +0x27, +0x29, +0x2C, +0x2E / +0x30 its screen words, +0x34 / +0x38
                                                      // x / z, +0x3E the height, +0x4C
constexpr std::uint32_t kMemberStride = 0x14C;        // ObjTrio's records
// kUnitSteps: now MoveCmd_F9Steps (symbols.toml). (dx, dz) dwords per facing, 8 bytes a row: one pixel's step
// kAreaTileBase: now AreaMap_Header[2] (symbols.toml). AreaMap_Header's u16 offset word: the tile layer starts at 2 x it
constexpr std::uint32_t kObjectStride = 0xA4;         // Sprite_Objects' and Sprite_ObjectsExtra's records
constexpr unsigned kObjects = 0x1E;                   // Sprite_Objects' 30; an index from 0x1E names an extra
constexpr unsigned kExtras = 4;                       // Sprite_ObjectsExtra's four
constexpr std::uint32_t kEffectStride = 0x80;         // Effect_Objects' records
constexpr unsigned kEffects = 20;                     // Effect_Objects' twenty
// kShards: now EffectKind30_Shards (symbols.toml). EffectKind30_Shards: 24 records of 0x38
constexpr std::uint32_t kShardStride = 0x38;
constexpr unsigned kShardCount = 24;
constexpr std::uint32_t kFaceStride = 0x28;           // the model's faces (+0x50's records)
// kSparkShade: now EffectKind30_Sparks (symbols.toml). EffectKind30_Sparks: the shade byte, the size word +2, then
constexpr std::uint32_t kSparkSize = 0x92C4C2;        // eight records of 0x20 from +4
// kSparkRecords: now EffectKind30_Sparks[4] (symbols.toml).
constexpr std::uint32_t kSparkStride = 0x20;
constexpr unsigned kSparkCount = 8;

}  // namespace field_c2::at
