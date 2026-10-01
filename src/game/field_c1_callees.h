// The raw addresses field_c1.cpp reads or calls that symbols.toml does not
// name as ours - each a load-bearing constant (CLAUDE.md rule 3).
// docs/field_c1.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // the constants below name their functions since 2026-10-01 (round twelve's debt 2): the same values, so the fuzz keys stand

namespace field_c1::at {

// Callees another group of round twelve's wave two owns (group FC2, by
// analysis/round12_cut.tsv), called through the harness by address (SH_AT)
// until FC2 merges; the coordinator rebinds them after both have merged.
constexpr std::uint32_t kFc2Aim = bof3::addr::EffectKind3A_LeaderPose;          // void(void): EffectKind1B_Start's one callee (FC2)
constexpr std::uint32_t kFc2Place = bof3::addr::EffectKind30_ClaimCells;        // void(void): EffectKind30_Start's one callee (FC2)

// The effect kinds' state tables are named in symbols.toml and taken by
// name (AddressOf(EffectKind19_States), ...): EffectKind19_States (3, by +1),
// EffectKind19_Ticks (6, by +6), CameraZoom_States (3), EffectKind32_States
// (4), EffectKind30_States (its end not established), EffectKind32_Hops,
// EffectKind14_Op, EffectKind3C_Op.

// Data read in place (the image's tables: never written by the field code).
constexpr std::uint32_t kWalkDelta = 0x6696DC;       // s32 x, z per direction, 8 bytes apart (event_ops_callees.h's kWalkDelta)
constexpr std::uint32_t kStoryFlags = 0x904030;      // Cond_Flags + 0xA0: the story flags Flags_Clear is handed
constexpr std::uint32_t kKind1BPalette = 0x80D440;   // EffectKind1B_Start's Sprite_LoadPalette destination (Gfx_ClutStrip's)
constexpr std::uint32_t kAreaCells = 0x8CB582;       // AreaMap_Header's word +2: the cell words' offset, in pairs of words
constexpr std::uint32_t kKind2ZHigh = 0x905E62;      // Field_Kind2Z's whole part (a word add; the name is a macro here)
constexpr std::uint32_t kKind2XHigh = 0x905E66;      // Field_Kind2X's whole part
constexpr std::uint32_t kRecordStride = 0xA4;        // Sprite_Objects' and Sprite_ObjectsExtra's
constexpr std::uint32_t kEffectStride = 0x80;        // Effect_Objects'
constexpr std::uint32_t kMemberStride = 0x14C;       // ObjTrio's
constexpr unsigned kSpriteCount = 30;                // Sprite_Objects' records
constexpr unsigned kExtraCount = 4;                  // Sprite_ObjectsExtra's
constexpr unsigned kEffectCount = 20;                // Effect_Objects'
constexpr unsigned kMemberCount = 3;                 // ObjTrio's

// Config_DrawRowLabel's operands that DIV-0015 / DIV-0017 / the layout patch
// of config_text.cpp rewrite in the original's code, read in place by ours at
// every call so that each patch holds for ours as it does for Capcom's
// (docs/field_c1.md section 2).
constexpr std::uint32_t kRowLabelOperands = 0x461832;   // six imm32, 8 bytes apart: the row labels
constexpr std::uint32_t kRowLabelWidth = 0x461894;      // 5 bytes: lea ecx,[ecx+ecx*2] / shl ecx,1 (len * 6), or DIV-0017's shl ecx,2 / nop / nop (len * 4)
constexpr std::uint32_t kRowAnchorBig = 0x46189D;       // imm8 of `add edx, 0x3A` (the large branch's right edge)
constexpr std::uint32_t kRowBigCall = 0x46189F;         // E8 rel32: Text_DrawAt, or DIV-0017's ConfigText_DrawSelected
constexpr std::uint32_t kRowAnchorSmall = 0x4618ED;     // imm8 of `add eax, 0x3A` (the small branch's)

}  // namespace field_c1::at
