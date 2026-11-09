// Steal's overlay (MAGIC216.EMI on the PSX; Magic_Rows row 87, the ability
// read one id down as Steal), compiled into the exe at 0x4F50B0..0x4F52E5:
// the kind-2 task, its start, and its roll. The fourth function of the
// extent, 0x4F52D0 (the end, once the message window is down), is
// MagicFx_EndWhenIdle, round eight's (magic_fx_reached.cpp), shared with
// Pilfer's overlay. docs/magic_steal.md.
//
// The same routine as Pilfer's (Steal_Start, magic_fx_reached.cpp), in an
// older shape: no thief's double, no animation, and the message queued at
// once rather than by a later phase.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals'
// copies. No divergence: each is a faithful replacement, except that a phase
// past the task's three-entry stack table aborts where the original would
// call through its own stack (docs/magic_fx_reached.md section 3, the
// precedent).
#include "game/magic_steal.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using move_script::Long;
using move_script::SetLong;

}  // namespace

void MagicSteal_Inject() {
    if (bof3::WantsShadow("magic_steal")) magic_steal::SelfTest();
}
