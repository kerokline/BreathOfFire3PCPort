// Round twelve group BE7 (docs/takeover-queue-field-battle.md section 3): the
// battle windows of 0x597FC0..0x59DB61, the 31 rows of analysis/round12_cut.tsv
// group BE7, each read to its last instruction with capstone (2026-09-29) and
// taken through the boss harness as an engine group (boss_harness.h,
// docs/boss_harness.md section 10). docs/battle_e7.md has every function one
// row each; names are from the code and its callers (no PSX twin of these is
// named in the sibling's corpus).
//
//   - the result screen (Window_Handler4Kinds' kinds 0 and 1): the level-up
//     window (six stat gains and two abilities learned), the drops window
//     (two items a row), one item's line, and the translucent frame both
//     draw - also BattleResultWin_DrawExp's frame;
//   - the gene windows (the window-kind handler 0x598890's kinds 1..4 and the
//     grid's draw its kind 0 calls): the grid window's boxes and frame, a
//     three-choice box, two list windows (six and twelve rows) with their
//     frame, a form icon and a flashing cursor box - each kind a state
//     dispatcher by the record's +3 over an open state and slides;
//   - the battle menu's equipment window 0x59D640 (Window_Handler8Kinds'
//     kind 3): a member's name, four stats, the preview of a set, the six
//     equipped items.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// four stack-table dispatchers abort past their tables where the original
// calls through whatever the stack holds beyond them, and the three open
// states abort on a task slot past the pool where the original writes past it
// (the owner's rule for an unchecked index, round9 doc section 6; both are
// described in docs/battle_e7.md section 7, never fixed). Every call goes
// through the harness (BH_CALL / BH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/battle_e7.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e7_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e7::at;
using U = std::uint32_t;
using boss_harness::Phase;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
// The record 0x905B84 names, read afresh wherever the original reads it.
unsigned char* Win() { return At(static_cast<U>(Long(At(at::kWindowCurrent)))); }
std::int32_t S16(unsigned v) { return static_cast<std::int16_t>(v); }
unsigned W(const unsigned char* p) { return Word(p); }
// `fild dword` then `fstp dword`: an int made a float (exact below 2^24).
void PutFloat(unsigned char* p, std::int32_t v) {
    const auto f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
const char* Str(U address) { return reinterpret_cast<const char*>(At(address)); }
char* Buf(U address) { return reinterpret_cast<char*>(At(address)); }
const unsigned char* Text(U address) { return At(address); }

// The window colour's first CLUT word (row s8 0x903A5A of the shadow
// 0x80B7A8), one five-bit field shifted into a byte. Read afresh each time,
// as the original reads it.
unsigned ColourField(unsigned shift) {
    const int row = static_cast<signed char>(B(at::kColour));
    const unsigned w = Word(At(at::kClutShadow + static_cast<U>(row * 64)));
    return (((w >> shift) & 0x1F) << 3) & 0xFF;
}

// The draw mode a window part opens with: page (x, y) under abr.
void OpenPage(unsigned abr, int x, int y) {
    const unsigned tpage = BH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFF;
    BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
}

using StatGainFn = unsigned (__cdecl*)(unsigned, unsigned);
unsigned StatGain(unsigned member, unsigned stat) { return BH_AT(StatGainFn, at::kStatGain)(member, stat) & 0xFFFF; }

// A task slot BattleTask_Create answered, as an index into the pool: the
// original writes +9 / +0xA of slot 0xFF (none) past the 48 slots.
unsigned char* TaskSlot(const char* who, unsigned slot) {
    if (slot >= at::kTaskCount)
        bof3::Fatal("%s: BattleTask_Create answered slot %u, past the %u of the pool - the original writes its +9 / "
                    "+0xA past the pool (docs/battle_e7.md section 7)",
                    who, slot, at::kTaskCount);
    return At(at::kTasks + slot * at::kTaskStride);
}

// call [esp + 4 * record +3] over a stack table of `n`: a Fatal past it (the
// original calls whatever the stack holds beyond the table).
void StackDispatch(const char* who, const U* table, unsigned n) {
    const unsigned s = Win()[3];
    if (s >= n)
        bof3::Fatal("%s: the window record's +3 is %u, past the %u entries of its stack table - the original calls "
                    "through the stack beyond it (docs/battle_e7.md section 7)",
                    who, s, n);
    Phase(table[s])();
}

// A list window's open state (0x598FA0, 0x599390, 0x5994A0): nine tasks of
// `kind` 0 and parameter `param`, the slot's +9 the row 0..2 and +0xA the
// column 0..2; then the record's +0x14 = `rows`.
void OpenTasks(const char* who, unsigned param, unsigned rows) {
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j) {
            unsigned char* const t = TaskSlot(who, BH_CALL(BattleTask_Create)(0, param));
            t[9] = static_cast<unsigned char>(i);
            t[0xA] = static_cast<unsigned char>(j);
        }
    SetWord(Win() + 0x14, rows);
}

// A row's cost: the cost bytes of its three indices (0xFF none after the
// first), added as bytes.
unsigned RowCost(const unsigned char* row) {
    unsigned cost = B(at::kCostTable + row[0]);
    if (row[1] != 0xFF) cost = (cost + B(at::kCostTable + row[1])) & 0xFF;
    if (row[2] != 0xFF) cost = (cost + B(at::kCostTable + row[2])) & 0xFF;
    return cost;
}
// 7 when the menu actor's AP (+0x9A of the member its +5 names) is below the
// cost, else 0.
unsigned CostColour(unsigned cost) {
    const unsigned char* const actor = At(static_cast<U>(Long(At(at::kMenuActor))));
    const unsigned ap = W(At(at::kPartyAp + actor[5] * at::kPartyStride));
    return ap < (cost & 0xFFFF) ? 7u : 0u;
}

}  // namespace

// ===========================================================================
// The result screen (Window_Handler4Kinds 0x597F60: kind 0 through 0x597FA0,
// kind 1)
// ===========================================================================

