// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_call_4.h"

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

void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes.v)(); }

}  // namespace

// original 0x51A510 (0x23 bytes; 11 A[4], 12 A[4]): PartySet_Load(7, 2, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00.
extern "C" void __cdecl ScenaCall_Set725ReloadJoin5(void) {
    Set(7, 2, 5);
    Reload();
    Join(5);
    Palettes();
}

// original 0x51A540 (0x2A bytes; 11 A[8], 12 A[8]): PartySet_Load(7, 4, 2, 0); Field_PartyLoad(0); Party_Join(4); Party_Join(2); 0x533E00.
extern "C" void __cdecl ScenaCall_Set742ReloadJoin42(void) {
    Set(7, 4, 2);
    Reload();
    Join(4);
    Join(2);
    Palettes();
}

void ScenaCall4_Inject() {
    BOF3_INJECT(ScenaCall_Set725ReloadJoin5);
    BOF3_INJECT(ScenaCall_Set742ReloadJoin42);
}
