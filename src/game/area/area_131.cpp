// Moved here by tools/move_funcs.py from area_w3c.cpp (docs/source-layout-round.md).
#include "game/area/area_131.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3c.h"
#include "game/area_w3c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

}  // namespace

// original 0x41D290 (+0x34[2]): answer 0: ScriptFlags_Set40, the focus scene
// at step 0xA (+0x83 = 4), message 0xC; else message 0xFFFF.
extern "C" void __cdecl Area131_ChoiceFocusStepA(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0xFFFF);
        return;
    }
    AH_CALL(ScriptFlags_Set40)();
    FocusScene(0xA, 4);
    SetMessage(0xC);
}

// original 0x41D300 (+0x34[3]): the answer read, the message 0xFFFF; answer 0:
// ScriptFlags_Set40, the focus scene at step 0 (+0x83 = 2).
extern "C" void __cdecl Area131_ChoiceFocusStep0(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    FocusScene(0, 2);
}

// original 0x41D360 (+0x3C[4], PSX 0x801F2E18): Camera_ShiftY - 0x1E, redraw.
extern "C" void __cdecl Area131_CameraShiftYLess1E(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 0x1E);
    MapView_Redraw = 2;
}

// original 0x41D370 (+0x3C[5], PSX 0x801F2E40): Camera_ShiftY + 0x1E, redraw.
extern "C" void __cdecl Area131_CameraShiftYMore1E(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 0x1E);
    MapView_Redraw = 2;
}

// original 0x41D380 (Field_ObjectTriggers id 15): tail kind 34 (0x22); al 0.
extern "C" unsigned char __cdecl Area131_Trigger15(unsigned char*, unsigned char*) {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x22;
    return 0;
}

// original 0x41D390 (Field_ModeTailKinds[34]), by the tail state (s8; a
// negative one is above 3): 0 - ScriptFlags_Set40, message 2, the request 2
// and the state (read after the call) + 1; 1 with the request not 2 - the
// tail disarmed; 2 with the request not 2 - state 3; 3 - Field_ScriptFlags ^
// 0x16, the tail disarmed, the byte 0x904152 0, story flag 0x77 cleared,
// Field_ChangeArea(0x79, 0x1A0000, 0x350000, 3).
extern "C" void __cdecl Area131_TailLeave34(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (static_cast<U>(static_cast<std::int32_t>(state))) {
    case 0: {
        AH_CALL(ScriptFlags_Set40)();
        AH_CALL(Msg_OpenScript)(2);
        const auto next = static_cast<unsigned char>(B(::bof3::Img(AreaW0c_TailState_addr)) + 1);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = next;
        break;
    }
    case 1:
        if (Field_Request != 2) AH_CALL(Area131_DisarmTail)();
        break;
    case 2:
        if (Field_Request != 2) B(::bof3::Img(AreaW0c_TailState_addr)) = 3;
        break;
    case 3:
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ 0x16);
        AH_CALL(Area131_DisarmTail)();
        B(::bof3::Img(AreaW3c_CampFlag_addr)) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        AH_CALL(Field_ChangeArea)(0x79, 0x1A0000, 0x350000, 3);
        break;
    default: break;
    }
}

// original 0x41D430 (called by tail kind 34 and by five of the engine's own
// tail phases, 0x456E29 .. 0x4575CE): ScriptFlags_Clear40, then the tail's
// kind, state and sub-kind 0.
extern "C" void __cdecl Area131_DisarmTail(void) {
    AH_CALL(ScriptFlags_Clear40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
}

void Area131_Inject() {
    BOF3_INJECT(Area131_ChoiceFocusStepA);
    BOF3_INJECT(Area131_ChoiceFocusStep0);
    BOF3_INJECT(Area131_CameraShiftYLess1E);
    BOF3_INJECT(Area131_CameraShiftYMore1E);
    BOF3_INJECT(Area131_Trigger15);
    BOF3_INJECT(Area131_TailLeave34);
    BOF3_INJECT(Area131_DisarmTail);
}