// original 0x5982D0: the result windows' frame at (x, y), w by h (each read
// as a word): a TILE in the window colour, semi-transparent, over page
// (0x3C0, 0); BattleWin_DrawQuadF4 shapes 8 and 9 at its top and bottom; two
// added lines (the top, the left) and two halved (the right, the bottom), in
// the window colour's three bytes. Callers: BattleResultWin_DrawLevelUp,
// BattleResultWin_DrawDrops, BattleResultWin_DrawExp.
//
// The colour arguments carry the stack slots they are built in (the red's
// upper bytes y's, the green's second byte h's, the blue's upper bytes h's);
// the lines read their colours as bytes.
extern "C" void __cdecl BattleResultWin_DrawFrame(U x, U y, U w, U h) {
    OpenPage(0, 0x3C0, 0);
    unsigned char* const p = Gfx_PacketNext;
    BH_CALL(Gpu_SetTile)(p);
    PutFloat(p + 8, S16(x));
    PutFloat(p + 0xC, S16(y) + 2);
    PutFloat(p + 0x14, static_cast<std::int32_t>(w & 0xFFFF));
    PutFloat(p + 0x18, static_cast<std::int32_t>(h & 0xFFFF));
    p[4] = static_cast<unsigned char>(ColourField(0));
    p[5] = static_cast<unsigned char>(ColourField(5));
    p[6] = static_cast<unsigned char>(ColourField(10));
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
    const unsigned r = ColourField(0), g = ColourField(5), b = ColourField(10);
    const U red = (y & 0xFFFFFF00u) | r;
    const U green = (h & 0xFF00u) | g;
    const U blue = (h & 0xFFFFFF00u) | b;
    BH_CALL(BattleWin_DrawQuadF4)(static_cast<int>(x), static_cast<int>(y), 8, 1);
    BH_CALL(BattleWin_DrawQuadF4)(static_cast<int>(x), static_cast<int>(y + h + 2), 9, 1);
    const int x2 = static_cast<int>(x + 2), xr = static_cast<int>(x + w - 3);
    const int y2 = static_cast<int>(y + 2), yb = static_cast<int>(y + h + 1);
    BH_CALL(BattleWin_DrawLineAdd)(x2, y2, xr, y2, static_cast<int>(red), static_cast<int>(green), static_cast<int>(blue));
    BH_CALL(BattleWin_DrawLineAdd)(x2, static_cast<int>(y + 3), x2, yb, static_cast<int>(red), static_cast<int>(green),
                                   static_cast<int>(blue));
    BH_CALL(BattleWin_DrawLineHalf)(xr, y2, xr, yb, static_cast<int>(red), static_cast<int>(green), static_cast<int>(blue));
    BH_CALL(BattleWin_DrawLineHalf)(x2, yb, xr, yb, static_cast<int>(red), static_cast<int>(green), static_cast<int>(blue));
}

// original 0x597FC0: the level-up window (0x597FA0's one state, kind 0 of
// Window_Handler4Kinds): the frame (0x14, 0x28, 0x118, record +9); for each
// stat 1..6 whose gain 0x432170(member +0xA, stat) is not 0, the gain again
// into its own buffer 0x904D20 + 0x20 (stat - 1) by Area08_MessageFormat and
// the system message 0xC, 0xD, 8, 9, 0xA, 0xB drawn at (0x1E, y), y from 0x2C
// by 0x10; then the two abilities byte +6 / +7 of the member's level row
// (Char_ExpTable, member +0xA, level +0xB) names: into 0x904DE0 / 0x904E00
// by 0x654900 and the system messages 0x12 / 0x13. The record re-read before
// each read, as the original.
extern "C" void __cdecl BattleResultWin_DrawLevelUp(void) {
    static constexpr unsigned char kLabels[6] = {0xC, 0xD, 8, 9, 0xA, 0xB};
    BH_CALL(BattleResultWin_DrawFrame)(0x14, 0x28, 0x118, Win()[9]);
    unsigned y = 0x2C;
    for (unsigned stat = 1; stat <= 6; ++stat) {
        if (StatGain(Win()[0xA], stat) == 0) continue;
        const unsigned gain = StatGain(Win()[0xA], stat);
        BH_CALL(Crt_sprintf)(Buf(at::kLevelBufs + 0x20 * (stat - 1)), Str(at::kFmtNumber), gain);
        const unsigned char* const label = BH_CALL(Msg_SystemPtr)(kLabels[stat - 1]);
        BH_CALL(Text_DrawAt)(0x1E, static_cast<int>(y), 0, 0xFF, label);
        y += 0x10;
    }
    const unsigned char* rec = Win();
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned row = rec[0xA] * 99u + rec[0xB];
        const unsigned ability = B(at::kLevelTable + 6 + k + row * 8);
        if (ability == 0) continue;
        BH_CALL(Crt_sprintf)(Buf(at::kLevelBufs + 0xC0 + 0x20 * k), Str(at::kFmtAbility),
                             Str(at::kAbilityRecords + ability * 24));
        const unsigned char* const label = BH_CALL(Msg_SystemPtr)(0x12 + k);
        BH_CALL(Text_DrawAt)(0x1E, static_cast<int>(y), 0, 0xFF, label);
        rec = Win();
        y += 0x10;
    }
}

// original 0x598750: one item's line of the drops window at (x, y): nothing
// unless the item and the count are not 0; the icon (0x66AF24 by
// Item_IconKind(category, item)) at (x, y + 2), dimmed by `dim`; the name
// (Item_NamePtr) at (x + 10, y) in `colour`, as many characters as
// Text_CharCount counts; and for a count above 1, the count by 0x653EC0 into
// 0x904BA0 in the 8-pixel font at (x + 0x6D, y + 2).
extern "C" void __cdecl BattleResultWin_DrawItem(U x, U y, U colour, U category, U item, U count, U dim) {
    if ((item & 0xFF) == 0 || (count & 0xFF) == 0) return;
    const unsigned kind = BH_CALL(Item_IconKind)(category, item);
    BH_CALL(Menu_DrawIcon8)(static_cast<int>(x), static_cast<int>(y + 2), B(at::kIconKinds + (kind & 0xFF)),
                            static_cast<int>(dim));
    const unsigned char* const name = BH_CALL(Item_NamePtr)(category, item);
    const unsigned n = BH_CALL(Text_CharCount)(name);
    BH_CALL(Text_DrawAt)(static_cast<int>(x + 10), static_cast<int>(y), static_cast<int>(colour), static_cast<int>(n), name);
    if ((count & 0xFF) <= 1) return;
    BH_CALL(Crt_sprintf)(Buf(at::kPrintBuf), Str(at::kFmtCount), count & 0xFF);
    BH_CALL(Text_DrawFont8)(static_cast<int>(x + 0x6D), static_cast<int>(y + 2), static_cast<int>(colour), Text(at::kPrintBuf));
}

// original 0x5984B0: the drops window (kind 1 of Window_Handler4Kinds): the
// frame at the record's (+4, +6), 0x118 wide, +9 high; then for each of the
// 0x904AE7 drops (the count re-read each time) whose word 0x904AF4[i]
// (category << 8 | item) is not 0, its line at (+4 + 5 + 138 (i & 1), +6 + 4
// + 13 (i >> 1)) - each sum a word, the +5 / +4 after - with the count
// 0x904B14[i], colour 0, not dimmed.
extern "C" void __cdecl BattleResultWin_DrawDrops(void) {
    const unsigned char* const rec = Win();
    BH_CALL(BattleResultWin_DrawFrame)(W(rec + 4), W(rec + 6), 0x118, rec[9]);
    for (unsigned i = 0; (i & 0xFF) < B(at::kDropCount); i = (i + 1) & 0xFF) {
        const unsigned word = W(At(at::kDropItems + 2 * i));
        if (word == 0) continue;
        const unsigned count = B(at::kDropCounts + i);
        const unsigned char* const r = Win();
        const U y = ((13u * (i >> 1) + W(r + 6)) & 0xFFFF) + 4;
        const U x = ((138u * (i & 1) + W(r + 4)) & 0xFFFF) + 5;
        BH_CALL(BattleResultWin_DrawItem)(x, y, 0, word >> 8, word & 0xFF, count, 0);
    }
}

