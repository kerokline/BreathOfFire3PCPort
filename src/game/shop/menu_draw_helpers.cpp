// Group DI of the eighth takeover round (docs/menu_draw_helpers.md): record
// handlers 7 and 8 of Field_RunTaskRecords, the window kinds under them that
// draw a shop or battle-menu panel, and the slides those kinds step through.
// Each read to its last instruction with capstone against bof3/BOF3.exe
// (2026-09-25). Faithful but for DIV-0027's shop yes / no line under a Latin
// overlay (ShopWin_TitleRun, amended 2026-10-03); DIV-0041's faster slide
// at MenuWin_SlideOutLeft (its bound read as widescreen.cpp holds it). The
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
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace menu_draw_helpers;
constexpr auto& AtMenuDrawHelpers = move_script::At;
constexpr auto& LongMenuDrawHelpers = move_script::Long;
using move_script::SetWord;
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

// The slides: x (+4) or y (+6) stepped by `by`; past `bound` (signed 16-bit;
// `below` says which side) it is set to `to` and the step +3 goes to 0.
void SlideX(int by, short bound, bool below, short to) {
    unsigned char* const w = RecMenuDrawHelpers();
    rv::As<records::WindowRecord>(w)->x = static_cast<unsigned>(rv::As<records::WindowRecord>(w)->x + by);
    const short x = S(w + 4);
    if (below ? x < bound : x > bound) {
        rv::As<records::WindowRecord>(w)->x = static_cast<std::uint16_t>(to);
        w[3] = 0;
    }
}
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

// ===========================================================================
// Record handler 7 (0x66B1F8)
// ===========================================================================

// ===========================================================================
// Record handler 8 (0x66B534)
// ===========================================================================

// ===========================================================================

void MenuDrawHelpers_Inject() {
    bof3::Log("menu_draw_helpers: MenuWin_SlideOutLeft's bound %d (0x59B446; Capcom's -150 at every width since DIV-0041's faster slide)",
              static_cast<int>(Widescreen_SlideBound(at::kSlideLeftBound)));
    if (bof3::WantsShadow("menu_draw_helpers")) menu_draw_helpers::SelfTest();
}
