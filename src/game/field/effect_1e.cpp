// Group E1E of round thirteen (wave one): 48 functions at 0x528CD0..0x52A6B0 -
// analysis/round13_cut.tsv's 48 rows for E1E, each read to its last
// instruction with capstone (2026-09-29) and taken through the scenario
// harness's field mode (scenario_harness.h, Shape::kSprite).
// docs/effect_1e.md has them one row each.
//
// They are not effect-kind states: every one is a step of the leader's state 9
// (Field_LeaderStates[9] = 0x528880, which jumps through LeaderPanel_Stages by
// Sprite_Current +2; each stage's dispatcher jumps through its own steps table
// by +3), run by Field_LeaderFrame with Sprite_Current the leader's ObjTrio
// record. What they drive is Effect_Objects records 0..6 (their state bytes
// +1, +6 and words), the panel draws FE1 took (FieldPanel_*), the music, and
// one Sprite_Objects record chosen by 0x939A1C (docs/effect_1e.md section 1).
//
//   stage 1 (steps 5, 6, 8..10 of 11)  two boxes in and out, a yes / no choice
//   stage 2 (4 steps)                  a start on the confirm button, steering
//   stage 3 (4 steps)                  effect record 0 moved by the pad
//   stage 4 (15 steps)                 a pull by the pad, then the panel's
//                                      header, icon, row, count and total in,
//                                      the take (Inventory_Add), a message,
//                                      a choice and out
//   stages 5..7 (3 steps each)         an item used (Inventory_Remove), waits
//                                      on effect record 4, the leave on a press
//   stage 8 (2 steps)                  the leave: the view back, the effects
//                                      cleared, a transition
//   stage 9 (3 of 5 steps)             a three-way menu on effect record 6
//
// Every one is a faithful replacement but one: LeaderPanel_S4Again's hand row
// under a Latin overlay (DIV-0027, amended 2026-10-03 - g_shop_ask_row). The
// stage dispatchers abort past their tables (the original jumps through the
// dword after, the next table's entry), and a read of the Sprite_Objects
// record 0x939A1C names aborts when the byte is 30 or more (the original reads
// and writes past the 30 records; 0xFF is the value 0x5289C0 leaves) - the
// owner's rule for an unchecked index, round9 doc section 6; docs/effect_1e.md
// section 6. Every call goes through the harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for the callees; the callees of other
// groups of this round and the unowned ones are raw addresses in
// effect_1e_callees.h.
//
// Sprite_Current is re-read after every call, as the originals re-read it, and
// held where the original holds it in a register. Where the original computes
// an argument from a register whose upper half is Sprite_Current's (a `movzx
// ax, byte [eax + 9]` on the pointer), ours computes the same 32-bit value;
// where the upper bytes are a callee's leftovers, the callee reads less than
// the whole word (each read in docs/effect_1e.md section 3) and ours hands on
// what it reads.
#include "game/effect_1e.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_1e_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/sound.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_1e::at;
using move_script::SetWord;

}  // namespace

// ===========================================================================
// Stage 1 (LeaderPanel_Stage1Steps 0x66022C, dispatched by 0x5289A0, not ours)
// ===========================================================================

// ===========================================================================
// Stage 2
// ===========================================================================

// ===========================================================================
// Stage 3
// ===========================================================================

// ===========================================================================
// Stage 4
// ===========================================================================


// ===========================================================================
// Stages 5..9
// ===========================================================================


void Effect1E_Inject() {
    if (bof3::WantsShadow("effect_1e")) effect_1e::SelfTest();
}
