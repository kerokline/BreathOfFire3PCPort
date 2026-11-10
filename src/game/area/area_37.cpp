// Moved here by tools/move_funcs.py from area_w0c.cpp (docs/source-layout-round.md).
#include "game/area/area_37.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// Area 37's paired choice handlers: "ask" (answer 0: message `yes`; else
// message `no` and the mark 6) and "confirm" (answer not 0: message `no` and
// the mark 6; else 0xFFFF). The answer is tested as a byte.
void ChoiceAsk(unsigned yes, unsigned no) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) == 0) {
        SetMessage(yes);
        return;
    }
    SetMessage(no);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}

void ChoiceConfirm(unsigned no) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(no);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// Area 37's four object triggers (0x405320..0x405380): ScriptFlags_Set40,
// the mode tail kind 4 with sub-kind `sub`; answer 0 in al.
unsigned char TriggerTail4(unsigned char sub) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = sub;
    return 0;
}

}  // namespace

// originals 0x405030 / 0x405060 (Area37_Choices[0] / [3], [4]), 0x405090 /
// 0x4050C0 ([5] / [8], [9]), 0x4050F0 / 0x405120 ([10] / [13], [14]),
// 0x405150 / 0x405180 ([15] / [18], [19]): ChoiceAsk / ChoiceConfirm.
extern "C" void __cdecl Area37_ChoiceAsk5E(void) { ChoiceAsk(0x5E, 0x60); }

extern "C" void __cdecl Area37_ChoiceConfirm60(void) { ChoiceConfirm(0x60); }

extern "C" void __cdecl Area37_ChoiceAsk72(void) { ChoiceAsk(0x72, 0x74); }

extern "C" void __cdecl Area37_ChoiceConfirm74(void) { ChoiceConfirm(0x74); }

extern "C" void __cdecl Area37_ChoiceAsk86(void) { ChoiceAsk(0x86, 0x88); }

extern "C" void __cdecl Area37_ChoiceConfirm88(void) { ChoiceConfirm(0x88); }

extern "C" void __cdecl Area37_ChoiceAsk9A(void) { ChoiceAsk(0x9A, 0x9C); }

extern "C" void __cdecl Area37_ChoiceConfirm9C(void) { ChoiceConfirm(0x9C); }

// original 0x4051B0 (Area37_Choices[20]): answer 0: message 0xFFFF, counter
// 3 = 0x64, Party_DropIn(3); else message 0xBD, counter 3 = 0.
extern "C" void __cdecl Area37_ChoiceDropIn(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) == 0) {
        SetMessage(0xFFFF);
        B(::bof3::Img(at::kCounter3.v)) = 0x64;
        AH_CALL(Party_DropIn)(3);
        return;
    }
    SetMessage(0xBD);
    B(::bof3::Img(at::kCounter3.v)) = 0;
}

