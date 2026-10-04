// Group R2G of round fourteen (wave two): window code of record handlers 4, 5
// and 6, 48 functions at 0x597FA0..0x59AA77 - the cut's 48 rows
// (analysis/round14_cut.tsv), each read to its last instruction with capstone
// (2026-10-04) and fuzzed through the scenario harness's field mode
// (rest_2g_fuzz.cpp). docs/rest_2g.md has them one row each.
//
//   - Record handler 4's kind 0 (Window_Handler4Kinds): the level-up window's
//     one-entry stack table; and the EXP a party slot still needs for its next
//     level, which the result's EXP window (BattleResultWin_DrawExp) prints.
//   - Record handler 5, the gene windows (Window_Handler5Kinds): its kind
//     dispatch over a stack table of five (kinds 1..4 are BE7's), kind 0's
//     three states - the grid's open (a battle task per filled cell), its slide
//     in and its slide out.
//   - Record handler 6 (MenuList_Run's kinds 5..13 and 15..19, DH took 0..4):
//     each runs its state from its own .data table by the record's +3, then
//     draws a panel, box or list of ours (field_o.cpp, menu_windows.cpp) at the
//     record's (+4, +6); kind 15 draws a list of labels itself. And 26 of the
//     slide states the kinds' tables hold: the record's x (+4) or y (+6)
//     stepped until it passes a bound, where it is held and +3 goes to 0.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original indexes a table by a byte it never bounds - a state dispatch
// (.data or stack), a task slot BattleTask_Create answered - ours aborts with
// a message (round9 doc section 6); where it reads data in place by an
// unmasked byte that stays inside the image (kind 10's sizes, kind 15's rows
// and texts, the EXP table) ours reads the same bytes in place. One divergence
// has patch sites inside these bodies and survives in ours: DIV-0041
// (widescreen.cpp kSlides) widens six slide bounds; ours reads each from the
// operand it patches. Every call goes through the harness (SH_CALL / SH_AT),
// so the start-up fuzz can stand recorders in for the callees; the current
// record 0x905B84 is re-read wherever the original re-reads it.
#include "game/rest_2g.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2g_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_2g::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char B(U address) { return At(address)[0]; }
short S(const unsigned char* p) { return static_cast<short>(Word(p)); }
std::int32_t Sx(const unsigned char* p) { return static_cast<std::int32_t>(S(p)); }

// The record the window layer is running, re-read from 0x905B84 wherever the
// original re-reads it. Volatile so that ours cannot cache it either: the
// fuzz's stand-ins repoint it between calls.
unsigned char* Rec() {
    return At(*reinterpret_cast<volatile U*>(static_cast<std::uintptr_t>(at::kCurrent)));
}

// `call [table + 4 * record +3]` over a .data table of `entries` handlers, read
// in place (the fuzz swaps the cells for recorders): a Fatal past them, where
// the original calls through the dword after (the next table's, and past the
// run of code pointers, data).
void State(const char* who, U table, unsigned entries) {
    const unsigned s = Rec()[3];
    if (s >= entries)
        bof3::Fatal("%s: the window record's +3 is %u, past the %u states of 0x%X - the original calls through the dword "
                    "after (docs/rest_2g.md section 7)",
                    who, s, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * s)))))();
}

// `call [esp + 4 * byte]` over a stack table of `n` immediates: a Fatal past
// them, where the original calls whatever the stack holds beyond the table
// (its own saved registers and return address).
void StackState(const char* who, const U* table, unsigned n, unsigned index, const char* byte) {
    if (index >= n)
        bof3::Fatal("%s: the window record's %s is %u, past the %u entries of its stack table - the original calls "
                    "through the stack beyond it (docs/rest_2g.md section 7)",
                    who, byte, index, n);
    scenario_harness::Phase(table[index])();
}

// The character record (CharacterRecords index) of the party slot `slot`:
// 0x66972C[0x904062[slot]], both bytes read in place.
unsigned PartyRecord(unsigned slot) { return B(at::kMemberRecord + B(at::kParty + slot)); }

// A slide: the record's word at `off` (+4 x, +6 y) moves by `step`, and when
// it is then past `bound` - signed, by `past` - it is held there and +3 goes
// to 0. As the originals have it: the word is added in place, the record
// re-read before the compare and again before +3.
enum class Past : std::uint8_t { kAbove, kAtOrAbove, kBelow, kAtOrBelow };
void Slide(unsigned off, int step, short bound, Past past) {
    unsigned char* const moved = Rec();
    SetWord(moved + off, Word(moved + off) + static_cast<unsigned>(step));
    unsigned char* const r = Rec();
    const short v = S(r + off);
    const bool hold = past == Past::kAbove ? v > bound : past == Past::kAtOrAbove ? v >= bound : past == Past::kBelow ? v < bound : v <= bound;
    if (!hold) return;
    SetWord(r + off, static_cast<std::uint16_t>(bound));
    Rec()[3] = 0;
}
// A bound DIV-0041 widens: the imm32 of `mov ecx, imm32` (its low word is cx).
short Imm32Bound(U operand) { return static_cast<short>(Long(At(operand))); }
// The imm16 of `cmp cx, imm16`.
short Imm16Bound(U operand) { return static_cast<short>(Word(At(operand))); }

}  // namespace

