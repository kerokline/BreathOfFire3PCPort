// Moved here by tools/move_funcs.py from area_w1d.cpp (docs/source-layout-round.md).
#include "game/area/area_61.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1d.h"
#include "game/area_w1d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1d::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetLong;

using move_script::SetWord;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// The message box's answer, and the message word a choice leaves.
unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// A message and the mark 0x9398CF = 6 beside it (areas 55, 59, 61).
void MessageMarked(unsigned id) {
    SetMessage(id);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}

// Areas 55, 61's object triggers 23 / 21 and area 59's 24 begin so:
// ScriptFlags_Set40, the mode tail kind 4.
void Tail4(unsigned char sub) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = sub;
}

}  // namespace

// original 0x40B550 (Area60 +0x40 init; PSX 0x801F3310): Field_ScriptFlags
// bit 0x2000 set (its high byte or'ed with 0x20).
extern "C" void __cdecl Area60_InitScriptFlag2000(void) {
    B(::bof3::Img(at::kScriptFlagsHigh.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kScriptFlagsHigh.v)) | 0x20);
}

// original 0x40B560 (Area61_Choices[0]): answer 0: Party_Zenny 0 and message
// 2; else message 3 and the mark.
extern "C" void __cdecl Area61_ChoiceClearZenny(void) {
    if (Answer() != 0) {
        MessageMarked(3);
        return;
    }
    SetLong(Mem(::bof3::Img(Party_Zenny_addr)), 0);
    SetMessage(2);
}

// original 0x40B590 (Area61_Choices[3] and [4]; also areas 59, 113 and 116's
// choices 3 and 4): an answer: message 4 and the mark; 0: message 0xFFFF.
extern "C" void __cdecl Area61_ChoiceMark4(void) {
    if (Answer() != 0) {
        MessageMarked(4);
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x40B5C0 (Field_ObjectTriggers id 21, (object, flags) ignored):
// the mode tail kind 4 with sub-kind 1. Answers 0 in al.
extern "C" unsigned char __cdecl Area61_Trigger21(unsigned char*, unsigned char*) {
    Tail4(1);
    return 0;
}

void Area61_Inject() {
    BOF3_INJECT(Area60_InitScriptFlag2000);
    BOF3_INJECT(Area61_ChoiceClearZenny);
    BOF3_INJECT(Area61_ChoiceMark4);
    BOF3_INJECT(Area61_Trigger21);
}
