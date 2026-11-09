// Moved here by tools/move_funcs.py from area_w4b.cpp (docs/source-layout-round.md).
#include "game/area/area_168.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4b.h"
#include "game/area_w4b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4b::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* Story() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

// The tail armed: ScriptFlags_Set40, then kind and state.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// The tail ended: kind and state 0.
void EndTail() {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// A choice that arms a tail on answer 0 (areas 168, 170): the answer read,
// the message 0xFFFF, then for 0 ScriptFlags_Set40 and the tail.
void ChoiceArm(unsigned char kind, unsigned char state) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) ArmTail(kind, state);
}

// Engine tail kind 10 armed at state 5 with sub-kind 0xFF (area 168's choice
// 1 on answer 3, area 171's choice 0 on answer 0).
void ArmTail10() {
    ArmTail(0xA, 5);
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0xFF;
}

// Area 168's choices 0 / 3 and 4: answer 0 asks for key item `item` - held,
// message 6 and `bit` set in Cond_ByteFE (read before the message store);
// not held, message 7. Any other answer: message 0xFFFF.
void ChoiceKeyItem(unsigned item, unsigned char bit) {
    if (Answer() != 0) {
        SetMessage(0xFFFF);
        return;
    }
    if (AH_CALL(KeyItem_Has)(item) != 0) {
        const unsigned char fe = Cond_ByteFE;
        SetMessage(6);
        Cond_ByteFE = static_cast<unsigned char>(fe | bit);
        return;
    }
    SetMessage(7);
}

}  // namespace

// original 0x426560 (area 168 +0x34[1]): the answer read, message 0xFFFF;
// answer 3 arms engine tail kind 10 (state 5, sub-kind 0xFF). Then the focus
// object's dwords +0x18 and +0x1C = the two bytes of Area168_FocusPairs by
// the answer (read again, signed; not checked - a byte answer stays in
// .data), each with the focus pointer read again.
extern "C" void __cdecl Area168_ChoiceFocusPair(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 3) ArmTail10();
    U index = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(Answer())));
    unsigned char* focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    SetLong(focus + 0x18, B(::bof3::Img(Area168_FocusPairs_addr) + index * 2));
    focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    index = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(Answer())));
    SetLong(focus + 0x1C, B(::bof3::Img(Area168_FocusPairs_addr) + 1 + index * 2));
}

// original 0x4265C0 (area 168 +0x34[0] and +0x34[3]): key item 0xC, bit 1.
extern "C" void __cdecl Area168_ChoiceKeyItemC(void) { ChoiceKeyItem(0xC, 1); }

// original 0x426610 (area 168 +0x34[4]): key item 0xD, bit 2.
extern "C" void __cdecl Area168_ChoiceKeyItemD(void) { ChoiceKeyItem(0xD, 2); }

// original 0x426660 (area 168 +0x34[5]): message 0xFFFF; answer 0: story flag
// 0x8F, then tail kind 59 armed at state 0.
extern "C" void __cdecl Area168_ChoiceFlag8FArm59(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(Flags_Set)(Story(), 0x8F);
    ArmTail(0x3B, 0);
}

// original 0x4266A0 (area 168 +0x34[6]): message 0xFFFF; answer 0: tail kind
// 59 armed at state 10.
extern "C" void __cdecl Area168_ChoiceArm59At10(void) { ChoiceArm(0x3B, 0xA); }

// original 0x4266D0 (area 168 +0x34[7]): message 0x12 for answer 0, 0x13 for
// 1, 0x14 for any other.
extern "C" void __cdecl Area168_ChoiceMessage12(void) {
    const unsigned char answer = Answer();
    if (answer == 0) {
        SetMessage(0x12);
        return;
    }
    SetMessage(0x13u + (answer != 1 ? 1u : 0u));
}

// original 0x426700 (tail kind 59): by the signed state - 0: once
// Field_Request is not 2, state 1; 1: Field_ChangeArea(0xA7, 0xB0000,
// 0x698000, 0x89), the tail ended; 10: Kind2_Place(0), state 11; 11: once
// counter 0 is 1, sound 0x204, story flag 0x91, state 12; 12: once counter 0
// is 2, ScriptFlags_Clear40, counter 0 and the tail cleared. Any other state
// (2..9, above 12, negative): nothing.
extern "C" void __cdecl Area168_Tail59(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0:
        if (Field_Request != 2) B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    case 1:
        AH_CALL(Field_ChangeArea)(0xA7, 0xB0000, 0x698000, 0x89);
        EndTail();
        return;
    case 10:
        AH_CALL(Kind2_Place)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xB;
        return;
    case 11:
        if (B(::bof3::Img(at::kCounter0.v)) != 1) return;
        AH_CALL(Sound_PlayEffect)(0x204);
        AH_CALL(Flags_Set)(Story(), 0x91);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xC;
        return;
    case 12:
        if (B(::bof3::Img(at::kCounter0.v)) != 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(::bof3::Img(at::kCounter0.v)) = 0;
        EndTail();
        return;
    default: return;
    }
}

void Area168_Inject() {
    BOF3_INJECT(Area168_ChoiceFocusPair);
    BOF3_INJECT(Area168_ChoiceKeyItemC);
    BOF3_INJECT(Area168_ChoiceKeyItemD);
    BOF3_INJECT(Area168_ChoiceFlag8FArm59);
    BOF3_INJECT(Area168_ChoiceArm59At10);
    BOF3_INJECT(Area168_ChoiceMessage12);
    BOF3_INJECT(Area168_Tail59);
}