// ===========================================================================
// Record handler 4: the battle result
// ===========================================================================

// original 0x597FA0: kind 0 of Window_Handler4Kinds - `call [esp + 4 +3]` over
// a stack table of one, BattleResultWin_DrawLevelUp. A Fatal at any other +3,
// where the original calls its own return address (state 1) or the caller's
// stack.
extern "C" void __cdecl BattleResultWin_LevelUpStates(void) {
    static const U kStates[1] = {bof3::addr::BattleResultWin_DrawLevelUp};
    StackState("BattleResultWin_LevelUpStates", kStates, 1, Rec()[3], "+3");
}

// original 0x598810: the EXP party slot `slot` (a byte) still needs for its
// next level, or 0 when it has as much: the character id ObjTrio[slot] +0x89
// made a roster index r by 0x4469D0 (R3B's; the argument the id under (5 slot)'s
// upper bytes, as the original's ecx holds them); the sum of the first word of
// rows 0..level of Char_ExpTable's r (99 rows of 8 bytes), level the record
// r's +0xA; less the record's EXP dword +0xC. Called by BattleResultWin_DrawExp
// (battle_result.cpp).
//
// As the original has it: the roster index is the answer's low byte, stored
// over the argument's; level + 1 rows are summed (256 at level 255); the
// compare is unsigned, and the answer the whole eax.
extern "C" unsigned __cdecl BattleResultWin_ExpToNext(unsigned slot) {
    const U s = slot & 0xFF;
    const unsigned id = B(at::kObjTrio + 0x89 + at::kObjStride * s);
    const U arg = ((s * 5) & 0xFFFFFF00u) | id;
    const unsigned r = SH_AT(unsigned (__cdecl*)(unsigned), rest_2g::kRosterIndex)(arg) & 0xFF;
    const unsigned char* const rec = At(at::kCharRecords + at::kCharStride * r);
    const unsigned rows = rec[0xA] + 1u;
    U sum = 0;
    for (unsigned k = 0; k < rows; ++k) sum += Word(At(at::kExpTable + 0x318 * r + 8 * k));
    const U exp = static_cast<U>(Long(rec + 0xC));
    return sum >= exp ? sum - exp : 0;
}

// ===========================================================================
// Record handler 5: the gene windows
// ===========================================================================

// original 0x598890: record handler 5 of Field_RunTaskRecords - `call [esp + 4
// kind]` on the record's +2 over a stack table of five: GeneWin_GridStates and
// BE7's GeneWin_ChoiceStates, _ListStates, _List2States, _List3States. A Fatal
// past five.
extern "C" void __cdecl Window_Handler5Kinds(void) {
    static const U kKinds[5] = {bof3::addr::GeneWin_GridStates, bof3::addr::GeneWin_ChoiceStates,
                                bof3::addr::GeneWin_ListStates, bof3::addr::GeneWin_List2States,
                                bof3::addr::GeneWin_List3States};
    StackState("Window_Handler5Kinds", kKinds, 5, Rec()[2], "kind +2");
}

// original 0x5988D0: kind 0 of handler 5, the gene grid: `call [esp + 4 +3]`
// over a stack table of three - GeneWin_GridOpen, _GridSlideIn, _GridSlideOut.
extern "C" void __cdecl GeneWin_GridStates(void) {
    static const U kStates[3] = {bof3::addr::GeneWin_GridOpen, bof3::addr::GeneWin_GridSlideIn,
                                 bof3::addr::GeneWin_GridSlideOut};
    StackState("GeneWin_GridStates", kStates, 3, Rec()[3], "+3");
}

// original 0x598900: the grid's open state: the record's +8 = 0x18 columns and
// +9 = 0x11 rows (the record re-read for each); the low byte of each of the
// eighteen words 0x939820.. zeroed; for each of the eighteen cells 0x939A60.. that is not 0, a battle
// task BattleTask_Create(0, 9) whose +0xB and +0x4B take the cell's index;
// then one BattleTask_Create(0, 0xA); +3 + 1 (the record re-read).
//
// As the original has it: each cell byte is read after the call before it; a
// slot the call answers past the pool's 48 (0xFF when none is free) is a Fatal
// here, where the original writes past the pool (docs/rest_2g.md section 7).
extern "C" void __cdecl GeneWin_GridOpen(void) {
    Rec()[8] = 0x18;
    Rec()[9] = 0x11;
    for (unsigned i = 0; i < 0x12; ++i) At(at::kGridWords + 2 * i)[0] = 0;
    for (unsigned i = 0; i < 0x12; ++i) {
        if (B(at::kGridCells + i) == 0) continue;
        const unsigned slot = SH_CALL(BattleTask_Create)(0, 9) & 0xFFu;
        if (slot >= at::kTaskCount)
            bof3::Fatal("GeneWin_GridOpen: BattleTask_Create answered slot %u, past the %u of the pool - the original "
                        "writes its +0xB / +0x4B past the pool (docs/rest_2g.md section 7)",
                        slot, at::kTaskCount);
        unsigned char* const task = At(at::kTasks + at::kTaskStride * slot);
        task[0x4B] = static_cast<unsigned char>(i);
        task[0xB] = static_cast<unsigned char>(i);
    }
    SH_CALL(BattleTask_Create)(0, 0xA);
    unsigned char* const r = Rec();
    r[3] = static_cast<unsigned char>(r[3] + 1);
}

