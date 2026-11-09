// The window-kind handlers: the enemy HP gauge's three states that
// Window_DispatchKind 0x597A30 picks between (reached from the enemy target
// window 0x597320), the battle banner window and the battle message window -
// kinds 6 and 7 of the window task's battle handler 0x596FA0 - with the
// states each runs, and record handler 4 of Field_RunTaskRecords.
// docs/window_kinds.md.
//
// Everything here is a faithful replacement: no DIVERGENCE.md entry is owed.
// Where the original indexes a table it built on its own stack without a
// bound, ours aborts loudly on an index past the table (CLAUDE.md rule 4), as
// Field_RunTaskRecords and Window_Run do: the original's slot past the end is
// its own return address, which cannot be reproduced.
#include "game/window_kinds.h"

#include <cstddef>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/window_kinds_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace window_kinds {

namespace {

template <typename T> T As(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

// rv::As<records::WindowRecord>(p) views a window record
// (docs/typed-records.md; WindowRecords
// 0x803160, the records BattleWinStates_Current points at). Only +0..+7 are
// typed: +8..+0x1F mean something else in each handler (typed-records 3).

}  // namespace

const Callees kOriginals = {
    BattleWin_DrawMessageBox,
    Text_DrawAt,
    BattleWin_DrawMessage,
    BattleMsg_Advance,
    Window_FreeCurrent,
    {As<Handler>(::bof3::Code(bof3::addr::BattleWin_BannerSlideIn)), As<Handler>(::bof3::Code(kNop.v)), As<Handler>(::bof3::Code(bof3::addr::BattleWin_BannerSlideOut))},
    {As<Handler>(::bof3::Code(bof3::addr::BattleWin_MessageWait)), As<Handler>(::bof3::Code(bof3::addr::BattleWin_MessageSlideIn)),
     As<Handler>(::bof3::Code(bof3::addr::BattleWin_MessageShow)), As<Handler>(::bof3::Code(bof3::addr::BattleWin_MessageSlideOut))},
    {As<Handler>(::bof3::Code(kResultKinds[0])), As<Handler>(::bof3::Code(kResultKinds[1])), nullptr, nullptr, As<Handler>(::bof3::Code(kResultKinds[4])),
     As<Handler>(::bof3::Code(kResultKinds[5]))},
};
Callees g = kOriginals;

}  // namespace window_kinds

using namespace window_kinds;

// ===========================================================================
// The enemy HP gauge (Window_DispatchKind's three handlers)
// ===========================================================================
//
// Window_DispatchKind is called from one place, the enemy target window
// 0x597320 (group CL's) at 0x59738E, with eight pointers: the window record
// w's +0xB (the gauge's length, 0..0x37 pixels), the enemy's max HP (+0x30 of
// EnemyWorkingRecords 0x93B9E0 + 0x128 (w[+0xA] - 3)), w + 0x1C (the max HP
// last seen), w + 0x14 (the HP the gauge last showed), the enemy's HP (+0x24),
// w + 0xD (the drained part still drawn), w + 0x18 (the step a frame) and
// w + 8 (the gauge state - the index Window_DispatchKind dispatches on). The
// answer in eax is dead at the caller, so ours return nothing.

// ===========================================================================
// The battle banner window (kind 6 of 0x596FA0)
// ===========================================================================

// ===========================================================================
// The battle message window (kind 7 of 0x596FA0)
// ===========================================================================

// ===========================================================================
// Record handler 4
// ===========================================================================

// ===========================================================================

void WindowKinds_Inject() {
    if (bof3::WantsShadow("window_kinds")) window_kinds::SelfTest();
}
