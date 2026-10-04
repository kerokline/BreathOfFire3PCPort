// Round thirteen group E1G (docs/effect_1g.md): the fourteen functions of
// analysis/round13_cut.tsv's group E1G, 0x594060..0x594D8A - the item-trade
// screen's rest and the item icon. The cut filed them under the effect engine
// (unit T66A470, "Sprite_ClutWord"); read, they are the trade screen round
// twelve's FE2 took the first seven states of (field_e2.cpp), none of them an
// effect kind's state:
//
//   the states    ItemTrade_FullMessage (ItemTrade_RunSteps[3]),
//                 ItemTrade_Leave (ItemTrade_States[2]) and its two steps
//                 ItemTrade_LeaveAsk / ItemTrade_LeaveWait (ItemTrade_LeaveSteps)
//   the draws     ItemTrade_DrawBackground, ItemTrade_DrawList and its frame,
//                 ItemTrade_DrawNeeds and its frame, ItemTrade_DrawCount and
//                 its frame
//   the tests     ItemTrade_Lacks (an ingredient short), ItemTrade_RowCount
//   the icon      Item_DrawIcon (also called by E1B's 0x469210)
//
// Every call goes through the scenario harness (SH_CALL / SH_AT, a table's word
// read in place), so the start-up fuzz (effect_1g_fuzz.cpp) stands recorders in
// for ours as for the originals' copies. Faithful but for DIV-0027's leave
// prompt under a Latin overlay (LeavePrompt, amended 2026-10-03) and the
// backdrop under a wide picture (DIV-0041, ItemTrade_DrawBackground; off in
// the fuzz). Where an
// original indexes past its table, ours aborts with a message
// (docs/effect_1g.md section 6).
#include "game/effect_1g.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_1g_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1g::at;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U a) { return *At(a); }
U L(U a) { return static_cast<U>(Long(At(a))); }
std::uint16_t W(U a) { return Word(At(a)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void PutFloat(unsigned char* p, float f) { __builtin_memcpy(p, &f, 4); }

using Handler = void (__cdecl*)();

// A .data table's entry read in place (swapped for recorders while the fuzz
// runs); an entry that is not code - an index past the table - aborts, where
// the original jumps there (field_e2.cpp's CodeAt).
U CodeAt(U table, U index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = L(cell);
    if (!scenario_harness::g_active && (entry < at::kTextLo || entry >= at::kTextHi))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who,
                    (unsigned)index, (unsigned)entry, (unsigned)cell);
    return entry;
}

// The record entry k of the screen's row names: kTradeRecords +
// 8 * kTradeIndex[k + row * 10], the row byte read here (the originals read it
// again before each use). Unchecked, as the originals: an entry past its row's
// 0xFF, or a row past the table, reads further into .data (docs/effect_1g.md
// section 6).
U Record(U k) { return at::kTradeRecords + 8u * B(at::kTradeIndex + k + 10u * B(at::kTradeRow)); }

// A prompt: MessagePools + the u16 at `word`.
const unsigned char* Pool(U word) { return At(bof3::addr::MessagePools + W(word)); }
char* NumberText() { return reinterpret_cast<char*>(At(at::kNumberText)); }
const char* NumberFormat() { return reinterpret_cast<const char*>(At(at::kNumberFormat)); }

// E1B's frame of the trade screen, as every state draws it.
void TradeBox() { SH_AT(void (__cdecl*)(int, int, int, int, int), at::kTradeBox)(0x14, 0x12, 0x118, 0x13, 0); }
void PieceMode(unsigned id) { SH_AT(void (__cdecl*)(unsigned, unsigned), at::kPieceMode)(id, 1); }
void Piece(unsigned id, U x, U y) { SH_AT(void (__cdecl*)(unsigned, unsigned, U, U), at::kPiece)(id, 1, x, y); }
void Quad(U x, U y, unsigned length, unsigned piece) { SH_AT(void (__cdecl*)(U, U, unsigned, unsigned), at::kQuad)(x, y, length, piece); }

}  // namespace

