// Moved here by tools/move_funcs.py from area_w1d.cpp (docs/source-layout-round.md).
#include "game/area/area_62.h"

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

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

}  // namespace

// original 0x40B5E0 (Area62_Handlers[0]; PSX 0x801F2C04): the active member's
// +0x80 bit 0 cleared; with the leader's +0x89 at 6: ScriptFlags_Set40, the
// mode tail kind 0x2A at state 0 and the active member's (read again) word
// +0x8A one on.
extern "C" void __cdecl Area62_ArmTailGiveItem(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) != 6) return;
    AH_CALL(ScriptFlags_Set40)();
    unsigned char* const member = Field_ActiveMember;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x2A;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    AddWord(member + 0x8A, 1);
}

// original 0x40B620 (Field_ModeTailKinds[0x2A], armed by
// Area62_ArmTailGiveItem): by the s8 state. 0: counter 0 = 0xA, sound 0x10D,
// state 1. 1: once counter 0 is 0xB, item (0, 0x59)'s 16-byte name record to
// Text_Records, Inventory_Add(0, 0x59, 1) - added: sound 0x106,
// Msg_OpenSystem(2), story flag 0x68; not: Msg_OpenSystem(3) - then
// Field_Request 2 and state 2. 2: once Field_Request leaves 2,
// ScriptFlags_Clear40, counter 0, the tail kind and state 0. Any other state
// returns.
extern "C" void __cdecl Area62_TailGiveItem59(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0:
        B(::bof3::Img(at::kCounter0.v)) = 0xA;
        AH_CALL(Sound_PlayEffect)(0x10D);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    case 1: {
        if (B(::bof3::Img(at::kCounter0.v)) != 0xB) return;
        const unsigned char* const name = AH_CALL(Item_NamePtr)(0, 0x59);
        for (unsigned i = 0; i < 16; i += 4) SetLong(Mem(::bof3::Img(at::kTextRecords.v) + i), Long(name + i));
        if (AH_CALL(Inventory_Add)(0, 0x59, 1) != 0) {
            AH_CALL(Sound_PlayEffect)(0x106);
            AH_CALL(Msg_OpenSystem)(2);
            AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x68);
        } else {
            AH_CALL(Msg_OpenSystem)(3);
        }
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 2;
        return;
    }
    case 2:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(::bof3::Img(at::kCounter0.v)) = 0;
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    default: return;
    }
}

void Area62_Inject() {
    BOF3_INJECT(Area62_ArmTailGiveItem);
    BOF3_INJECT(Area62_TailGiveItem59);
}
