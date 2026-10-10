// The raw addresses effect_3d.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_3d.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/rdata_consts.h"

namespace effect_3d::at {

// --- callees nobody names, called by address (SH_AT); the effect-standard set
// lists both (scenario_harness.cpp kEffectStd) ------------------------------
constexpr std::uint32_t kPolyF3 = bof3::addr::Gpu_SetPolyF3;        // (unsigned char *prim): a flat triangle's tag and code (0x2C bytes)
constexpr std::uint32_t kBox = bof3::addr::Menu_DrawPanelBox;   // (x, y, w, h, style) five words: a menu box (effect_2g_callees.h kBox)

// --- the chapters' counters (scenario_harness at::kCounter) ----------------
constexpr std::uint32_t kCounter = 0x903848;       // u8: the count the event scripts raise; kinds 0x77, 0x79, 0x7C, 0x81 wait on it
constexpr std::uint32_t kCounterB = 0x903849;      // u8: kind 0x82's push count (3, 4, 5) or 0xFF the end
constexpr std::uint32_t kCounterC = 0x90384A;      // u8: 0x80 sends kind 0x82 to its state 0x17

// --- kinds 0x7B and 0x7C: the pulsing shade ----------------------------------
constexpr std::uint32_t kPulseShade = 0x676278;    // u8: the red the full-screen quad subtracts
constexpr std::uint32_t kPulseIndex = 0x676279;    // u8: 0 or 1, which of the two shades is next
constexpr std::uint32_t kPulseShades = 0x654AF8;   // two bytes, the shades (then 0xFF, 0x00 and EffectKind7B_States)
constexpr unsigned kPulseShadeCount = 2;

// --- kind 0x77: a count in a box ---------------------------------------------
constexpr std::uint32_t kTally = 0x939A00;         // u16: the low byte the count (to 200), the high byte bit 7 the
                                                   // pace (0: 30 frames a count, 1: 40) and bits 0..6 the frames
constexpr std::uint32_t kFlagRow = 0x929ED0;       // unsigned char *: the chapter's flag row (scenario_harness at::kFlagRow)
constexpr unsigned kTallySoundFlag = 0x14;         // the row's flag that makes each count sound (0x20A)
constexpr unsigned kTallyEndFlag = 0x2E;           // the row's flag that keeps the end from starting run 5
constexpr std::uint32_t kScriptFlagsHigh = 0x9039A3;  // u8: Field_ScriptFlags + 1 (bit 0 set at the end)
constexpr std::uint32_t kStep = 0x8034E5;          // u8: the chapter's step (scenario_harness at::kStep)
constexpr std::uint32_t kTallyFormat = 0x653074;   // the count's format (one number)
constexpr std::uint32_t kText = 0x904BA0;          // the text scratch Crt_sprintf prints into

// --- kind 0x78: a ring about Sprite_Objects record 2 -------------------------
constexpr std::uint32_t kObject2 = 0x7DEFC8;       // Sprite_Objects + 2 * 0xA4
constexpr std::uint32_t kObject2Screen = 0x7DEFF6; // its +0x2E, +0x30 (s16 screen x, y)
constexpr std::uint32_t kObject2Turn = 0x7DEFFA;   // its +0x32 (u16, the turn the ring spins it by)
constexpr std::uint32_t kObject2Point = 0x7DEFFC;  // its +0x34, +0x38 (x, z)

// --- kind 0x7D: area 170's three dials (Area170_Tail37's case 40 spawns it) --
constexpr std::uint32_t kDials = 0x675DC8;         // three bytes 0..8, the dials' turns
constexpr unsigned kDialCount = 3;
constexpr unsigned kDialTurns = 9;
constexpr std::uint32_t kDialAnswer = 0x9039F6;    // u16: 1 confirmed, 0xFF cancelled
constexpr std::uint32_t kInputPressed = 0x7E1BEC;  // Input_Pressed, read as a dword (its bits 15 and 13)
constexpr std::uint32_t kConfirmButtons = 0x90358E;  // Field_ConfirmButtons (u16)
constexpr std::uint32_t kCancelButtons = 0x903590;   // Field_CancelButtons (u16)
constexpr std::uint32_t kDialGrids = 0x63CA48;     // three grids of 7 rows by 9 columns (0x3F bytes each): 1 a mark, 0xFF a bar
constexpr unsigned kGridSize = 0x3F;
constexpr unsigned kGridRows = 7;
constexpr unsigned kGridColumns = 9;
constexpr std::uint32_t kStoryFlags = 0x904030;    // Cond_Flags' story row (Flags_Test's bank)
constexpr unsigned kDialsSolvedFlag = 0x7E;        // set: the map's cells all off
constexpr std::uint32_t kMapBits = 0x654B4C;       // 2 x 7 x 9 bytes: a cell's low byte, off then on
constexpr std::uint32_t kMapHeights = 0x654BCC;    // 7 x 9 bytes: AreaMap_SetByte's value where a cell is on
constexpr unsigned kMapColumn0 = 0x16, kMapRow0 = 0x8E;  // the cells' first column and row on the map
constexpr std::uint32_t kAreaHeader = 0x8CB580;    // AreaMap_Header: u8 width, u8 depth, u16 base
constexpr std::uint32_t kAreaWords = 0x8CB5AC;     // the header's cell words
constexpr std::uint32_t kDrawItemStride = 0x90;    // the draw items (DrawItems 0x905E80, draw_pool.h)

// --- kind 0x80: a trail of 32 points in EffectKind30_Shards ------------------
constexpr std::uint32_t kTrail = 0x92BF80;         // EffectKind30_Shards: 32 points of 0x20, 31 angles, the size
constexpr unsigned kTrailPoints = 32;
constexpr unsigned kTrailStride = 0x20;
constexpr std::uint32_t kTrailSize = 0x92C3BE;     // u16: the trail's width (kTrail + 0x43E)

// --- kind 0x81: drops from 16 sources ----------------------------------------
constexpr std::uint32_t kDrops = 0x92BF80;         // 256 records of 0x18 (over EffectKind30_Shards and on)
constexpr unsigned kDropCount = 256;
constexpr unsigned kDropStride = 0x18;
constexpr std::uint32_t kSources = 0x92D780;       // 16 records of 0x14, after the drops
constexpr unsigned kSourceCount = 16;
constexpr unsigned kSourceStride = 0x14;
constexpr std::uint32_t kSourceCells = 0x654C3C;   // 16 pairs of s8: a source's cell (x, z)
constexpr std::uint32_t kDropsMoving = 0x67627C;   // u16: how many drops moved this frame
constexpr rdata::Const kOne{0x5C41B8};           // float 1.0 (effect_2c_callees.h kOne)
constexpr std::uint32_t kLeaderPoint = 0x802D74;   // ObjTrio + 0x34: the leader's x, z, height

// --- kind 0x82: Sprite_Objects record 0 pushed --------------------------------
constexpr std::uint32_t kObject0X = 0x7DEEB4;      // Sprite_Objects + 0x34
constexpr std::uint32_t kLeaderX = 0x802D74;       // ObjTrio + 0x34

}  // namespace effect_3d::at
