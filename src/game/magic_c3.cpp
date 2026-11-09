// The two spell overlays no ability loads, round nine group C3
// (docs/magic_c3.md): the PSX's MAGIC002 and MAGIC111.EMI, Magic_Rows rows 2
// and 119. No label in the sibling's names/magic.toml names either: no ability
// id's row byte is 2 or 119. Row 119 is item magic (the item row table
// 0x64B274's category 0, index 32); row 2 is reached by no index inside the
// item or ability tables on the PC (docs/magic_c3.md section 1). The names
// below say what the code does.
//
//   - MAGIC002 0x499D80..0x49A7AD: a kind-3 task that flies a shaded ball from
//     party member 0's screen point towards the source sprite's for ten frames
//     (radius growing), then shatters it for thirty (each facet shrunk round
//     its own centre while the centres spread) - drawn as two mirrored domes
//     of gouraud quads straight in screen space;
//   - MAGIC111 0x4D6110..0x4D67E5: three kind-1 children of 0x57 - a copy of
//     the caster's record that stands in while the others run, a full-screen
//     subtractive wash of pulsing colours, then a full-screen additive flash -
//     and the caster's animation 4 at the end.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_c3.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::SetLong;
using move_script::Word;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

std::int32_t AddU(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}


// The phase handlers of other units a table holds (docs/magic_c3.md section 3)
// are ours now and named in the tables: the engine's MagicFx_DoneAndFree, S12's
// MagicFx_CountDownRelease / _CountDown2Release, S30's MagicFx_CountUp9By2.


[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// --- MAGIC002's ball ---------------------------------------------------------

}  // namespace

#define C3_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC002 (row 2)

// ===========================================================================
// MAGIC111 (row 119)

void MagicC3_Inject() {
    if (bof3::WantsShadow("magic_c3")) magic_c3::SelfTest();
}