// original 0x5989B0: the grid slides in, after its draw (GeneWin_DrawGrid; the
// record read once after it): from the right (+0xA 0) x moves left by 0x20
// while above 0x42, else is set to 0x42; from the left (+0xA not 0) right by
// 0x20 while below 0x42, else 0x42. As the original has it: a step can pass
// 0x42, which the next frame sets back; +3 is never changed here.
extern "C" void __cdecl GeneWin_GridSlideIn(void) {
    SH_CALL(GeneWin_DrawGrid)();
    unsigned char* const r = Rec();
    const short x = S(r + 4);
    if (r[0xA] == 0) SetWord(r + 4, x > 0x42 ? static_cast<unsigned>(x - 0x20) : 0x42u);
    else SetWord(r + 4, x < 0x42 ? static_cast<unsigned>(x + 0x20) : 0x42u);
}

// original 0x5989F0: the grid slides out, after its draw: to the right (+0xA
// 0) by 0x20 until x reaches 0x142, to the left by 0x20 until x is -0xBE or
// less; there the window is freed instead (a tail jump to Window_FreeCurrent).
// Both bounds are operands DIV-0041 widens (`cmp cx, imm16` at 0x598A05 /
// 0x598A19), read there.
extern "C" void __cdecl GeneWin_GridSlideOut(void) {
    SH_CALL(GeneWin_DrawGrid)();
    unsigned char* const r = Rec();
    const short x = S(r + 4);
    if (r[0xA] == 0) {
        if (x >= Imm16Bound(at::kGridOutRight)) {
            SH_CALL(Window_FreeCurrent)();
            return;
        }
        SetWord(r + 4, static_cast<unsigned>(x + 0x20));
        return;
    }
    if (x <= Imm16Bound(at::kGridOutLeft)) {
        SH_CALL(Window_FreeCurrent)();
        return;
    }
    SetWord(r + 4, static_cast<unsigned>(x - 0x20));
}

// ===========================================================================
// Record handler 6: MenuList_Run's kinds 5..19
// ===========================================================================

// original 0x59A030, kind 5: the state from MenuList_StatsStates, then
// Menu_DrawStatsPanel at the record's (+4, +6) for the character record of
// party slot +0xA.
extern "C" void __cdecl MenuList_StatsPanel(void) {
    State("MenuList_StatsPanel", at::kStatsStates, 7);
    const unsigned char* const r = Rec();
    SH_CALL(Menu_DrawStatsPanel)(Word(r + 4), Word(r + 6), PartyRecord(r[0xA]));
}

// original 0x59A190, kind 6: the state from MenuList_EquipStates, then
// Menu_DrawEquipPanel likewise.
extern "C" void __cdecl MenuList_EquipPanel(void) {
    State("MenuList_EquipPanel", at::kEquipStates, 3);
    const unsigned char* const r = Rec();
    SH_CALL(Menu_DrawEquipPanel)(Word(r + 4), Word(r + 6), PartyRecord(r[0xA]));
}

// original 0x59A200, kind 7: the state from MenuList_ExpStates, then
// Menu_DrawExpPanel likewise with `next` 0 (the record's EXP).
extern "C" void __cdecl MenuList_ExpPanel(void) {
    State("MenuList_ExpPanel", at::kExpStates, 3);
    const unsigned char* const r = Rec();
    SH_CALL(Menu_DrawExpPanel)(Word(r + 4), Word(r + 6), PartyRecord(r[0xA]), 0);
}

// original 0x59A270, kind 8: the state from MenuList_NextLevelStates, then
// Menu_DrawExpPanel with `next` 1 (the EXP of the next level).
extern "C" void __cdecl MenuList_NextLevelPanel(void) {
    State("MenuList_NextLevelPanel", at::kNextLevelStates, 3);
    const unsigned char* const r = Rec();
    SH_CALL(Menu_DrawExpPanel)(Word(r + 4), Word(r + 6), PartyRecord(r[0xA]), 1);
}

