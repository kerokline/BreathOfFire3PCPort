// Moved here by tools/move_funcs.py from area_w4d.cpp (docs/source-layout-round.md).
#include "game/area/area_187.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4d::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// Field_ChangeArea to the return point (its area word, x, z) with flags 4.
void ChangeToReturnPoint() {
    AH_CALL(Field_ChangeArea)(Word(Mem(::bof3::Img(AreaW4d_ReturnArea_addr))), Long(Mem(::bof3::Img(AreaW2d_ReturnPoint_addr))), Long(Mem(::bof3::Img(AreaW4d_ReturnZ_addr))), 4);
}

}  // namespace

// original 0x42A240 (choice 0): the answer read signed picks a byte pair of
// Area187_FocusPairs (unchecked; any s8 stays in .data): the focus object's
// dwords +0x18 and +0x1C the pair's two bytes (the focus and the answer read
// again for the second); message 0xFFFF between.
extern "C" void __cdecl Area187_ChoiceFocusPair(void) {
    const std::int32_t first = static_cast<signed char>(Answer());
    unsigned char* const focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const unsigned char x = B(::bof3::Img(Area187_FocusPairs_addr) + static_cast<U>(first * 2));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, x);
    unsigned char* const again = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const std::int32_t second = static_cast<signed char>(Answer());
    SetLong(again + 0x1C, B(::bof3::Img(Area187_FocusPairs_addr) + static_cast<U>(second * 2) + 1));
}

// original 0x42A280 (Field_ObjectTriggers id 14; a gap of the tool): tail
// kind 0x1D; al 0.
extern "C" unsigned char __cdecl Area187_Trigger14(unsigned char*, unsigned char*) {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x1D;
    return 0;
}

// original 0x42A290 (Field_ModeTailKinds[29]; a gap of the tool, armed by
// Area187_Trigger14): the s8 state - 0: ScriptFlags_Set40, Msg_OpenScript(2),
// Field_Request 2, the state (read again) on 1; 1: Field_Request not 2:
// Area131_DisarmTail (a tail jmp); 2: Field_Request not 2:
// Area131_DisarmTail, the byte 0x904152 cleared, story flag 0x77 cleared,
// Field_ChangeArea to the return point with flags 4.
extern "C" void __cdecl Area187_TailMessage2(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0: {
        AH_CALL(ScriptFlags_Set40)();
        AH_CALL(Msg_OpenScript)(2);
        const unsigned char state = B(::bof3::Img(AreaW0c_TailState_addr));
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = static_cast<unsigned char>(state + 1);
        break;
    }
    case 1:
        if (Field_Request != 2) AH_CALL(Area131_DisarmTail)();
        break;
    case 2:
        if (Field_Request == 2) break;
        AH_CALL(Area131_DisarmTail)();
        B(::bof3::Img(AreaW3c_CampFlag_addr)) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        ChangeToReturnPoint();
        break;
    default: break;
    }
}

void Area187_Inject() {
    BOF3_INJECT(Area187_ChoiceFocusPair);
    BOF3_INJECT(Area187_Trigger14);
    BOF3_INJECT(Area187_TailMessage2);
}
