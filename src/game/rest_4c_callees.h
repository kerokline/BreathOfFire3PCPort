// Group R4C's callees of other groups, by address (ours since their owners
// merged, named by symbol, the values unchanged: round fourteen's rebinding,
// docs/round-14-cleanup.md), and the cells and image tables its functions read
// in place. docs/rest_4c.md sections 3 and 8.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace rest_4c::at {

// --- callees of other groups (wave four, called by address) --------------------
//
// R4D: the second game's guess panel - a window (Menu_DrawBox at x, y, 0x52 x
// 0x7A, the style colour), R4D's frame 0x45C700, then what R4D's doc says of
// the rows; (int x, int y, unsigned flag), the flag's low byte read (0x45C447).
// void.
constexpr std::uint32_t kGuessPanel = bof3::addr::CommuBoard_DrawRows;
// R4D: one guess record's three digits (0x675F9B + 5 row + i) as cards at
// (x + 32 i, y), the one at the cursor 0x675F8D raised 8 when the flag's byte
// is set; (int x, int y, unsigned row, unsigned flag), the row's and the
// flag's low bytes read (0x45C7D0, 0x45C7FB). void.
constexpr std::uint32_t kGuessRow = bof3::addr::CommuBoard_DrawRowCells;
// R4D: the three secret digits 0x675F98.. as cards at (x + 32 i, y), or three
// backs (0xFF) when the flag's byte is 0; (int x, int y, unsigned show), the
// third argument's low byte read (0x45C850). void.
constexpr std::uint32_t kSecret = bof3::addr::CommuBoard_DrawCells;

// --- a library callee nobody owns (Capcom's, raw) ------------------------------
//
// 0x5A7570: a POLY_F3's set-up - code byte +7 = 0x20 and the three vertices'
// z floats (+0x10, +0x1C, +0x28) set; (unsigned char *prim). The effect
// standard set lists it (FxPrim0_44); this group lists it itself (field mode).
constexpr std::uint32_t kSetPolyF3 = bof3::addr::Gpu_SetPolyF3;

// --- the cells -------------------------------------------------------------------
//
// The message-choice bytes 0x939A3E.. (area_w4c's kChoiceByte3E is the first):
// the games' phase, step and state; a message script's choice lands in them.
constexpr std::uint32_t kPhase = 0x939A3E;   // u8: the game's phase (its Phases table)
constexpr std::uint32_t kStep = 0x939A3F;    // u8: a state's step (the Bet / Deal / Start tables)
constexpr std::uint32_t kState = 0x939A40;   // u8: the phase's state (the Open / Play tables)
constexpr std::uint32_t kAgain = 0x939A41;   // u8: the second game's "again" choice (0: a new game)
// The field hook's tail state (area_w*'s kTailState) and the game's index R4B
// dispatches by (CommuGame table 0x652A84, R4B's).
constexpr std::uint32_t kTailState = 0x9039F4;
// The community block 0x675F78.. (R4B's screen cells before 0x675F88).
constexpr std::uint32_t kSavedSub = 0x675F78;   // u16: 0x675F7A kept by Commu_PushSubscreen
constexpr std::uint32_t kSub = 0x675F7A;        // u16: R4B's sub-screen
constexpr std::uint32_t kSub7C = 0x675F7C;      // u8
constexpr std::uint32_t kSavedState = 0x675F81; // u8: 0x939A40 kept by Commu_PushSubscreen
constexpr std::uint32_t kBet = 0x675F88;        // s32: the stake, then the winnings (the first game); 500 (the second)
constexpr std::uint32_t kCount = 0x675F8C;      // s8: the first game's digit cursor, then its picks; the second's guess row
constexpr std::uint32_t kCursor = 0x675F8D;     // s8: a cursor; the first game's shown-card bits; the second's prize rank
constexpr std::uint32_t kTurn = 0x675F8E;       // s8: the first game's card being turned (1..8)
constexpr std::uint32_t kLost = 0x675F8F;       // u8: the first game's lost flag
constexpr std::uint32_t kMusic = 0x675F90;      // u8 (read as a dword & 0xFF): Music_Track kept
constexpr std::uint32_t kSlide = 0x675F95;      // u8: a slide's or a wait's counter
constexpr std::uint32_t kCards = 0x675F98;      // u8 x 9: the first game's nine values; the second's three digits
constexpr std::uint32_t kPicks = 0x675FA1;      // u8 x 9: the first game's picks at +1..+8 (0 / 1)
constexpr std::uint32_t kRecords = 0x675F9B;    // 8 x 5 bytes: the second game's guesses (3 digits, hits, blows)
// The bet's three digits (DamageScratch's block, docs: hundreds, tens, ones).
constexpr std::uint32_t kDigits = 0x903850;     // s8 x 3: ones +0, tens +1, hundreds +2
constexpr std::uint32_t kStyle = 0x903A5A;      // u8: the window style Menu_DrawBox is handed
constexpr std::uint32_t kTextB = 0x904D00;      // Text_Records' second record (sprintf and the prize name)

// --- image tables read in place (.data / .rdata; their bytes are not copied here) --
constexpr std::uint32_t kPayouts = 0x652CF2;    // u16 by the turn 1..8 (0x652CF4..0x652D03; entry 0 would be a code pointer's high half)
constexpr std::uint32_t kPieces = 0x652D04;     // 18 x 4 bytes: a menu piece's u, v, w, h (to 0x652D4C)
constexpr unsigned kPiecesCount = 18;
constexpr std::uint32_t kPrizes = 0x652D94;     // 24 x (item, category): the second game's prizes by row * 3 + rank (to 0x652DC4)
constexpr unsigned kPrizesCount = 24;
constexpr std::uint32_t kChoiceLabels = 0x669F54;   // 3 text pointers (the first game's three choices)
constexpr unsigned kChoiceLabelsCount = 3;
constexpr std::uint32_t kZennyTitle = 0x669F60;     // text: the money box's title
constexpr std::uint32_t kBetTitle = 0x669F68;       // text: the stake box's title
constexpr std::uint32_t kZennyUnit = 0x66A31C;      // text: the unit after an amount
constexpr std::uint32_t kFmtWide = 0x64ADDC;        // sprintf format: the amount, wide
constexpr std::uint32_t kFmt1 = 0x65306C;           // sprintf format: below 10
constexpr std::uint32_t kFmt2 = 0x653074;           // below 100
constexpr std::uint32_t kFmt3 = 0x65307C;           // below 1000

}  // namespace rest_4c::at
