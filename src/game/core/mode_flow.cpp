// The top-level task flow: the boot task WinMain starts (0x496B60) and the
// title's loader it hands over to (0x496C90); the field task 0x495800 with
// its mode-0 and mode-1 handlers, the play clock it ticks every frame, and
// the area-entry hub 0x594E60 under mode 1; the screen-transition task
// 0x495070 with its 21 kinds and the fade loops under them; and the small
// resets they share. docs/mode-flow.md.
//
// Every call goes through mode_flow::g (mode_flow_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike.
#include "game/mode_flow.h"

#include "game/area_data_reload.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/mode_flow_callees.h"
#include "game/widescreen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "input/sigma.h"
#include "hook/log.h"

namespace mode_flow {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace mode_flow

using namespace mode_flow;

// ===========================================================================
// The screen transition (0x495040..0x495800)
// ===========================================================================

// ===========================================================================
// The field task (0x495800) and its modes 0 and 1
// ===========================================================================

// original 0x496870 (PSX 0x80199CAC): the play clock and two countdowns, once a frame from
// Field_Task (PSX 0x80199CAC, gap pairing - the same code). Unless
// Field_Request is 9: the clock's frames count to 30, seconds and minutes to
// 60, hours to 99, and at 99:59:59 it stops; then each countdown ticks.
//
// As the original has it: the stop is hours == 0x63 && minutes == 0x3B &&
// seconds == minutes, exactly (hours above 99 keep counting); the rollover
// tests are unsigned (>= 30, >= 60); at 99 hours a rollover of the minutes
// holds the clock at 99:59:59 rather than counting the hour.

// ===========================================================================
// Boot (0x496B60, 0x496C90) and the small resets
// ===========================================================================

// ===========================================================================

void ModeFlow_Inject() {
    if (bof3::WantsShadow("mode_flow")) mode_flow::SelfTest();
}
