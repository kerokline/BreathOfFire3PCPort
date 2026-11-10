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

std::uint32_t g_area_track_fallback = 0;  // DIV-0092; 1 through PatchBytes("AreaTrackFallback") in ModeRest_Inject

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

// original 0x59E330 (PSX 0x8015990C, the JP executable): every window
// record's in-use, handler, kind, state and pass bytes (+0..+3, +0xF)
// zeroed, and the pass the walk starts from. No calls.
//
// As the original has it, and the JP disc: the other fields keep what the
// last window left, and one is read before any write - +0xD bit 0 (no
// frame), which Window_Grow / Window_Shrink read for kinds 1 and 2 and never
// set, so a list or battle window that set it leaves the next window
// frameless. The US executable's reset (PSX 0x80158E50) zeroes +0..+0x1F of
// every record - the bytes, the words +4 / +6 and the eight words +0x10..
// +0x1E - and leaves +0x20..+0x23, as here.
//
// DIVERGENCE DIV-0175 (known-defects D352): 1 through
// PatchBytes("WindowResetFull") in ModeFlow_Inject, after the self-test,
// which compared Capcom's five-field reset.
namespace mode_flow { std::uint8_t g_reset_full = 0; }

// ===========================================================================
// Boot (0x496B60, 0x496C90) and the small resets
// ===========================================================================

// ===========================================================================

void ModeFlow_Inject() {
    if (bof3::WantsShadow("mode_flow")) mode_flow::SelfTest();
    {
        // DIV-0175: after the self-test, which compared Capcom's five-field reset.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("WindowResetFull", Low32(&mode_flow::g_reset_full), &was, &is, 1);
        bof3::Log("DIV-0175    the window reset clears +0..+0x1F of every record, as the US disc (on unless the line above says OFF)");
    }
}