// DIVERGENCE DIV-0027 (amended 2026-10-03, group YN): Manillo's "Will that be
// all?  Yes No". The original's hand stops (0xE0 and 0x104) were fitted to the
// Chinese line; under the English overlay the hand on Yes is a word short of
// it and the hand on No covers Yes. With the flag on (Effect1G_Inject, a Latin
// overlay, after the self-test, which compares the original's draw) the line is
// re-spaced and the hand stops two units left of each answer - the load / save
// screen's spacing (yes_no_layout.cpp's YesNoLayout_Tail).
unsigned char g_trade_leave_layout = 0;

namespace {

// The yes / no prompt of the leave steps: the frame, the question, the hand
// at (36 * the answer + 0xE0, 0x16), the list, the ingredients, the backdrop.
void LeavePrompt() {
    TradeBox();
    if (g_trade_leave_layout) {
        const YesNoTail t = YesNoLayout_Tail(Pool(at::kPoolWordAsk), 0x18, "Manillo's prompt");
        SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, t.line);
        SH_CALL(Menu_DrawHand)(t.stop[0] + static_cast<int>(B(at::kTradeAnswer)) * (t.stop[1] - t.stop[0]), 0x16, 0);
    } else {
        SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, Pool(at::kPoolWordAsk));
        SH_CALL(Menu_DrawHand)(static_cast<int>(36u * B(at::kTradeAnswer) + 0xE0u), 0x16, 0);
    }
    SH_CALL(ItemTrade_DrawList)(0);
    SH_CALL(ItemTrade_DrawNeeds)();
    SH_CALL(ItemTrade_DrawBackground)();
}

}  // namespace

// DIV-0041 (effect_1g.h): the left quad from 0 - columns to 0xA0, its u from
// (-columns) mod 32, so the texel at column 0 is u 0 mod 32 as in the
// original; the right one from 0xA0 to 0x140 + columns, u from 0. The page
// texture is the 32 x 32 window tiled over 256 x 256 (Tex_Convert4 /
// Tex_Convert8), so any u below 256 repeats the pattern.
effect_1g::BackdropSpan effect_1g::ItemTrade_BackdropSpan(unsigned half, unsigned columns) {
    BackdropSpan s;
    if (half == 0) {
        const unsigned u0 = (32u - (columns & 31u)) & 31u;
        const unsigned u1 = u0 + 0xA0u + columns;
        if (u1 > 0xFFu) bof3::Fatal("ItemTrade_BackdropSpan: %u columns put the left u at %u, past the byte", columns, u1);
        s.x0 = 0.0f - static_cast<float>(columns);   // not -columns: -0.0f when narrow
        s.x1 = static_cast<float>(0xA0);
        s.u0 = static_cast<unsigned char>(u0);
        s.u1 = static_cast<unsigned char>(u1);
    } else {
        const unsigned u1 = 0xA0u + columns;
        if (u1 > 0xFFu) bof3::Fatal("ItemTrade_BackdropSpan: %u columns put the right u at %u, past the byte", columns, u1);
        s.x0 = static_cast<float>(0xA0);
        s.x1 = static_cast<float>(0x140u + columns);
        s.u0 = 0;
        s.u1 = static_cast<unsigned char>(u1);
    }
    return s;
}

// ============================================================================
// The states
// ============================================================================

// original 0x594060 (ItemTrade_RunSteps[3], hidden in Sprite_ClutWord's
// catalog extent; PSX twin 0x800F5AE0): the step ItemTrade_PickItem sets when
// the item held is 99 or more. The frame; then with a confirm or cancel button
// pressed: the quantity 1 (before the sound), sound 0x106, the step 0, the
// pick's bit 7 cleared, the list ItemTrade_DrawList(0), done. Else the notice
// (MessagePools + the word at 0x80361A) at (0x18, 0x14), the list with its
// flag 1, and a tail jump to ItemTrade_DrawCount.
extern "C" void __cdecl ItemTrade_FullMessage(void) {
    TradeBox();
    if (static_cast<std::uint16_t>(W(at::kCancel) | W(at::kConfirm)) & Input_Pressed) {
        B(at::kTradeQuantity) = 1;
        SH_CALL(Sound_PlayEffect)(0x106);
        const auto pick = static_cast<unsigned char>(B(at::kTradePick) & 0x7F);
        B(at::kTradeStep) = 0;
        B(at::kTradePick) = pick;
        SH_CALL(ItemTrade_DrawList)(0);
        return;
    }
    SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, Pool(at::kPoolWordFull));
    SH_CALL(ItemTrade_DrawList)(1);
    SH_CALL(ItemTrade_DrawCount)();
}

