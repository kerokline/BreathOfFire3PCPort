// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_06.h"

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

// original 0x519BF0 (0x2A bytes; 6 A[0]): PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(0); Party_Join(5); 0x533E00.
extern "C" void __cdecl Scena06_Set015ReloadJoin05(void) {
    Set(0, 1, 5);
    Reload();
    Join(0);
    Join(5);
    Palettes();
}

// original 0x519C20 (0x23 bytes; 6 A[1]): PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00.
extern "C" void __cdecl Scena06_Set015ReloadJoin5(void) {
    Set(0, 1, 5);
    Reload();
    Join(5);
    Palettes();
}

// original 0x519C50 (0x23 bytes; 6 A[2]): PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(1); 0x533E00.
extern "C" void __cdecl Scena06_Set015ReloadJoin1(void) {
    Set(0, 1, 5);
    Reload();
    Join(1);
    Palettes();
}

// original 0x519C80 (0x2A bytes; 6 A[3]): PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(1); Party_Join(5); 0x533E00.
extern "C" void __cdecl Scena06_Set015ReloadJoin15(void) {
    Set(0, 1, 5);
    Reload();
    Join(1);
    Join(5);
    Palettes();
}

void Scena06_Inject() {
    BOF3_INJECT(Scena06_Set015ReloadJoin05);
    BOF3_INJECT(Scena06_Set015ReloadJoin5);
    BOF3_INJECT(Scena06_Set015ReloadJoin1);
    BOF3_INJECT(Scena06_Set015ReloadJoin15);
}