// ===========================================================================
// The gene windows (the window-kind handler 0x598890: kind 0's grid through
// 0x5989B0 / 0x5989F0, kinds 1..4 the stack dispatchers below)
// ===========================================================================

// original 0x598BE0: a frame of 8-pixel pieces at (x, y), `cols` by `rows`
// cells (bytes), over page (0x340, 0x100): piece 0x25 inside (cells 1..cols-2
// by 1..rows-2), 0x24 / 0x28 along the top and bottom (cells 2..cols-3),
// 0x2A / 0x2B down the left and right (rows 2..rows-3), and the corners
// 0x23, 0x26, 0x27, 0x29 at 16 pixels in from the far edges.
extern "C" void __cdecl GeneWin_DrawFrame(U x, U y, U cols_arg, U rows_arg) {
    OpenPage(0, 0x340, 0x100);
    const int cols = static_cast<int>(cols_arg & 0xFF), rows = static_cast<int>(rows_arg & 0xFF);
    const int xi = static_cast<int>(x), yi = static_cast<int>(y);
    for (int i = 1; i < rows - 1; ++i)
        for (int j = 1; j < cols - 1; ++j) BH_CALL(Menu_DrawPiece)(xi + 8 * j, yi + 8 * i, 0x25, 1);
    for (int j = 2; j < cols - 2; ++j) {
        BH_CALL(Menu_DrawPiece)(xi + 8 * j, yi, 0x24, 1);
        BH_CALL(Menu_DrawPiece)(xi + 8 * j, yi + 8 * rows - 8, 0x28, 1);
    }
    for (int i = 2; i < rows - 2; ++i) {
        BH_CALL(Menu_DrawPiece)(xi, yi + 8 * i, 0x2A, 1);
        BH_CALL(Menu_DrawPiece)(xi + 8 * cols - 8, yi + 8 * i, 0x2B, 1);
    }
    const int xr = xi + 8 * cols - 0x10, yb = yi + 8 * rows - 0x10;
    BH_CALL(Menu_DrawPiece)(xi, yi, 0x23, 1);
    BH_CALL(Menu_DrawPiece)(xr, yi, 0x26, 1);
    BH_CALL(Menu_DrawPiece)(xi, yb, 0x27, 1);
    BH_CALL(Menu_DrawPiece)(xr, yb, 0x29, 1);
}

// original 0x598A30: the gene grid window (kind 0's two slide states call it
// before they move the record): its frame at (+4, +6), +8 by +9 cells; three
// boxes in the window colour - (+4 + 6, +6 + 0x21) sized (+8 * 8 - 0xF, +9 *
// 8 - 0x27), (+4 + 0x36, +6 + 6) 0x48 by 0x18, (+4 + 0x87, +6 + 0xA) 0x30 by
// 0x10; the label 0x66AF38 and the cost 0x904B78 (by 0x64D3EC into 0x904BA0)
// in the 12-pixel font at (+4 + 0x88 / + 0xA0, +6 + 0xC), in colour 7 when
// the menu actor's AP (+0x9A) is below the cost, else 0; and in step 4
// (0x904AA3), the hand at the first box (0x904AA6 = 0xFF) or at cell
// (0x904AA7, 0x904AA6): (+4 + 0xE + 30 column, +6 + 0x2C + 32 row). Every
// coordinate a word; the record re-read before each draw, as the original.
extern "C" void __cdecl GeneWin_DrawGrid(void) {
    const unsigned char* rec = Win();
    BH_CALL(GeneWin_DrawFrame)(W(rec + 4), W(rec + 6), rec[8], rec[9]);
    unsigned colour = B(at::kColour);
    rec = Win();
    BH_CALL(Menu_DrawBox)(static_cast<int>(W(rec + 4) + 6), static_cast<int>(W(rec + 6) + 0x21),
                          static_cast<int>(rec[8] * 8u - 0xF), static_cast<int>(rec[9] * 8u - 0x27), 0,
                          static_cast<int>(colour));
    colour = B(at::kColour);
    rec = Win();
    BH_CALL(Menu_DrawBox)(static_cast<int>(W(rec + 4) + 0x36), static_cast<int>(W(rec + 6) + 6), 0x48, 0x18, 0,
                          static_cast<int>(colour));
    colour = B(at::kColour);
    rec = Win();
    BH_CALL(Menu_DrawBox)(static_cast<int>(W(rec + 4) + 0x87), static_cast<int>(W(rec + 6) + 0xA), 0x30, 0x10, 0,
                          static_cast<int>(colour));
    const unsigned char* const actor = At(static_cast<U>(Long(At(at::kMenuActor))));
    const unsigned text_colour = W(actor + 0x9A) < B(at::kApCost) ? 7u : 0u;
    rec = Win();
    BH_CALL(Text_DrawFont12)(static_cast<int>(W(rec + 4) + 0x88), static_cast<int>(W(rec + 6) + 0xC),
                             static_cast<int>(text_colour), Text(at::kApLabel));
    BH_CALL(Crt_sprintf)(Buf(at::kPrintBuf), Str(at::kFmtAp), B(at::kApCost));
    rec = Win();
    BH_CALL(Text_DrawFont12)(static_cast<int>(W(rec + 4) + 0xA0), static_cast<int>(W(rec + 6) + 0xC),
                             static_cast<int>(text_colour), Text(at::kPrintBuf));
    if (B(at::kStep2) != 4) return;
    const unsigned row = B(at::kGeneRow);
    rec = Win();
    if (row == 0xFF) {
        BH_CALL(Menu_DrawHand)(static_cast<int>(W(rec + 4) + 0xE), static_cast<int>(W(rec + 6) + 0xC), 0);
        return;
    }
    const U hy = (row << 5) + W(rec + 6) + 0x2C;
    const U hx = B(at::kGeneColumn) * 30u + W(rec + 4) + 0xE;
    BH_CALL(Menu_DrawHand)(static_cast<int>(hx), static_cast<int>(hy), 0);
}

