// Group BI of the seventh round: the random encounter's placement on the field
// map and the battle intro's party steps. docs/inventory_ops.md.
//
//   - The placement, 0x5920E0..0x592EFE (PSX GAME.EMI section 0,
//     0x801C73A8..0x801C881C): under 0x591F30 (not ours - nobody's this
//     round), which Field_EncounterDue asks whether a fight fits where the
//     party stands. The party is put round a centre by a formation table,
//     each member tried at 16 cells; the enemies round it by a triangle of
//     offsets and 49 jitters; every spot must fit the cell map, keep apart
//     from the others, be reachable in steps from the centre or a member,
//     and (0x5928F0) project on screen; then the camera is aimed (0x592A30).
//   - The intro's party steps, 0x532550..0x532C0E, 0x52F570 and 0x534880
//     (PSX 0x801BFBA8.., 0x801B3CF8, 0x801C3998): the initiative roll, the
//     turn to face the fight, the shade fade out and back, the walk to the
//     placed spots.
//
// Every call goes through inventory_ops::g (inventory_ops_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. "SC" is Sprite_Current, read afresh wherever the
// original reads [0x937F88] again, and held where it holds it in a register.
#include "game/inventory_ops.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/inventory_ops_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace inventory_ops {

namespace {
template <typename F>
F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace inventory_ops

using namespace inventory_ops;

namespace {

// move_script::SetWord is named in full here (step B0, analysis/tu_renames.tsv)
// move_script::Word is named in full here (step B0, analysis/tu_renames.tsv)

using i32 = std::int32_t;
using u32 = std::uint32_t;

unsigned char* PosX(u32 m) { return move_script::At(::bof3::Img(inventory_ops::at::kMemberPos.v) + 8 * m); }
unsigned char* PosZ(u32 m) { return move_script::At(::bof3::Img(inventory_ops::at::kMemberPos.v) + 8 * m + 4); }

// A shift of a value the original holds as a 32-bit register.
i32 Shl(i32 v, unsigned n) { return static_cast<i32>(static_cast<u32>(v) << n); }
i32 AddInventoryOps(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }

}  // namespace

// ===========================================================================
// The placement: the cell map and the geometry under it


// ===========================================================================
// The placement: the party

// ===========================================================================
// The placement: the enemies

// ===========================================================================
// The battle intro's party steps

// ===========================================================================

void InventoryOps_Inject() {
    if (bof3::WantsShadow("inventory_ops")) inventory_ops::SelfTest();
}
