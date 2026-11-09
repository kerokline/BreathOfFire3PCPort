// Scenario chapter 1's bank (the PSX's SCENA01.EMI), compiled into the exe at
// 0x539AE0..0x53DD92. docs/scena_sc1.md.
//
//   - The state machine: Scena01_Frame (ours since round eight, mode_states)
//     jumps through Scena01_States: state 0 Scena01_Start, 1
//     Scena01_EnterArea (mode_states), 2 Scena01_Run, which jumps through
//     Scena01_Runs on the run MoveScript_Var7 to one of eighteen scenes (the
//     other six entries are 0x437CC0, a bare ret).
//   - A scene is a switch on the step byte 0x8034E5: each step waits on a
//     script counter (0x903848..B), the message box (Field_Request), the wait
//     word (Transition_Busy), a flag, a timer or a position, does one
//     thing - a message, a sound, the music, an effect, an area change, a
//     party member dropped in - and sets the next step; the last clears the
//     counters and sets the run and step back to 0.
//   - Slot 1, the object hook: through Scena01_ObjectHandlers on the object's
//     +0x86 to one of seventeen small handlers that start a scene.
//   - Slot 4, the cell hook: 0x56D800 over Scena01_Cells, then through
//     Scena01_CellHandlers to Scena01_Cell, which starts run 5.
//
// Every call goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that an index past a
// table - the run past Scena01_Runs, the object's +0x86 past
// Scena01_ObjectHandlers, 0x56D800's answer past Scena01_CellHandlers - aborts
// where the original would call through whatever lies there (the spell
// round's rule, docs/takeover-queue-round9.md section 6).
#include "game/scena_sc1.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc1_callees.h"
#include "game/scenario_harness.h"
#include "game/sound_sp.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = scena_sc1::at;

// --- the chapter's bytes ------------------------------------------------------

}  // namespace

void ScenaSc1_Inject() {
    if (bof3::WantsShadow("scena_sc1")) scena_sc1::SelfTest();
}
