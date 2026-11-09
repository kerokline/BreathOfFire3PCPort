// The effect library (queue group L of round nine), compiled into the exe at
// 0x4FAFF0..0x4FC32F after MAGIC226/227 and before MAGIC080: the helpers the
// BMAGIC overlays call - by 1 to 94 of them each - and the engine's Restore
// Form now and then. Nine of its 34 functions were ours before (FxDim_*,
// magic_fx_reached.cpp; BattleActor_*, battle_items.cpp); these are the other
// 25. The first function of the band, 0x4FAF90, is not the library's but
// MAGIC226/227's own pool allocator (docs/magic_lib.md section 1).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that the two popup
// tasks abort on a phase past their four-entry tables (the project's
// precedent, docs/magic_fx_reached.md section 3), and that where the original
// divides by zero (the CLUT helpers' kinds 5..7, the side's centre with every
// actor out) ours aborts with a message instead of the processor's exception.
#include "game/magic_lib.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

// --- the buff popup ----------------------------------------------------------

// --- the sprite-CLUT helpers' cell -------------------------------------------

// --- the steps toward a point ------------------------------------------------

}  // namespace

void MagicLib_Inject() {
    if (bof3::WantsShadow("magic_lib")) magic_lib::SelfTest();
}
