// The field menu and the list draws (round eight, group DH).
// docs/menu_lists.md.
//
// The field menu is mode 3's per-frame call 0x589970 (docs/menu-screens.md
// section 1): a dispatch on the state byte 0x929F00, the set-up (state 0),
// the top bar (state 1) with its three steps, and the two helpers the top
// bar calls - the window placement 0x589E60 and the party check on leaving,
// 0x589FE0. The PSX's START.EMI, compiled into the exe.
//
// The list draws are record handler 6 of Field_RunTaskRecords (0x599B50):
// the top bar's five windows - the three member panels, the money box, the
// play-time box, the icon row and the screen title - by the record's kind,
// each running its state first and then drawing; and six of the slide steps
// the state tables of handlers 1, 6 and 7 share (0x59A3A0..0x59A680).
//
// Every call goes through menu_lists::g (menu_lists_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. The .data dispatch tables are read afresh and
// unchecked, as the originals read them: a state or kind past a table's end
// reaches the next table's entries in both (they are data, so ours reads the
// same dword the original would). The current window record 0x905B84 is
// re-read wherever the original re-reads it.
//
// Upper halves. The originals push coordinates as 16-bit registers and ids
// and colours as byte registers whose upper bits are whatever the register
// held; every callee here reads only the low 16 bits of a coordinate and the
// low byte of an id, count, flag or colour (the stand-ins record exactly
// that; the evidence for each callee is in symbols.toml and
// docs/menu_lists.md section 2), so ours passes the value as C++ computes it.
//
// Everything here is a faithful replacement: no DIVERGENCE.md entry is owed.
// One divergence has patch sites inside these bodies and survives in ours:
// DIV-0041's faster slide (widescreen.cpp) at MenuSlide_LeftOff and
// MenuSlide_RightOff; ours reads the bound as widescreen.cpp holds it and
// leaves the drawn x to Widescreen_SlideHeld.
#include "game/menu_lists.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/lang_layout.h"
#include "game/menu_lists_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace menu_lists {

using move_script::At;
using move_script::Long;
using move_script::SetWord;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace menu_lists

using namespace menu_lists;

namespace {

using Step = void (__cdecl*)();

// Entry `index` of a code-pointer table in .data, read afresh and unchecked.
Step Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Step>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(menu_lists::Long(menu_lists::At(table + index * 4)))));
}

// rv::As<records::WindowRecord>(p) views a window record
// (docs/typed-records.md; WindowRecords
// 0x803160, the records BattleWinStates_Current points at). Only +0..+7 are
// typed: +8..+0x1F mean something else in each handler (typed-records 3).

}  // namespace

// ===========================================================================
// The field menu (START.EMI)
// ===========================================================================

// ===========================================================================
// Record handler 6: the top bar's windows
// ===========================================================================

// DIVERGENCE DIV-0058: under a Latin language overlay the screen title is
// centred on the width the pen will really cover (DIV-0006's advances)
// instead of on 12 units a character - the owner's "Ability sits left of
// centre" (known-defects.md D86). 0 until MenuLists_Inject patches it to 1
// under the name MenuTitleCentre, so BOF3X_ORIGINAL=MenuTitleCentre and the
// fuzz (which runs before the patch) keep the original's arithmetic.
unsigned char g_title_centre = 0;

// ===========================================================================
// The slide steps
// ===========================================================================
//
// Six states the window records' state tables share (handlers 1, 6, 7 and
// the shop's, symbols.toml): the record's x (+4) or y (+6) moves by a step a
// frame until it passes a bound, where it is held and the state (+3) goes to
// 0. As the originals have them: the word is compared signed after the move;
// the record is re-read after the move and after the clamp.
// ===========================================================================

void MenuLists_Inject() {
    if (bof3::WantsShadow("menu_lists")) menu_lists::SelfTest();
    // DIVERGENCE DIV-0058: a Latin language overlay only - BOF3X_LANG set,
    // not "original", not a full-width language (as MenuVerbs_Inject
    // tests it): the screen title centred on its real width.
    {
        if (Lang_Latin()) {
            static const std::uint8_t was = 0, is = 1;
            bof3::PatchBytes("MenuTitleCentre",
                             Low32(&g_title_centre),
                             &was, &is, 1);
            bof3::Log("DIV-0058    menu screen title centred on its width: %s", g_title_centre ? "on" : "off");
        }
    }
}