// original 0x59A2E0, kind 9: the state from MenuList_WideTitleStates; the
// title box Menu_DrawTitleBox(+4, +6, 0x118, 0x13, the style byte); when the
// word +0x10 is not 0, its system text (Msg_SystemPtr) at (+4 + 7, +6 + 3) in
// colour 0, and for +0x10 0x1A or 0x31 system text 0xF over it at the same
// place. The record re-read after each call, as the original.
extern "C" void __cdecl MenuList_WideTitleBox(void) {
    State("MenuList_WideTitleBox", at::kWideTitleStates, 3);
    const unsigned colour = B(at::kColour);
    const unsigned char* r = Rec();
    SH_CALL(Menu_DrawTitleBox)(Word(r + 4), Word(r + 6), 0x118, 0x13, static_cast<int>(colour));
    r = Rec();
    if (Word(r + 0x10) == 0) return;
    const unsigned char* text = SH_CALL(Msg_SystemPtr)(Word(r + 0x10));
    r = Rec();
    SH_CALL(Text_DrawAt)(static_cast<int>((Word(r + 4) + 7) & 0xFFFF), static_cast<int>((Word(r + 6) + 3) & 0xFFFF), 0, 0xFF,
                         text);
    r = Rec();
    const unsigned id = Word(r + 0x10);
    if (id != 0x1A && id != 0x31) return;
    text = SH_CALL(Msg_SystemPtr)(0xF);
    r = Rec();
    SH_CALL(Text_DrawAt)(static_cast<int>((Word(r + 4) + 7) & 0xFFFF), static_cast<int>((Word(r + 6) + 3) & 0xFFFF), 0, 0xFF,
                         text);
}

// original 0x59A400, kind 10 (no state table): Menu_DrawCursorBox at the
// record's (+4, +6), its width and height the word pair +0xA of
// MenuList_CursorBoxSizes (four pairs; the byte unbounded, read in place), the
// blink +0xB, the sides 6.
extern "C" void __cdecl MenuList_CursorBox(void) {
    const unsigned char* const r = Rec();
    const U pair = at::kCursorBoxSizes + 4u * r[0xA];
    SH_CALL(Menu_DrawCursorBox)(Word(r + 4), Word(r + 6), Word(At(pair)), Word(At(pair + 2)), r[0xB], 6);
}

// original 0x59A440, kind 11: the state from MenuList_IconWheelStates;
// Menu_DrawIconWheel(+4, +6, +0xA, +0xC, the s16 +0x10); then while +0xC (the
// lit bits) is not 0 the angle +0x10 steps by 0x40 and Menu_DrawCursorBox
// frames the wheel at (+4 + 4, +6), 0x44 by 0x34, sides 6.
//
// As the original has it: x and the kind carry the angle's sign-extension
// above them (the register it was loaded into); the record re-read after the
// wheel and after the step.
extern "C" void __cdecl MenuList_IconWheel(void) {
    State("MenuList_IconWheel", at::kIconWheelStates, 3);
    const unsigned char* r = Rec();
    const std::int32_t angle = Sx(r + 0x10);
    const U above = static_cast<U>(angle);
    SH_CALL(Menu_DrawIconWheel)(static_cast<int>((above & 0xFFFF0000u) | Word(r + 4)), Word(r + 6),
                                (above & 0xFFFFFF00u) | r[0xA], r[0xC], angle);
    unsigned char* const lit = Rec();
    if (lit[0xC] == 0) return;
    SetWord(lit + 0x10, Word(lit + 0x10) + 0x40u);
    r = Rec();
    SH_CALL(Menu_DrawCursorBox)(static_cast<int>((Word(r + 4) + 4) & 0xFFFF), Word(r + 6), 0x44, 0x34, 0, 6);
}

// original 0x59A560, kind 12: the state from MenuList_ItemListStates, then
// Menu_DrawItemList of the record (re-read).
extern "C" void __cdecl MenuList_ItemList(void) {
    State("MenuList_ItemList", at::kItemListStates, 5);
    SH_CALL(Menu_DrawItemList)(Rec());
}

// original 0x59A640, kind 13: the state from MenuList_ButtonRowStates, then
// Menu_DrawButtonRow(+4, +6, set +0xA, selected +0xB, 0). As the original has
// it: x carries the record pointer's upper half (it is loaded into the
// register that held the pointer).
extern "C" void __cdecl MenuList_ButtonRow(void) {
    State("MenuList_ButtonRow", at::kButtonRowStates, 3);
    const unsigned char* const r = Rec();
    SH_CALL(Menu_DrawButtonRow)(static_cast<int>((Key(r) & 0xFFFF0000u) | Word(r + 4)), Word(r + 6), r[0xA], r[0xB], 0);
}

// original 0x59A6B0, kind 15 (no state table): MenuList_DrawLabelList of the
// record.
extern "C" void __cdecl MenuList_LabelList(void) { SH_CALL(MenuList_DrawLabelList)(Rec()); }

