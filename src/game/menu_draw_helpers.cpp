// Group DI of the eighth takeover round (docs/menu_draw_helpers.md): record
// handlers 7 and 8 of Field_RunTaskRecords, the window kinds under them that
// draw a shop or battle-menu panel, and the slides those kinds step through.
// Each read to its last instruction with capstone against bof3/BOF3.exe
// (2026-09-25). Faithful: no divergence of its own; DIV-0041's widened bound
// inside MenuWin_SlideOutLeft is read back from the patched immediate. The
// start-up fuzz is menu_draw_helpers_fuzz.cpp.
//
//   Window_Handler7Kinds            0x59B220..0x59B231 (0x12)  jmp [0x66B1F8 + kind * 4]
//   ShopWin_TitleRun                0x59B240..0x59B301 (0xC2)  kind 0
//   ShopWin_ButtonsRun              0x59B310..0x59B343 (0x34)  kind 2
//   ShopWin_MoneyRun                0x59B350..0x59B382 (0x33)  kind 1
//   ShopWin_MoneySlideUp            0x59B390..0x59B3B7 (0x28)  kind 1, step 1
//   ShopWin_CursorBoxDraw           0x59B3C0..0x59B3ED (0x2E)  kind 4
//   ShopWin_MemberStatsRun          0x59B3F0..0x59B435 (0x46)  kind 5
//   MenuWin_SlideOutLeft            0x59B440..0x59B467 (0x28)  kind 5 step 1, kind 6 step 1, 0x66B304
//   ShopWin_MemberSlideUp           0x59B470..0x59B497 (0x28)  kind 5, step 3
//   ShopWin_MemberSlideDown         0x59B4A0..0x59B4EB (0x4C)  kind 5, step 4
//   ShopWin_EquipRun                0x59B4F0..0x59B52D (0x3E)  kind 6
//   ShopWin_EquipSlideIn            0x59B530..0x59B557 (0x28)  kind 6, step 2
//   ShopWin_BuyListRun              0x59B560..0x59B57F (0x20)  kind 7
//   ShopWin_BuyListSlideTo84        0x59B7B0..0x59B7D7 (0x28)  kind 7, step 2
//   ShopWin_BuyListSlideTo46        0x59B7E0..0x59B807 (0x28)  kind 7, step 3
//   ShopWin_BuyDetailRun            0x59B810..0x59B81C (0xD)   kind 8
//   ShopWin_ItemListRun             0x59BB60..0x59BB7F (0x20)  kind 9
//   ShopWin_ItemListSlideIn         0x59BB80..0x59BBA7 (0x28)  kind 9, step 2
//   ShopWin_SellDetailRun           0x59BBB0..0x59BBBC (0xD)   kind 10
//   Window_Handler8Kinds            0x59CB00..0x59CB11 (0x12)  jmp [0x66B534 + kind * 4]
//   MenuWin_Hand                    0x59CB20..0x59CB39 (0x1A)  handler 7 kind 3, handler 8 kind 0, and three more tables
//   BattleMenuWin_ItemListRun       0x59CB40..0x59CB5F (0x20)  handler 8 kind 1
//   BattleMenuWin_ItemListSlideRight 0x59CB60..0x59CB85 (0x26) handler 8 kind 1, step 2
//   BattleMenuWin_SkillListRun      0x59CBC0..0x59CBDF (0x20)  handler 8 kind 2
//   BattleMenuWin_SkillListSlideIn  0x59CBE0..0x59CC07 (0x28)  handler 8 kind 2, step 2
//
// Common to all: the record is the dword at 0x905B84, re-read where the
// original re-reads it (after every call out; volatile, so ours cannot cache
// it). The originals push coordinates built by 16-bit moves and bytes built
// by 8-bit moves into registers whose upper bits are left over; every callee
// here reads only the low word of a coordinate and the low byte of a byte
// argument (docs/menu-windows.md section 2), so ours passes them
// zero-extended. No original returns anything a caller reads.
#include "game/menu_draw_helpers.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/menu_draw_helpers_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace menu_draw_helpers {

