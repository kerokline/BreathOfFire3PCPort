// Round fourteen group R3C (docs/takeover-queue-round14.md section 2, the
// cut analysis/round14_cut.tsv's group column): 61 slots of Effect_Handlers
// (0x64E73C) in the band 0x44D000..0x44E4AA, each read to its last
// instruction with capstone (2026-10-04) and taken through the boss harness's
// engine frame (boss_harness.h, docs/boss_harness.md section 10).
// docs/rest_3c.md has every function one row each.
//
// Effect_ApplyResult (ours, battle_damage.cpp) calls a slot with no argument
// and drops its eax. A slot reads the actor 0x904B34 and the target 0x904B54
// (0..2 a member of ObjTrio, 3..10 an enemy object) and fills the result
// record *0x904B60 (+4 the HP delta, positive damage; +6 the AP delta; +8
// flags), or changes a record directly. Eleven set the acting kind 4 and an
// ability and tail-jump to an ability's damage or heal (R3B's), others call
// R3D's helpers (the resist test 0x44F6A0, the common tail 0x44FB30, the
// status and stat helpers): those are raw addresses (rest_3c_callees.h) until
// their groups merge. A slot that ends in a call or a jump to one of them
// answers that callee's eax (al for the status helpers); the rest are void.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original writes through an index past its records - a member past the
// three, an enemy past the eight, the party count past 3, the turn order past
// the byte after its eleven - ours aborts with a Fatal naming the function
// (the owner's rule for an unchecked index, round9 doc section 6); reads by an
// actor or target byte stay unchecked, as in the rest of our battle code.
// Every call goes through the harness (BH_CALL / BH_AT), so the start-up fuzz
// can stand recorders in for the callees.
#include "game/rest_3c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3c_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3c::at;

using V0 = void (__cdecl*)();

}  // namespace

// ===========================================================================
// Slots 50..61, 83: an ability's effect by its number
// ===========================================================================

// ===========================================================================
// Slots 62..76: damage forms, the status helpers, small heals
// ===========================================================================

// ===========================================================================
// Slots 82..101
// ===========================================================================

// ===========================================================================
// Slots 103..111
// ===========================================================================

void Rest3C_Inject() {
    if (bof3::WantsShadow("rest_3c")) rest_3c::SelfTest();
}
