// The raw addresses effect_5c.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_5c.md.
// Every callee of the group is ours (by name) or the group's own, but three of
// group E5D's (round thirteen, wave five), called raw until E5D merges and the
// rebinding pass names them.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_5c::at {

// --- group E5D's (wave five; E5D merges before E5C) --------------------------------
constexpr std::uint32_t kDrawMoveStep = bof3::addr::EffectKind18Sub17_CopyFrame;   // (step): a Gpu_SetDrawMove from the byte pairs at 0x65E268 + 2 *
                                                    // step and Sprite_Current +0xB, committed at slot 6 (0x18) - the
                                                    // step read as a whole word (an index)
constexpr std::uint32_t kDrawQuads = bof3::addr::EffectKind18Sub17_DrawPatch;      // (variant): the scenario harness's 0x503FA0 row (E5D's; sixteen
                                                    // textured quads round Sprite_Current's point)
constexpr std::uint32_t kDrawMoves = bof3::addr::EffectKind18Sub17_ScrollTexture;      // (void): four Gpu_SetDrawMoves at Sprite_Current +0x34 / +0x38's
                                                    // cell; sub-kind 0x17's states tail-jump to it

// --- cells ------------------------------------------------------------------------
constexpr std::uint32_t kFlagRow9 = 0x903FD8;       // Cond_Flags row 9 (Cond_Flags + 0x48): sub-kinds 0x10 and 0x50 test it
constexpr std::uint32_t kStoryFlags = 0x904030;     // the story flags (Cond_Flags + 0xA0): sub-kinds 0x11 and 0x12
constexpr std::uint32_t kCounter = 0x903848;        // the chapter counter byte sub-kind 0x17 waits on and sets
constexpr std::uint32_t kLeaderX = 0x802D74;        // ObjTrio record 0's +0x34 (x, 16.16): sub-kind 0x10's near test
constexpr std::uint32_t kLeaderZ = 0x802D78;        // ObjTrio record 0's +0x38 (z, 16.16)
constexpr std::uint32_t kKind2ZCell = 0x905E62;     // Field_Kind2Z's high word (s16): sub-kind 0x15's corner rows
constexpr std::uint32_t kEnemy0Bits = 0x93B9F2;     // EnemyWorkingRecords record 0's +0x12 (a dword read; bit 14 tested)
constexpr std::uint32_t kEnemy1Bits = 0x93BB1A;     // EnemyWorkingRecords record 1's +0x12 (the same)
constexpr std::uint32_t kLayer15Tails = 0x802594;   // DrawLayers + 0x2F4: layer 15's third list head's last pointer,
                                                    // one per display buffer 8 apart (sub-kind 0x15's commit)
constexpr std::uint32_t kPoolLimit = 0x7F1BAC;      // Gfx_PacketPools 0x7E1C00 + 0x10000 - 0x54 (draw_emit.cpp's)

// --- the image's tables (read in place) --------------------------------------------------
constexpr std::uint32_t kSub50Lights = 0x65E034;    // 4 bytes: the row-9 flag that picks the wall's texture, by
                                                    // +0xB - E5B's 0x50096F and 0x5011E0 read it too (not named here)
constexpr unsigned kSub50FlagCount = 4;             // EffectKind18Sub50_Flags' bytes, and the four above before
                                                    // other data
constexpr unsigned kSub50Variants = 2;              // EffectKind18Sub50_Cells' pairs (the heights follow them)
constexpr unsigned kSub57PointCount = 8;            // EffectKind18Sub57_Points' (x, z) pairs
constexpr unsigned kTileCount = 15;                 // EffectKind18Sub11_Tiles' / _Sub12_Tiles' triplets

// --- texture words ---------------------------------------------------------------------
constexpr unsigned kTexture50 = 0x285000F3u;        // sub-kind 0x50's wall: added to the caller's texture word
constexpr unsigned kTextureTiles = 0xB600B102u;     // sub-kinds 0x11 / 0x12: OR'ed with shade << 16

}  // namespace effect_5c::at