// original 0x59A6C0: a list of labels in the window `rec` (kind 15's draw):
// row +0xA of the label rows 0x66B158 (6 bytes: a count, then a text index a
// line) gives n lines. A box (x + 3, y + 3) 0x65 by 16 n + 0x19 in the style
// colour; the title (the first text pointer, 0x66B12C) at (x + 0x2A, y + 7);
// the frame's pieces 0x66B0CC; then each line i's text (0x66B12C by its index)
// at (x + 7, y + 16 i + 0x1A) - the line +0xC in colour 7 and again two up in
// colour 2, the line +0xB in 7 and two up in 0, any other once in 0 - with the
// pieces 0x66B0F0 at y + 16 i + 0x18 and y + 16 i + 0x20; the closing pieces
// 0x66B0FC at y + 16 n + 0x18. Every coordinate a word.
//
// As the original has it: +0xA is read again after each line (the count and
// the next index from the row it names then); the row and text indices are
// unbounded bytes, read in place; the fields are read afresh before each call.
extern "C" void __cdecl MenuList_DrawLabelList(unsigned char* rec) {
    auto x = [rec](unsigned d) { return static_cast<int>((Word(rec + 4) + d) & 0xFFFF); };
    auto y = [rec](unsigned d) { return static_cast<int>((Word(rec + 6) + d) & 0xFFFF); };
    auto text = [](unsigned index) { return At(static_cast<U>(Long(At(at::kLabelTexts + 4 * index)))); };
    const unsigned colour = B(at::kColour);
    const unsigned count = B(at::kLabelRows + 6u * rec[0xA]);
    SH_CALL(Menu_DrawBox)(x(3), y(3), 0x65, static_cast<int>((count << 4) + 0x19), 0, static_cast<int>(colour));
    SH_CALL(Text_DrawAt)(x(0x2A), y(7), 0, 0xFF, text(0));
    SH_CALL(Menu_DrawPieces)(x(0), y(0), At(at::kLabelPieces), 1);
    unsigned row = 0;
    unsigned line = 6u * rec[0xA];
    if (B(at::kLabelRows + line) != 0) {
        do {
            const unsigned char* const t = text(B(at::kLabelRows + 1 + line + row));
            const unsigned dy = row << 4;
            if (row == rec[0xC]) {
                SH_CALL(Text_DrawAt)(x(7), y(dy + 0x1A), 7, 0xFF, t);
                SH_CALL(Text_DrawAt)(x(7), y(dy + 0x18), 2, 0xFF, t);
            } else if (row == rec[0xB]) {
                SH_CALL(Text_DrawAt)(x(7), y(dy + 0x1A), 7, 0xFF, t);
                SH_CALL(Text_DrawAt)(x(7), y(dy + 0x18), 0, 0xFF, t);
            } else {
                SH_CALL(Text_DrawAt)(x(7), y(dy + 0x1A), 0, 0xFF, t);
            }
            SH_CALL(Menu_DrawPieces)(x(0), y(dy + 0x18), At(at::kLabelRowPieces), 1);
            SH_CALL(Menu_DrawPieces)(x(0), y(dy + 0x20), At(at::kLabelRowPieces), 1);
            row = (row + 1) & 0xFF;
            line = 6u * rec[0xA];
        } while (row < B(at::kLabelRows + line));
    }
    SH_CALL(Menu_DrawPieces)(x(0), y((row << 4) + 0x18), At(at::kLabelEndPieces), 1);
}

// original 0x59A8E0, kind 16: the state from MenuList_EquipCompareStates, then
// Menu_DrawEquipCompare(the character record of party slot +0xC, +4, +6, the
// set +0x20, +0xD, the record).
//
// As the original has it: x carries the upper half of +0x20 (loaded into the
// register that held it), and the record index x's upper three bytes.
extern "C" void __cdecl MenuList_EquipCompare(void) {
    State("MenuList_EquipCompare", at::kEquipCompareStates, 5);
    unsigned char* const r = Rec();
    const U set = static_cast<U>(Long(r + 0x20));
    const U x = (set & 0xFFFF0000u) | Word(r + 4);
    const U record = (x & 0xFFFFFF00u) | PartyRecord(r[0xC]);
    SH_CALL(Menu_DrawEquipCompare)(record, static_cast<int>(x), Word(r + 6), At(set), r[0xD], r);
}

// original 0x59A9C0, kind 17: the state from MenuList_AbilityStates, then
// Menu_DrawAbilityPanel of the record (re-read).
extern "C" void __cdecl MenuList_AbilityPanel(void) {
    State("MenuList_AbilityPanel", at::kAbilityStates, 5);
    SH_CALL(Menu_DrawAbilityPanel)(Rec());
}

