// One spell overlay compiled into the exe, round nine group S13
// (docs/magic_s13.md): the PSX's MAGIC063.EMI, Magic_Rows row 143, loaded by
// id 0x3F. Read one id down (docs/cut-content.md section 2) the sibling
// labels it Sudden Death; the names below use that label as a hypothesis, and
// say what the code does.
//
//   - the kind-2 task (row 143): a child (kind 1, parameter 0x68) on every
//     actor but the caster that is not out, enemies first; then, every frame,
//     the walk of its own pool of 96 motes (SuddenDeath_Motes);
//   - each child: its actor's position, one orbit mote circling it; after a
//     random delay every child but the last frees itself once its motes are
//     gone, and the last bursts 92 motes that close in on it, spin, and rise
//     away, then flags its actor 0x40 and ends;
//   - each mote draws four semi-transparent gouraud triangles about its point
//     (SuddenDeathMote_Shape), coloured by its +5..+7 times its +9.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// stack or .data table aborts where the original would call through whatever
// follows it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s13.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

// kV: now Prim_VertexScratch (symbols.toml).

// One MATRIX block as the original lays it out on its stack, Gte_RotTrans
// writing its translation at +0x14.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

}  // namespace

// ===========================================================================
// The task

// ===========================================================================
// The children (kind 1, parameter 0x68)

// ===========================================================================
// The motes (SuddenDeath_Motes, the current one SuddenDeath_CurrentMote, its
// owner 0x93B940 the child)

void MagicS13_Inject() {
    if (bof3::WantsShadow("magic_s13")) magic_s13::SelfTest();
}
