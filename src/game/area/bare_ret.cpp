// Moved here by tools/move_funcs.py from boss_h.cpp (docs/source-layout-round.md).
#include "game/area/bare_ret.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_h_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_h::at;

}  // namespace

// original 0x437CC0 (PSX 0x801F3414): a bare ret. Boss_SetupTable[0], every hook slot a set-up
// leaves empty, and 455 references engine-wide (the effect dispatchers' slot
// 0, the kinds' tables, field tables).
extern "C" void __cdecl BareRet(void) {}

void BareRet_Inject() {
    BOF3_INJECT(BareRet);
}