// original 0x5940F0 (ItemTrade_States[2], hidden in Sprite_ClutWord's catalog
// extent; PSX twin 0x800F5BAC): the leave state - a tail jump through
// ItemTrade_LeaveSteps 0x66A494 by the step byte 0x93985E, unbounded, as the
// original (past the two entries ours aborts where the table runs into bytes).
extern "C" void __cdecl ItemTrade_Leave(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAt(at::kLeaveSteps, B(at::kTradeStep), "ItemTrade_Leave")))();
}

// original 0x594100 (ItemTrade_LeaveSteps[0], hidden in Sprite_ClutWord's
// catalog extent; PSX twin 0x800F5BE8): the leave question. Input_AutoRepeat of
// the pressed word's 0xA000; a move (its bits 13 or 15) flips the hand
// 0x6BE08F, sound 0x100. Else: cancel - the hand 1, sound 0x106, the pick's
// bit 7 cleared and the state 0x93985C down (both read after the sound); then,
// Input_Pressed read again, confirm - on yes (the hand 0) Transition_Start(0)
// and the step up (read after it), on no sound 0x104, bit 7 cleared, the state
// down. Both tests run in one frame when both buttons are down. Then the
// prompt (LeavePrompt, its last draw a tail jump in the original).
extern "C" void __cdecl ItemTrade_LeaveAsk(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xA000u);
    if ((moved >> 8) & 0xA0u) {
        B(at::kTradeAnswer) = static_cast<unsigned char>(B(at::kTradeAnswer) ^ 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    } else {
        if (W(at::kCancel) & Input_Pressed) {
            B(at::kTradeAnswer) = 1;
            SH_CALL(Sound_PlayEffect)(0x106);
            const auto pick = static_cast<unsigned char>(B(at::kTradePick) & 0x7F);
            const auto state = static_cast<unsigned char>(B(at::kTradeState) - 1);
            B(at::kTradePick) = pick;
            B(at::kTradeState) = state;
        }
        if (W(at::kConfirm) & Input_Pressed) {
            if (B(at::kTradeAnswer) == 0) {
                SH_CALL(Transition_Start)(0);
                B(at::kTradeStep) = static_cast<unsigned char>(B(at::kTradeStep) + 1);
            } else {
                SH_CALL(Sound_PlayEffect)(0x104);
                const auto pick = static_cast<unsigned char>(B(at::kTradePick) & 0x7F);
                const auto state = static_cast<unsigned char>(B(at::kTradeState) - 1);
                B(at::kTradePick) = pick;
                B(at::kTradeState) = state;
            }
        }
    }
    LeavePrompt();
}

// original 0x594240 (ItemTrade_LeaveSteps[1], hidden in Sprite_ClutWord's
// catalog extent; PSX twin 0x800F5DA8): once MoveScript_WaitWordDA is 0 (the
// fade Transition_Start began), Game_Step = 1 and nothing drawn; until then
// the prompt as ItemTrade_LeaveAsk draws it.
extern "C" void __cdecl ItemTrade_LeaveWait(void) {
    if (MoveScript_WaitWordDA == 0) {
        Game_Step = 1;
        return;
    }
    LeavePrompt();
}

// ============================================================================
// The draws
// ============================================================================

