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

const Callees kOriginals = {
    Party_Count,
    AreaMap_Elevation,
    Sprite_ObjectAt,
    Gte_RotTransPers,
    Gte_RotMatrix,
    Gte_ApplyMatrix,
    Gte_SetRotMatrix,
    Gte_SetTransMatrix,
    Rand,
    MapView_GroundAt,
    Effect_FindFree,
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned char*, unsigned)>(::bof3::Code(kSetAnimFrom.v)),
    Sprite_ScriptTick,
    Sprite_ShadeFadeBegin,
    Sprite_ShadeRaise,
    Sprite_ScriptTickOnce,
    Sprite_SetAnimation,
    Encounter_PlaceMember,
    Encounter_MemberClear,
    Encounter_MemberStands,
    Encounter_EnemyClear,
    AreaMap_CellNibble,
    Encounter_Project,
    Encounter_Apart,
    Encounter_CellFits,
    Encounter_PathClear,
    Short_Abs,
    Short_Sign,
    Encounter_StepOpen,
    Sprite_TurnSense,
    Sprite_ShadeLower,
};
Callees g = kOriginals;

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

// original 0x5928F0 (PSX 0x801C805C): whether a thing of size class `size`
// at (x, z) is on screen. Its four corners, (size + 1) * 0x4000 out, each on
// the ground (AreaMap_Elevation) and projected: refused if any screen x is
// 320 or more or any y 240 or more. Then the first corner again: yes if its
// screen y less `margin` (a byte) is 8 or more. The PSX compares unsigned
// shorts, so a corner left of or above the screen fails there too; the PC's
// floats accept one (docs/inventory_ops.md section 5). A NaN passes the
// corners and fails the last test, as the x87 compares have it.
//
// DIVERGENCE DIV-0116 (D56): the PSX's lower bounds restored. Its twin,
// GAME.EMI 0x801C805C, loads each corner's screen x and y with lhu and tests
// sltiu 0x140 / sltiu 0xF0 (0x801C80D4, 0x801C80E8), so a corner left of or
// above the screen - a negative short - is refused as well; the port's float
// compares kept only the upper bounds. Once the flag is on, a corner whose
// screen x or y is below 0 is refused too (a NaN still passes, as the upper
// tests let it). InventoryOps_Inject sets it after the self-test, whose fuzz
// compares the original's bounds; BOF3X_ORIGINAL=EncounterOnScreenLowerBounds
// keeps ours with the port's bounds (docs/d56-encounter-bounds.md). Once the
// self-test is past, a corner below 0 is logged either way ("D56 ... refused"
// or "... passed, the port's bounds"), so a run shows where the two part; the
// log changes nothing the game reads.
unsigned char g_on_screen_lower_bounds = 0;
bool g_on_screen_log = false;

// ===========================================================================
// The placement: the party

// ===========================================================================
// The placement: the enemies

// ===========================================================================
// The battle intro's party steps

// ===========================================================================

void InventoryOps_Inject() {
    if (bof3::WantsShadow("inventory_ops")) inventory_ops::SelfTest();
    // DIVERGENCE DIV-0116 (D56): after the self-test, which compares the original's bounds.
    {
        static const std::uint8_t was = 0, is = 1;
        if (bof3::PatchBytes("EncounterOnScreenLowerBounds",
                             Low32(&g_on_screen_lower_bounds),
                             &was, &is, 1))
            bof3::Log("DIV-0116    the encounter's on-screen test refuses a corner left of or above the screen (D56)");
        g_on_screen_log = true;
    }
}
