// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_call.h"

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

using move_script::At;

void Set(unsigned a, unsigned b, unsigned c) { SH_CALL(PartySet_Load)(a, b, c, 0); }

void Join(unsigned id) { SH_CALL(Party_Join)(id); }

void Leave(unsigned id) { SH_AT(void (__cdecl*)(unsigned), at::kLeave.v)(id); }

void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes.v)(); }

// Both 3-byte party lists 0x904062..0x904067 to 0xFF.
void EmptyLists() {
    unsigned char* const lists = At(::bof3::Img(Cond_Flags_addr) + 0xd2);
    for (unsigned i = 0; i < 6; ++i) lists[i] = 0xFF;
}

}  // namespace

// original 0x519A00 (0x3F bytes; 3 A[0]): after the load both party lists are
// emptied (0xFF), then member 0 joins alone.
extern "C" void __cdecl Scena03_Set012Party0(void) {
    Set(0, 1, 2);
    EmptyLists();
    Field_MemberCount = 0;
    Join(0);
}

// original 0x519A40 (0x9 bytes; 3 A[1], 5 A[1]): Party_Join(1).
extern "C" void __cdecl ScenaCall_Join1(void) {
    Join(1);
}

// original 0x519A50 (0xF bytes; 3 B[0], 5 B[1]): 0x534030(1); 0x533E00.
extern "C" void __cdecl ScenaCall_Leave1(void) {
    Leave(1);
    Palettes();
}

void ScenaCall_Inject() {
    BOF3_INJECT(Scena03_Set012Party0);
    BOF3_INJECT(ScenaCall_Join1);
    BOF3_INJECT(ScenaCall_Leave1);
}