// original 0x598E90: the three-choice box's draw at (x, y): three boxes of
// 0x2D by 0x14, 48 pixels apart, each with its text (the pointers at
// 0x66A164, 16 characters, at +4 / +3) and its pieces - 0x66AF44 for the
// fixed choice (0x904AA5 bit 7 and bits 0..6 the box), its text colour 0,
// the others 0x66AF3C in colour 7; with no choice fixed every box in colour 0
// and 0x66AF3C, and the hand at box (0x904AA5 & 0x7F), y + 4.
extern "C" void __cdecl GeneWin_DrawChoices(U x, U y) {
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned colour = B(at::kColour);
        const U bx = x + 48 * k;
        BH_CALL(Menu_DrawBox)(static_cast<int>(bx), static_cast<int>(y), 0x2D, 0x14, 0, static_cast<int>(colour));
        const unsigned choice = B(at::kGeneChoice);
        const unsigned char* const text = At(static_cast<U>(Long(At(at::kChoiceTexts + 4 * k))));
        U pieces = at::kChoicePieces;
        unsigned text_colour = 0;
        if (choice & 0x80) {
            if (k == (choice & 0x7F)) pieces = at::kChoicePiecesOn;
            else text_colour = 7;
        }
        BH_CALL(Text_DrawAt)(static_cast<int>(bx + 4), static_cast<int>(y + 3), static_cast<int>(text_colour), 0x10, text);
        BH_CALL(Menu_DrawPieces)(static_cast<int>(bx), static_cast<int>(y), Text(pieces), 1);
    }
    const unsigned choice = B(at::kGeneChoice);
    if (choice & 0x80) return;
    BH_CALL(Menu_DrawHand)(static_cast<int>(x + 48 * (choice & 0x7F)), static_cast<int>(y + 4), 0);
}

// original 0x598DF0: the choice box's open state: +4 = 0x5A, +6 = -0x17, +3
// + 1.
extern "C" void __cdecl GeneWin_ChoiceOpen(void) {
    SetWord(Win() + 4, 0x5A);
    SetWord(Win() + 6, 0xFFE9);
    ++Win()[3];
}

// original 0x598E10: the box slides down: +6 (signed) to 0x29 by 0x10, then
// its draw at (+4, +6).
extern "C" void __cdecl GeneWin_ChoiceSlideIn(void) {
    unsigned char* const rec = Win();
    const std::int32_t v = S16(W(rec + 6));
    SetWord(rec + 6, v >= 0x29 ? 0x29u : static_cast<unsigned>(v + 0x10));
    const unsigned char* const r = Win();
    BH_CALL(GeneWin_DrawChoices)(W(r + 4), W(r + 6));
}

// original 0x598E50: the box slides up: +6 (signed) to -0x17 by 0x10, then
// its draw.
extern "C" void __cdecl GeneWin_ChoiceSlideOut(void) {
    unsigned char* const rec = Win();
    const std::int32_t v = S16(W(rec + 6));
    SetWord(rec + 6, v <= -0x17 ? 0xFFE9u : static_cast<unsigned>(v - 0x10));
    const unsigned char* const r = Win();
    BH_CALL(GeneWin_DrawChoices)(W(r + 4), W(r + 6));
}

// original 0x598DC0: kind 1 of 0x598890, the choice box: its stack table by
// the record's +3 - GeneWin_ChoiceOpen, _ChoiceSlideIn, _ChoiceSlideOut.
extern "C" void __cdecl GeneWin_ChoiceStates(void) {
    static const U kStates[3] = {bof3::addr::GeneWin_ChoiceOpen, bof3::addr::GeneWin_ChoiceSlideIn,
                                 bof3::addr::GeneWin_ChoiceSlideOut};
    StackDispatch("GeneWin_ChoiceStates", kStates, 3);
}

// original 0x599780: a list window's frame at (x, y): three boxes in the
// window colour (0x89 by 0x82, by 0x14, and 0x89 by 8 at y + 0x7A, all at x +
// 3), the title (the 5-byte string 0x66AF64 + 5 `title`, 16 characters)
// centred on x + 0x46 by six pixels a character, the piece lists 0x66AF4C and
// 0x66AF58, and its edges of pieces 1, 4, 0x16..0x1A, 0x32, 0x33. The third
// and fourth words are pushed by its callers and never read.
extern "C" void __cdecl GeneWin_DrawListFrame(U x, U y, U, U, U title) {
    const int xi = static_cast<int>(x), yi = static_cast<int>(y);
    BH_CALL(Menu_DrawBox)(xi + 3, yi + 3, 0x89, 0x82, 0, B(at::kColour));
    BH_CALL(Menu_DrawBox)(xi + 3, yi + 3, 0x89, 0x14, 0, B(at::kColour));
    BH_CALL(Menu_DrawBox)(xi + 3, yi + 0x7A, 0x89, 8, 0, B(at::kColour));
    const unsigned char* const text = At(at::kTitles + (title & 0xFF) * 5);
    const unsigned n = BH_CALL(Text_CharCount)(text);
    BH_CALL(Text_DrawAt)(xi + 0x46 - 6 * static_cast<int>(n & 0xFF), yi + 7, 0, 0x10, text);
    BH_CALL(Menu_DrawPieces)(xi, yi, Text(at::kListPieces), 1);
    BH_CALL(Menu_DrawPieces)(xi, yi, Text(at::kListPieces2), 1);
    for (int j = 0; j < 8; ++j) BH_CALL(Menu_DrawPiece)(xi + 8 * j + 0x28, yi, 1, 1);
    for (int j = 0; j < 12; ++j) BH_CALL(Menu_DrawPiece)(xi, yi + 8 * j + 0x18, 4, 1);
    BH_CALL(Menu_DrawPiece)(xi + 0x80, yi + 0x18, 0x16, 1);
    for (int j = 0; j < 9; ++j) BH_CALL(Menu_DrawPiece)(xi + 0x80, yi + 8 * j + 0x28, 0x17, 1);
    BH_CALL(Menu_DrawPiece)(xi + 0x80, yi + 0x70, 0x18, 1);
    BH_CALL(Menu_DrawPiece)(xi, yi + 0x78, 0x19, 1);
    for (int j = 0; j < 15; ++j) BH_CALL(Menu_DrawPiece)(xi + 8 * j + 8, yi + 0x78, 0x1A, 1);
    BH_CALL(Menu_DrawPiece)(xi + 8, yi, 0x32, 1);
    BH_CALL(Menu_DrawPiece)(xi + 0x80, yi, 0x33, 1);
}

