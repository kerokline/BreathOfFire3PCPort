// The raw addresses effect_1g.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_1g.md.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_1g::at {

// --- callees another group of round thirteen owns, called by address (SH_AT) --
constexpr std::uint32_t kTradeBox = bof3::addr::Panel_DrawWindow;        // E1B: (x, y, w, h, colour) five words - a window frame and a fill
constexpr std::uint32_t kQuad = bof3::addr::Panel_DrawEdgeQuad;            // E1B: four words (x, y, a length, a piece 0..3) - a window side, committed
constexpr std::uint32_t kPieceMode = bof3::addr::UiSprite_SetMode;       // E1F: (id, slot) - a draw-mode primitive, committed
constexpr std::uint32_t kPiece = bof3::addr::UiSprite_Draw;           // E1F: (id, slot, x, y) - a sprite primitive, committed

// --- the trade screen's cells (field_e2_callees.h names the same ones) --------
// kTradeState: now Effect1g_TradeState (symbols.toml). u8: ItemTrade_States' index (0x593950 dispatches)
// kTradeStep: now Effect1g_TradeStep (symbols.toml). u8: the open, run and leave steps' index
// kLeaveSteps: now ItemTrade_LeaveSteps (symbols.toml). ItemTrade_LeaveSteps, 2 (its symbols.gen.h name is a typed-pointer macro)
// kTradePick: now Effect1g_TradePick (symbols.toml). u8: the entry picked, bit 7 and bit 6 marks
// kTradeRowCount: now Effect1g_TradeRowCount (symbols.toml). u8: the entries on the screen
// kTradeQuantity: now Effect1g_TradeQuantity (symbols.toml). s8: 1..99
// kTradeAnswer: now Effect1g_TradeAnswer (symbols.toml). u8: the yes / no hand
// kTradeRow: now AreaEntry_PendingFlags (symbols.toml). u8: which ten-byte row of kTradeIndex the screen shows
constexpr std::uint32_t kTradeIndex = 0x66AD10;      // 10 bytes a row: the entries' record numbers, 0xFF ends a row
constexpr std::uint32_t kTradeRecords = 0x66AB58;    // 8 bytes a record: +0 item, +1 category, +2..+4 ingredients, +5..+7 their counts
// kWaitWord: now Transition_Busy (symbols.toml). Transition_Busy (u16)
// kConfirm: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons (u16)
// kCancel: now Field_CancelButtons (symbols.toml). Field_CancelButtons (u16)
// kStyle: now Config_WindowColour (symbols.toml). u8: the window style Menu_DrawBox is handed
constexpr std::uint32_t kPoolWordAsk = 0x80360A;     // MessagePools' offsets of the prompts (u16 each)
constexpr std::uint32_t kPoolWordList = 0x80360E;
constexpr std::uint32_t kPoolWordNeeds = 0x803610;
constexpr std::uint32_t kPoolWordHeld = 0x803612;
constexpr std::uint32_t kPoolWordBag = 0x803616;
constexpr std::uint32_t kPoolWordWorn = 0x803618;
constexpr std::uint32_t kPoolWordFull = 0x80361A;
constexpr std::uint32_t kNumberFormat = 0x653EC0;    // the format Crt_sprintf is handed for every number
constexpr std::uint32_t kNumberText = 0x904BA0;      // the text scratch it prints into
constexpr std::uint32_t kIngredientBase = 0x38;      // an ingredient byte + 0x38 is the consumable's item number

// --- ranges ------------------------------------------------------------------
constexpr std::uint32_t kTextLo = 0x401000, kTextHi = 0x5C3000;   // .text
constexpr unsigned kIconKinds = 16;                               // Item_IconByKind's entries (Item_IconKind answers a nibble)

}  // namespace effect_1g::at
