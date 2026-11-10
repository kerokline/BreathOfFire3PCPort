// Moved here by tools/move_funcs.py from area_w1f.cpp (docs/source-layout-round.md).
#include "game/area/area_74.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1f_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = area_w1f::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

}  // namespace

// original 0x40D650 (Area74_Choices[0] = Area74_Handlers[2]): answer not 0:
// message 0x94 and the mark 6; 0: the byte 0x90409C bit 0x20 (bit 0xA5 of the
// abilities-learnt set 0x904088, no story flag) set: 0x92, else 0x93 and the mark.
extern "C" void __cdecl Area74_ChoiceAsk92(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0x94);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    if (B(::bof3::Img(AreaW1f_FlagByte9C_addr)) & 0x20) {
        SetMessage(0x92);
        return;
    }
    SetMessage(0x93);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}

// original 0x40D690 (Area74_Choices[3], [4] = Area74_Handlers[5], [6]):
// answer not 0: message 0x94 and the mark 6; 0: message 0xFFFF.
extern "C" void __cdecl Area74_ChoiceAnswer94(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0x94);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x40D6C0 (Area74_Handlers[0]; PSX 0x801F2D48): the active member's
// +0x80 bit 0 cleared; with the leader's +0x89 at 2, the running object's +9 at
// 0 and the leader's direction (+8 & 7) 5 or 1: the running object turned to
// it; unless Field_ObjectBlockedAhead(the active member): its +0x87 = 3,
// MoveCmd_Move(the script object, the direction); direction 5 toggles Cond
// row 7's flag 6 off (Flags_Clear) or sets flag 5; direction 1 sets flag 6;
// then Sound_PlayEffect(the leader's dword +0x2C + 0x100).
extern "C" void __cdecl Area74_PushToggleRow7(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) != 2) return;
    unsigned char* const cur = Sprite_Current;
    const auto dir = static_cast<unsigned char>(B(::bof3::Img(ObjTrio_addr) + 0x8) & 7);
    if (cur[9] != 0) return;
    if (dir != 5 && dir != 1) return;
    cur[8] = dir;
    if (AH_CALL(Field_ObjectBlockedAhead)(Field_ActiveMember) != 0) return;
    Field_ActiveMember[0x87] = 3;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, dir);
    if (dir == 5) {
        if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x38), 6) != 0)
            AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0x38), 6);
        else
            AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0x38), 5);
    } else {
        AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0x38), 6);
    }
    AH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Long(Mem(::bof3::Img(ObjTrio_addr) + 0x2c)) + 0x100));
}

// original 0x40D790 (Area74_Handlers[1]; PSX 0x801F2E74; also named three
// times by Area_ObjectHandler's fallback table 0x660CA0): the active member's
// +0x80 bit 0 cleared.
extern "C" void __cdecl Area74_ClearMemberBit0(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
}

// original 0x40D7A0 (Field_ObjectTriggers id 28; its arguments not read):
// ScriptFlags_Set40, tail kind 4 with sub-kind 7. al 0.
extern "C" unsigned char __cdecl Area74_Trigger28(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 7;
    return 0;
}

// original 0x40D7C0 (Area75_Choices[2] = Area75_Handlers[0]): Party_DropIn(0).
extern "C" void __cdecl Area75_DropIn0(void) { AH_CALL(Party_DropIn)(0); }

void Area74_Inject() {
    BOF3_INJECT(Area74_ChoiceAsk92);
    BOF3_INJECT(Area74_ChoiceAnswer94);
    BOF3_INJECT(Area74_PushToggleRow7);
    BOF3_INJECT(Area74_ClearMemberBit0);
    BOF3_INJECT(Area74_Trigger28);
    BOF3_INJECT(Area75_DropIn0);
}
