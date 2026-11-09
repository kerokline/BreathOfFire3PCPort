// Moved here by tools/move_funcs.py from menu_draw_helpers.cpp (docs/source-layout-round.md).
#include "game/shop/shop_win.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/lang_layout.h"
#include "game/menu_draw_helpers.h"
#include "game/menu_draw_helpers_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace menu_draw_helpers;

}  // namespace

namespace {

constexpr auto& AtMenuDrawHelpers = move_script::At;

constexpr auto& LongMenuDrawHelpers = move_script::Long;

constexpr auto& WordMenuDrawHelpers = move_script::Word;

using Handler = void (__cdecl*)();

unsigned char* RecMenuDrawHelpers() {
    return AtMenuDrawHelpers(*reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr))));
}

// rv::As<records::WindowRecord>(p) views a window record
// (docs/typed-records.md; WindowRecords
// 0x803160, the records BattleWinStates_Current points at). Only +0..+7 are
// typed: +8..+0x1F mean something else in each handler (typed-records 3).
short S(const unsigned char* p) { return static_cast<short>(WordMenuDrawHelpers(p)); }

unsigned X(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->x; }

unsigned Y(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->y; }

// `call / jmp [table + index * 4]`: the .data dword read at the call, the
// index unbounded as in the original (a byte, zero-extended).
void Dispatch(menu_draw_helpers::U table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<menu_draw_helpers::U>(LongMenuDrawHelpers(AtMenuDrawHelpers(table + index * 4)))))();
}

// The step: the current record's byte +3 through a step table.
void Step(menu_draw_helpers::U table) { Dispatch(table, RecMenuDrawHelpers()[3]); }

// A party slot's character record: 0x66972C[0x904062[slot]], both bytes, the
// slot unbounded.
unsigned Member(unsigned slot) { return AtMenuDrawHelpers(::bof3::Img(Character_IndexById_addr) + AtMenuDrawHelpers(::bof3::Img(Cond_Flags_addr) + 0xd2 + slot)[0])[0]; }

void SlideY(int by, short bound, bool below, short to) {
    unsigned char* const w = RecMenuDrawHelpers();
    rv::As<records::WindowRecord>(w)->y = static_cast<unsigned>(rv::As<records::WindowRecord>(w)->y + by);
    const short y = S(w + 6);
    if (below ? y < bound : y > bound) {
        rv::As<records::WindowRecord>(w)->y = static_cast<std::uint16_t>(to);
        w[3] = 0;
    }
}

}  // namespace

extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_TitleRun(void) {
    Step(::bof3::Img(ShopWin_TitleSteps_addr));
    const unsigned colour = static_cast<unsigned char>(Config_WindowColour);
    const unsigned char* w = RecMenuDrawHelpers();
    g.title_box(static_cast<int>(X(w)), static_cast<int>(Y(w)), 0x118, 0x13, static_cast<int>(colour));
    const unsigned id = WordMenuDrawHelpers(RecMenuDrawHelpers() + 0x10);
    if (id == 0) return;
    const unsigned char* text = g.msg_system(id);
    w = RecMenuDrawHelpers();
    g.text_draw_at(static_cast<std::uint16_t>(X(w) + 7), static_cast<std::uint16_t>(Y(w) + 3), 0, 0xFF, text);
    const unsigned again = WordMenuDrawHelpers(RecMenuDrawHelpers() + 0x10);
    if (again != 0x36 && again != 0x49 && again != 0x4A && again != 0x52) return;
    text = g.msg_system(0xF);
    if (g_shop_yes_no_line) text = YesNoLayout_SystemLine(text);   // DIV-0027, amended 2026-10-03
    w = RecMenuDrawHelpers();
    g.text_draw_at(static_cast<std::uint16_t>(X(w) + 7), static_cast<std::uint16_t>(Y(w) + 3), 0, 0xFF, text);
}

// original 0x59B310, kind 2: the step (ShopWin_ButtonsSteps: a bare ret,
// 0x59A3A0, 0x59A680 - DH's), then Menu_DrawButtonRow(+4, +6, set +0xA,
// selected +0xB, 0). The record is re-read after the step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_ButtonsRun(void) {
    Step(::bof3::Img(ShopWin_ButtonsSteps_addr));
    const unsigned char* const w = RecMenuDrawHelpers();
    g.button_row(static_cast<int>(X(w)), static_cast<int>(Y(w)), w[0xA], w[0xB], 0);
}

// original 0x59B350, kind 1: the step (ShopWin_MoneySteps: a bare ret,
// ShopWin_MoneySlideUp, 0x59A680 - DH's), then Menu_DrawMoneyBox(+4, +6, 0,
// the zenny 0x904058). Record and zenny read after the step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_MoneyRun(void) {
    Step(::bof3::Img(ShopWin_MoneySteps_addr));
    const unsigned char* const w = RecMenuDrawHelpers();
    const auto zenny = Party_Zenny;
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
    const unsigned char* const w = RecMenuDrawHelpers();
    const unsigned width = WordMenuDrawHelpers(AtMenuDrawHelpers(::bof3::Img(ShopWin_CursorBoxWidths_addr) + w[0xA] * 2u));
    g.cursor_box(static_cast<int>(X(w)), static_cast<int>(Y(w)), static_cast<int>(width), 0x34, w[0xB], 6);
}

// original 0x59B3F0, kind 5: the step (ShopWin_MemberStatsSteps: a bare ret,
// MenuWin_SlideOutLeft, 0x59A5B0 - DH's, ShopWin_MemberSlideUp,
// ShopWin_MemberSlideDown), then Shop_DrawMemberStats(+4, +6, the character
// record of party slot +0xA, slot +0xB, item +0xC). Record re-read after the
// step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_MemberStatsRun(void) {
    Step(::bof3::Img(ShopWin_MemberStatsSteps_addr));
    const unsigned char* const w = RecMenuDrawHelpers();
    const unsigned item = w[0xC], slot = w[0xB];
    const unsigned member = Member(w[0xA]);
    g.member_stats(static_cast<int>(X(w)), static_cast<int>(Y(w)), member, slot, item);
}

void ShopWin_Inject() {
    BOF3_INJECT(ShopWin_TitleRun);
    BOF3_INJECT(ShopWin_ButtonsRun);
    BOF3_INJECT(ShopWin_MoneyRun);
    BOF3_INJECT(ShopWin_MoneySlideUp);
    BOF3_INJECT(ShopWin_CursorBoxDraw);
    BOF3_INJECT(ShopWin_MemberStatsRun);
}
