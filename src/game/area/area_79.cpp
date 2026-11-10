// Moved here by tools/move_funcs.py from area_w2a.cpp (docs/source-layout-round.md).
#include "game/area/area_79.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
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

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

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

void ChoiceCounter0(unsigned char on0, unsigned char on1) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    CountersByAnswer(answer, on0, on1);
}

}  // namespace

// original 0x40F2B0 (Area79_Choices[0], [1]): counter 0 1 for answer 0,
// 0x14 for 1.
extern "C" void __cdecl Area79_ChoiceCounter1or14(void) { ChoiceCounter0(1, 0x14); }

// original 0x40F2E0 (Area79_Choices[2]): the message word left as it is;
// an answer 0..5 (unsigned: a negative one is none) sets counter 3 to the
// answer + 1 (a jump table of six in .text).
extern "C" void __cdecl Area79_ChoiceCounter3(void) {
    const int answer = Answer();
    if (static_cast<unsigned>(answer) > 5) return;
    B(::bof3::Img(at::kCounter3.v)) = static_cast<unsigned char>(answer + 1);
}

// original 0x40F340 (Area79_Choices[3]): the message word from
// Area79_ChoiceMessages by the signed answer (read before the bound, so
// unchecked); an answer 0..5 sets counter 3 to 0xB + the answer.
extern "C" void __cdecl Area79_ChoiceMessageCounter3(void) {
    const signed char answer = Answer();
    SetMessage(MessageAt(::bof3::Img(Area79_ChoiceMessages_addr), answer));
    if (static_cast<unsigned>(static_cast<int>(answer)) > 5) return;
    B(::bof3::Img(at::kCounter3.v)) = static_cast<unsigned char>(0xB + answer);
}

// original 0x40F3B0 (Area79_Choices[4]): counter 0 8 for answer 0, 9 for 1.
extern "C" void __cdecl Area79_ChoiceCounter8or9(void) { ChoiceCounter0(8, 9); }

// original 0x40F3E0 (Area79_Choices[5]): counter 0 5 for answer 0, 0xA for 1.
extern "C" void __cdecl Area79_ChoiceCounter5orA(void) { ChoiceCounter0(5, 0xA); }

// original 0x40F410 (Area79_Handlers[0]; PSX 0x801F2E24): jumps through
// Area79_States[Sprite_Current +4] (two entries: 0x40D380, another block's,
// and Area79_StateSlide). Read in place and called directly; a state of 2 or
// more aborts where the original jumps through the dwords after the table.
extern "C" void __cdecl Area79_ObjectState(void) {
    const unsigned state = Sprite_Current[4];
    if (state >= at::kArea79StateCount)
        bof3::Fatal("Area79_ObjectState: state %u outside Area79_States' %u at 0x%X", state, at::kArea79StateCount,
                    static_cast<unsigned>(::bof3::Img(Area79_States_addr)));
    reinterpret_cast<area_harness::Handler>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Area79_States_addr) + state * 4)))))();
}

// original 0x40F430 (Area79_States[1]): with a count at Sprite_Current +0xA,
// the object's x moved by Area79_SlideSteps[count & 0xF] << 11 and its z by
// minus that, the count less 1, Field_State's word +0x12E less 2 (Sprite_Current
// and the count read again for each, as the original); a count of 0: the
// state +4 back to 0.
extern "C" void __cdecl Area79_StateSlide(void) {
    unsigned char* s = Sprite_Current;
    auto* tS = rv::As<records::SpriteObject>(s);
    if (s[0xA] == 0) {
        s[4] = 0;
        return;
    }
    const auto dx = static_cast<std::uint32_t>(static_cast<int>(static_cast<signed char>(B(::bof3::Img(Area79_SlideSteps_addr) + (s[0xA] & 0xFu)))));
    tS->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(tS->x) + (dx << 11));
    s = Sprite_Current;
    tS = rv::As<records::SpriteObject>(s);
    const auto dz = static_cast<std::uint32_t>(-static_cast<int>(static_cast<signed char>(B(::bof3::Img(Area79_SlideSteps_addr) + (s[0xA] & 0xFu)))));
    tS->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(tS->z) + (dz << 11));
    s = Sprite_Current;
    tS = rv::As<records::SpriteObject>(s);
    s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
    AddWord(Field_State + 0x12E, -2);
}

void Area79_Inject() {
    BOF3_INJECT(Area79_ChoiceCounter1or14);
    BOF3_INJECT(Area79_ChoiceCounter3);
    BOF3_INJECT(Area79_ChoiceMessageCounter3);
    BOF3_INJECT(Area79_ChoiceCounter8or9);
    BOF3_INJECT(Area79_ChoiceCounter5orA);
    BOF3_INJECT(Area79_ObjectState);
    BOF3_INJECT(Area79_StateSlide);
}
