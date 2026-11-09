// Three spell overlays compiled into the exe, round nine group S28
// (docs/magic_s28.md): the PSX's MAGIC122..MAGIC124.EMI, Magic_Rows rows 74,
// 76 and 124. Read one id down (docs/cut-content.md section 2) the sibling
// labels them Firebreath, Icebreath and ThundrBreath; the names below use those
// labels as hypotheses, and say what the code does.
//
//   - MAGIC122 0x4DD8B0..0x4DE8BD: a kind-2 task that makes one beam (kind-1
//     task 0x38) and makes CLUT row 26 half transparent. The beam's body
//     (BreathBeam_*) is reached from MAGIC114, 115, 118, 120 and 121 too:
//     it aims from the caster at the targets' centre, widens, hits, holds and
//     fades, drawing a textured band and a glow band of quads each frame;
//   - MAGIC123 0x4DE8C0..0x4DF4A6: a kind-2 task that runs a pool of 90 motes
//     (0x69ACC8) itself; each mote (BreathMote_*, reached from seven files)
//     leaves the caster at a random angle, flies, and bursts;
//   - MAGIC124 0x4DF4B0..0x4E0906: a kind-2 task that runs a pool of 64
//     (0x69DB30) - 32 bolts at the targets' centre and the sparks they drop -
//     and draws an orb round itself.
//
// Port_DroppedCall 0x4DF820 lies inside MAGIC124 but is no spell's: it is the
// program's empty function (a bare ret), which the linker folded every empty
// function into and happened to keep here (docs/magic_s28.md section 1).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), and a phase a
// .data table holds is called through the cell, read in place, so the start-up
// fuzz can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement, except that a phase past a
// table aborts where the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent), and the beam's widths
// abort where the original would divide by a zero +0xB.
#include "game/magic_s28.h"

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

// kV: now Prim_VertexScratch (symbols.toml).

// The .data tables the dispatchers read in place, index unchecked by the
// originals (ours aborts past each).
// kBeamPhases: now BreathBeam_Phases (symbols.toml). BreathBeam_Task, by +1: one entry
// kBeamSteps: now BreathBeam_Steps (symbols.toml). BreathBeam_Run, by +2: five
// kMoteKinds: now IcePool_Kinds (symbols.toml). IcePool_Dispatch, by +1: two
// kMoteSteps: now BreathMote_Steps (symbols.toml). BreathMote_Task, by +2: four
// kMoteAngles: now BreathMote_Angles (symbols.toml). seven angle steps (u8) the mote draws walk
// kBoltKinds: now ThunderPool_Kinds (symbols.toml). ThunderPool_Dispatch, by +1: two
// kBoltSteps: now ThunderBolt_Steps (symbols.toml). ThunderBolt_Task, by +2: four
// kSparkSteps: now ThunderSpark_Steps (symbols.toml). ThunderSpark_Task, by +2: three

}  // namespace

// ===========================================================================
// MAGIC122 (row 74, Firebreath read one id down)

// ===========================================================================
// MAGIC123 (row 76, Icebreath read one id down)

// ===========================================================================
// MAGIC124 (row 124, ThundrBreath read one id down)

void MagicS28_Inject() {
    if (bof3::WantsShadow("magic_s28")) magic_s28::SelfTest();
}
