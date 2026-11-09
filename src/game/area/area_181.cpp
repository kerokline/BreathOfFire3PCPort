// Moved here by tools/move_funcs.py from area_w4d.cpp (docs/source-layout-round.md).
#include "game/area/area_181.h"

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

using move_script::SetWord;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

// Areas 175..185's choice 27 (eleven copies): the message by the answer read
// signed - 0xFA, 0xFB, 0xFC, 0xFD for 0..3, 0xF9 for 4; anything else (a
// negative answer is above 4 unsigned) leaves the message word alone.
void ChoiceMessageByAnswer() {
    static const std::uint16_t kMessages[5] = {0xFA, 0xFB, 0xFC, 0xFD, 0xF9};
    const auto answer = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(Answer())));
    if (answer > 4) return;
    SetMessage(kMessages[answer]);
}

}  // namespace

extern "C" void __cdecl Area181_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }

// original 0x429A10 (choice 1 of all eleven): message 0xFFFF; answer 0:
// counter 0 = 0x28 and the byte after MoveScript_Var7 0x15; else 0x32 and
// 0x19.
extern "C" void __cdecl Area181_ChoiceCounter0(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) {
        B(::bof3::Img(at::kCounter0.v)) = 0x28;
        B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0x15;
    } else {
        B(::bof3::Img(at::kCounter0.v)) = 0x32;
        B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0x19;
    }
}

void Area181_Inject() {
    BOF3_INJECT(Area181_ChoiceMessageByAnswer);
    BOF3_INJECT(Area181_ChoiceCounter0);
}