const Callees kOriginals = {
    Menu_DrawTitleBox,  Msg_SystemPtr,       Text_DrawAt,        Menu_DrawButtonRow,       Menu_DrawMoneyBox,
    Menu_DrawCursorBox, Shop_DrawMemberStats, Menu_DrawEquipPanel, Shop_DrawBuyList,       Shop_DrawBuyDetail,
    Menu_DrawItemList,  Shop_DrawSellDetail, Menu_DrawHand,      BattleMenu_DrawItemList, BattleMenu_DrawSkillList,
    at::kSlideLeftBound,
};
Callees g = kOriginals;

}  // namespace menu_draw_helpers

namespace {

using namespace menu_draw_helpers;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

using Handler = void (__cdecl*)();

unsigned char* Rec() {
    return At(*reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(at::kCurrent)));
}
short S(const unsigned char* p) { return static_cast<short>(Word(p)); }
unsigned X(const unsigned char* w) { return Word(w + 4); }
unsigned Y(const unsigned char* w) { return Word(w + 6); }

// `call / jmp [table + index * 4]`: the .data dword read at the call, the
// index unbounded as in the original (a byte, zero-extended).
void Dispatch(U table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + index * 4)))))();
}
// The step: the current record's byte +3 through a step table.
void Step(U table) { Dispatch(table, Rec()[3]); }

// A party slot's character record: 0x66972C[0x904062[slot]], both bytes, the
// slot unbounded.
unsigned Member(unsigned slot) { return At(at::kMemberRecord + At(at::kPartyList + slot)[0])[0]; }

// The slides: x (+4) or y (+6) stepped by `by`; past `bound` (signed 16-bit;
// `below` says which side) it is set to `to` and the step +3 goes to 0.
void SlideX(int by, short bound, bool below, short to) {
    unsigned char* const w = Rec();
    SetWord(w + 4, static_cast<unsigned>(Word(w + 4) + by));
    const short x = S(w + 4);
    if (below ? x < bound : x > bound) {
        SetWord(w + 4, static_cast<std::uint16_t>(to));
        w[3] = 0;
    }
}
void SlideY(int by, short bound, bool below, short to) {
    unsigned char* const w = Rec();
    SetWord(w + 6, static_cast<unsigned>(Word(w + 6) + by));
    const short y = S(w + 6);
    if (below ? y < bound : y > bound) {
        SetWord(w + 6, static_cast<std::uint16_t>(to));
        w[3] = 0;
    }
}

}  // namespace

// ===========================================================================
// Record handler 7 (0x66B1F8)
// ===========================================================================

// original 0x59B220: record handler 7 of Field_RunTaskRecords' nine (the
// imm32 at 0x59E27C) - `jmp [0x66B1F8 + byte +2 * 4]`, unbounded, into the
// nineteen kinds of Window_Handler7KindTable. The kinds take nothing and
// return nothing; ecx (the record) and eax (the kind) on entry are reloaded
// by every kind (read 2026-09-25), so a call is the jump.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Window_Handler7Kinds(void) {
    Dispatch(table::kHandler7Kinds, Rec()[2]);
}

// original 0x59B240, kind 0: the step (ShopWin_TitleSteps by +3: a bare ret,
// 0x59A3A0, 0x59A3D0 - group DH's slides), then the title box
// Menu_DrawTitleBox(+4, +6, 0x118, 0x13, the window colour 0x903A5A); then,
// when the word +0x10 is not 0, system message +0x10 (Msg_SystemPtr) through
// Text_DrawAt at (+4 + 7, +6 + 3), colour 0, to its end; and when +0x10 is
// 0x36, 0x49, 0x4A or 0x52, system message 0xF at the same place after it.
//
// As the original has it: the colour is read after the step; the record after
// the step, after the box, after each Msg_SystemPtr and after the first
// Text_DrawAt (whose +0x10 is tested again). The original's id argument is
// the record pointer's upper half over the word; Msg_SystemPtr reads the low
// word alone.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_TitleRun(void) {
    Step(table::kTitleSteps);
    const unsigned colour = At(at::kColour)[0];
    const unsigned char* w = Rec();
    g.title_box(static_cast<int>(X(w)), static_cast<int>(Y(w)), 0x118, 0x13, static_cast<int>(colour));
    const unsigned id = Word(Rec() + 0x10);
    if (id == 0) return;
    const unsigned char* text = g.msg_system(id);
    w = Rec();
    g.text_draw_at(static_cast<std::uint16_t>(X(w) + 7), static_cast<std::uint16_t>(Y(w) + 3), 0, 0xFF, text);
    const unsigned again = Word(Rec() + 0x10);
    if (again != 0x36 && again != 0x49 && again != 0x4A && again != 0x52) return;
    text = g.msg_system(0xF);
    w = Rec();
    g.text_draw_at(static_cast<std::uint16_t>(X(w) + 7), static_cast<std::uint16_t>(Y(w) + 3), 0, 0xFF, text);
}

