// Round fourteen group R4F: the addresses its functions read that symbols.toml
// does not name - each a load-bearing constant (CLAUDE.md rule 3). Every callee
// of the group is ours by name (symbols.gen.h); the call sites the divergences
// re-aim are read in place (docs/rest_4f.md sections 2 and 6). Cells and the
// image's read-only tables are read in place, never copied.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/rdata_consts.h"

namespace rest_4f::at {

// --- the Config screen (field menu state 7, ConfigMenu_Body's machine) ----------------
// The menu block 0x929F00.. (docs/menu-screens.md section 1): +1 the screen's
// step, +2 the Config screen's own state (ConfigScreen_Run's index), +3 its
// opening sub-state, +4 a frame counter, +5 the cursor (0, 1 the top bar's two
// buttons; 2..7 the six rows; 8..13 the controller panel's six rows).
constexpr std::uint32_t kScreenStep = 0x929F01;
constexpr std::uint32_t kState = 0x929F02;
constexpr std::uint32_t kOpenState = 0x929F03;
constexpr std::uint32_t kCounter = 0x929F04;
constexpr std::uint32_t kCursor = 0x929F05;
// The settings (docs/config-screen.md section 2 has the rows): row 0 0x903A58
// (0..2), row 1 the window style 0x903A5A (0..3), row 2 the backdrop 0x903A5B
// (0..3), row 3 0x903A59 and row 4 0x903A5E (0 / 1); 0x903A5C, 0x903A5D are
// cleared with them by the defaults.
constexpr std::uint32_t kSetting0 = 0x903A58;
constexpr std::uint32_t kSetting3 = 0x903A59;
constexpr std::uint32_t kStyle = 0x903A5A;
constexpr std::uint32_t kBackdrop = 0x903A5B;
constexpr std::uint32_t kSetting5C = 0x903A5C;
constexpr std::uint32_t kSetting5D = 0x903A5D;
constexpr std::uint32_t kSetting4 = 0x903A5E;
// The button words: 0x903580 + 2 * n (n = the controller table's byte; 2 is
// Field_MenuButton, 7 Field_ConfirmButtons, 8 Field_CancelButtons).
constexpr std::uint32_t kButtons = 0x903580;
// Input_Pressed (0x7E1BEC) as the original reads it in places: a dword, the
// word after it in its high half - only bits of the low word tested.
constexpr std::uint32_t kPressedDword = 0x7E1BEC;
// The controller panel's rows: six bytes, the button word of each row (read by
// the row, 0x6536F0 + row, and by the cursor, 0x6536E8 + cursor, cursors 8..13).
constexpr std::uint32_t kPadRows = 0x6536F0;
constexpr std::uint32_t kPadRowsByCursor = 0x6536E8;
constexpr unsigned kPadRowCount = 6;
// The option rows (docs/config-screen.md section 1): the first record and the
// count of each row by the row byte, the 16-byte records (+0 a count, +1 an s8
// x, +2 the string).
constexpr std::uint32_t kOptionFirst = 0x653808;
constexpr std::uint32_t kOptionCount = 0x653810;
constexpr std::uint32_t kOptionRecords = 0x6536F8;
// The controller panel's six names (a pointer table; config_text.cpp re-points it).
constexpr std::uint32_t kPadNames = 0x66A368;
constexpr unsigned kPadNameCount = 6;
// The sites and operands the divergences rewrite, read in place at every call
// (menu_frame.cpp DIV-0011; config_text.cpp DIV-0017, DIV-0026, DIV-0051).
constexpr std::uint32_t kPanelFrameSite = 0x461778;   // in Config_DrawPanel: E8 rel32 (Port_DroppedCall, or Menu_DrawFrame)
constexpr std::uint32_t kPadFrameSite = 0x461A84;     // in Config_DrawControllerPanel: the same
constexpr std::uint32_t kPadFrameW = 0x461A62;        // imm8 of its `push 0xC` (DIV-0026 / DIV-0051: 0xD)
constexpr std::uint32_t kPadFrameH = 0x461A60;        // imm8 of its `push 0xF`
constexpr std::uint32_t kOptionWidth = 0x4619E1;      // 3 bytes: lea eax,[eax+eax*2] (x 12 with the shl 2), or DIV-0017's shl eax,1 / nop (x 8)
constexpr std::uint32_t kOptionBigSite = 0x4619F9;    // E8 rel32: Text_DrawAt, or DIV-0017's ConfigText_DrawSelected
constexpr std::uint32_t kPadBoxWidth = 0x461B07;      // imm8 of `push 0x68` (DIV-0051: 0x58)
constexpr std::uint32_t kPadNameWidth = 0x461B36;     // 3 bytes: lea eax,[ecx+ecx*2] (x 6 with the shl 1), or DIV-0026's [ecx+ecx] (x 4)
constexpr std::uint32_t kPadNameEdge = 0x461B41;      // imm8 of `add ecx, 0x20` (DIV-0026: 0x36)
constexpr std::uint32_t kPadNameSite = 0x461B43;      // E8 rel32: Text_DrawAt, or DIV-0026's ConfigText_DrawSelected
constexpr std::uint32_t kPadSecondCell = 0x461BAC;    // 6 bytes: mov esi,[Gfx_PacketNext], or DIV-0051's jmp over the second cell

// --- the world map's exit records (Field_ExitFromCell's list) ---------------------------
constexpr std::uint32_t kExitRecords = 0x653924;      // WorldMap_Records' +0x14, a data pointer
constexpr std::uint32_t kRecordSize = 0x1C;
constexpr unsigned kRecordCount = 12;                 // the eleven records and index 11, "none" (as effect_1a.cpp)

// --- effect kinds 2, 7, 8, 9 and 0xB -----------------------------------------------------
constexpr std::uint32_t kChapterCount = 0x903848;     // the chapter's count: kind 9's last open adds one
constexpr std::uint32_t kChapterByte4A = 0x90384A;    // compared by kind 8's lines (0x32, 0x34)
constexpr unsigned kObjectCount = 30;                 // Sprite_Objects: kind 8's object by +6
constexpr std::uint32_t kExtra0 = 0x802000;           // Sprite_ObjectsExtra[0]: kind 0xB's point and angles
constexpr std::uint32_t kKind0BOffset = 0x653AAC;     // two s8: kind 0xB's start x, y offsets from the extra record's word +0x2E / +0x30
constexpr std::uint32_t kKind0BRise = 0x653AB0;       // s8 by +9 >> 2: kind 0xB's rise per frame
constexpr rdata::Const kKind0BStepX{0x5C41C8};      // 4.0f
constexpr rdata::Const kKind0BStepY{0x5C41C0};      // 2.0f
// Kind 2: ObjTrio record 0 (0x802D40): its +2 (0x802D42), +0x27 the palette,
// +0x25.., +0x2A, +0x49.. the sprite fields copied; the word 0x905E62 compared
// with 0x38; the CLUT shadow 0x80F580 (64 bytes a palette), palette 0x7B
// (0x811440) the record's own.
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kKind02Word = 0x905E62;
constexpr std::uint32_t kClutShadow = 0x80F580;
constexpr std::uint32_t kClut7B = 0x811440;
constexpr unsigned kClutBytes = 0x40;

// --- the state tables this group's dispatchers read (symbols.toml [[data]]) ----------------
constexpr std::uint32_t kConfigStates = 0x6536C0;     // ConfigScreen_States, 5
constexpr unsigned kConfigStateCount = 5;
constexpr std::uint32_t kConfigOpenStates = 0x6536D4; // ConfigScreen_OpenStates, 3
constexpr unsigned kConfigOpenCount = 3;
constexpr std::uint32_t kKind02Modes = 0x653AD4;      // EffectKind02_Modes, 4 (by +2)
constexpr unsigned kKind02ModeCount = 4;
constexpr std::uint32_t kKind02Mode1 = 0x653AE4;      // EffectKind02_Mode1Steps, 2 (by +3)
constexpr unsigned kKind02Mode1Count = 2;
constexpr std::uint32_t kKind02Mode3 = 0x653AEC;      // EffectKind02_Mode3Steps, 3 (by +3)
constexpr unsigned kKind02Mode3Count = 3;

}  // namespace rest_4f::at
