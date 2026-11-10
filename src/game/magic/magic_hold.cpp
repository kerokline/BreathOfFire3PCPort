// Moved here by tools/move_funcs.py from magic_engine.cpp (docs/source-layout-round.md).
#include "game/magic/magic_hold.h"

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

}  // namespace

namespace {

using magic_harness::Mem;

// The effect-done bit: `or byte [0x904AA8], 4`.
void SetDone() { Mem(::bof3::Img(magic_harness::at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(magic_harness::at::kFlags))[0] | 4); }

// A stack-table dispatcher: the phase byte through the table the original
// builds on its stack, unchecked there; past the table ours aborts.
void DispatchMagicEngine(const char* who, const std::uint32_t* phases, unsigned n, unsigned phase) {
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(phases[phase])();
}

}  // namespace

#define MENGINE_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4378D0 (Magic_Rows rows 0 and 126): phase +1 through a
// two-entry stack table - Task_StartHold60, MagicHold_Countdown.
MENGINE_EXPORT void __cdecl MagicHold_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Task_StartHold60, bof3::addr::MagicHold_Countdown};
    DispatchMagicEngine("MagicHold_Task", kPhases, 2, Sprite_Current[1]);
}

// original 0x437900: +9 down while it is not 0; at 0 flag 0x40 on the target
// (Battle_SetTargetFlag40), the effect-done bit, and the slot freed (a tail
// jmp).
MENGINE_EXPORT void __cdecl MagicHold_Countdown(void) {
    unsigned char* const sc = Sprite_Current;
    const unsigned char left = sc[9];
    if (left != 0) {
        sc[9] = static_cast<unsigned char>(left - 1);
        return;
    }
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    SetDone();
    MH_CALL(BattleTask_FreeCurrent)();
}

void MagicHold_Inject() {
    BOF3_INJECT(MagicHold_Task);
    BOF3_INJECT(MagicHold_Countdown);
}