// original 0x59B310, kind 2: the step (ShopWin_ButtonsSteps: a bare ret,
// 0x59A3A0, 0x59A680 - DH's), then Menu_DrawButtonRow(+4, +6, set +0xA,
// selected +0xB, 0). The record is re-read after the step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_ButtonsRun(void) {
    Step(table::kButtonsSteps);
    const unsigned char* const w = Rec();
    g.button_row(static_cast<int>(X(w)), static_cast<int>(Y(w)), w[0xA], w[0xB], 0);
}

// original 0x59B350, kind 1: the step (ShopWin_MoneySteps: a bare ret,
// ShopWin_MoneySlideUp, 0x59A680 - DH's), then Menu_DrawMoneyBox(+4, +6, 0,
// the zenny 0x904058). Record and zenny read after the step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_MoneyRun(void) {
    Step(table::kMoneySteps);
    const unsigned char* const w = Rec();
    const auto zenny = static_cast<unsigned>(Long(At(at::kZenny)));
    g.money_box(static_cast<int>(X(w)), static_cast<int>(Y(w)), 0, zenny);
}

// original 0x59B390, kind 1 step 1: y (+6) -= 0x10; then, when y is ABOVE
// -0x14 (signed words), y = -0x14 and the step +3 = 0.
//
// As the original has it: `jle`, where its twin 0x59A3A0 (group DH's, the
// same constants) has `jge`. So the box does not slide: from any y above -4
// it lands at -0x14 in one frame, and from -4 or less it moves up 0x10 a frame
// with the step never ending (the word wraps). A candidate defect of the
// original's, kept (docs/menu_draw_helpers.md section 5).
extern "C" void __cdecl ShopWin_MoneySlideUp(void) { SlideY(-0x10, -0x14, false, -0x14); }

// original 0x59B3C0, kind 4: Menu_DrawCursorBox(+4, +6, width, 0x34, blink
// +0xB, flags 6), the width the u16 of ShopWin_CursorBoxWidths by +0xA,
// unbounded (0x70, then 0, then the low words of the next table's pointers).
// No step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_CursorBoxDraw(void) {
    const unsigned char* const w = Rec();
    const unsigned width = Word(At(table::kCursorWidths + w[0xA] * 2u));
    g.cursor_box(static_cast<int>(X(w)), static_cast<int>(Y(w)), static_cast<int>(width), 0x34, w[0xB], 6);
}

// original 0x59B3F0, kind 5: the step (ShopWin_MemberStatsSteps: a bare ret,
// MenuWin_SlideOutLeft, 0x59A5B0 - DH's, ShopWin_MemberSlideUp,
// ShopWin_MemberSlideDown), then Shop_DrawMemberStats(+4, +6, the character
// record of party slot +0xA, slot +0xB, item +0xC). Record re-read after the
// step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_MemberStatsRun(void) {
    Step(table::kMemberSteps);
    const unsigned char* const w = Rec();
    const unsigned item = w[0xC], slot = w[0xB];
    const unsigned member = Member(w[0xA]);
    g.member_stats(static_cast<int>(X(w)), static_cast<int>(Y(w)), member, slot, item);
}

// original 0x59B440, a step of kinds 5 and 6 (and of 0x59BEC0's table at
// 0x66B304): x (+4) -= 0x20; below the bound (-150; signed words) x = the
// bound and the step +3 = 0 - the window off the left edge.
//
// The bound is the imm32 at 0x59B446, which Widescreen_Inject (DIV-0041)
// moves out by its columns; ours reads the low word of it there at every
// call, as the original's `cmp word [eax + 4], cx` does, so the divergence is
// kept and BOF3X_ORIGINAL=Widescreen still restores -150.
extern "C" void __cdecl MenuWin_SlideOutLeft(void) {
    const short bound = static_cast<short>(Word(At(g.slide_left_bound)));
    SlideX(-0x20, bound, true, bound);
}

