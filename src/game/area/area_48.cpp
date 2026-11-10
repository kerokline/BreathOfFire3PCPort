// Moved here by tools/move_funcs.py from area_w1c.cpp (docs/source-layout-round.md).
#include "game/area/area_48.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

std::uint16_t High(std::int32_t v) { return static_cast<std::uint16_t>(static_cast<std::uint32_t>(v) >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The four callees nobody owns, by their raw addresses (area_w1c_callees.h).
void FlagsToggle(unsigned char* bits, unsigned index) {
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), at::kFlagsToggle.v)(bits, index);
}

// Area 48's handlers 2 and 3 (0x4090C0, 0x409170): the running object's
// direction +8 = `dir`; the script object's +7 = the count << 2 (a byte); if
// that is not 0, by MoveScript_ObjectKind: 2 MoveCmd_MoveKind2(+8),
// MoveScript_FAWord 0 and the script object's bit 0x40; else
// MoveCmd_Move(the script object, +8) and its bit 0x80. Then the running
// object's +0x14 = 0 and word +0x3E = 0x480. Sprite_Current and
// MoveScript_Object read again after every call.
void MoveScriptObject(unsigned char dir) {
    Sprite_Current[8] = dir;
    MoveScript_Object[7] = static_cast<unsigned char>(B(::bof3::Img(AreaW1c_Count48_addr)) << 2);
    if (MoveScript_Object[7] != 0) {
        if (AH_CALL(MoveScript_ObjectKind)() == 2) {
            AH_CALL(MoveCmd_MoveKind2)(Sprite_Current[8]);
            unsigned char* const object = MoveScript_Object;
            MoveScript_FAWord = 0;
            object[0] = static_cast<unsigned char>(object[0] | 0x40);
        } else {
            const unsigned char* const cur = Sprite_Current;
            AH_CALL(MoveCmd_Move)(MoveScript_Object, cur[8]);
            MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x80);
        }
    }
    rv::Current<records::SpriteObject>()->step_y = 0;
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x480);
}

// The count 0x92BEE7 as area 48 reads it: 2, 6, 8, or any other.
unsigned char ByCount(unsigned char two, unsigned char six, unsigned char eight, unsigned char other) {
    switch (B(::bof3::Img(AreaW1c_Count48_addr))) {
    case 2: return two;
    case 6: return six;
    case 8: return eight;
    default: return other;
    }
}

// The tail kind 3 with sub-kind `sub` and ScriptFlags_Set40 (area 48's arrive hook).
void ArmTail3(unsigned char sub) {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 3;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = sub;
    AH_CALL(ScriptFlags_Set40)();
}

}  // namespace

// original 0x408FF0 (Area48_Choices[0]): message 0xFFFF; an answer with bit 0
// clear: counter 0 = 0xB and Sound_PlayEffect(0x205); set: counter 0 = 1,
// ScriptFlags_Clear40, MoveScript_Var7 and its step 0.
extern "C" void __cdecl Area48_ChoiceCounterBOr1(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if ((answer & 1) == 0) {
        B(::bof3::Img(at::kCounter0.v)) = 0xB;
        AH_CALL(Sound_PlayEffect)(0x205);
        return;
    }
    B(::bof3::Img(at::kCounter0.v)) = 1;
    AH_CALL(ScriptFlags_Clear40)();
    MoveScript_Var7 = 0;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0;
}

// original 0x409040 (Area48_Choices[1] = Area48_Handlers[0]; PSX 0x801F4804):
// the active member's +0x80 bit 0 cleared; with Character_IndexById[the
// leader's +0x89] at 1 (the index unchecked), story flag 0xD toggled
// (0x57C160).
extern "C" void __cdecl Area48_ToggleFlagD(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    if (B(::bof3::Img(Character_IndexById_addr) + B(::bof3::Img(ObjTrio_addr) + 0x89)) != 1) return;
    FlagsToggle(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xD);
}

// original 0x409080 (Area48_Choices[2] = Area48_Handlers[1]; PSX 0x801F4864):
// as Area48_ToggleFlagD's test; then the count 0x92BEE7 up by one below 8,
// and Sound_PlayEffect(0x20D).
extern "C" void __cdecl Area48_BumpCount(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    if (B(::bof3::Img(Character_IndexById_addr) + B(::bof3::Img(ObjTrio_addr) + 0x89)) != 1) return;
    const unsigned char count = B(::bof3::Img(AreaW1c_Count48_addr));
    if (count < 8) B(::bof3::Img(AreaW1c_Count48_addr)) = static_cast<unsigned char>(count + 1);
    AH_CALL(Sound_PlayEffect)(0x20D);
}