// original 0x59AA10, kind 18: the state from MenuList_ItemPanelStates, then
// Menu_DrawItemPanel of the record.
extern "C" void __cdecl MenuList_ItemPanel(void) {
    State("MenuList_ItemPanel", at::kItemPanelStates, 5);
    SH_CALL(Menu_DrawItemPanel)(Rec());
}

// original 0x59AA30, kind 19: the state from MenuList_ReserveStates, then
// 0x59AA80 (R2H's: the reserve list, menu_frame.cpp) of the record.
extern "C" void __cdecl MenuList_ReservePanel(void) {
    State("MenuList_ReservePanel", at::kReserveStates, 3);
    SH_AT(void (__cdecl*)(unsigned char*), rest_2g::kReserveList)(Rec());
}

// ===========================================================================
// The slide states of handler 6's tables
// ===========================================================================
//
// Each: x (+4) by 0x20 or y (+6) by 0x10 a frame until past its bound, held
// there with +3 = 0 (Slide above). "Ge" / "Le" where the original holds the
// word at the bound itself too (`jl` / `jg` over the store, where the others
// have `jle` / `jge`).

// original 0x599C30 (MenuList_PanelStates[1]): right to x 0xB6.
extern "C" void __cdecl MenuList_SlideRightTo182(void) { Slide(4, 0x20, 0xB6, Past::kAbove); }
// original 0x599C60 (MenuList_PanelStates[3]): left to x 0x5C.
extern "C" void __cdecl MenuList_SlideLeftTo92(void) { Slide(4, -0x20, 0x5C, Past::kBelow); }
// original 0x599C90 (MenuList_PanelStates[4]): right to x 0x5C.
extern "C" void __cdecl MenuList_SlideRightTo92(void) { Slide(4, 0x20, 0x5C, Past::kAbove); }
// original 0x599CC0 (MenuList_PanelStates[7]): left to x -300 or less (off
// the left); the bound the operand DIV-0041 widens (-353).
extern "C" void __cdecl MenuList_SlideLeftOff300(void) { Slide(4, -0x20, Imm32Bound(at::kLeftOff300Bound), Past::kAtOrBelow); }
// original 0x599CF0 (MenuList_PanelStates[8]): right to x 0x11 or more.
extern "C" void __cdecl MenuList_SlideRightTo17Ge(void) { Slide(4, 0x20, 0x11, Past::kAtOrAbove); }
// original 0x599D20 (MenuList_PanelStates[10]): left to x 0xB6.
extern "C" void __cdecl MenuList_SlideLeftTo182(void) { Slide(4, -0x20, 0xB6, Past::kBelow); }
// original 0x599D90 (MenuList_MoneyStates[2]): left to x 0xC8.
extern "C" void __cdecl MenuList_SlideLeftTo200(void) { Slide(4, -0x20, 0xC8, Past::kBelow); }
// original 0x599DF0 (MenuList_TimeStates[1]): left to x -100 (off the left);
// DIV-0041 widens the bound (-153).
extern "C" void __cdecl MenuList_SlideLeftOff100(void) { Slide(4, -0x20, Imm32Bound(at::kLeftOff100Bound), Past::kBelow); }
// original 0x599E20 (MenuList_TimeStates[2]): right to x 0x10.
extern "C" void __cdecl MenuList_SlideRightTo16(void) { Slide(4, 0x20, 0x10, Past::kAbove); }
// original 0x599F70 (MenuList_IconStates[2]): down to y 0x2A.
extern "C" void __cdecl MenuList_SlideDownTo42(void) { Slide(6, 0x10, 0x2A, Past::kAbove); }

// original 0x59A070 (MenuList_StatsStates[2]): left by 0x20 until x is below
// window record 4's x + 0x78 (the field menu's first member panel's right),
// where it takes that. As the original has it: the bound is the s16 at
// 0x8031F4 plus 0x78 as an int, read after the step and before the record is
// re-read; the store its low word.
extern "C" void __cdecl MenuList_SlideLeftBesidePanel(void) {
    unsigned char* const moved = Rec();
    SetWord(moved + 4, Word(moved + 4) - 0x20u);
    const U panel = Word(At(at::kRecord4X));
    unsigned char* const r = Rec();
    if (Sx(r + 4) >= static_cast<std::int32_t>(static_cast<short>(panel)) + 0x78) return;
    SetWord(r + 4, panel + 0x78);
    Rec()[3] = 0;
}

// original 0x59A0B0 (MenuList_PanelStates[5], MenuList_StatsStates[3]): up to
// y 0x3E or less.
extern "C" void __cdecl MenuList_SlideUpTo62Le(void) { Slide(6, -0x10, 0x3E, Past::kAtOrBelow); }

