// Four sparkle overlays: the PSX's MAGIC071..074.EMI (Magic_Rows rows 45, 113,
// 47, 114; read one id down, Healing Herb / Rejuvenate, Restore, Vitalize and
// Vigor - names that are hypotheses, cut-content section 2), compiled into the
// exe at 0x4B9930..0x4BCBEE. docs/magic_s16.md.
//
//   - Each overlay holds a copy of the Healing Herb's sparkle code (MAGIC070,
//     battle_items.cpp and magic_fx_reached.cpp): the sparkle's type dispatch,
//     its update and three phases, its three draws, the pool's alloc and free
//     - instruction for instruction the same but for the addresses of its own
//     pool (0x2C-byte records), its "current sparkle" cell and its .data
//     tables. Written once here, over a table per overlay (kPools).
//   - MAGIC071 and 072 are MAGIC070 whole: the task (a six-entry stack table,
//     then the pool walk) and the spawn, with the effect's kind 1 and 2 where
//     MAGIC070's is 0; the task's other phases are shared bodies (round
//     eight's BattleFx_* and Sparkle_End).
//   - MAGIC073 and 074 put a child task (kind 1, parameter 0x26 / 0x24) on
//     every actor of the target side that is not out; each child tints its
//     actor, sparkles on it, and fades; the parent waits for the children
//     (073 also for every actor to leave state 6).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// stack table aborts where the original would call through its own stack
// (docs/magic_fx_reached.md section 3, the precedent). The dispatches through
// .data read their table in place, unchecked, as round seven's
// Sparkle_Dispatch does.
#include "game/magic_s16.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

// kTints: now MoveScript_TintRecords (symbols.toml). MoveScript_TintRecords, 12-byte records
constexpr ::bof3::image::Va kEnemyState{0x93BCD9};    // what 0x4BBCE0 reads: 0x93B961 + 0x128 * 3 (docs/magic_s16.md section 6)
// kPartyState: now ObjTrio[0].state (symbols.toml). a member's +1

// ===========================================================================
// The sparkle code, one copy per overlay (MAGIC070's, battle_items.cpp)

// ===========================================================================
// The tasks

}  // namespace

// ===========================================================================
// MAGIC071 (row 45) and MAGIC072 (row 113): MAGIC070 whole

// ===========================================================================
// MAGIC073 (row 47) and MAGIC074 (row 114): a child on every actor

namespace {
// kActorPhases073: now Magic073_ActorPhases (symbols.toml). the child's dispatch table, read in place by +1
// kActorPhases074: now Magic074_ActorPhases (symbols.toml).

}  // namespace

// ===========================================================================
// The sparkle code's named copies

void MagicS16_Inject() {
    if (bof3::WantsShadow("magic_s16")) magic_s16::SelfTest();
}