// original 0x599910: a form's icon at (s16 x, s16 y): page (0x340, 0x100)
// under abr 1; a SPRT of 0x28 by 0x18 from cell v = 0x66AF70[form] - u (v %
// 6) * 40, v (v / 6) * 24 - in CLUT (v * 16, 0x1FA), shade 0x80.
extern "C" void __cdecl GeneWin_DrawFormIcon(U x, U y, U form) {
    OpenPage(1, 0x340, 0x100);
    unsigned char* const p = Gfx_PacketNext;
    const unsigned v = B(at::kFormCells + (form & 0xFF));
    SetWord(p + 0x16, BH_CALL(Gpu_GetClut)(static_cast<int>(v << 4), 0x1FA));
    p[4] = p[5] = p[6] = 0x80;
    PutFloat(p + 8, S16(x));
    PutFloat(p + 0xC, S16(y));
    p[0x14] = static_cast<unsigned char>((v % 6) * 40);
    SetWord(p + 0x18, 0x28);
    SetWord(p + 0x1A, 0x18);
    p[0x15] = static_cast<unsigned char>((v / 6) * 24);
    BH_CALL(Gpu_SetSprt)(p);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x599A00: the cursor box at (x, y), w by h (words): a LINE_F3
// (x, y), (x, y), (x, y + h - 1) and a LINE_F4 (x, y), (x + w - 1, y), (x + w
// - 1, y + h - 1), (x, y + h - 1), both in one colour - the brightness 0xFF,
// or with `flash` a pulse of Frame_Counter (bit 3 set: ((c & 6) << 5) +
// 0x3F; else ((~c & 6) << 5) + 0x3F) - in the channels `sides` bits 0..2 name
// (bit 0 the prim's +6, bit 1 its +5, bit 2 its +4), 0 in the others.
extern "C" void __cdecl GeneWin_DrawCursorBox(U x, U y, U w, U h, U flash, U sides) {
    unsigned bright = 0xFF;
    if (flash & 0xFF) {
        const unsigned c = static_cast<unsigned char>(Frame_Counter);
        bright = (c & 8) ? ((((c & 6) << 5) + 0x3F) & 0xFF) : ((((~c) & 6) << 5) + 0x3F) & 0xFF;
    }
    unsigned char ch[3];
    for (unsigned i = 0; i < 3; ++i) ch[i] = static_cast<unsigned char>((sides & (1u << i)) ? bright : 0);
    const std::int32_t xs = static_cast<std::int32_t>(x & 0xFFFF), ys = static_cast<std::int32_t>(y & 0xFFFF);
    const std::int32_t yb = static_cast<std::int32_t>(h & 0xFFFF) + ys - 1;
    const std::int32_t xr = static_cast<std::int32_t>(w & 0xFFFF) + xs - 1;
    unsigned char* p = Gfx_PacketNext;
    BH_CALL(Gpu_SetLineF3)(p);
    PutFloat(p + 8, xs);
    PutFloat(p + 0xC, ys);
    p[4] = ch[2];
    PutFloat(p + 0x14, xs);
    p[5] = ch[1];
    p[6] = ch[0];
    PutFloat(p + 0x18, ys);
    PutFloat(p + 0x20, xs);
    PutFloat(p + 0x24, yb);
    BH_CALL(Gfx_CommitPrim)(1, 0x2C);
    p = Gfx_PacketNext;
    BH_CALL(Gpu_SetLineF4)(p);
    PutFloat(p + 8, xs);
    PutFloat(p + 0x2C, xs);
    PutFloat(p + 0x30, yb);
    PutFloat(p + 0xC, ys);
    PutFloat(p + 0x18, ys);
    PutFloat(p + 0x24, yb);
    p[4] = ch[2];
    p[5] = ch[1];
    p[6] = ch[0];
    PutFloat(p + 0x20, xr);
    PutFloat(p + 0x14, xr);
    BH_CALL(Gfx_CommitPrim)(1, 0x38);
}

// original 0x5990E0: the first list window's draw (kind 2's states): its
// frame at (+4, +6) with title 0; each of the six rows' form byte + 1 into
// 0x939848 and the scroll bar (0x939848, top +0x12, at (+4 + 0x80, +6 + 0x18),
// 3 rows of +0x14, 0x5C high); then rows +0x12 .. +0x12 + 2 of 0x904608 (four
// bytes each): the row's number + 1 (by Area08_MessageFormat into 0x904BA0)
// in the 12-pixel font, and when its form byte is not 0xFF the form's icon,
// label (the system message 0x64ECB0[form & 0x1F], small font) and cost (by
// 0x66AF8C, 8-pixel font); each in colour 7 when the menu actor's AP is below
// the row's cost, else 0. Then in step 6 (0x904AA3) but not sub-step 5
// (0x904AA4) the cursor box at the row +0x10, else the hand there.
extern "C" void __cdecl GeneWin_DrawList(void) {
    const unsigned char* rec = Win();
    BH_CALL(GeneWin_DrawListFrame)(W(rec + 4), W(rec + 6), rec[8], rec[9], 0);
    for (unsigned k = 0; k < 6; ++k) B(at::kScrollMarks + k) = static_cast<unsigned char>(B(at::kGeneList + 3 + 4 * k) + 1);
    rec = Win();
    BH_CALL(Menu_DrawScrollBar)(At(at::kScrollMarks), rec[0x12], static_cast<int>(W(rec + 4) + 0x80),
                                static_cast<int>(W(rec + 6) + 0x18), 3, rec[0x14], 0x5C);
    rec = Win();
    for (unsigned i = 0; i < 3; ++i) {
        const std::int32_t index = S16(W(rec + 0x12)) + static_cast<std::int32_t>(i);
        const unsigned char* const row = At(at::kGeneList + static_cast<U>(index * 4));
        const unsigned cost = RowCost(row);
        const unsigned colour = CostColour(cost);
        BH_CALL(Crt_sprintf)(Buf(at::kPrintBuf), Str(at::kFmtNumber), index + 1);
        rec = Win();
        BH_CALL(Text_DrawFont12)(static_cast<int>(W(rec + 4) + 4), static_cast<int>(((i + 1) << 5) + W(rec + 6)),
                                 static_cast<int>(colour), Text(at::kPrintBuf));
        rec = Win();
        const unsigned form = B(at::kGeneList + 3 + static_cast<U>((S16(W(rec + 0x12)) + static_cast<std::int32_t>(i)) * 4));
        if (form == 0xFF) continue;
        BH_CALL(GeneWin_DrawFormIcon)(W(rec + 4) + 0x10, W(rec + 6) + (i << 5) + 0x18, form & 0x1F);
        rec = Win();
        const unsigned f = B(at::kGeneList + 3 + static_cast<U>((S16(W(rec + 0x12)) + static_cast<std::int32_t>(i)) * 4)) & 0x1F;
        const unsigned char* const label = BH_CALL(Msg_SystemPtr)(B(at::kFormLabels + f));
        rec = Win();
        BH_CALL(Text_DrawSmall)(static_cast<int>(W(rec + 4) + 0x10), static_cast<int>(W(rec + 6) + (i << 5) + 0x2D), colour,
                                0xFF, label);
        BH_CALL(Crt_sprintf)(Buf(at::kPrintBuf), Str(at::kFmtCost), cost);
        rec = Win();
        BH_CALL(Text_DrawFont8)(static_cast<int>(W(rec + 4) + 0x58), static_cast<int>(W(rec + 6) + (i << 5) + 0x2D),
                                static_cast<int>(colour), Text(at::kPrintBuf));
        rec = Win();
    }
    if (B(at::kStep2) == 6 && B(at::kStep3) != 5) {
        BH_CALL(GeneWin_DrawCursorBox)(W(rec + 4) + 5, (W(rec + 0x10) << 5) + W(rec + 6) + 0x18, 0x79, 0x20, 0, 6);
        return;
    }
    BH_CALL(Menu_DrawHand)(static_cast<int>(W(rec + 4) + 7), static_cast<int>(((W(rec + 0x10) + 1) << 5) + W(rec + 6)), 0);
}

// original 0x599570: the second list window's draw (kinds 3 and 4): its frame
// with title 1; the twelve rows' form bytes + 1 into 0x939850 and the scroll
// bar over them; then rows +0x12 .. +0x12 + 2 of 0x904620 that have a form
// byte (not 0xFF): the icon, the label and the cost as GeneWin_DrawList's
// (no row number). The hand at row +0x10 unless step 6 sub-step 5.
extern "C" void __cdecl GeneWin_DrawList2(void) {
    const unsigned char* rec = Win();
    BH_CALL(GeneWin_DrawListFrame)(W(rec + 4), W(rec + 6), rec[8], rec[9], 1);
    for (unsigned k = 0; k < 12; ++k)
        B(at::kScrollMarks2 + k) = static_cast<unsigned char>(B(at::kGeneList2 + 3 + 4 * k) + 1);
    rec = Win();
    BH_CALL(Menu_DrawScrollBar)(At(at::kScrollMarks2), rec[0x12], static_cast<int>(W(rec + 4) + 0x80),
                                static_cast<int>(W(rec + 6) + 0x18), 3, rec[0x14], 0x5C);
    rec = Win();
    for (unsigned i = 0; i < 3; ++i) {
        const std::int32_t index = S16(W(rec + 0x12)) + static_cast<std::int32_t>(i);
        const unsigned char* const row = At(at::kGeneList2 + static_cast<U>(index * 4));
        const unsigned form = row[3];
        if (form == 0xFF) continue;
        const unsigned cost = RowCost(row);
        const unsigned colour = CostColour(cost);
        BH_CALL(GeneWin_DrawFormIcon)(W(rec + 4) + 0x10, W(rec + 6) + (i << 5) + 0x18, form & 0x1F);
        rec = Win();
        const unsigned f = B(at::kGeneList2 + 3 + static_cast<U>((S16(W(rec + 0x12)) + static_cast<std::int32_t>(i)) * 4)) & 0x1F;
        const unsigned char* const label = BH_CALL(Msg_SystemPtr)(B(at::kFormLabels + f));
        rec = Win();
        BH_CALL(Text_DrawSmall)(static_cast<int>(W(rec + 4) + 0x10), static_cast<int>(W(rec + 6) + (i << 5) + 0x2D), colour,
                                0xFF, label);
        BH_CALL(Crt_sprintf)(Buf(at::kPrintBuf), Str(at::kFmtCost), cost);
        rec = Win();
        BH_CALL(Text_DrawFont8)(static_cast<int>(W(rec + 4) + 0x58), static_cast<int>(W(rec + 6) + (i << 5) + 0x2D),
                                static_cast<int>(colour), Text(at::kPrintBuf));
        rec = Win();
    }
    if (B(at::kStep2) == 6 && B(at::kStep3) == 5) return;
    BH_CALL(Menu_DrawHand)(static_cast<int>(W(rec + 4) + 7), static_cast<int>(((W(rec + 0x10) + 1) << 5) + W(rec + 6)), 0);
}

// original 0x598FA0: the first list's open state: nine tasks (0, 0xD), each
// slot's +9 / +0xA its row and column 0..2; the record's +0x14 = 6, +8 =
// 0x10, +9 = 0x11, +3 + 1.
extern "C" void __cdecl GeneWin_ListOpen(void) {
    OpenTasks("GeneWin_ListOpen", 0xD, 6);
    Win()[8] = 0x10;
    Win()[9] = 0x11;
    ++Win()[3];
}

// original 0x599020: the list slides right to +4 = 0x5B by 0x20, after its
// draw.
extern "C" void __cdecl GeneWin_ListSlideIn(void) {
    BH_CALL(GeneWin_DrawList)();
    unsigned char* const rec = Win();
    const std::int32_t v = S16(W(rec + 4));
    SetWord(rec + 4, v >= 0x5B ? 0x5Bu : static_cast<unsigned>(v + 0x20));
}

// original 0x599050: the list slides left by 0x20, after its draw; at +4 <=
// -0xA5 (signed) the window is freed instead (a tail jump).
extern "C" void __cdecl GeneWin_ListSlideOut(void) {
    BH_CALL(GeneWin_DrawList)();
    unsigned char* const rec = Win();
    const std::int32_t v = S16(W(rec + 4));
    if (v <= -0xA5) {
        BH_CALL(Window_FreeCurrent)();
        return;
    }
    SetWord(rec + 4, static_cast<unsigned>(v - 0x20));
}

// original 0x599080: after the draw, +4 moves left by 0x20 while above 0x11
// (signed), and is held at 0x11 (the record re-read after a move).
extern "C" void __cdecl GeneWin_ListShiftLeft(void) {
    BH_CALL(GeneWin_DrawList)();
    unsigned char* rec = Win();
    const std::int32_t v = S16(W(rec + 4));
    if (v > 0x11) {
        SetWord(rec + 4, static_cast<unsigned>(v - 0x20));
        rec = Win();
    }
    if (S16(W(rec + 4)) < 0x11) SetWord(rec + 4, 0x11);
}

// original 0x5990B0: the same to the right, to 0x5B.
extern "C" void __cdecl GeneWin_ListShiftRight(void) {
    BH_CALL(GeneWin_DrawList)();
    unsigned char* rec = Win();
    const std::int32_t v = S16(W(rec + 4));
    if (v < 0x5B) {
        SetWord(rec + 4, static_cast<unsigned>(v + 0x20));
        rec = Win();
    }
    if (S16(W(rec + 4)) > 0x5B) SetWord(rec + 4, 0x5B);
}

// original 0x598F60: kind 2 of 0x598890, the first list: its stack table by
// the record's +3 - GeneWin_ListOpen, _ListSlideIn, _ListSlideOut,
// _ListShiftLeft, _ListShiftRight.
extern "C" void __cdecl GeneWin_ListStates(void) {
    static const U kStates[5] = {bof3::addr::GeneWin_ListOpen, bof3::addr::GeneWin_ListSlideIn,
                                 bof3::addr::GeneWin_ListSlideOut, bof3::addr::GeneWin_ListShiftLeft,
                                 bof3::addr::GeneWin_ListShiftRight};
    StackDispatch("GeneWin_ListStates", kStates, 5);
}

// original 0x599390: kind 3's open state: nine tasks (0, 0xE); +0x14 = 0xC,
// +8 = 0x18, +9 = 0x11, +3 + 1.
extern "C" void __cdecl GeneWin_List2Open(void) {
    OpenTasks("GeneWin_List2Open", 0xE, 0xC);
    Win()[8] = 0x18;
    Win()[9] = 0x11;
    ++Win()[3];
}

// original 0x599410: after the second list's draw, +4 moves left to 0x5B by
// 0x20.
extern "C" void __cdecl GeneWin_List2SlideIn(void) {
    BH_CALL(GeneWin_DrawList2)();
    unsigned char* const rec = Win();
    const std::int32_t v = S16(W(rec + 4));
    SetWord(rec + 4, v <= 0x5B ? 0x5Bu : static_cast<unsigned>(v - 0x20));
}

// original 0x599440: after the draw, right by 0x20; at +4 >= 0x15B the
// window is freed instead.
extern "C" void __cdecl GeneWin_List2SlideOut(void) {
    BH_CALL(GeneWin_DrawList2)();
    unsigned char* const rec = Win();
    const std::int32_t v = S16(W(rec + 4));
    if (v >= 0x15B) {
        BH_CALL(Window_FreeCurrent)();
        return;
    }
    SetWord(rec + 4, static_cast<unsigned>(v + 0x20));
}

// original 0x599360: kind 3 of 0x598890: GeneWin_List2Open, _List2SlideIn,
// _List2SlideOut by the record's +3.
extern "C" void __cdecl GeneWin_List2States(void) {
    static const U kStates[3] = {bof3::addr::GeneWin_List2Open, bof3::addr::GeneWin_List2SlideIn,
                                 bof3::addr::GeneWin_List2SlideOut};
    StackDispatch("GeneWin_List2States", kStates, 3);
}

// original 0x5994A0: kind 4's open state: nine tasks (0, 0xE); +0x14 = 0xC,
// +3 + 1 (+8 / +9 left as they are).
extern "C" void __cdecl GeneWin_List3Open(void) {
    OpenTasks("GeneWin_List3Open", 0xE, 0xC);
    ++Win()[3];
}

// original 0x599510: after the second list's draw, +4 moves left by 0x20
// while above 0xA3, held at 0xA3.
extern "C" void __cdecl GeneWin_List3SlideIn(void) {
    BH_CALL(GeneWin_DrawList2)();
    unsigned char* rec = Win();
    const std::int32_t v = S16(W(rec + 4));
    if (v > 0xA3) {
        SetWord(rec + 4, static_cast<unsigned>(v - 0x20));
        rec = Win();
    }
    if (S16(W(rec + 4)) < 0xA3) SetWord(rec + 4, 0xA3);
}

// original 0x599540: after the draw, right by 0x20; at +4 >= 0x143 the
// window is freed instead.
extern "C" void __cdecl GeneWin_List3SlideOut(void) {
    BH_CALL(GeneWin_DrawList2)();
    unsigned char* const rec = Win();
    const std::int32_t v = S16(W(rec + 4));
    if (v >= 0x143) {
        BH_CALL(Window_FreeCurrent)();
        return;
    }
    SetWord(rec + 4, static_cast<unsigned>(v + 0x20));
}

// original 0x599470: kind 4 of 0x598890: GeneWin_List3Open, _List3SlideIn,
// _List3SlideOut by the record's +3.
extern "C" void __cdecl GeneWin_List3States(void) {
    static const U kStates[3] = {bof3::addr::GeneWin_List3Open, bof3::addr::GeneWin_List3SlideIn,
                                 bof3::addr::GeneWin_List3SlideOut};
    StackDispatch("GeneWin_List3States", kStates, 3);
}

// ===========================================================================
// The battle menu's equipment window (Window_Handler8Kinds kind 3, 0x59CC10)
// ===========================================================================

// original 0x59D640: (member, x, y, set, flags, record) - the window at (x +
// 4, y + 4) 0x78 by 0xA3; the member's name (CharacterRecords + 0xA4 member,
// 5 characters at most) centred on x + 0x3E; the four stat labels 0x66A0F8..
// at (x + 5, y + 0x27 + 13 k); Char_RecalcStats, then the four stats +0x24,
// +0x26, +0x2A, +0x28 (by 0x64E324, 8-pixel font, at (x + 0x31, y + 0x1C + 13
// k)). Unless flags bit 0: Equip_PreviewSet(member, set, marks, values) into
// the stack, and per stat a bar 0x59DB70(x + 0x51, y + 0x1C + 13 k, 0x11 for
// mark 4, 0x12 for 1, else 0x13, 0x1C, CLUT (0, 0x1E0), 0x80) and the value
// in the mark's colour at (x + 0x59, same y). Then the six equipped items
// (+0x12..+0x17, categories 0x66B5BC): the record's +0x10 = 0 first; row r
// in colour 2 when it is the record's +0xB; with the record's +0xD bit 1, an
// item Item_CanUse(2, the member 0x66972C[0x904065[s8 0x929F06]], category,
// item) refuses is drawn dim in colour 7, and for one it allows on row +0xA
// the record's +0x10 = category << 8 | item, and it is first drawn dim in
// colour 7; the row +0xA / +0xB raised by 2 pixels; then its icon
// (0x66B5B4) and name. Last the pieces 0x66B508 and the frame's edges.
extern "C" void __cdecl BattleEquipWin_Draw(U member, U x, U y, U set, U flags, U record) {
    const int xi = static_cast<int>(x), yi = static_cast<int>(y);
    BH_CALL(Menu_DrawBox)(xi + 4, yi + 4, 0x78, 0xA3, 0, B(at::kColour));
    unsigned char* const ch = At(at::kCharRecords + (member & 0xFF) * at::kCharStride);
    unsigned n = BH_CALL(Text_CharCount)(ch) & 0xFF;
    if (n > 5) n = 5;
    BH_CALL(Text_DrawAt)(xi - 6 * static_cast<int>(n) + 0x3E, yi + 7, 0, 5, ch);
    for (unsigned k = 0; k < 4; ++k)
        BH_CALL(Text_DrawAt)(xi + 5, yi + 0x27 + 13 * static_cast<int>(k), 0, 4, Text(at::kStatLabels[k]));
    BH_CALL(Char_RecalcStats)(ch);
    static constexpr unsigned kStats[4] = {0x24, 0x26, 0x2A, 0x28};
    for (unsigned k = 0; k < 4; ++k) {
        BH_CALL(Crt_sprintf)(Buf(at::kPrintBuf), Str(at::kFmtStat), W(ch + kStats[k]));
        BH_CALL(Text_DrawFont8)(xi + 0x31, yi + 0x1C + 13 * static_cast<int>(k), 0, Text(at::kPrintBuf));
    }
    if ((flags & 1) == 0) {
        unsigned char marks[4];
        unsigned short values[4];
        BH_CALL(Equip_PreviewSet)(member, reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(set)), marks,
                                  values);
        for (unsigned k = 0; k < 4; ++k) {
            const unsigned mark = marks[k];
            const unsigned colour = mark == 4 ? 0x11u : (mark != 1 ? 0x13u : 0x12u);
            BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xF, 0);
            BH_CALL(Gfx_CommitPrim)(1, 0xC);
            const int by = yi + 13 * static_cast<int>(k) + 0x1C;
            const unsigned clut = BH_CALL(Gpu_GetClut)(0, 0x1E0);
            using BarFn = void (__cdecl*)(U, U, U, U, U, U);
            BH_AT(BarFn, at::kDrawBar)(x + 0x51, static_cast<U>(by), colour, 0x1C, clut, 0x80);
            BH_CALL(Crt_sprintf)(Buf(at::kPrintBuf), Str(at::kFmtStat), static_cast<unsigned>(values[k]));
            BH_CALL(Text_DrawFont8)(xi + 0x59, by, static_cast<int>(marks[k]), Text(at::kPrintBuf));
        }
    }
    unsigned char items[6];
    std::memcpy(items, ch + 0x12, sizeof items);
    unsigned char* const rec = At(record);
    SetWord(rec + 0x10, 0);
    int text_y = yi + 0x55, icon_y = yi + 0x57;
    for (unsigned r = 0; r < 6; ++r) {
        const unsigned category = B(at::kEquipCategories + r);
        const unsigned char* const name = BH_CALL(Item_NamePtr)(category, items[r]);
        unsigned colour = 0;
        if (r == rec[0xB]) colour = 2;
        if (rec[0xD] & 2) {
            const unsigned slot = B(at::kPartyOrder + static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kFieldMember)))));
            const unsigned who = B(at::kMemberIds + slot);
            if (BH_CALL(Item_CanUse)(2, who, category, items[r]) == 0) {
                colour = 7;
            } else if (r == rec[0xA]) {
                SetWord(rec + 0x10, (category << 8) + items[r]);
            }
        }
        if (colour != 7) {
            BH_CALL(Menu_DrawIcon8)(xi + 6, icon_y, B(at::kEquipIcons + r), 1);
            const unsigned m = BH_CALL(Text_CharCount)(name);
            BH_CALL(Text_DrawAt)(xi + 0x11, text_y, 7, static_cast<int>(m), name);
        }
        const bool raised = r == rec[0xB] || r == rec[0xA];
        if (raised) {
            text_y -= 2;
            icon_y -= 2;
        }
        BH_CALL(Menu_DrawIcon8)(xi + 6, icon_y, B(at::kEquipIcons + r), colour == 7 ? 1 : 0);
        const unsigned m = BH_CALL(Text_CharCount)(name);
        BH_CALL(Text_DrawAt)(xi + 0x11, text_y, static_cast<int>(colour), static_cast<int>(m), name);
        if (r == rec[0xB] || r == rec[0xA]) {
            icon_y += 2;
            text_y += 2;
        }
        icon_y += 0xD;
        text_y += 0xD;
    }
    BH_CALL(Menu_DrawPieces)(xi, yi, Text(at::kEquipPieces), 1);
    for (int j = 0; j < 7; ++j) {
        BH_CALL(Menu_DrawPiece)(xi, yi + 8 * j + 0x18, 4, 1);
        BH_CALL(Menu_DrawPiece)(xi + 0x78, yi + 8 * j + 0x18, 8, 1);
    }
    for (int j = 0; j < 9; ++j) {
        BH_CALL(Menu_DrawPiece)(xi, yi + 8 * j + 0x58, 4, 1);
        BH_CALL(Menu_DrawPiece)(xi + 0x78, yi + 8 * j + 0x58, 8, 1);
    }
    for (int j = 0; j < 14; ++j) {
        BH_CALL(Menu_DrawPiece)(xi + 8 * j + 8, yi + 0x50, 0xE, 1);
        BH_CALL(Menu_DrawPiece)(xi + 8 * j + 8, yi + 0xA0, 0x11, 1);
    }
    BH_CALL(Menu_DrawPiece)(xi, yi + 0x50, 0xD, 1);
    BH_CALL(Menu_DrawPiece)(xi + 0x78, yi + 0x50, 0xF, 1);
    BH_CALL(Menu_DrawPiece)(xi, yi + 0xA0, 0x10, 1);
    BH_CALL(Menu_DrawPiece)(xi + 0x78, yi + 0xA0, 0x12, 1);
}

