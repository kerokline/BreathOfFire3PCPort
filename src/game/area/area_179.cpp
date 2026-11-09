// Moved here by tools/move_funcs.py from area_w4d.cpp (docs/source-layout-round.md).
#include "game/area/area_179.h"

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

extern "C" void __cdecl Area179_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }

// original 0x4298A0 (the init of all eleven; PSX 0x801F32D8 / area 175's
// 0x801F44D8): Cond_ByteFA above 7 (signed): the engine's field reset
// 0x455450. The hook's arm byte cleared; Cond_ByteFD not 0:
// Sound_PlayEffect(0x20C).
extern "C" void __cdecl Area179_Init(void) {
    if (Cond_ByteFA > 7) AH_AT(void (__cdecl*)(void), at::kFieldReset.v)();
    const unsigned char fd = Cond_ByteFD;
    B(::bof3::Img(AreaW4d_HookArmed_addr)) = 0;
    if (fd != 0) AH_CALL(Sound_PlayEffect)(0x20C);
}

// original 0x4298D0 (Area_CellHooks' handler for all eleven): the cell's x
// word 0x3F, z word 0xE and the leader's pose 1: tail kind 0x1A (Cond_ByteFA
// above 7, signed) or 6, al 1; else al 0.
extern "C" unsigned char __cdecl Area179_CellHook(unsigned x, unsigned z) {
    if (static_cast<std::uint16_t>(x) != 0x3F) return 0;
    if (static_cast<std::uint16_t>(z) != 0xE) return 0;
    if (B(::bof3::Img(ObjTrio_addr) + 0x8) != 1) return 0;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = Cond_ByteFA > 7 ? 0x1A : 6;
    return 1;
}

// original 0x429910 (choice 0 of all eleven): message 0xFFFF; the byte after
// MoveScript_Var7 is 3 for answer 0, else 2.
extern "C" void __cdecl Area179_ChoiceVar8(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = static_cast<unsigned char>((answer == 0 ? 1 : 0) + 2);
}

void Area179_Inject() {
    BOF3_INJECT(Area179_ChoiceMessageByAnswer);
    BOF3_INJECT(Area179_Init);
    BOF3_INJECT(Area179_CellHook);
    BOF3_INJECT(Area179_ChoiceVar8);
}
