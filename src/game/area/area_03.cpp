// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_03.h"

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

unsigned char CursorByte() { return Mem(::bof3::Img(AreaW0c_ChoiceAnswer_addr))[0]; }

void SetMessage(unsigned v) { SetWord(Mem(::bof3::Img(at::kMessage.v)), v); }

void SetCells(const unsigned char (*cells)[3], unsigned n) {
    for (unsigned i = 0; i < n; ++i) AH_CALL(AreaMap_SetByte)(cells[i][0], cells[i][1], cells[i][2]);
}

}  // namespace

// original 0x401190 (area 3's choice 0 and handler 2; PSX 0x801F2C04): row 0
// opens message 0x42; another row message 0x44 and the byte 0x9398CF 6.
extern "C" void __cdecl Area03_ChoiceMessage42(void) {
    if (CursorByte() == 0) {
        SetMessage(0x42);
        return;
    }
    SetMessage(0x44);
    Mem(::bof3::Img(AreaW0c_AnswerMark_addr))[0] = 6;
}

// original 0x4011C0 (area 3's choices 3 and 4, handlers 5 and 6; PSX
// 0x801F2CA8): a row other than 0 opens message 0x44 and sets the byte
// 0x9398CF 6; row 0 no new message.
extern "C" void __cdecl Area03_ChoiceMessage44(void) {
    if (CursorByte() != 0) {
        SetMessage(0x44);
        Mem(::bof3::Img(AreaW0c_AnswerMark_addr))[0] = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x4011F0 (area 3's handler 1; PSX 0x801F2D88): four map cells,
// (0x55 and 0x54, 0x1C and 0x1D), set 0xC0 and 0xA1.
extern "C" void __cdecl Area03_SetCells(void) {
    static const unsigned char kCells[][3] = {{0x55, 0x1C, 0xC0}, {0x55, 0x1D, 0xC0}, {0x54, 0x1C, 0xA1}, {0x54, 0x1D, 0xA1}};
    SetCells(kCells, 4);
}

// original 0x401230 (Field_ObjectTriggers entry 22, called by 0x56E020 with
// the object and 0x904030, both unread; no area's descriptor names it, it
// lies in area 3's block): ScriptFlags_Set40, tail kind 4 with the byte
// 0x9039F5 0, and al 0 (eax above it ScriptFlags_Set40's leftover).
extern "C" unsigned char __cdecl Area03_Trigger22(unsigned char* object, unsigned char* flags) {
    static_cast<void>(object);
    static_cast<void>(flags);
    AH_CALL(ScriptFlags_Set40)();
    Mem(::bof3::Img(AreaW0a_TailKind_addr))[0] = 4;
    Mem(::bof3::Img(AreaW0c_TailSub_addr))[0] = 0;
    return 0;
}

void Area03_Inject() {
    BOF3_INJECT(Area03_ChoiceMessage42);
    BOF3_INJECT(Area03_ChoiceMessage44);
    BOF3_INJECT(Area03_SetCells);
    BOF3_INJECT(Area03_Trigger22);
}
