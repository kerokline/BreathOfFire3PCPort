// Moved here by tools/move_funcs.py from area_w1d.cpp (docs/source-layout-round.md).
#include "game/area/area_55.h"

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

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// The message box's answer, and the message word a choice leaves.
unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// A message and the mark 0x9398CF = 6 beside it (areas 55, 59, 61).
void MessageMarked(unsigned id) {
    SetMessage(id);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}

// The byte a table indexed by the s8 answer holds (`movsx eax, answer`).
std::uint32_t BySignedAnswer(std::uint32_t table, unsigned stride, unsigned offset) {
    const auto answer = static_cast<signed char>(Answer());
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(table) + answer * static_cast<int>(stride) + static_cast<int>(offset));
}

// The pointer 0x903804 (a field object), read afresh at each use.
unsigned char* Focus() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(AreaW1a_FocusObject_addr))))));
}

// Areas 55, 61's object triggers 23 / 21 and area 59's 24 begin so:
// ScriptFlags_Set40, the mode tail kind 4.
void Tail4(unsigned char sub) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = sub;
}

}  // namespace

// original 0x40AB00 (Area53_Choices[0]): message 0xFFFF; the focus object's
// dwords +0x18 and +0x1C = Area53_ChoicePairs[the s8 answer] bytes 0 and 1
// (the pointer and the answer read again for the second).
extern "C" void __cdecl Area53_ChoiceFocusPair(void) {
    unsigned char* focus = Focus();
    const unsigned char first = B(BySignedAnswer(::bof3::Img(Area53_ChoicePairs_addr), 2, 0));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, first);
    focus = Focus();
    SetLong(focus + 0x1C, B(BySignedAnswer(::bof3::Img(Area53_ChoicePairs_addr), 2, 1)));
}

// original 0x40AB40 (Field_ObjectTriggers id 42, (object, flags) ignored; in
// area 53's block by address): ScriptFlags_Set40, the mode tail kind 0x2C
// with state 0 and sub-kind 0xA. No answer of its own: eax is what
// ScriptFlags_Set40 left (Capcom's: Field_StatusBits | 0x40 in al), passed on.
extern "C" unsigned __cdecl Area53_Trigger42(unsigned char*, unsigned char*) {
    const unsigned left = area_harness::Call(reinterpret_cast<unsigned (__cdecl*)()>(&ScriptFlags_Set40))();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x2C;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0xA;
    return left;
}

// original 0x40AB60 (Area55_Choices[0]): answer 0 - with item (0, 8) held
// (Inventory_Count's word not 0) one taken (Inventory_Remove(0, 8, 1)) and
// message 0x22, else message 0x23 and the mark; any other answer message 0x24
// and the mark.
extern "C" void __cdecl Area55_ChoiceTakeItem8(void) {
    if (Answer() != 0) {
        MessageMarked(0x24);
        return;
    }
    if (static_cast<std::uint16_t>(AH_CALL(Inventory_Count)(0, 8, 0)) == 0) {
        MessageMarked(0x23);
        return;
    }
    AH_CALL(Inventory_Remove)(0, 8, 1);
    SetMessage(0x22);
}

// original 0x40ABC0 (Area55_Choices[3] and [4]): an answer: message 0x24 and
// the mark; 0: message 0xFFFF.
extern "C" void __cdecl Area55_ChoiceMark24(void) {
    if (Answer() != 0) {
        MessageMarked(0x24);
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x40ABF0 (Field_ObjectTriggers id 23, (object, flags) ignored):
// the mode tail kind 4 with sub-kind 2. Answers 0 in al.
extern "C" unsigned char __cdecl Area55_Trigger23(unsigned char*, unsigned char*) {
    Tail4(2);
    return 0;
}

void Area55_Inject() {
    BOF3_INJECT(Area53_ChoiceFocusPair);
    BOF3_INJECT(Area53_Trigger42);
    BOF3_INJECT(Area55_ChoiceTakeItem8);
    BOF3_INJECT(Area55_ChoiceMark24);
    BOF3_INJECT(Area55_Trigger23);
}
