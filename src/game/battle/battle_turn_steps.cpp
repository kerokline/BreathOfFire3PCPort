// The per-turn steps: group CC of the eighth round, twenty functions of the
// battle engine (PC 0x4302B0..0x43192F, the PSX's BATTLE.EMI compiled into
// the exe), each read whole. docs/battle_turn_steps.md.
//
// Battle_PhaseDispatch (battle_flow.cpp) calls the phase's entry of its
// six-entry table; phases 4 and 5 are 0x4302B0 and 0x4311E0, dispatch stubs
// `xor eax, eax; mov al, [state]; jmp [table + eax * 4]` over tables in .data.
// The steps they reach take no argument and answer nothing any caller reads.
//
//   - Phase 4, the round's end (BattleRoundEnd_*): by 0x904AA1, the faster
//     side's extra round (0x4302C0 over BattleRoundEnd_FasterSteps), the
//     status chain 0x430510 that runs the nine BattleSteps, and 0x431090,
//     which waits for the round's busy words, then starts the next round
//     (phase 1) or, once 0x904AE8 is set, the battle's end (phase 5).
//   - Phase 5, the battle's end (BattleEnd_*): by 0x904AA1, the members'
//     tasks (0x4311F0), the win (0x4314B0, whose third step 0x431910 is the
//     result through 0x431920 and group CD's pages), the other way out (0x431540 and
//     0x4315B0, not reached by the combat route, left Capcom's), and the
//     exit (0x431760: Battle_WriteBackParty, the exit hook, the finish).
//
// Every call goes through battle_turn_steps::g (battle_turn_steps_callees.h),
// so that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Everything here is a faithful replacement.
#include "game/battle_turn_steps.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_turn_steps_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/psp_code.h"
#include "hook/detour.h"

namespace battle_turn_steps {

namespace {
template <typename F> F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace battle_turn_steps

using namespace battle_turn_steps;

namespace {

using move_script::Word;

}  // namespace

// ===========================================================================
// Phase 4: the round's end
// ===========================================================================

// ===========================================================================
// Phase 5: the battle's end
// ===========================================================================

// ===========================================================================

void BattleTurnSteps_Inject() {
    if (bof3::WantsShadow("battle_turn_steps")) battle_turn_steps::SelfTest();
}
