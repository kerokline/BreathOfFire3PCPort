// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_12.h"

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

// original 0x51A570 (0x2A bytes; 12 A[9]): PartySet_Load(7, 5, 2, 0); Field_PartyLoad(0); Party_Join(2); Party_Join(5); 0x533E00.
extern "C" void __cdecl Scena12_Set752ReloadJoin25(void) {
    Set(7, 5, 2);
    Reload();
    Join(2);
    Join(5);
    Palettes();
}

void Scena12_Inject() {
    BOF3_INJECT(Scena12_Set752ReloadJoin25);
}
