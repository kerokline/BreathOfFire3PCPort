// Scenario chapter 0 (the PSX's SCENA00.EMI, compiled into the exe at
// 0x537F20..0x539AD0): its vtable Scena00_Hooks 0x660CD0 - the frame, the
// object hook, the step hook (slot 3 is Scenario_NoHook, slot 4 none) - the
// frame's three states, the run table's ten runs and the helpers they share.
// Round ten group SCH, the scenario harness's proof (scenario_harness.h).
// docs/scena_sc0.md.
//
// Every call out goes through the scenario harness (SH_CALL by name, the
// callees nobody owns by address in scena_sc0_callees.h), and every cell is
// read where and when the original reads it: the fuzz stands recorders in
// that move the step, the run, the timer, the counters and the flags after a
// call, so a value held across a call where the original reads memory again
// shows. Each function's comment says what it does and what of the
// original's shape it keeps.
#include "game/scena_sc0.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc0_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace scena_sc0;

// The record views (game/record_views.h): rv::As<records::EffectObject>(p) is
// given effect objects (Effect_Objects).

}  // namespace

void ScenaSc0_Inject() {
    if (bof3::WantsShadow("scena_sc0")) scena_sc0::SelfTest();
}