// original 0x59B470, kind 5 step 3: y -= 0x10; below 0x3E, y = 0x3E and step 0.
extern "C" void __cdecl ShopWin_MemberSlideUp(void) { SlideY(-0x10, 0x3E, true, 0x3E); }

// original 0x59B4A0, kind 5 step 4: y += 0x10; above +0xA * 0x36 + 0x3E (the
// y comparing as a signed word, the bound as the 32-bit sum), y = that sum's
// low word and step 0 - the panel for party slot +0xA, 0x36 below the last.
extern "C" void __cdecl ShopWin_MemberSlideDown(void) {
    unsigned char* const w = Rec();
    SetWord(w + 6, static_cast<unsigned>(Word(w + 6) + 0x10));
    const int bound = static_cast<int>(w[0xA]) * 0x36 + 0x3E;
    if (static_cast<int>(S(w + 6)) > bound) {
        SetWord(w + 6, static_cast<unsigned>(bound));
        w[3] = 0;
    }
}

// original 0x59B4F0, kind 6: the step (ShopWin_EquipSteps: a bare ret,
// MenuWin_SlideOutLeft, ShopWin_EquipSlideIn), then Menu_DrawEquipPanel(+4,
// +6, the character record of party slot +0xA). Record re-read after the step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_EquipRun(void) {
    Step(table::kEquipSteps);
    const unsigned char* const w = Rec();
    const unsigned member = Member(w[0xA]);
    g.equip_panel(static_cast<int>(X(w)), static_cast<int>(Y(w)), member);
}

// original 0x59B530, kind 6 step 2: x += 0x20; above 0xF, x = 0xF and step 0.
extern "C" void __cdecl ShopWin_EquipSlideIn(void) { SlideX(0x20, 0xF, false, 0xF); }

// original 0x59B560, kind 7: the step (ShopWin_BuyListSteps: a bare ret,
// 0x59A5E0 - DH's, ShopWin_BuyListSlideTo84, ShopWin_BuyListSlideTo46), then
// Shop_DrawBuyList(the record, re-read after the step).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_BuyListRun(void) {
    Step(table::kBuyListSteps);
    g.buy_list(Rec());
}

// original 0x59B7B0, kind 7 step 2: x -= 0x20; below 0x84, x = 0x84 and step 0.
extern "C" void __cdecl ShopWin_BuyListSlideTo84(void) { SlideX(-0x20, 0x84, true, 0x84); }

// original 0x59B7E0, kind 7 step 3: x -= 0x20; below 0x46, x = 0x46 and step 0.
extern "C" void __cdecl ShopWin_BuyListSlideTo46(void) { SlideX(-0x20, 0x46, true, 0x46); }

// original 0x59B810, kind 8: Shop_DrawBuyDetail(the record). No step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_BuyDetailRun(void) { g.buy_detail(Rec()); }

// original 0x59BB60, kind 9: the step (ShopWin_ItemListSteps: a bare ret,
// 0x59A5E0 - DH's, ShopWin_ItemListSlideIn), then Menu_DrawItemList(the
// record, re-read after the step).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_ItemListRun(void) {
    Step(table::kItemListSteps);
    g.item_list(Rec());
}

// original 0x59BB80, kind 9 step 2: x -= 0x20; below 0x4B, x = 0x4B and step 0.
extern "C" void __cdecl ShopWin_ItemListSlideIn(void) { SlideX(-0x20, 0x4B, true, 0x4B); }

// original 0x59BBB0, kind 10: Shop_DrawSellDetail(the record). No step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_SellDetailRun(void) { g.sell_detail(Rec()); }

// ===========================================================================
// Record handler 8 (0x66B534)
// ===========================================================================

// original 0x59CB00: record handler 8 of Field_RunTaskRecords (the imm32 at
// 0x59E284) - `jmp [0x66B534 + byte +2 * 4]`, unbounded, into the six kinds
// of Window_Handler8KindTable, which reload ecx and eax as handler 7's do.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Window_Handler8Kinds(void) {
    Dispatch(table::kHandler8Kinds, Rec()[2]);
}

// original 0x59CB20: the pointing hand at the record's +4, +6
// (Menu_DrawHand(x, y, 0)). Kind 0 of handler 8, kind 3 of handler 7, and
// jumped to from three more kind tables (0x66AE40 handler 1's kind 0,
// 0x66AECC handler 2's kind 1, 0x66AFCC handler 6's kind 14).
extern "C" __attribute__((disable_tail_calls)) void __cdecl MenuWin_Hand(void) {
    const unsigned char* const w = Rec();
    g.hand(static_cast<int>(X(w)), static_cast<int>(Y(w)), 0);
}

