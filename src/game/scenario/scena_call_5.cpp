// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_call_5.h"

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

void Set(unsigned a, unsigned b, unsigned c) { SH_CALL(PartySet_Load)(a, b, c, 0); }

void Join(unsigned id) { SH_CALL(Party_Join)(id); }

}  // namespace

// original 0x51AC20 (0x2D bytes; 17 A[0], 18 A[0], 19 A[0]): PartySet_Load(0, 3, 4, 0); Field_MemberCount 0; Party_Join(0); Party_Join(3); Party_Join(4).
extern "C" void __cdecl ScenaCall_Party034(void) {
    Set(0, 3, 4);
    Field_MemberCount = 0;
    Join(0);
    Join(3);
    Join(4);
}

void ScenaCall5_Inject() {
    BOF3_INJECT(ScenaCall_Party034);
}
