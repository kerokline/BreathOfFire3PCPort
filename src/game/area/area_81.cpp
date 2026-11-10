// Moved here by tools/move_funcs.py from area_w2a.cpp (docs/source-layout-round.md).
#include "game/area/area_81.h"

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

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

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

// The same with the message word from `table` by the answer.
void ChoiceMessageCounter0(std::uint32_t table, unsigned char on0, unsigned char on1) {
    const signed char answer = Answer();
    SetMessage(MessageAt(table, answer));
    CountersByAnswer(answer, on0, on1);
}

// A free effect record of kind `kind` (Effect_FindFree; none: nothing), its
// +0 set 1 and +5 the kind; answers the record, or null for none.
unsigned char* SpawnEffectRecord(unsigned char kind) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = kind;
    return e;
}

// Camera_ShiftY moved by `by`, MapView_Redraw 2.
void ShiftCamera(int by) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + by);
    MapView_Redraw = 2;
}

}  // namespace

// original 0x40F550 (Area81_Choices[0]): counter 0 1 for answer 0, 2 for 1.
extern "C" void __cdecl Area81_ChoiceCounter1or2(void) { ChoiceCounter0(1, 2); }

// original 0x40F580 (Area81_Choices[1]): the message word from
// Area81_ChoiceMessages by the signed answer (unchecked); counter 0 3 for
// answer 0, 4 for 1.
extern "C" void __cdecl Area81_ChoiceMessageCounter3or4(void) { ChoiceMessageCounter0(::bof3::Img(Area81_ChoiceMessages_addr), 3, 4); }

// original 0x40F5B0 (Area81_Handlers[0]; PSX 0x801F2C94): a free effect
// record of kind 0x27 put at the running object's x and z (Sprite_Current
// read again for each, after the record's +0 and +5 are written).
extern "C" void __cdecl Area81_SpawnEffect27AtObject(void) {
    unsigned char* const e = SpawnEffectRecord(0x27);
    if (e == nullptr) return;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(e)->x, rv::Current<records::SpriteObject>()->x);
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(e)->z, rv::Current<records::SpriteObject>()->z);
}

// original 0x40F5F0 (Area81_Handlers[1]; PSX 0x801F2D14): Camera_ShiftY -
// 0xF, MapView_Redraw 2.
extern "C" void __cdecl Area81_ShiftCameraDown(void) { ShiftCamera(-0xF); }

// original 0x40F600 (Area81_Handlers[2]; PSX 0x801F2D3C): Camera_ShiftY +
// 0xF, MapView_Redraw 2.
extern "C" void __cdecl Area81_ShiftCameraUp(void) { ShiftCamera(0xF); }

void Area81_Inject() {
    BOF3_INJECT(Area81_ChoiceCounter1or2);
    BOF3_INJECT(Area81_ChoiceMessageCounter3or4);
    BOF3_INJECT(Area81_SpawnEffect27AtObject);
    BOF3_INJECT(Area81_ShiftCameraDown);
    BOF3_INJECT(Area81_ShiftCameraUp);
}