// original 0x59CB40, handler 8 kind 1: the step (BattleMenuWin_ItemListSteps:
// a bare ret, 0x59A580 - DH's, BattleMenuWin_ItemListSlideRight, 0x59CB90 -
// in no group, 0x59A5E0 - DH's), then BattleMenu_DrawItemList(the record,
// re-read after the step).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMenuWin_ItemListRun(void) {
    Step(table::kBattleItemSteps);
    g.battle_items(Rec());
}

// original 0x59CB60, handler 8 kind 1 step 2: x += 0x20; above 0x53, x = 0x52
// and step 0. As the original has it: the test is against 0x53 and the store
// 0x52, so an x that lands on 0x53 stays there a frame and goes on to 0x73
// and then 0x52.
extern "C" void __cdecl BattleMenuWin_ItemListSlideRight(void) { SlideX(0x20, 0x53, false, 0x52); }

// original 0x59CBC0, handler 8 kind 2: the step (BattleMenuWin_SkillListSteps:
// a bare ret, 0x59A5E0 - DH's, BattleMenuWin_SkillListSlideIn), then
// BattleMenu_DrawSkillList(the record, re-read after the step).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMenuWin_SkillListRun(void) {
    Step(table::kBattleSkillSteps);
    g.battle_skills(Rec());
}

// original 0x59CBE0, handler 8 kind 2 step 2: x -= 0x20; below 0x53, x = 0x53
// and step 0.
extern "C" void __cdecl BattleMenuWin_SkillListSlideIn(void) { SlideX(-0x20, 0x53, true, 0x53); }

// ===========================================================================

void MenuDrawHelpers_Inject() {
    // The bound ours reads back must still be the operand of the original's
    // `mov ecx, imm32` (B9): anything else there means the site moved.
    if (At(at::kSlideLeftMov)[0] != 0xB9)
        bof3::Fatal("menu_draw_helpers: 0x%X holds 0x%02X, not MenuWin_SlideOutLeft's mov ecx",
                    static_cast<unsigned>(at::kSlideLeftMov), At(at::kSlideLeftMov)[0]);
    bof3::Log("menu_draw_helpers: MenuWin_SlideOutLeft's bound %d (0x59B446; -150 unless DIV-0041 widened it)",
              static_cast<int>(static_cast<short>(Word(At(at::kSlideLeftBound)))));
    if (bof3::WantsShadow("menu_draw_helpers")) menu_draw_helpers::SelfTest();
    BOF3_INJECT(Window_Handler7Kinds);
    BOF3_INJECT(ShopWin_TitleRun);
    BOF3_INJECT(ShopWin_ButtonsRun);
    BOF3_INJECT(ShopWin_MoneyRun);
    BOF3_INJECT(ShopWin_MoneySlideUp);
    BOF3_INJECT(ShopWin_CursorBoxDraw);
    BOF3_INJECT(ShopWin_MemberStatsRun);
    BOF3_INJECT(MenuWin_SlideOutLeft);
    BOF3_INJECT(ShopWin_MemberSlideUp);
    BOF3_INJECT(ShopWin_MemberSlideDown);
    BOF3_INJECT(ShopWin_EquipRun);
    BOF3_INJECT(ShopWin_EquipSlideIn);
    BOF3_INJECT(ShopWin_BuyListRun);
    BOF3_INJECT(ShopWin_BuyListSlideTo84);
    BOF3_INJECT(ShopWin_BuyListSlideTo46);
    BOF3_INJECT(ShopWin_BuyDetailRun);
    BOF3_INJECT(ShopWin_ItemListRun);
    BOF3_INJECT(ShopWin_ItemListSlideIn);
    BOF3_INJECT(ShopWin_SellDetailRun);
    BOF3_INJECT(Window_Handler8Kinds);
    BOF3_INJECT(MenuWin_Hand);
    BOF3_INJECT(BattleMenuWin_ItemListRun);
    BOF3_INJECT(BattleMenuWin_ItemListSlideRight);
    BOF3_INJECT(BattleMenuWin_SkillListRun);
    BOF3_INJECT(BattleMenuWin_SkillListSlideIn);
}
