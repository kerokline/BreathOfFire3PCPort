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
// has sites inside these bodies and survives in ours: DIV-0041's faster
// slide (widescreen.cpp g_slides) over six slide bounds; ours reads each as
// widescreen.cpp holds it. Every call goes through the harness (SH_CALL / SH_AT),
// so the start-up fuzz can stand recorders in for the callees; the current
// record 0x905B84 is re-read wherever the original re-reads it.
#include "game/rest_2g.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2g_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_2g::at;

}  // namespace

// ===========================================================================
// Record handler 4: the battle result
// ===========================================================================

// ===========================================================================
// Record handler 5: the gene windows
// ===========================================================================

// ===========================================================================
// Record handler 6: MenuList_Run's kinds 5..19
// ===========================================================================

// ===========================================================================
// The slide states of handler 6's tables
// ===========================================================================
//
// Each: x (+4) by 0x20 or y (+6) by 0x10 a frame until past its bound, held
// there with +3 = 0 (Slide above). "Ge" / "Le" where the original holds the
// word at the bound itself too (`jl` / `jg` over the store, where the others
// have `jle` / `jge`).

// ===========================================================================

void Rest2G_Inject() {
    if (bof3::WantsShadow("rest_2g")) rest_2g::SelfTest();
}
