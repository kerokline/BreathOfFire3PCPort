// Scenario chapter 2's bank (the PSX's SCENA02.EMI), compiled into the exe at
// 0x53DDA0..0x5428B2. docs/scena_sc2.md.
//
//   - The state machine: Scena02_Frame (vtable slot 0) jumps through
//     Scena02_States on the state byte: 0 Scena02_Start, 1 Scena02_EnterArea
//     (once per area entered; every way out stores state 2), 2 Scena02_Run,
//     which jumps through Scena02_Runs on the run MoveScript_Var7 to one of
//     twenty-four scenes (the other four entries are 0x437CC0, a bare ret).
//     Run 0 is not a bare ret here: Scena02_Scene00 runs whenever no scene
//     does, and in area 0x1A starts run 7 when the leader walks up to one of
//     the sprites (the twelve Scena02_Near helpers) and run 0x1A at three
//     cells (Scena02_DoorStart / Scena02_DoorSound).
//   - A scene is a switch on the step byte 0x8034E5: each step waits on a
//     script counter (0x903848..B), the message box (Field_Request), the wait
//     word (Transition_Busy), a flag, a stream or the leader's cell,
//     does one thing - a message, a sound, the music, an effect, an area
//     change, a party member dropped in, an event battle - and sets the next
//     step; the last clears the counters and sets the run and step to 0.
//   - Slot 1, the object hook: through Scena02_ObjectHandlers on the object's
//     +0x86 to one of twenty-two small handlers that start a scene or set a
//     counter (entry 0xA is 0x557170, group SC9b's block).
//   - Slot 2, the step hook: by area, rectangles of the leader's target
//     (x, z) that start a scene, answering 1 in al; 0 otherwise.
//
// Every call goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that an index past a
// table - the state past Scena02_States, the run past Scena02_Runs, the
// object's +0x86 past Scena02_ObjectHandlers - aborts where the original
// would jump through whatever lies there (the spell round's rule,
// docs/takeover-queue-round9.md section 6).
#include "game/scena_sc2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc2_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc2::at;
using move_script::Word;

// --- the chapter's bytes ------------------------------------------------------

}  // namespace

void ScenaSc2_Inject() {
    if (bof3::WantsShadow("scena_sc2")) scena_sc2::SelfTest();
}
