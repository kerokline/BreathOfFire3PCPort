// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_05_2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0a::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

// The cursor row as a table index (movsx) and as a test (a byte).
int CursorRow() { return static_cast<signed char>(Mem(::bof3::Img(AreaW0c_ChoiceAnswer_addr))[0]); }

void SetMessage(unsigned v) { SetWord(Mem(::bof3::Img(at::kMessage.v)), v); }

}  // namespace

// original 0x401670 (area 5's choice 0, area 10's choice 0, area 15's choice
// 0 and handler 6; PSX 0x801F2CB8 as area 15's handler 6): no new message;
// row 0 sets variable 3 to 0x64, row 1 to 0x65.
extern "C" void __cdecl Area05_ChoiceVar3At64(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) Mem(::bof3::Img(at::kVar3.v))[0] = 0x64;
    else if (row == 1) Mem(::bof3::Img(at::kVar3.v))[0] = 0x65;
}

void Area052_Inject() {
    BOF3_INJECT(Area05_ChoiceVar3At64);
}
