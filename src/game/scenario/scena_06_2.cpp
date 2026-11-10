// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_06_2.h"

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

void Leave(unsigned id) { SH_AT(void (__cdecl*)(unsigned), at::kLeave.v)(id); }

void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes.v)(); }

}  // namespace

// original 0x519CE0 (0x1F bytes; 6 A[7]): PartySet_Load(0, 1, 2, 0); Field_PartyLoad(0); Party_Join(2).
extern "C" void __cdecl Scena06_Set012ReloadJoin2(void) {
    Set(0, 1, 2);
    Reload();
    Join(2);
}

// original 0x519D00 (0x26 bytes; 6 A[9]): PartySet_Load(0, 1, 2, 0); Field_PartyLoad(0); Party_Join(1); Party_Join(2).
extern "C" void __cdecl Scena06_Set012ReloadJoin12(void) {
    Set(0, 1, 2);
    Reload();
    Join(1);
    Join(2);
}

// original 0x519D30 (0x16 bytes; 6 B[0]): 0x534030(0); 0x534030(2); 0x533E00.
extern "C" void __cdecl Scena06_Leave02(void) {
    Leave(0);
    Leave(2);
    Palettes();
}

// original 0x519D50 (0x16 bytes; 6 B[1]): 0x534030(0); 0x534030(5); 0x533E00.
extern "C" void __cdecl Scena06_Leave05(void) {
    Leave(0);
    Leave(5);
    Palettes();
}

// original 0x519D70 (0x16 bytes; 6 B[2]): 0x534030(0); 0x534030(6); 0x533E00.
extern "C" void __cdecl Scena06_Leave06(void) {
    Leave(0);
    Leave(6);
    Palettes();
}

// original 0x519D90 (0xF bytes; 6 B[3], 9 B[2], 11 B[1], 12 B[1]): 0x534030(2); 0x533E00.
extern "C" void __cdecl ScenaCall_Leave2(void) {
    Leave(2);
    Palettes();
}

// original 0x519DA0 (0x16 bytes; 6 B[5]): 0x534030(6); 0x534030(2); 0x533E00.
extern "C" void __cdecl Scena06_Leave62(void) {
    Leave(6);
    Leave(2);
    Palettes();
}

// original 0x519DC0 (0x16 bytes; 6 B[7]): 0x534030(5); 0x534030(6); 0x533E00.
extern "C" void __cdecl Scena06_Leave56(void) {
    Leave(5);
    Leave(6);
    Palettes();
}

void Scena062_Inject() {
    BOF3_INJECT(Scena06_Set012ReloadJoin2);
    BOF3_INJECT(Scena06_Set012ReloadJoin12);
    BOF3_INJECT(Scena06_Leave02);
    BOF3_INJECT(Scena06_Leave05);
    BOF3_INJECT(Scena06_Leave06);
    BOF3_INJECT(ScenaCall_Leave2);
    BOF3_INJECT(Scena06_Leave62);
    BOF3_INJECT(Scena06_Leave56);
}
