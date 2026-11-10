// The window/task layer: Field_RunTaskRecords' per-frame walk of the 22
// window records, the first of its nine handlers (the window kinds 0, 1 and
// 2), the five states each of them runs, the frame, outline and line an open
// window draws, and the record free. With the area-change funnel above it in
// the image - Field_ChangeArea, the pending area's classification, the zone
// lookup and the music the new area wants. docs/window-task.md.
#include "game/window_task.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "game/window_task_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace window_task {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

namespace {

template <typename T> T As(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

// The record the layer is running, re-read from 0x905B84 at every point the
// original re-reads it - after every call, and in most of these functions
// after every store too (the port's compiler never kept it in a register
// across one). Volatile so that ours cannot cache it either.
unsigned char* Rec() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(
        *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr)))));
}

// rv::As<records::WindowRecord>(p) views a window record
// (docs/typed-records.md; WindowRecords
// 0x803160, the records BattleWinStates_Current points at). Only +0..+7 are
// typed: +8..+0x1F mean something else in each handler (typed-records 3).

}  // namespace


}  // namespace window_task

using namespace window_task;

// ===========================================================================
// The area-change funnel (0x594E00..0x595450)
// ===========================================================================

// original 0x5953B0 (PSX 0x801A1388): the track the new area wants, into
// 0x904CD0, which the mode-5 step hands to Music_Play unless it is 0xFF
// (docs/mode-tasks.md section 3).
//
// With bit 0x80 of Field_ScriptFlags' low byte set the current track is kept
// (0x904131 copied over). Otherwise the zone record at (x, z) decides: byte
// +7 zero means byte +6 IS the track; otherwise +6 names one of the 8-byte
// records at 0x669A48 - a pointer to `count` three-byte entries (track, flag
// row, flag bit) and the count - and the track is the first entry whose flag
// is set, or the byte that follows the list when none is.
//
// As the original has it: the count is stashed into the caller's third
// argument slot as a byte and read back as a dword, then masked to 8 bits, so
// the stale bytes above it never reach the loop; there is no bound on the set
// index or on the flag row.
//
// DIVERGENCE: DIV-0143 (issue #88) - under g_keep_stopping_none, a track kept
// while a stopping fade is still running (Music_FadeOutStop's Music_FadeStops
// with Music_FadeCount not yet 0) is kept as 0xFF, no track. The PlayStation's
// stop at this place is immediate and leaves its current-track byte alone, so
// the kept track equals the current one and its play does nothing: silence.
// The PC's fade marks 0x904131 0xFF when it ends, frames before Area_Enter
// plays the kept byte, and the stopped song starts again - the bedroom's song
// through Ryu's dream in Teepo's bed (Scena01_Scene02 step 2).
std::uint8_t g_keep_stopping_none = 0;   // 1 through PatchBytes("AreaKeepStoppingNone")

unsigned char Area_ZoneTrack(const unsigned char* zone) {
    if (zone[7] == 0) return zone[6];
    const std::uint32_t set = ::bof3::Img(at::kMusicSets.v) + static_cast<std::uint32_t>(zone[6]) * 8u;
    const unsigned count = At(set)[4];
    const unsigned char* entry = At(static_cast<std::uint32_t>(Long(At(set))));   // a 32-bit word
    for (unsigned i = 0; i < count; ++i, entry += 3)
        if (g.flags_test(Cond_Flags + entry[1] * 8u, entry[2]) != 0) break;
    return entry[0];
}

// ===========================================================================
// The window records (0x5954B0..0x596150, 0x59E230, 0x59E310)
// ===========================================================================

// ===========================================================================
// What an open window draws (0x595C50, 0x595F60, 0x596150)
// ===========================================================================

void AreaKeepStoppingNone_Arm() {
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("AreaKeepStoppingNone",
                     Low32(&g_keep_stopping_none), &was, &is, 1);
    bof3::Log("DIV-0143    an area change that keeps the track while it is fading to a stop keeps none "
              "(on unless the line above says OFF)");
}

void WindowTask_Inject() {
    if (bof3::WantsShadow("window_task")) window_task::SelfTest();
}