// original 0x5942C0 (PSX twin 0x800F5E5C): the screen's backdrop. A RECT in
// the packet pool - (32 * (Frame_Counter >> 4 & 3), 0x80, 0x20, 0x20) - as the
// texture window of a draw mode (page 0x95) at the next packet, committed to
// slot 7 (0xC); two POLY_FT4s side by side, x 0..0xA0 and 0xA0..0x140, y 0 to
// 240.0 (floats), u 0..0xA0, v 0..0xF0, CLUT 0x7A80, page 0x95, shade 0x40,
// each committed to slot 7 (0x48); then a draw mode whose texture window is
// the RECT (0, 0, 0x100, 0x100), committed to slot 7. The quads are built at
// Gfx_PacketNext as read before Gpu_SetPolyFT4.
//
// Not as the original under a wide picture (DIV-0041, docs/widescreen.md
// section 3d): the left quad starts at 0 - columns and the right one ends at
// 320 + columns, each with its u range grown by the same columns, so the
// pattern carries on into the bands - more tiles, not a stretch
// (ItemTrade_BackdropSpan). Off, and while any self-test runs
// (Widescreen_Fill() is 0 until InjectAll arms it), the original's quads.
extern "C" void __cdecl ItemTrade_DrawBackground(void) {
    unsigned char* rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetWord(rect, ((Frame_Counter >> 4) & 3u) << 5);
    SetWord(rect + 2, 0x80);
    SetWord(rect + 6, 0x20);
    SetWord(rect + 4, 0x20);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, static_cast<unsigned long>(Key(rect)));
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
    const unsigned columns = Widescreen_Fill();
    for (unsigned half = 0; half < 2; ++half) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        const effect_1g::BackdropSpan s = effect_1g::ItemTrade_BackdropSpan(half, columns);
        const float x0 = s.x0, x1 = s.x1, y1 = 240.0f;
        PutFloat(prim + 8, x0);
        SetWord(prim + 0x26, 0x95);
        SetWord(prim + 0x16, 0x7A80);
        PutFloat(prim + 0xC, 0.0f);
        PutFloat(prim + 0x1C, 0.0f);
        PutFloat(prim + 0x18, x1);
        PutFloat(prim + 0x2C, y1);
        PutFloat(prim + 0x28, x0);
        PutFloat(prim + 0x38, x1);
        PutFloat(prim + 0x3C, y1);
        prim[0x14] = s.u0;
        prim[0x15] = 0;
        prim[0x24] = s.u1;
        prim[0x25] = 0;
        prim[0x34] = s.u0;
        prim[0x35] = 0xF0;
        prim[0x44] = s.u1;
        prim[0x45] = 0xF0;
        prim[4] = 0x40;
        prim[5] = 0x40;
        prim[6] = 0x40;
        SH_CALL(Gfx_CommitPrim)(7, 0x48);
    }
    rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetWord(rect, 0);
    SetWord(rect + 2, 0);
    SetWord(rect + 6, 0x100);
    SetWord(rect + 4, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, static_cast<unsigned long>(Key(rect)));
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
}

