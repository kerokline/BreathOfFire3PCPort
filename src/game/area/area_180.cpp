// Moved here by tools/move_funcs.py from area_w4d.cpp (docs/source-layout-round.md).
#include "game/area/area_180.h"

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

extern "C" void __cdecl Area180_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }

// original 0x429990 (choice 24 of all eleven): message 0xFFFF; the facility
// byte +1 is the answer.
extern "C" void __cdecl Area180_ChoiceStoreAnswer(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    B(::bof3::Img(Rest4b_BoardFlag_addr) + 1) = answer;
}

void Area180_Inject() {
    BOF3_INJECT(Area180_ChoiceMessageByAnswer);
    BOF3_INJECT(Area180_ChoiceStoreAnswer);
}
