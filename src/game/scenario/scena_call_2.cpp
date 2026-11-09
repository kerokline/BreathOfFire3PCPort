// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_call_2.h"

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

void Reload() { SH_CALL(Field_PartyLoad)(0); }

void Join(unsigned id) { SH_CALL(Party_Join)(id); }

}  // namespace

// original 0x519CB0 (0x9 bytes; 5 A[3], 6 A[5]): Party_Join(6).
extern "C" void __cdecl ScenaCall_Join6(void) {
    Join(6);
}

// original 0x519CC0 (0x1F bytes; 5 A[9], 5 A[10], 6 A[6], 6 A[8]): PartySet_Load(0, 1, 2, 0); Field_PartyLoad(0); Party_Join(1).
extern "C" void __cdecl ScenaCall_Set012ReloadJoin1(void) {
    Set(0, 1, 2);
    Reload();
    Join(1);
}

void ScenaCall2_Inject() {
    BOF3_INJECT(ScenaCall_Join6);
    BOF3_INJECT(ScenaCall_Set012ReloadJoin1);
}