// original 0x594410 (PSX twin 0x800F6084): the list of the row's entries.
// ItemTrade_DrawListFrame(0x14, 0x38, flag); the heading (MessagePools + the
// word at 0x80360E) at (0x36, 0x3E) in colour 7 * the flag's byte. Then for
// each entry i below the row count 0x6BE08D (read again every pass), y = 13 i:
// its name (Item_NamePtr(category, item)) and ItemTrade_Lacks(i, 1); with the
// flag's byte set the name at (0x25, y + 0x51) in colour 7 and the icon at
// (0x1C, y + 0x54) dim 1. Else on the picked entry (s8 0x6BE08C): unless an
// ingredient is short, the name at (0x25, y + 0x51) in colour 7 and the icon at
// (0x1C, y + 0x54) dim 1 - a shadow - and then, short or not, the name at
// (0x25, y + 0x4F) in colour 7 * the answer's byte and the icon at (0x1C,
// y + 0x52) dimmed by it; on every other entry the name at (0x25, y + 0x51) in
// colour 7 * the answer and the icon at (0x1C, y + 0x54) dimmed by it. The
// count of characters is Text_CharCount's; each record is read again for the
// icon (the row byte after the calls).
extern "C" void __cdecl ItemTrade_DrawList(unsigned flag) {
    SH_CALL(ItemTrade_DrawListFrame)(0x14, 0x38, flag);
    const auto on = static_cast<unsigned char>(flag);
    SH_CALL(Text_DrawAt)(0x36, 0x3E, static_cast<unsigned char>(on * 7u), 0xFF, Pool(at::kPoolWordList));
    for (U i = 0; i < B(at::kTradeRowCount); ++i) {
        U record = Record(i);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(B(record + 1), B(record));
        const unsigned char lacks = SH_CALL(ItemTrade_Lacks)(i, 1);
        const U y = 13u * i;
        if (on != 0) {
            SH_CALL(Text_DrawAt)(0x25, static_cast<int>(y + 0x51), 7, SH_CALL(Text_CharCount)(name), name);
            record = Record(i);
            SH_CALL(Item_DrawIcon)(0x1C, static_cast<int>(y + 0x54), B(record), B(record + 1), 1);
            continue;
        }
        if (static_cast<int>(i) == static_cast<signed char>(B(at::kTradePick))) {
            if (lacks == 0) {
                SH_CALL(Text_DrawAt)(0x25, static_cast<int>(y + 0x51), 7, SH_CALL(Text_CharCount)(name), name);
                record = Record(i);
                SH_CALL(Item_DrawIcon)(0x1C, static_cast<int>(y + 0x54), B(record), B(record + 1), 1);
            }
            SH_CALL(Text_DrawAt)(0x25, static_cast<int>(y + 0x4F), static_cast<unsigned char>(lacks * 7u), SH_CALL(Text_CharCount)(name), name);
            record = Record(i);
            SH_CALL(Item_DrawIcon)(0x1C, static_cast<int>(y + 0x52), B(record), B(record + 1), lacks);
        } else {
            SH_CALL(Text_DrawAt)(0x25, static_cast<int>(y + 0x51), static_cast<unsigned char>(lacks * 7u), SH_CALL(Text_CharCount)(name), name);
            record = Record(i);
            SH_CALL(Item_DrawIcon)(0x1C, static_cast<int>(y + 0x54), B(record), B(record + 1), lacks);
        }
    }
}

// original 0x594610 (PSX twin 0x800F63E4): the list's frame at (x, y).
// Menu_DrawBox(x, y + 1, 0x8C, 0xA0, the flag's byte | 0x80, the style byte
// 0x903A5A); E1F's pieces - the mode 9, the top row (0x25 at x, ten 0x26 from
// x + 0x20 by 8, 0x27 at x + 0x70, 0x28 at x + 0x88, all at y), E1B's two sides
// (x and x + 0x88, y + 0x18, length 0x84, pieces 0 and 1), the bottom row (0x30
// at x, sixteen 0x31 from x + 8 by 8, 0x32 at x + 0x88, all at y + 0x9C).
extern "C" void __cdecl ItemTrade_DrawListFrame(int x, int y, unsigned flag) {
    const U xs = static_cast<U>(x), ys = static_cast<U>(y);
    SH_CALL(Menu_DrawBox)(x, static_cast<int>(ys + 1), 0x8C, 0xA0, static_cast<unsigned char>(flag | 0x80), B(at::kStyle));
    PieceMode(9);
    Piece(0x25, xs, ys);
    for (U i = 0; i < 10; ++i) Piece(0x26, xs + 8 * i + 0x20, ys);
    Piece(0x27, xs + 0x70, ys);
    Piece(0x28, xs + 0x88, ys);
    Quad(xs, ys + 0x18, 0x84, 0);
    Quad(xs + 0x88, ys + 0x18, 0x84, 1);
    Piece(0x30, xs, ys + 0x9C);
    for (U i = 0; i < 16; ++i) Piece(0x31, xs + 8 * i + 8, ys + 0x9C);
    Piece(0x32, xs + 0x88, ys + 0x9C);
}

// original 0x594700 (PSX twin 0x800F65C0): whether an ingredient of entry k (a
// byte) falls short for `quantity` (a byte). For each of the record's three
// ingredient bytes +2.. up to an 0xFF: Inventory_Count(0, the byte + 0x38, 0)
// (a consumable held), its low word below the ingredient's count +5.. times
// the quantity (signed) answers 1; all three met (or an 0xFF first) answers 0.
// The record is read again (the row byte) before each call, the ingredient
// byte of the next pass from the record read before the call - as the
// original's registers hold it.
extern "C" unsigned char __cdecl ItemTrade_Lacks(unsigned entry, unsigned quantity) {
    const U k = entry & 0xFF, q = quantity & 0xFF;
    U record = Record(k);
    for (U i = 0; i < 3; ++i) {
        const unsigned char ingredient = B(record + 2 + i);
        if (ingredient == 0xFF) return 0;
        record = Record(k);
        const U held = SH_CALL(Inventory_Count)(0, static_cast<unsigned char>(ingredient + at::kIngredientBase), 0) & 0xFFFFu;
        if (static_cast<std::int32_t>(held) < static_cast<std::int32_t>(B(record + 5 + i) * q)) return 1;
    }
    return 0;
}

