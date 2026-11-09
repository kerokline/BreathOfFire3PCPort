// Moved here by tools/move_funcs.py from menu_draw_helpers.cpp (docs/source-layout-round.md).
#include "game/menu/menu_win.h"

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

constexpr auto& WordMenuDrawHelpers = move_script::Word;

unsigned char* RecMenuDrawHelpers() {
    return AtMenuDrawHelpers(*reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr))));
}

// rv::As<records::WindowRecord>(p) views a window record
// (docs/typed-records.md; WindowRecords
// 0x803160, the records BattleWinStates_Current points at). Only +0..+7 are
// typed: +8..+0x1F mean something else in each handler (typed-records 3).
short S(const unsigned char* p) { return static_cast<short>(WordMenuDrawHelpers(p)); }

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

}  // namespace

// original 0x59B440, a step of kinds 5 and 6 (and of 0x59BEC0's table at
// 0x66B304): x (+4) -= 0x20; below the bound (-150; signed words) x = the
// bound and the step +3 = 0 - the window off the left edge.
//
// The bound is the imm32 at 0x59B446, which Widescreen_Inject (DIV-0041)
// moves out by its columns; ours reads the low word of widescreen.cpp's value
// for it (g.slide_left_bound's address; a fuzz points that at a clone's
// immediate) at every call, as the original's `cmp word [eax + 4], cx` reads
// the operand, so the divergence is kept and BOF3X_ORIGINAL=Widescreen still
// restores -150.
//
// DIVERGENCE DIV-0041 (amended 2026-10-13, the faster slide): decided by that
// bound at every width; in the wide view the box is drawn to the bound moved
// out by the columns (Widescreen_SlideHeld, docs/ultrawide-slides.md).
extern "C" void __cdecl MenuWin_SlideOutLeft(void) {
    const short bound = Widescreen_SlideLogic(static_cast<short>(WordMenuDrawHelpers(AtMenuDrawHelpers(g.slide_left_bound))));
    const short x_read = S(RecMenuDrawHelpers() + 4);
    SlideX(-0x20, bound, true, bound);
    Widescreen_SlideHeld(RecMenuDrawHelpers(), x_read, bound);
}

void MenuWin_Inject() {
    BOF3_INJECT(MenuWin_SlideOutLeft);
}
