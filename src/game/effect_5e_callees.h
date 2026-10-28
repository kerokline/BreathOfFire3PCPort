// The raw addresses effect_5e.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_5e.md.
// Every callee of the group is ours (by name) or the group's own: no raw call.
// The .data tables below are read in place (never copied): their lengths are
// the code's, read by hand (docs/effect_5e.md section 3).
#pragma once

#include <cstdint>

#include "game/rdata_consts.h"

namespace effect_5e::at {

// --- cells ------------------------------------------------------------------------
constexpr ::bof3::image::Va kCounter{0x903848};        // the chapter counter byte the sub-kinds wait on and raise
                                                    // (scenario_harness kCounter)
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0xA0: the story flags Flags_Test reads
// kFlagRow50: now Cond_Flags[0x50] (symbols.toml). Cond_Flags + 0x50: the row sub-kind 0x24 sets bit 4 of
// kLeaderX: now ObjTrio[0].x (symbols.toml). ObjTrio record 0's +0x34 (the leader's x, 16.16)
// kLeaderZ: now ObjTrio[0].z (symbols.toml). ObjTrio record 0's +0x38 (z)
// kLeaderShade: now ObjTrio[0].shade[0] (symbols.toml). ObjTrio record 0's +0x5D..+0x5F (sub-kind 0x3F dims them)
// kClutRow4: now Gfx_ClutStripSource[0x400] (symbols.toml). Gfx_ClutStripSource + 0x800: CLUT row 4 as loaded (0x100 words)
// kClutRow4Live: now Gfx_ClutStrip[0x400] (symbols.toml). Gfx_ClutStrip + 0x800: the row the game uploads
constexpr unsigned kClutRowWords = 0x100;
constexpr rdata::Const kShadeConst{0x5C41B8};     // a float the trails add to a projected x and y (read in place)

// --- the counter's cues -------------------------------------------------------------
constexpr unsigned kSub24Cue = 5;                   // sub-kind 0x24 starts at it
constexpr unsigned kSub24Hand = 6;                  // ... and moves it to 7 while it spins fast
constexpr unsigned kSub3FDim = 0x26;                // sub-kind 0x3F's cues, one a state
constexpr unsigned kSub3FCue29 = 0x29;
constexpr unsigned kSub3FCue32 = 0x32;
constexpr unsigned kSub3FCue35 = 0x35;
constexpr unsigned kSub3FCue37 = 0x37;
constexpr unsigned kSub3FCue3A = 0x3A;
constexpr unsigned kSub3FCue3B = 0x3B;
constexpr unsigned kSub23Flag = 0x28;               // the story flag sub-kind 0x23 waits on
constexpr unsigned kSub39Flag = 0x5A;               // ... and sub-kind 0x39

// --- .data the code indexes (read in place) -------------------------------------------
constexpr ::bof3::image::Va kSub24Points{0x65E6A4};    // four (x, z) signed-byte pairs, sub-kind 0x24's four corners
constexpr unsigned kSub24PointCount = 4;
constexpr ::bof3::image::Va kTrailPairs{0x65E6CC};     // 34 signed-byte pairs, the trail's polyline (to EffectKind18Sub3F_States)
constexpr unsigned kTrailPairCount = 34;
// Sub-kind 0x25's variants (+0x36 at its start), eight of each:
constexpr ::bof3::image::Va kSub25Heights{0x65E764};   // s16: +0x3E
constexpr ::bof3::image::Va kSub25Pages{0x65E774};     // u16: +0x2E (the texture word's low half)
constexpr ::bof3::image::Va kSub25Cells{0x65E784};     // byte pairs: +0x36, +0x3A (the cell)
constexpr ::bof3::image::Va kSub25Lids{0x65E794};      // bytes: +0x32 (0: none, else an index of kSub25Textures)
constexpr unsigned kSub25Variants = 8;
constexpr ::bof3::image::Va kSub25Textures{0x65E79C};  // dwords; entry 0's two low bytes are the leaves' signs (1, -1)
constexpr unsigned kSub25TextureCount = 5;
// Sub-kind 0x26's variants, five of each:
constexpr ::bof3::image::Va kSub26Heights{0x65E7C4};   // s16: +0x3E
constexpr ::bof3::image::Va kSub26Lifts{0x65E7D0};     // s16: +0x2E (subtracted from the height)
constexpr ::bof3::image::Va kSub26Pages{0x65E7DC};     // dwords: +0x20 (the texture word)
constexpr ::bof3::image::Va kSub26Cells{0x65E7F0};     // byte pairs: +0x36, +0x3A
constexpr ::bof3::image::Va kSub26Lids{0x65E7FC};      // bytes: +0x32
constexpr unsigned kSub26Variants = 5;
constexpr ::bof3::image::Va kSub26Textures{0x65E804};  // dwords; entry 0's two low bytes the leaves' signs
constexpr unsigned kSub26TextureCount = 3;
// Sub-kind 0x39's three quads:
constexpr ::bof3::image::Va kSub39Signs{0x65E820};     // two signed bytes (1, -1), by the variant's bit 0
constexpr ::bof3::image::Va kSub39Vertices{0x65E824};  // 3 quads x 4 vertices x (x, y, z) s16
constexpr unsigned kSub39Quads = 3;
constexpr ::bof3::image::Va kSub39Textures{0x65E86C};  // nine dwords, by quad + 3 * variant (E5F's state table
constexpr unsigned kSub39TextureCount = 9;          // 0x65E890 follows)

}  // namespace effect_5e::at
