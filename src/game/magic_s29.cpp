// Spell group S29 of round nine (docs/magic_s29.md): two BMAGIC overlays of
// Magic_Rows compiled into the exe, 49 functions.
//
//   MAGIC125, row 125 (read one id down: DivineBreath) 0x4E0910..0x4E1996: a
//     kind-2 task with two kind-1 children (parameter 0x5C) - a beam that
//     drops from above the side's centre and widens (a disc of POLY_G3 and a
//     wall of POLY_G4), and a burst that closes in, sets the targets' 0x10
//     flag and sheds 64 motes into a pool of its own (0x69FC30, task-shaped
//     records) - each mote flying out and up, drawn as a screen-space star of
//     POLY_G3 and a ring of POLY_G4.
//   MAGIC126, row 127 (read one id down: ShadowBreath) 0x4E19A0..0x4E3254: a
//     kind-2 task with eight kind-1 seekers (parameter 0x4E) launched from the
//     acting actor, homing on the side's centre; the first to arrive makes an
//     orb (three rings of POLY_G3 / POLY_G4), a glow (two POLY_GT4 sprites),
//     sets the targets' 0x10 flag, and sheds 64 motes into a pool of 0x20-byte
//     records (0x6A1D30) that spread, lift and fade as POLY_FT4 sprites.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a direct call to another function of the group goes by its address, which
// in the game is the jmp Inject put there to ours.
//
// No divergence: each function is a faithful replacement, except that a
// dispatch past its table - a stack table's or a .data table's - aborts where
// the original would call through whatever follows (docs/magic_fx_reached.md
// section 3, the precedent). Where the original reads memory again after a
// call (Sprite_Current, the owner, the pool's current record, the scratch
// words), ours reads it again; where it holds a pointer across a call, ours
// holds it.
#include "game/magic_s29.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s29_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
namespace tbl = magic_s29::tbl;

// --- small helpers ---------------------------------------------------------

}  // namespace

// ===========================================================================
// MAGIC125 (row 125, DivineBreath read one id down)

// ===========================================================================
// MAGIC126 (row 127, ShadowBreath read one id down)

void MagicS29_Inject() {
    if (bof3::WantsShadow("magic_s29")) magic_s29::SelfTest();
}
