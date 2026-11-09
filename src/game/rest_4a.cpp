// Round fourteen group R4A (docs/rest_4a.md): the 48 functions of
// analysis/round14_cut.tsv's group R4A, 0x452DD0..0x456D4F, each read with
// capstone to its last instruction (2026-10-05). The band is three things, not
// the "field core" the cut proposed:
//
//   Battle_*        six helpers of the battle's targeting (0x452DD0..0x454372)
//   Field_RunSlot, Field_SlotClutCopy   a Field_Slots record's step: its
//                   script's frame copies columns of a CLUT row
//   CommuSim_*, CommuPose_*   the community's simulation (areas 175..185):
//                   the records 0x9046D0 (60 of 8) and the buildings 0x9048B0
//                   (8 of 8) moved on by the battle count 0x904134, the event queue
//                   0x904CA0, and the village's objects placed and posed
//   FieldTrigger*   twelve entries of Field_ObjectTriggers (ids 1..11, 61)
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again; a memory cell
// the original reads after a call is read after it here, each call's answer
// into a local first. Where the original divides by zero or walks a record
// list that has no end, ours aborts with a message before the fault
// (docs/rest_4a.md section 7). One function is not a plain copy:
// Battle_RandomLiveEnemy reads six bytes of its own frame it never wrote when
// five or more enemies are standing; ours takes them as 0 (section 7: wants a
// ledger entry).
#include "game/rest_4a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_4a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

U UL(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U UL(U a) { return UL(move_script::At(a)); }
void SetUL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }
void SetUL(U a, U v) { SetUL(move_script::At(a), v); }

// The community's records and buildings, and the traits table by record.
unsigned char* Rec(U i) { return At(::bof3::Img(Community_Records_addr) + 8u * i); }

}  // namespace

// ===========================================================================
// The battle's targeting helpers
// ===========================================================================

// ===========================================================================
// A Field_Slots record's step
// ===========================================================================

// ===========================================================================
// The community's simulation
// ===========================================================================

// --- the poses (each: the object's +0x88 word; +6 through Sprite_Current) ---

// ===========================================================================
// Field_ObjectTriggers entries (object, 0x904030) -> al 0
// ===========================================================================

void Rest4A_Inject() {
    if (bof3::WantsShadow("rest_4a")) rest_4a::SelfTest();
}
