// The raw addresses rest_3g.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/rest_3g.md.
//
// Callees by address (owned by a group of this round's wave three; ours, named
// by symbol below, the value unchanged: round fourteen's rebinding,
// docs/round-14-cleanup.md):
//   0x492400  R3F's EffectKindAA_DrawFill, ours: (shade byte) - two opaque
//             shaded quads over the whole frame, 320.0 x 240.0 narrow and the
//             frame's width under the wide picture (DIV-0041); kind 0xAC's
//             fade states call it.
// Everything else the group calls is ours, by name. The constants below are
// cells and the image's read-only tables, read in place (never copied).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_3g::at {

using U = std::uint32_t;

// --- callees ours, by address (through the harness) -------------------------------
constexpr U kFadeDraw = bof3::addr::EffectKindAA_DrawFill;        // R3F's EffectKindAA_DrawFill: void(unsigned shade) - reads the argument's low byte

// --- kinds 0xAC, 0xAD, 0xBA ------------------------------------------------------
constexpr U kMember2X = 0x80300C;          // ObjTrio record 2 +0x34: the third member's x, 16.16
constexpr U kMember2Z = 0x803010;          // ObjTrio record 2 +0x38: its z
constexpr U kCounter = 0x903848;           // the chapters' counter byte kind 0xAD's rise waits on (0xB)
constexpr U kExtra1X = 0x8020D8;           // Sprite_ObjectsExtra record 1 +0x34 (kind 0xBA's line's anchor)
constexpr U kExtra1Z = 0x8020DC;           // ... +0x38
constexpr U kExtra1Y = 0x8020E0;           // ... +0x3C

// --- the battle bytes and records ------------------------------------------------
constexpr U kEventBattle = 0x904AAA;       // u8: the event battle (EventBattle_Records' index)
constexpr U kEventRows = 0x64DDEE;         // EventBattle_Records + 2: each record's encounter row (image .data)
constexpr U kPlaced = 0x904AB2;            // u8: the boss actors placed so far
constexpr U kPlacedTotal = 0x904AB3;       // u8: ... copied at the end of the pass
constexpr U kFormation = 0x904AAC;         // u8: the formation (its bit 1 flips each enemy's facing +8)
constexpr U kEnemies = 0x93B960;           // the eight enemy records (EnemyWorkingRecords' first byte), 0x128 apart
constexpr U kEnemyStride = 0x128;
constexpr unsigned kEnemyCount = 8;
constexpr U kEncounterRowsAt = 0x8C5580;   // Encounter_Rows (a data name is a macro: the address for a constexpr)
constexpr unsigned kEncounterRows = 8;     // Encounter_Rows: 8 rows of 9 bytes (symbols.toml count 0x48)

// --- Quake's lift table (MAGIC102's .bss; magic_s23_callees.h names each) --------------
constexpr U kQuakeFacing = 0x6959CC;       // u8: bit 0 swaps the axes
constexpr U kQuakeLast = 0x695A2C;         // u8 x 16 x 15: the lift one frame back
constexpr U kQuakeX = 0x695C2C;            // s16: the heaved block's first column
constexpr U kQuakeY = 0x695C2E;            // s16: its first row

// --- area 109's switch (Area_CellHooks' entry for area 0x6D) -------------------------
constexpr U kStoryFlags = 0x904030;        // the story flag row Flags_* are handed
constexpr U kPatternFlags = 0x65DE60;      // the three story flag numbers (image .data, E5A's kStoryBits)
constexpr U kLeaderFacing = 0x802D48;      // ObjTrio +8: the leader's facing byte

// --- kind 0x18 sub-kind 0x41's two draws ------------------------------------------
constexpr U kPanelVertices = 0x65ED30;     // 8 vertices of three bytes (x, z, lift) - two quads (image .data)
constexpr U kPanelVerticesEnd = 0x65ED48;

// --- game modes 8..11 --------------------------------------------------------------
constexpr U kModeCounter = 0x904144;       // dword: mode 8's entry counts it
constexpr U kBankByte = 0x90412C;          // u8: bit 7 set on leaving; its low 7 bits + 0x2C2 the sound bank
constexpr U kMusicTrack = 0x904131;        // Music_Track (u8)
constexpr U kMusicPlaying = 0x904CD0;      // u8: the track compared with it
constexpr U kReturnArea = 0x802290;        // u16: the area mode 8 goes back to
constexpr U kReturnX = 0x7E091C;           // dword: its x
constexpr U kReturnZ = 0x7E0920;           // dword: its z
constexpr U kFlags2 = 0x905BA4;            // Field_ScriptFlags2 (u16)
constexpr U kEdgeBits = 0x905B80;          // Field_EdgeBits (u16)
constexpr U kYaw = 0x929EC8;               // Camera_Angles[0] (u16)
constexpr U kPitch = 0x929ECC;             // Cond_AngleFB (u16)

// --- area 0xBD's view (AreaMap_FrameAreaBD and its build) --------------------------------
constexpr U kAnglesDrawn = 0x905D88;       // Camera_AnglesDrawn: two dwords
constexpr U kOriginX = 0x903850;           // s16: the build's first column (the effect slot's word too)
constexpr U kOriginZ = 0x903852;           // s16: its first row
constexpr U kRows = 0x903854;              // dword: the rows the build walks (0x38 or 0x2C)
constexpr U kColumns = 0x903858;           // dword: the columns (0x1C or 0x28)
constexpr U kOctants = 0x65ED48;           // 8 records of 6 s8 by the camera's octant (image .data)
constexpr U kBlocks = 0x65ED78;            // 256 bytes: the 16 x 16 block map, each byte a block's row / column nibbles
constexpr unsigned kMapWidth = 0x60;       // cells a row of AreaMap_Corners in area 0xBD
constexpr U kLayerLast = 0x802CF4;         // DrawLayers + 0xA54: layer 0x37's list 0, buffer 0, its last link
// The operands Widescreen_Inject (DIV-0041) and DrawPool_Grow (DIV-0062) patch
// inside 0x510780: read back, so ours follows whichever is in place.
constexpr U kTopBoundAt = 0x51087F;        // u16 of `cmp word [DrawItemPool_Top], 0x400`
constexpr U kItemsAt = 0x5109FC;           // u32 of `add esi, 0x905E80` (the draw-item array)
constexpr U kWideYAt = 0x510968;           // the operand of `fcomp [0x5C4244]` (120.0), not patched
constexpr U kWideLoAt = 0x51097E;          // `fcomp [0x5C4240]` (-200.0)
constexpr U kWideHiAt = 0x510991;          // `fcomp [0x5C423C]` (520.0)
constexpr U kNarrowYAt = 0x5109A4;         // `fcomp [0x5C4238]` (121.0), not patched
constexpr U kNarrowLoAt = 0x5109BB;        // `fcomp [0x5C4234]` (-50.0)
constexpr U kNarrowHiAt = 0x5109D2;        // `fcomp [0x5C4230]` (370.0)

}  // namespace rest_3g::at
