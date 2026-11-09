// Moved here by tools/move_funcs.py from area_w2a.cpp (docs/source-layout-round.md).
#include "game/area/area_80.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2a.h"
#include "game/area_w2a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2a::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// The choice answer, as every choice of the band reads it: a signed byte.
signed char Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// A message word from a choice's table by the signed answer (unchecked: a
// negative answer reads before the table, a large one past it - in .data).
unsigned MessageAt(std::uint32_t table, signed char answer) {
    return Word(Mem(table + static_cast<std::uint32_t>(static_cast<int>(answer) * 2)));
}

// The band's commonest choice: no new message; answer 0 sets counter 0 to
// `on0`, answer 1 to `on1`, any other nothing.
void CountersByAnswer(signed char answer, unsigned char on0, unsigned char on1) {
    if (answer == 0) B(::bof3::Img(at::kCounter0.v)) = on0;
    else if (answer == 1) B(::bof3::Img(at::kCounter0.v)) = on1;
}

// The same with the message word from `table` by the answer.
void ChoiceMessageCounter0(std::uint32_t table, unsigned char on0, unsigned char on1) {
    const signed char answer = Answer();
    SetMessage(MessageAt(table, answer));
    CountersByAnswer(answer, on0, on1);
}

}  // namespace

// originals 0x40F4A0, 0x40F4D0, 0x40F500 (Area80_Choices[0..2]): the message
// word from the choice's pair in Area80_ChoiceMessages by the signed answer
// (unchecked); counter 0 1 for answer 0, 0xA for 1.
extern "C" void __cdecl Area80_ChoiceMessage0(void) { ChoiceMessageCounter0(::bof3::Img(Area80_ChoiceMessages_addr), 1, 0xA); }

extern "C" void __cdecl Area80_ChoiceMessage1(void) { ChoiceMessageCounter0(::bof3::Img(Area80_ChoiceMessages_addr) + 4, 1, 0xA); }

extern "C" void __cdecl Area80_ChoiceMessage2(void) { ChoiceMessageCounter0(::bof3::Img(Area80_ChoiceMessages_addr) + 8, 1, 0xA); }

// original 0x40F530 (Area80_Handlers[2], PSX 0x801F3D34; named by twelve
// areas' tables: 7, 8, 15, 23, 49, 77, 80, 81, 100, 119, 134, 188):
// Camera_ShiftY 0, MapView_Redraw 2.
extern "C" void __cdecl Area80_ResetCameraShift(void) {
    Camera_ShiftY = 0;
    MapView_Redraw = 2;
}

void Area80_Inject() {
    BOF3_INJECT(Area80_ChoiceMessage0);
    BOF3_INJECT(Area80_ChoiceMessage1);
    BOF3_INJECT(Area80_ChoiceMessage2);
    BOF3_INJECT(Area80_ResetCameraShift);
}
