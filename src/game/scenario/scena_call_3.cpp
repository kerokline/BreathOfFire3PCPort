// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_call_3.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/scena_calls.h"
#include "game/scena_calls_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_calls::at;

}  // namespace

namespace {

void Join(unsigned id) { SH_CALL(Party_Join)(id); }

}  // namespace

// original 0x51A420 (0x9 bytes; 5 A[2], 6 A[4], 10 A[3]): Party_Join(5).
extern "C" void __cdecl ScenaCall_Join5(void) {
    Join(5);
}

void ScenaCall3_Inject() {
    BOF3_INJECT(ScenaCall_Join5);
}
