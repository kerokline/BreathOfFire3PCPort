// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_04.h"

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

// Both 3-byte party lists 0x904062..0x904067 to 0xFF.
void EmptyLists() {
    unsigned char* const lists = At(::bof3::Img(Cond_Flags_addr) + 0xd2);
    for (unsigned i = 0; i < 6; ++i) lists[i] = 0xFF;
}

}  // namespace

// original 0x519A60 (0x46 bytes; 4 A[0]): as 0x519A00 with the set (0, 1, 5)
// and members 0 and 1.
extern "C" void __cdecl Scena04_Set015Party01(void) {
    Set(0, 1, 5);
    EmptyLists();
    Field_MemberCount = 0;
    Join(0);
    Join(1);
}

// original 0x519AB0 (0x2D bytes; 4 A[1], 7 A[0]): PartySet_Load(0, 1, 5, 0); Field_MemberCount 0; Party_Join(0); Party_Join(1); Party_Join(5).
extern "C" void __cdecl ScenaCall_Party015(void) {
    Set(0, 1, 5);
    Field_MemberCount = 0;
    Join(0);
    Join(1);
    Join(5);
}

void Scena04_Inject() {
    BOF3_INJECT(Scena04_Set015Party01);
    BOF3_INJECT(ScenaCall_Party015);
}
