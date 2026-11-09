// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_02.h"

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

// original 0x401150 (area 2's choice 0): no new message; row 0 sets the byte
// 0x9039F5 to 5, row 1 to 4, another row nothing more; then tail kind 5 and
// story flag 0x1E.
extern "C" void __cdecl Area02_ChoiceArmTail(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) Mem(::bof3::Img(AreaW0c_TailSub_addr))[0] = 5;
    else if (row == 1) Mem(::bof3::Img(AreaW0c_TailSub_addr))[0] = 4;
    else return;
    Mem(::bof3::Img(AreaW0a_TailKind_addr))[0] = 5;
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1E);
}

void Area02_Inject() {
    BOF3_INJECT(Area02_ChoiceArmTail);
}
