// Moved here by tools/move_funcs.py from area_w4d.cpp (docs/source-layout-round.md).
#include "game/area/area_184.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4d.h"
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

using move_script::SetWord;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

// The focus object's trigger id cleared (0xFF): the focus read before the
// message store, as the originals do.
void DropFocusTrigger(unsigned message) {
    unsigned char* const focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    SetMessage(message);
    focus[0x86] = 0xFF;
}

// Areas 175..185's choice 27 (eleven copies): the message by the answer read
// signed - 0xFA, 0xFB, 0xFC, 0xFD for 0..3, 0xF9 for 4; anything else (a
// negative answer is above 4 unsigned) leaves the message word alone.
void ChoiceMessageByAnswer() {
    static const std::uint16_t kMessages[5] = {0xFA, 0xFB, 0xFC, 0xFD, 0xF9};
    const auto answer = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(Answer())));
    if (answer > 4) return;
    SetMessage(kMessages[answer]);
}

// Areas 175 and 184's Zenny question (choices 10 and 14): the answer and
// Party_Zenny at 500 or more (unsigned) pick a message and a byte.
bool HasZenny500() { return static_cast<U>(Long(Mem(::bof3::Img(Party_Zenny_addr)))) >= 0x1F4; }

}  // namespace

extern "C" void __cdecl Area184_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }

// original 0x429BA0 (choice 2 of all eleven): answer 1: message 0x5A and the
// focus object's trigger id cleared; else 0xFFFF.
extern "C" void __cdecl Area184_ChoiceMessage5A(void) {
    if (Answer() == 1) {
        DropFocusTrigger(0x5A);
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x429BD0 (choice 3 of all eleven): answer 1: message 0x5F and the
// tail state 1; else 0xFFFF.
extern "C" void __cdecl Area184_ChoiceTailState1(void) {
    if (Answer() == 1) {
        SetMessage(0x5F);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x429C00 (choice 6 of all eleven): answer 0: message 0x7A; else
// 0x6E and the focus object's trigger id cleared.
extern "C" void __cdecl Area184_ChoiceMessage7A(void) {
    if (Answer() == 0) {
        SetMessage(0x7A);
        return;
    }
    DropFocusTrigger(0x6E);
}

// original 0x429C30 (choice 9 of all eleven): answer 0: message 0xFFFF;
// else 0x77 and the facility byte +4 0xA.
extern "C" void __cdecl Area184_ChoiceMessage77(void) {
    if (Answer() == 0) {
        SetMessage(0xFFFF);
        return;
    }
    SetMessage(0x77);
    B(::bof3::Img(Rest4b_BoardFlag_addr) + 4) = 0xA;
}

// original 0x429C60 (choice 14 of all eleven): answer not 0: message 0x7B,
// the facility byte +5 1; else Party_Zenny at 500 or more: 0x7F and 0; else
// 0x7C and 1.
extern "C" void __cdecl Area184_ChoiceAskZenny7F(void) {
    if (Answer() != 0) {
        SetMessage(0x7B);
        B(::bof3::Img(Rest4b_BoardFlag_addr) + 5) = 1;
        return;
    }
    if (HasZenny500()) {
        SetMessage(0x7F);
        B(::bof3::Img(Rest4b_BoardFlag_addr) + 5) = 0;
        return;
    }
    SetMessage(0x7C);
    B(::bof3::Img(Rest4b_BoardFlag_addr) + 5) = 1;
}

// original 0x429CB0 (choice 15 of all eleven): answer 0: message 0x89; else
// 0x88 and the facility byte +2 2.
extern "C" void __cdecl Area184_ChoiceMessage89(void) {
    if (Answer() == 0) {
        SetMessage(0x89);
        return;
    }
    SetMessage(0x88);
    B(::bof3::Img(Rest4b_BoardFlag_addr) + 2) = 2;
}

// original 0x429CE0 (choice 21 of all eleven): answer 0: message 0xFFFF;
// else 0xF7 and the focus object's trigger id cleared.
extern "C" void __cdecl Area184_ChoiceMessageF7(void) {
    if (Answer() == 0) {
        SetMessage(0xFFFF);
        return;
    }
    DropFocusTrigger(0xF7);
}

void Area184_Inject() {
    BOF3_INJECT(Area184_ChoiceMessageByAnswer);
    BOF3_INJECT(Area184_ChoiceMessage5A);
    BOF3_INJECT(Area184_ChoiceTailState1);
    BOF3_INJECT(Area184_ChoiceMessage7A);
    BOF3_INJECT(Area184_ChoiceMessage77);
    BOF3_INJECT(Area184_ChoiceAskZenny7F);
    BOF3_INJECT(Area184_ChoiceMessage89);
    BOF3_INJECT(Area184_ChoiceMessageF7);
}