// original 0x4090C0 (Area48_Choices[3] = Area48_Handlers[2]; PSX 0x801F48DC):
// MoveScriptObject with direction 5.
extern "C" void __cdecl Area48_MoveScriptObject5(void) { MoveScriptObject(5); }

// original 0x409170 (Area48_Choices[4] = Area48_Handlers[3]; PSX 0x801F49C8):
// MoveScriptObject with direction 1.
extern "C" void __cdecl Area48_MoveScriptObject1(void) { MoveScriptObject(1); }

// original 0x409220 (Area48_Choices[5] = Area48_Handlers[4]; PSX 0x801F4AB4):
// the active member's +0x83 by the count - 2: 6, 6: 7, 8: 8, else 9 - and its
// word +0x8A = 0xFFFE.
extern "C" void __cdecl Area48_PoseByCount(void) {
    Field_ActiveMember[0x83] = ByCount(6, 7, 8, 9);
    rv::As<records::SpriteObject>(Field_ActiveMember)->script_position = 0xFFFE;
}

// original 0x4092B0 (Area48_Choices[6] = Area48_Handlers[5]; PSX 0x801F4B40):
// the count 0 and the byte 0x803490's bit 0 set.
extern "C" void __cdecl Area48_ResetCount(void) {
    const unsigned char byte = B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x10);
    B(::bof3::Img(AreaW1c_Count48_addr)) = 0;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x10) = static_cast<unsigned char>(byte | 1);
}

// original 0x4092D0 (Field_ModeTailKinds[3], armed by Area48_ArriveHook): by
// the sub-kind 0x9039F5 - 1: Party_DropIn by the count (2: 6, 6: 7, 8: 1,
// else 8); 0: Party_DropIn(0), then the sub-kind read again; 2 (either way):
// Party_DropIn(2). Then ScriptFlags_Clear40 and the kind, state and sub-kind
// 0.
extern "C" void __cdecl Area48_TailDropIn(void) {
    unsigned char sub = B(::bof3::Img(AreaW0c_TailSub_addr));
    if (sub == 1) {
        AH_CALL(Party_DropIn)(ByCount(6, 7, 1, 8));
    } else {
        if (sub == 0) {
            AH_CALL(Party_DropIn)(0);
            sub = B(::bof3::Img(AreaW0c_TailSub_addr));
        }
        if (sub == 2) AH_CALL(Party_DropIn)(2);
    }
    AH_CALL(ScriptFlags_Clear40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
}

// original 0x409340 (Area_ArriveHook's case for area 48, `(x, z)` answering
// in al): three doorways, each arming the tail kind 3 (Area48_TailDropIn)
// with ScriptFlags_Set40 - sub-kind 0 for x's high word 0x17 and z's
// 0xD..0xE; 1 for z's high word 6 or z exactly 0x58000, x's 0xC..0xD; 2 for
// z's high word 0xB or z 0xA8000, x's 0x13..0x14. The words compared as
// 16-bit; every test is made. Answers 0.
extern "C" int __cdecl Area48_ArriveHook(long x, long z) {
    const std::uint16_t xh = High(x), zh = High(z);
    if (xh == 0x17 && static_cast<std::uint16_t>(zh - 0xD) < 2) ArmTail3(0);
    if ((zh == 6 || z == 0x58000) && static_cast<std::uint16_t>(xh - 0xC) < 2) ArmTail3(1);
    if ((zh == 0xB || z == 0xA8000) && static_cast<std::uint16_t>(xh - 0x13) < 2) ArmTail3(2);
    return 0;
}

// original 0x4093D0 (area 48's init, the descriptor's +0x40; PSX
// 0x801F4D2C): with Cond_ByteFD 5, Sound_PlayEffect(0x20C).
extern "C" void __cdecl Area48_InitSound(void) {
    if (Cond_ByteFD == 5) AH_CALL(Sound_PlayEffect)(0x20C);
}

void Area48_Inject() {
    BOF3_INJECT(Area48_ChoiceCounterBOr1);
    BOF3_INJECT(Area48_ToggleFlagD);
    BOF3_INJECT(Area48_BumpCount);
    BOF3_INJECT(Area48_MoveScriptObject5);
    BOF3_INJECT(Area48_MoveScriptObject1);
    BOF3_INJECT(Area48_PoseByCount);
    BOF3_INJECT(Area48_ResetCount);
    BOF3_INJECT(Area48_TailDropIn);
    BOF3_INJECT(Area48_ArriveHook);
    BOF3_INJECT(Area48_InitSound);
}