// original 0x59A0E0 (MenuList_PanelStates[6], MenuList_StatsStates[4]): down
// by 0x10 until y reaches its member panel's row, 0x3E + 0x36 +0xA (the rows
// FieldMenu_PlaceWindows gives records 4..6). As the original has it: +0xA is
// read from the record re-read after the step; the bound an int, the store its
// low word.
extern "C" void __cdecl MenuList_SlideDownToPanelRow(void) {
    unsigned char* const moved = Rec();
    SetWord(moved + 6, Word(moved + 6) + 0x10u);
    unsigned char* const r = Rec();
    const std::int32_t bound = 0x36 * static_cast<std::int32_t>(r[0xA]) + 0x3E;
    if (Sx(r + 6) < bound) return;
    SetWord(r + 6, static_cast<unsigned>(bound));
    Rec()[3] = 0;
}

// original 0x59A130 (MenuList_StatsStates[5]): left to x -180 or less;
// DIV-0041 widens the bound (-233).
extern "C" void __cdecl MenuList_SlideLeftOff180(void) { Slide(4, -0x20, Imm32Bound(at::kLeftOff180Bound), Past::kAtOrBelow); }
// original 0x59A160 (MenuList_StatsStates[6]): right to x 0x89 or more.
extern "C" void __cdecl MenuList_SlideRightTo137Ge(void) { Slide(4, 0x20, 0x89, Past::kAtOrAbove); }
// original 0x59A1D0 (MenuList_EquipStates[2]): left to x 0xA6.
extern "C" void __cdecl MenuList_SlideLeftTo166(void) { Slide(4, -0x20, 0xA6, Past::kBelow); }
// original 0x59A240 (MenuList_ExpStates[2], MenuList_NextLevelStates[2]):
// right to x 0x2D.
extern "C" void __cdecl MenuList_SlideRightTo45(void) { Slide(4, 0x20, 0x2D, Past::kAbove); }
// original 0x59A2B0 (MenuList_ExpStates[1], MenuList_NextLevelStates[1]): left
// to x -110; DIV-0041 widens the bound (-163).
extern "C" void __cdecl MenuList_SlideLeftOff110(void) { Slide(4, -0x20, Imm32Bound(at::kLeftOff110Bound), Past::kBelow); }

// original 0x59A4B0 (MenuList_IconWheelStates[1]): right by 0x20 until x is
// past 0x140 + 75 (+0xB & 1) - off the right, one column further for an odd
// +0xB. As the original has it: +0xB from the record re-read after the step.
extern "C" void __cdecl MenuList_SlideRightOffColumn(void) {
    unsigned char* const moved = Rec();
    SetWord(moved + 4, Word(moved + 4) + 0x20u);
    unsigned char* const r = Rec();
    const std::int32_t bound = 75 * static_cast<std::int32_t>(r[0xB] & 1) + 0x140;
    if (Sx(r + 4) <= bound) return;
    SetWord(r + 4, static_cast<unsigned>(bound));
    Rec()[3] = 0;
}

// original 0x59A510 (MenuList_IconWheelStates[2]): left by 0x20 until x is
// below 75 ((+0xB & 1) + 2) - 150 or 225 - where it takes that.
extern "C" void __cdecl MenuList_SlideLeftToColumn(void) {
    unsigned char* const moved = Rec();
    SetWord(moved + 4, Word(moved + 4) - 0x20u);
    unsigned char* const r = Rec();
    const std::int32_t bound = 75 * (static_cast<std::int32_t>(r[0xB] & 1) + 2);
    if (Sx(r + 4) >= bound) return;
    SetWord(r + 4, static_cast<unsigned>(bound));
    Rec()[3] = 0;
}

// original 0x59A610 (MenuList_ItemListStates[4], MenuList_ItemPanelStates[4]
// and two tables of other handlers): left to x 0x98.
extern "C" void __cdecl MenuList_SlideLeftTo152(void) { Slide(4, -0x20, 0x98, Past::kBelow); }
// original 0x59A930 (MenuList_EquipCompareStates[2]): left to x 0xA5.
extern "C" void __cdecl MenuList_SlideLeftTo165(void) { Slide(4, -0x20, 0xA5, Past::kBelow); }
// original 0x59A960 (MenuList_PanelStates[2], MenuList_EquipCompareStates[3],
// MenuList_AbilityStates[4] and two tables of other handlers): left to x 0x11.
extern "C" void __cdecl MenuList_SlideLeftTo17(void) { Slide(4, -0x20, 0x11, Past::kBelow); }
// original 0x59A990 (MenuList_EquipCompareStates[4]): right to x 0xA5.
extern "C" void __cdecl MenuList_SlideRightTo165(void) { Slide(4, 0x20, 0xA5, Past::kAbove); }
// original 0x59A9E0 (MenuList_AbilityStates[3] and a table of another
// handler): right to x 0x96.
extern "C" void __cdecl MenuList_SlideRightTo150(void) { Slide(4, 0x20, 0x96, Past::kAbove); }
// original 0x59AA50 (MenuList_AbilityStates[2], MenuList_ReserveStates[2], the
// next table's [2] and two of other handlers): left to x 0x96.
extern "C" void __cdecl MenuList_SlideLeftTo150(void) { Slide(4, -0x20, 0x96, Past::kBelow); }

