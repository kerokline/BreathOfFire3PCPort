// Moved here by tools/move_funcs.py from menu_draw_helpers.cpp (docs/source-layout-round.md).
#include "game/menu/window_2.h"

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

using Handler = void (__cdecl*)();

unsigned char* RecMenuDrawHelpers() {
    return AtMenuDrawHelpers(*reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr))));
}

unsigned X(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->x; }

unsigned Y(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->y; }

// `call / jmp [table + index * 4]`: the .data dword read at the call, the
// index unbounded as in the original (a byte, zero-extended).
void Dispatch(menu_draw_helpers::U table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<menu_draw_helpers::U>(LongMenuDrawHelpers(AtMenuDrawHelpers(table + index * 4)))))();
}

}  // namespace

// original 0x59CB00: record handler 8 of Field_RunTaskRecords (the imm32 at
// 0x59E284) - `jmp [0x66B534 + byte +2 * 4]`, unbounded, into the six kinds
// of Window_Handler8KindTable, which reload ecx and eax as handler 7's do.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Window_Handler8Kinds(void) {
    Dispatch(::bof3::Img(Window_Handler8KindTable_addr), RecMenuDrawHelpers()[2]);
}

// original 0x59CB20: the pointing hand at the record's +4, +6
// (Menu_DrawHand(x, y, 0)). Kind 0 of handler 8, kind 3 of handler 7, and
// jumped to from three more kind tables (0x66AE40 handler 1's kind 0,
// 0x66AECC handler 2's kind 1, 0x66AFCC handler 6's kind 14).
extern "C" __attribute__((disable_tail_calls)) void __cdecl MenuWin_Hand(void) {
    const unsigned char* const w = RecMenuDrawHelpers();
    g.hand(static_cast<int>(X(w)), static_cast<int>(Y(w)), 0);
}

void Window2_Inject() {
    BOF3_INJECT(Window_Handler8Kinds);
    BOF3_INJECT(MenuWin_Hand);
}