// original 0x594790 (PSX twin 0x800F66EC): the entries in the screen's row -
// its bytes from the first up to the first 0xFF, ten at most.
extern "C" unsigned char __cdecl ItemTrade_RowCount(void) {
    const U row = at::kTradeIndex + 10u * B(at::kTradeRow);
    unsigned char n = 0;
    while (B(row + n) != 0xFF) {
        ++n;
        if (n >= 10) break;
    }
    return n;
}

// original 0x5947D0 (PSX twin 0x800F6748): the ingredients of the picked entry
// (its low nibble, read once). Two frames ItemTrade_DrawNeedsFrame at (0xA8,
// 0x40) and (0xA8, 0x8C), each with its heading (the words at 0x803610 and
// 0x803612) at (0xDA, 0x47) and (0xDA, 0x93). Unless the pick has bit 7 (read
// after those), for each ingredient i up to an 0xFF, y = 13 i (the record read
// again - the row byte - before each use): its name (Item_NamePtr(0, the byte
// + 0x38)) at (0xAD, y + 0x58); the count needed times the s8 quantity through
// Crt_sprintf at (0x110, y + 0x5C) (Text_DrawFont8); the count held
// (Inventory_Count(0, the byte + 0x38, 0), its byte) against the count
// needed: below it, the name at (0xAD, y + 0xA4) and the held count at (0x110,
// y + 0xA8) in colour 7; else in colour 0.
extern "C" void __cdecl ItemTrade_DrawNeeds(void) {
    const U k = B(at::kTradePick) & 0xFu;
    SH_CALL(ItemTrade_DrawNeedsFrame)(0xA8, 0x40);
    SH_CALL(Text_DrawAt)(0xDA, 0x47, 0, 0xFF, Pool(at::kPoolWordNeeds));
    SH_CALL(ItemTrade_DrawNeedsFrame)(0xA8, 0x8C);
    SH_CALL(Text_DrawAt)(0xDA, 0x93, 0, 0xFF, Pool(at::kPoolWordHeld));
    if (B(at::kTradePick) & 0x80) return;
    for (U i = 0; i < 3; ++i) {
        const unsigned char ingredient = B(Record(k) + 2 + i);
        if (ingredient == 0xFF) return;
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, static_cast<unsigned char>(ingredient + at::kIngredientBase));
        const U y = 13u * i;
        SH_CALL(Text_DrawAt)(0xAD, static_cast<int>(y + 0x58), 0, SH_CALL(Text_CharCount)(name), name);
        const std::int32_t need = static_cast<std::int32_t>(B(Record(k) + 5 + i)) * static_cast<signed char>(B(at::kTradeQuantity));
        SH_CALL(Crt_sprintf)(NumberText(), NumberFormat(), need);
        SH_CALL(Text_DrawFont8)(0x110, static_cast<int>(y + 0x5C), 0, reinterpret_cast<const unsigned char*>(NumberText()));
        const auto held = static_cast<unsigned char>(
            SH_CALL(Inventory_Count)(0, static_cast<unsigned char>(B(Record(k) + 2 + i) + at::kIngredientBase), 0));
        const int colour = held < B(Record(k) + 5 + i) ? 7 : 0;
        SH_CALL(Text_DrawAt)(0xAD, static_cast<int>(y + 0xA4), colour, SH_CALL(Text_CharCount)(name), name);
        SH_CALL(Crt_sprintf)(NumberText(), NumberFormat(), static_cast<int>(held));
        SH_CALL(Text_DrawFont8)(0x110, static_cast<int>(y + 0xA8), colour, reinterpret_cast<const unsigned char*>(NumberText()));
    }
}

