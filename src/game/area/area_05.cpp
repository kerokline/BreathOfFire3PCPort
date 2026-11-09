// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_05.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0a.h"
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

using move_script::Word;

// The cursor row as a table index (movsx) and as a test (a byte).
int CursorRow() { return static_cast<signed char>(Mem(::bof3::Img(AreaW0c_ChoiceAnswer_addr))[0]); }

void SetMessage(unsigned v) { SetWord(Mem(::bof3::Img(at::kMessage.v)), v); }

// A u16 of an area's message table by the s8 cursor row, unchecked as the
// original's (a row outside the table reads its neighbours in the image).
unsigned MessageAt(std::uint32_t table, int row) {
    return Word(Mem(static_cast<std::uint32_t>(static_cast<std::int32_t>(table) + row * 2)));
}

}  // namespace

// original 0x401250 (area 4's choice 0): no new message; row 0 sets the byte
// 0x9039F5 to 0, row 1 to 1, another row nothing more; then tail kind 5 and
// story flag 0x1F.
extern "C" void __cdecl Area04_ChoiceArmTail(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) Mem(::bof3::Img(AreaW0c_TailSub_addr))[0] = 0;
    else if (row == 1) Mem(::bof3::Img(AreaW0c_TailSub_addr))[0] = 1;
    else return;
    Mem(::bof3::Img(AreaW0a_TailKind_addr))[0] = 5;
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1F);
}

// original 0x401290 (area 5's choice 2): the message Area05_MessagesA[row].
extern "C" void __cdecl Area05_ChoiceMessageA(void) { SetMessage(MessageAt(::bof3::Img(Area05_MessagesA_addr), CursorRow())); }

// original 0x4012B0 (area 5's choice 3): the message Area05_MessagesB[row].
extern "C" void __cdecl Area05_ChoiceMessageB(void) { SetMessage(MessageAt(::bof3::Img(Area05_MessagesB_addr), CursorRow())); }

void Area05_Inject() {
    BOF3_INJECT(Area04_ChoiceArmTail);
    BOF3_INJECT(Area05_ChoiceMessageA);
    BOF3_INJECT(Area05_ChoiceMessageB);
}
