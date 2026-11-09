// Moved here by tools/move_funcs.py from area_w1d.cpp (docs/source-layout-round.md).
#include "game/area/area_59.h"

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

using move_script::SetWord;

using Handler = void (__cdecl*)();

unsigned char& B(std::uint32_t address) { return *Mem(address); }

std::uint32_t Key(const void* p) { return Low32(p); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, std::uint32_t table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + index * 4u)))));
}

// The message box's answer, and the message word a choice leaves.
unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// A message and the mark 0x9398CF = 6 beside it (areas 55, 59, 61).
void MessageMarked(unsigned id) {
    SetMessage(id);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}

// The pointer 0x903804 (a field object), read afresh at each use.
unsigned char* Focus() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(AreaW1a_FocusObject_addr))))));
}

// Area 59's handlers 2 and 3 (0x40B360, 0x40B3B0): cells x 0x47..0x49 of row
// 8 set to `row8`, then of row 9 to `row9`.
void SetCells59(unsigned row8, unsigned row9) {
    for (unsigned x = 0x47; x <= 0x49; ++x) AH_CALL(AreaMap_SetByte)(x, 8, row8);
    for (unsigned x = 0x47; x <= 0x49; ++x) AH_CALL(AreaMap_SetByte)(x, 9, row9);
}

}  // namespace

// original 0x40B330 (Area59_Choices[0]): answer 0: message 2; else message 4
// and the mark.
extern "C" void __cdecl Area59_ChoiceMessage2or4(void) {
    if (Answer() == 0) {
        SetMessage(2);
        return;
    }
    MessageMarked(4);
}

// original 0x40B360 (Area59_Handlers[2]; PSX 0x801F2D90): cells x 0x47..0x49
// of rows 8 and 9 cleared.
extern "C" void __cdecl Area59_ClearCells(void) { SetCells59(0, 0); }

// original 0x40B3B0 (Area59_Handlers[3]; PSX 0x801F2E08): the same cells, row
// 8 0xC0 and row 9 0xA1.
extern "C" void __cdecl Area59_SetCells(void) { SetCells59(0xC0, 0xA1); }

// original 0x40B410 (Area_StepHook's case for area 59, `(x, z)` answering in
// al): with Cond_ByteFD 2 and story flag 0x33, a step whose high words fall
// in x 0x46..0x48 and z 0x29..0x2B while the leader's byte +8 is 0, 7 or 6:
// counter 0 = 0, Party_DropIn(0), answer 1. Otherwise 0. Area 36's hook with
// z 0x29 for 0x23.
extern "C" int __cdecl Area59_StepHook(long x, long z) {
    if (Cond_ByteFD != 2) return 0;
    if (!AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x33)) return 0;
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(x) >> 16) - 0x46) >= 3) return 0;
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(z) >> 16) - 0x29) >= 3) return 0;
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    if (pose != 0 && pose != 7 && pose != 6) return 0;
    B(::bof3::Img(at::kCounter0.v)) = 0;
    AH_CALL(Party_DropIn)(0);
    return 1;
}

// original 0x40B470 (Field_ObjectTriggers id 24, (object, flags) ignored):
// ScriptFlags_Set40; the focus object's +1 = 4, +0x83 = 0, word +0x8A = 0
// (the pointer read afresh each time); the mode tail kind 4 with sub-kind 5
// and counter 3 the focus object's index among Sprite_Objects (its offset
// divided by 0xA4, signed, truncated; the low byte). Answers 0 in al.
extern "C" unsigned char __cdecl Area59_Trigger24(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    Focus()[1] = 4;
    Focus()[0x83] = 0;
    SetWord(Focus() + 0x8A, 0);
    const auto offset = static_cast<std::int32_t>(Key(Focus()) - ::bof3::Img(Sprite_Objects_addr));
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 5;
    B(::bof3::Img(at::kCounter3.v)) = static_cast<unsigned char>(offset / static_cast<std::int32_t>(at::kObjectStride));
    return 0;
}

// original 0x40B4D0 (Effect_KindHandlers entry 0xB6, the dword 0x655628; in
// area 59's block by address): Area59_EffectStates by Sprite_Current[1] -
// Area59_EffectGround, Area59_EffectStep; ours aborts past 2.
extern "C" void __cdecl Area59_EffectRun(void) {
    StateEntry("Area59_EffectRun", ::bof3::Img(Area59_EffectStates_addr), at::kArea59EffectStateCount, Sprite_Current[1])();
}

// original 0x40B4F0 (Area59_EffectStates[0]; also the first state of areas 36,
// 100, 112, 116, 143 and 146's effect tables): the object's +0x3C the ground
// at its x, z (a signed word) << 16, its state +1 one on (Sprite_Current read
// again after the call).
extern "C" void __cdecl Area59_EffectGround(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::EffectObject>(cur);
    const auto ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(tCur->x, tCur->z));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(ground)) << 16));
    ++Sprite_Current[1];
}

// original 0x40B520 (Area59_EffectStates[1]): the object's position dwords
// +0x34, +0x38, +0x3C copied to the stack and handed to 0x4220D0 (area 36's
// Area36_EffectStep instruction for instruction).
extern "C" void __cdecl Area59_EffectStep(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::EffectObject>(cur);
    std::int32_t position[4] = {tCur->x, tCur->z, tCur->height, 0};
    AH_AT(void (__cdecl*)(std::int32_t*), at::kPositionHook.v)(position);
}

void Area59_Inject() {
    BOF3_INJECT(Area59_ChoiceMessage2or4);
    BOF3_INJECT(Area59_ClearCells);
    BOF3_INJECT(Area59_SetCells);
    BOF3_INJECT(Area59_StepHook);
    BOF3_INJECT(Area59_Trigger24);
    BOF3_INJECT(Area59_EffectRun);
    BOF3_INJECT(Area59_EffectGround);
    BOF3_INJECT(Area59_EffectStep);
}