// original 0x5949F0 (PSX twin 0x800F6A9C): an ingredient window's frame at (x,
// y). Menu_DrawBox(x, y + 1, 0x84, 0x42, 0x80, the style byte); E1F's pieces -
// the mode 9, the top row (0x25 at x, nine 0x26 from x + 0x20 by 8, 0x27 at
// x + 0x68, 0x28 at x + 0x80, at y), E1B's sides (x and x + 0x80, y + 0x18,
// length 0x28, pieces 0 and 1), the bottom row (0x30 at x, fifteen 0x31 from
// x + 8 by 8, 0x32 at x + 0x80, at y + 0x40).
extern "C" void __cdecl ItemTrade_DrawNeedsFrame(int x, int y) {
    const U xs = static_cast<U>(x), ys = static_cast<U>(y);
    SH_CALL(Menu_DrawBox)(x, static_cast<int>(ys + 1), 0x84, 0x42, 0x80, B(at::kStyle));
    PieceMode(9);
    Piece(0x25, xs, ys);
    for (U i = 0; i < 9; ++i) Piece(0x26, xs + 8 * i + 0x20, ys);
    Piece(0x27, xs + 0x68, ys);
    Piece(0x28, xs + 0x80, ys);
    Quad(xs, ys + 0x18, 0x28, 0);
    Quad(xs + 0x80, ys + 0x18, 0x28, 1);
    Piece(0x30, xs, ys + 0x40);
    for (U i = 0; i < 15; ++i) Piece(0x31, xs + 8 * i + 8, ys + 0x40);
    Piece(0x32, xs + 0x80, ys + 0x40);
}

// original 0x594AD0 (PSX twin 0x800F6C74): the count window of the picked entry
// (its low nibble, read once; the record read again - the row byte - before
// each use). ItemTrade_DrawCountFrame(0x14, 0x64); the entry's name
// (Item_NamePtr(category, item)) at (0x21, 0x68); the s8 quantity through
// Crt_sprintf at (0x84, 0x6C) (Text_DrawFont8); its icon at (0x18, 0x6C), not
// dim; the label (the word at 0x803616) at (0x21, 0x7C) and
// Inventory_Count(category, item, 0)'s low word at (0x84, 0x80); the label
// (the word at 0x803618) at (0x21, 0x88) and Inventory_Count(category, item,
// 1)'s at (0x84, 0x8C). Colour 0 throughout.
extern "C" void __cdecl ItemTrade_DrawCount(void) {
    const U k = B(at::kTradePick) & 0xFu;
    SH_CALL(ItemTrade_DrawCountFrame)(0x14, 0x64);
    U record = Record(k);
    const unsigned char* const name = SH_CALL(Item_NamePtr)(B(record + 1), B(record));
    SH_CALL(Text_DrawAt)(0x21, 0x68, 0, SH_CALL(Text_CharCount)(name), name);
    SH_CALL(Crt_sprintf)(NumberText(), NumberFormat(), static_cast<int>(static_cast<signed char>(B(at::kTradeQuantity))));
    SH_CALL(Text_DrawFont8)(0x84, 0x6C, 0, reinterpret_cast<const unsigned char*>(NumberText()));
    record = Record(k);
    SH_CALL(Item_DrawIcon)(0x18, 0x6C, B(record), B(record + 1), 0);
    SH_CALL(Text_DrawAt)(0x21, 0x7C, 0, 0xFF, Pool(at::kPoolWordBag));
    record = Record(k);
    U held = SH_CALL(Inventory_Count)(B(record + 1), B(record), 0) & 0xFFFFu;
    SH_CALL(Crt_sprintf)(NumberText(), NumberFormat(), static_cast<int>(held));
    SH_CALL(Text_DrawFont8)(0x84, 0x80, 0, reinterpret_cast<const unsigned char*>(NumberText()));
    SH_CALL(Text_DrawAt)(0x21, 0x88, 0, 0xFF, Pool(at::kPoolWordWorn));
    record = Record(k);
    held = SH_CALL(Inventory_Count)(B(record + 1), B(record), 1) & 0xFFFFu;
    SH_CALL(Crt_sprintf)(NumberText(), NumberFormat(), static_cast<int>(held));
    SH_CALL(Text_DrawFont8)(0x84, 0x8C, 0, reinterpret_cast<const unsigned char*>(NumberText()));
}

