// Moved here by tools/move_funcs.py from magic_fx_reached.cpp (docs/source-layout-round.md).
#include "game/magic/magic_fx_3.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/magic_fx_reached.h"
#include "game/magic_fx_reached_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace magic_fx_reached;

namespace magic_fx_reached {

using move_script::At;

}  // namespace magic_fx_reached

#define MFX_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4F52D0 (the last phase of the steal and of the effect at
// 0x4F50B0): once the message window is down, 0x904AA8 bit 2 and the task
// freed (a tail jmp in the original).
MFX_EXPORT void __cdecl MagicFx_EndWhenIdle(void) {
    if (BattleE1_MessageUp != 0) return;
    At(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(At(::bof3::Img(at::kFlags))[0] | 4);
    g.free_current();
}

void MagicFx3_Inject() {
    BOF3_INJECT(MagicFx_EndWhenIdle);
}