// ============================================================================

void BattleE7_Inject() {
    if (bof3::WantsShadow("battle_e7")) battle_e7::SelfTest();
    BOF3_INJECT(BattleResultWin_DrawLevelUp);
    BOF3_INJECT(BattleResultWin_DrawFrame);
    BOF3_INJECT(BattleResultWin_DrawDrops);
    BOF3_INJECT(BattleResultWin_DrawItem);
    BOF3_INJECT(GeneWin_DrawGrid);
    BOF3_INJECT(GeneWin_DrawFrame);
    BOF3_INJECT(GeneWin_ChoiceStates);
    BOF3_INJECT(GeneWin_ChoiceOpen);
    BOF3_INJECT(GeneWin_ChoiceSlideIn);
    BOF3_INJECT(GeneWin_ChoiceSlideOut);
    BOF3_INJECT(GeneWin_DrawChoices);
    BOF3_INJECT(GeneWin_ListStates);
    BOF3_INJECT(GeneWin_ListOpen);
    BOF3_INJECT(GeneWin_ListSlideIn);
    BOF3_INJECT(GeneWin_ListSlideOut);
    BOF3_INJECT(GeneWin_ListShiftLeft);
    BOF3_INJECT(GeneWin_ListShiftRight);
    BOF3_INJECT(GeneWin_DrawList);
    BOF3_INJECT(GeneWin_List2States);
    BOF3_INJECT(GeneWin_List2Open);
    BOF3_INJECT(GeneWin_List2SlideIn);
    BOF3_INJECT(GeneWin_List2SlideOut);
    BOF3_INJECT(GeneWin_List3States);
    BOF3_INJECT(GeneWin_List3Open);
    BOF3_INJECT(GeneWin_List3SlideIn);
    BOF3_INJECT(GeneWin_List3SlideOut);
    BOF3_INJECT(GeneWin_DrawList2);
    BOF3_INJECT(GeneWin_DrawListFrame);
    BOF3_INJECT(GeneWin_DrawFormIcon);
    BOF3_INJECT(GeneWin_DrawCursorBox);
    BOF3_INJECT(BattleEquipWin_Draw);
}