// original 0x594C90 (PSX twin 0x800F6EF8): the count window's frame at (x, y).
// Menu_DrawBox(x, y, 0x88, 0x36, 0x80, the style byte); E1F's pieces - the
// mode 8, the top row (0x33 at x, sixteen 0x34 from x + 8 by 8, 0x35 at
// x + 0x84, at y), E1B's sides (x and x + 0x84, y + 8, length 0x2C, pieces 2
// and 3), the bottom row (0x36 at x, sixteen 0x37 from x + 8 by 8, 0x38 at
// x + 0x84, at y + 0x34).
extern "C" void __cdecl ItemTrade_DrawCountFrame(int x, int y) {
    const U xs = static_cast<U>(x), ys = static_cast<U>(y);
    SH_CALL(Menu_DrawBox)(x, y, 0x88, 0x36, 0x80, B(at::kStyle));
    PieceMode(8);
    Piece(0x33, xs, ys);
    for (U i = 0; i < 16; ++i) Piece(0x34, xs + 8 * i + 8, ys);
    Piece(0x35, xs + 0x84, ys);
    Quad(xs, ys + 8, 0x2C, 2);
    Quad(xs + 0x84, ys + 8, 0x2C, 3);
    Piece(0x36, xs, ys + 0x34);
    for (U i = 0; i < 16; ++i) Piece(0x37, xs + 8 * i + 8, ys + 0x34);
    Piece(0x38, xs + 0x84, ys + 0x34);
}

// ============================================================================
// The icon
// ============================================================================

// original 0x594D50 (PSX twin 0x800F70AC): an item's 8 x 8 icon at (x, y).
// Nothing when the item's byte is 0; else Item_IconKind(category, item) (a
// third word, `dim`, pushed and unread), its byte through Item_IconByKind
// 0x66A49C (16 entries: the kind is a nibble - past them ours aborts), and
// Menu_DrawIcon8(x, y, that byte, dim). Called by the trade screen's draws and
// by E1B's 0x469210 (eight sites).
extern "C" void __cdecl Item_DrawIcon(int x, int y, unsigned item, unsigned category, int dim) {
    if ((item & 0xFF) == 0) return;
    const U kind = SH_CALL(Item_IconKind)(category, item) & 0xFFu;
    if (kind >= at::kIconKinds)
        bof3::Fatal("Item_DrawIcon: Item_IconKind answered %u, past Item_IconByKind's %u entries", (unsigned)kind, at::kIconKinds);
    SH_CALL(Menu_DrawIcon8)(x, y, Item_IconByKind[kind], dim);
}

// ============================================================================

void Effect1G_Inject() {
    if (bof3::WantsShadow("effect_1g")) effect_1g::SelfTest();
    // DIVERGENCE DIV-0027 (amended 2026-10-03): after the self-test, which
    // compares the original's prompt; a Latin overlay only (DIV-0056).
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("TradeLeaveLayout", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_trade_leave_layout)),
                         &was, &is, 1);
        bof3::Log("DIV-0027    Manillo's \"Will that be all?\": answers re-spaced, the hand two units left of each");
    }
    BOF3_INJECT(ItemTrade_FullMessage);
    BOF3_INJECT(ItemTrade_Leave);
    BOF3_INJECT(ItemTrade_LeaveAsk);
    BOF3_INJECT(ItemTrade_LeaveWait);
    BOF3_INJECT(ItemTrade_DrawBackground);
    BOF3_INJECT(ItemTrade_DrawList);
    BOF3_INJECT(ItemTrade_DrawListFrame);
    BOF3_INJECT(ItemTrade_Lacks);
    BOF3_INJECT(ItemTrade_RowCount);
    BOF3_INJECT(ItemTrade_DrawNeeds);
    BOF3_INJECT(ItemTrade_DrawNeedsFrame);
    BOF3_INJECT(ItemTrade_DrawCount);
    BOF3_INJECT(ItemTrade_DrawCountFrame);
    BOF3_INJECT(Item_DrawIcon);
}
