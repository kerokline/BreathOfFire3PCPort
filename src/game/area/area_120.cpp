// Moved here by tools/move_funcs.py from area_w3b.cpp (docs/source-layout-round.md).
#include "game/area/area_120.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3b::at;

}  // namespace

namespace {

using move_script::At;

using move_script::SetWord;

using move_script::Word;

constexpr ::bof3::image::Va kMessage{0x7DEE48};   // the message word a choice handler leaves (0xFFFF: none)

}  // namespace

// original 0x41A9D0 (area 120's choice 0): no message (0xFFFF); movement-script
// variable 3 (0x903848) = 0x14 when the answer byte 0x7DEE67 is not 0, else 0xA.
extern "C" void __cdecl Area120_ChoiceVar3A(void) {
    const unsigned char answer = static_cast<unsigned char>(AreaW0c_ChoiceAnswer);
    SetWord(At(::bof3::Img(kMessage.v)), 0xFFFF);
    At(::bof3::Img(at::kScriptVar3.v))[0] = answer != 0 ? 0x14 : 0x0A;
}

// original 0x41A9F0 (area 120's choice 1): no message; variable 3 = 0x1E when
// the answer is not 0, else 0x28.
extern "C" void __cdecl Area120_ChoiceVar3B(void) {
    const unsigned char answer = static_cast<unsigned char>(AreaW0c_ChoiceAnswer);
    SetWord(At(::bof3::Img(kMessage.v)), 0xFFFF);
    At(::bof3::Img(at::kScriptVar3.v))[0] = answer != 0 ? 0x1E : 0x28;
}

// original 0x41AA10 (area 120's choice 2): answer 0 - no message, the word
// +0x8A of the record the pointer 0x903804 names + 1, run 0xD (MoveScript_Var7)
// at step 8; any other answer - message 0x35.
extern "C" void __cdecl Area120_ChoiceStartRun13(void) {
    if (static_cast<unsigned char>(AreaW0c_ChoiceAnswer) != 0) {
        SetWord(At(::bof3::Img(kMessage.v)), 0x35);
        return;
    }
    unsigned char* const record = reinterpret_cast<unsigned char*>(AreaW1a_FocusObject);
    SetWord(At(::bof3::Img(kMessage.v)), 0xFFFF);
    SetWord(record + 0x8A, Word(record + 0x8A) + 1u);
    MoveScript_Var7 = 0xD;
    MoveScript_PartyRecords[0x65] = 8;
}

void Area120_Inject() {
    BOF3_INJECT(Area120_ChoiceVar3A);
    BOF3_INJECT(Area120_ChoiceVar3B);
    BOF3_INJECT(Area120_ChoiceStartRun13);
}