// ===========================================================================

// DIV-0041's six patch sites inside our bodies: the slides read their bound
// from the operand widescreen.cpp's kSlides patches. Refused unless the bytes
// before each are the instruction's (`mov ecx, imm32` B9, `cmp cx, imm16` 66
// 81 F9) and the bound is the original's or the widened one.
namespace {
void CheckBound(U operand, unsigned size, int original) {
    const int wide = original < 0 ? original - static_cast<int>(Widescreen_Live()) : original + static_cast<int>(Widescreen_Live());
    const bool op = size == 4 ? At(operand - 1)[0] == 0xB9
                              : At(operand - 3)[0] == 0x66 && At(operand - 2)[0] == 0x81 && At(operand - 1)[0] == 0xF9;
    const int bound = size == 4 ? static_cast<int>(Long(At(operand))) : static_cast<int>(static_cast<short>(Word(At(operand))));
    if (!op || (bound != original && bound != wide))
        bof3::Fatal("rest_2g: the operand at 0x%X should be the bound %d (or %d, DIV-0041), holds %d", operand, original, wide,
                    bound);
}
}  // namespace

void Rest2G_Inject() {
    CheckBound(at::kLeftOff300Bound, 4, -300);
    CheckBound(at::kLeftOff100Bound, 4, -100);
    CheckBound(at::kLeftOff180Bound, 4, -180);
    CheckBound(at::kLeftOff110Bound, 4, -110);
    CheckBound(at::kGridOutRight, 2, 0x142);
    CheckBound(at::kGridOutLeft, 2, -0xBE);
    if (bof3::WantsShadow("rest_2g")) rest_2g::SelfTest();
    BOF3_INJECT(BattleResultWin_LevelUpStates);
    BOF3_INJECT(BattleResultWin_ExpToNext);
    BOF3_INJECT(Window_Handler5Kinds);
    BOF3_INJECT(GeneWin_GridStates);
    BOF3_INJECT(GeneWin_GridOpen);
    BOF3_INJECT(GeneWin_GridSlideIn);
    BOF3_INJECT(GeneWin_GridSlideOut);
    BOF3_INJECT(MenuList_SlideRightTo182);
    BOF3_INJECT(MenuList_SlideLeftTo92);
    BOF3_INJECT(MenuList_SlideRightTo92);
    BOF3_INJECT(MenuList_SlideLeftOff300);
    BOF3_INJECT(MenuList_SlideRightTo17Ge);
    BOF3_INJECT(MenuList_SlideLeftTo182);
    BOF3_INJECT(MenuList_SlideLeftTo200);
    BOF3_INJECT(MenuList_SlideLeftOff100);
    BOF3_INJECT(MenuList_SlideRightTo16);
    BOF3_INJECT(MenuList_SlideDownTo42);
    BOF3_INJECT(MenuList_StatsPanel);
    BOF3_INJECT(MenuList_SlideLeftBesidePanel);
    BOF3_INJECT(MenuList_SlideUpTo62Le);
    BOF3_INJECT(MenuList_SlideDownToPanelRow);
    BOF3_INJECT(MenuList_SlideLeftOff180);
    BOF3_INJECT(MenuList_SlideRightTo137Ge);
    BOF3_INJECT(MenuList_EquipPanel);
    BOF3_INJECT(MenuList_SlideLeftTo166);
    BOF3_INJECT(MenuList_ExpPanel);
    BOF3_INJECT(MenuList_SlideRightTo45);
    BOF3_INJECT(MenuList_NextLevelPanel);
    BOF3_INJECT(MenuList_SlideLeftOff110);
    BOF3_INJECT(MenuList_WideTitleBox);
    BOF3_INJECT(MenuList_CursorBox);
    BOF3_INJECT(MenuList_IconWheel);
    BOF3_INJECT(MenuList_SlideRightOffColumn);
    BOF3_INJECT(MenuList_SlideLeftToColumn);
    BOF3_INJECT(MenuList_ItemList);
    BOF3_INJECT(MenuList_SlideLeftTo152);
    BOF3_INJECT(MenuList_ButtonRow);
    BOF3_INJECT(MenuList_LabelList);
    BOF3_INJECT(MenuList_DrawLabelList);
    BOF3_INJECT(MenuList_EquipCompare);
    BOF3_INJECT(MenuList_SlideLeftTo165);
    BOF3_INJECT(MenuList_SlideLeftTo17);
    BOF3_INJECT(MenuList_SlideRightTo165);
    BOF3_INJECT(MenuList_AbilityPanel);
    BOF3_INJECT(MenuList_SlideRightTo150);
    BOF3_INJECT(MenuList_ItemPanel);
    BOF3_INJECT(MenuList_ReservePanel);
    BOF3_INJECT(MenuList_SlideLeftTo150);
}
