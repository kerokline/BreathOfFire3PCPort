// Moved here by tools/move_funcs.py from area_w4a.cpp (docs/source-layout-round.md).
#include "game/area/area_155.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4a.h"
#include "game/area_w4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::SetLong;

using move_script::SetWord;

unsigned char& B(U address) { return move_script::At(address)[0]; }

void SetMessage(unsigned v) { SetWord(At(::bof3::Img(at::kMessage.v)), v); }

std::int32_t Answer() { return AreaW0c_ChoiceAnswer; }

unsigned char* Focus() { return reinterpret_cast<unsigned char*>(AreaW1a_FocusObject); }

void ArmTail(unsigned kind, unsigned state) {
    AreaW0a_TailKind = static_cast<unsigned char>(kind);
    AreaW0c_TailState = static_cast<unsigned char>(state);
}

}  // namespace

// original 0x425C30 (area 155's block; the choice of seven areas: area 1
// +0x34[1], 23 [1], 40 [6], 41 [2], 143 [7], 166 [0], 168 [2]): the message
// 0xFFFF; answer (byte) 0: tail kind 0xA (0x56DB80), state 0, argument 0xFF.
extern "C" void __cdecl Area155_ChoiceArmTail10(void) {
    const unsigned char answer = static_cast<unsigned char>(AreaW0c_ChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer != 0) return;
    ArmTail(0xA, 0);
    AreaW0c_TailSub = 0xFF;
}

// original 0x425C60 (area 166 +0x34[1]): the message 0xFFFF; answer (byte) 3:
// ScriptFlags_Set40, tail kind 0xA, state 5, argument 0xFF. Then FocusPair
// over Area166_ChoicePairs 0x63A79C (the answer read again after the call).
extern "C" void __cdecl Area166_ChoiceFocusPair(void) {
    const unsigned char answer = static_cast<unsigned char>(AreaW0c_ChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer == 3) {
        AH_CALL(ScriptFlags_Set40)();
        ArmTail(0xA, 5);
        AreaW0c_TailSub = 0xFF;
    }
    const std::int32_t a = Answer();
    unsigned char* focus = Focus();
    SetLong(focus + 0x18, B(::bof3::Img(at::kA166Pairs.v) + static_cast<U>(a) * 2u));
    focus = Focus();
    SetLong(focus + 0x1C, B(::bof3::Img(at::kA166Pairs.v) + static_cast<U>(Answer()) * 2u + 1));
}

void Area155_Inject() {
    BOF3_INJECT(Area155_ChoiceArmTail10);
    BOF3_INJECT(Area166_ChoiceFocusPair);
}
