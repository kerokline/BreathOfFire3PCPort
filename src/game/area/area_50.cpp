// Moved here by tools/move_funcs.py from area_w1c.cpp (docs/source-layout-round.md).
#include "game/area/area_50.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

}  // namespace

// original 0x409C50 (Area50_Choices[0]): answer not 0: message 0x84 and the
// mark 0x9398CF = 6; answer 0: the byte 0x9045FB at 0x1E or more message
// 0x82, below message 0x83 and the mark 6.
extern "C" void __cdecl Area50_ChoiceAsk82(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0x84);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    if (B(::bof3::Img(AreaW1c_Byte9045FB_addr)) >= 0x1E) {
        SetMessage(0x82);
        return;
    }
    SetMessage(0x83);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}

// original 0x409C90 (Area50_Handlers[0]; PSX 0x801F2D48): map cells x 8 of
// rows 0x25..0x28 set to 0x10.
extern "C" void __cdecl Area50_SetCells(void) {
    for (unsigned z = 0x25; z <= 0x28; ++z) SetByte(8, z, 0x10);
}

// original 0x409CC0 (Field_ObjectTriggers id 43, (object, flags) ignored):
// ScriptFlags_Set40, the mode tail kind 4 with sub-kind 4 (a register store,
// mov al 4: the tool's immediate scan does not see it); answers 0 in al.
extern "C" unsigned char __cdecl Area50_Trigger43(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 4;
    return 0;
}

void Area50_Inject() {
    BOF3_INJECT(Area50_ChoiceAsk82);
    BOF3_INJECT(Area50_SetCells);
    BOF3_INJECT(Area50_Trigger43);
}