// original 0x4051F0 (Area37_Choices[21]): answer 0: ScriptFlags_Set40 and
// the mode tail kind 0x2E (Area37_TailLeave); 1: counter 3 = 0x64; 2:
// counter 3 = 0x5F; message 0xFFFF for those three, the message word left
// alone for any other answer.
extern "C" void __cdecl Area37_ChoiceTail2E(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    if (answer == 0) {
        AH_CALL(ScriptFlags_Set40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x2E;
        SetMessage(0xFFFF);
    } else if (answer == 1) {
        SetMessage(0xFFFF);
        B(::bof3::Img(at::kCounter3.v)) = 0x64;
    } else if (answer == 2) {
        SetMessage(0xFFFF);
        B(::bof3::Img(at::kCounter3.v)) = 0x5F;
    }
}

// original 0x405240 (Field_ModeTailKinds[0x2E]): by the s8 state 0x9039F4,
// 0..3 (a 4-entry jump table in the body): 0 waits for Field_Request to
// leave 2, then story flag 0x72, Field_ChangeArea(0x25, 0x2C8000, 0x270000,
// 1), the pending kind 0xFE, state + 1; 1 waits for Field_Request 0, then
// message 0xB5 and Field_Request 2, state + 1; 2 waits for Field_Request to
// leave 2, then Transition_Start(1) and Draw_PassFlags 0x1F, state + 1; 3
// waits for Transition_Busy 0, then ScriptFlags_Clear40, the tail
// disarmed and counter 3 = 0. The state is re-read after each call.
extern "C" void __cdecl Area37_TailLeave(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0: {
        if (Field_Request == 2) return;
        AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x72);
        AH_CALL(Field_ChangeArea)(0x25, 0x2C8000, 0x270000, 1);
        const auto next = static_cast<unsigned char>(B(::bof3::Img(AreaW0c_TailState_addr)) + 1);
        B(::bof3::Img(Area_EnterTransition_addr)) = 0xFE;
        B(::bof3::Img(AreaW0c_TailState_addr)) = next;
        return;
    }
    case 1: {
        if (Field_Request != 0) return;
        AH_CALL(Msg_OpenScript)(0xB5);
        const auto next = static_cast<unsigned char>(B(::bof3::Img(AreaW0c_TailState_addr)) + 1);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = next;
        return;
    }
    case 2: {
        if (Field_Request == 2) return;
        AH_CALL(Transition_Start)(1);
        const auto next = static_cast<unsigned char>(B(::bof3::Img(AreaW0c_TailState_addr)) + 1);
        Draw_PassFlags = 0x1F;
        B(::bof3::Img(AreaW0c_TailState_addr)) = next;
        return;
    }
    case 3:
        if (Transition_Busy != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        B(::bof3::Img(at::kCounter3.v)) = 0;
        return;
    default: return;   // above 3 or negative: `ja` to the ret
    }
}

// originals 0x405320, 0x405340, 0x405360, 0x405380 (Field_ObjectTriggers
// [45..48], (object, flags) ignored): the mode tail kind 4 with sub-kind
// 0xB, 0xC, 0xE, 0xD. Answer 0 in al.
extern "C" unsigned char __cdecl Area37_Trigger45(unsigned char*, unsigned char*) { return TriggerTail4(0xB); }

extern "C" unsigned char __cdecl Area37_Trigger46(unsigned char*, unsigned char*) { return TriggerTail4(0xC); }

extern "C" unsigned char __cdecl Area37_Trigger47(unsigned char*, unsigned char*) { return TriggerTail4(0xE); }

extern "C" unsigned char __cdecl Area37_Trigger48(unsigned char*, unsigned char*) { return TriggerTail4(0xD); }

// original 0x4053A0 (Area37_Handlers[0]; PSX 0x801F3B14): Field_ScriptFlags
// bit 3 toggled.
extern "C" void __cdecl Area37_ToggleScriptFlag8(void) { Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ 8); }

void Area37_Inject() {
    BOF3_INJECT(Area37_ChoiceAsk5E);
    BOF3_INJECT(Area37_ChoiceConfirm60);
    BOF3_INJECT(Area37_ChoiceAsk72);
    BOF3_INJECT(Area37_ChoiceConfirm74);
    BOF3_INJECT(Area37_ChoiceAsk86);
    BOF3_INJECT(Area37_ChoiceConfirm88);
    BOF3_INJECT(Area37_ChoiceAsk9A);
    BOF3_INJECT(Area37_ChoiceConfirm9C);
    BOF3_INJECT(Area37_ChoiceDropIn);
    BOF3_INJECT(Area37_ChoiceTail2E);
    BOF3_INJECT(Area37_TailLeave);
    BOF3_INJECT(Area37_Trigger45);
    BOF3_INJECT(Area37_Trigger46);
    BOF3_INJECT(Area37_Trigger47);
    BOF3_INJECT(Area37_Trigger48);
    BOF3_INJECT(Area37_ToggleScriptFlag8);
}
