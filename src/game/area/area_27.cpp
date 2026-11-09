// Moved here by tools/move_funcs.py from area_w0c.cpp (docs/source-layout-round.md).
#include "game/area/area_27.h"

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

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

// Choice handlers read the message box's answer; most set the message word
// to 0xFFFF (no new message) first.
signed char Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// Area 27's two mode-tail phases (0x403570 kind 1, 0x403660 kind 2): one
// machine by the s8 state 0x9039F4, 0..4, differing only in the story flag,
// Party_DropIn's entry, the counter value state 2 waits for, and the
// destination's x and flags.
struct DropInTail {
    unsigned flag, entry;
    unsigned char wait;
    std::int32_t x;
    unsigned flags;
};

void RunDropInTail(const DropInTail& t) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Set40)();
        AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), t.flag);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    case 1:
        AH_CALL(Party_DropIn)(t.entry);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 2;
        return;
    case 2:
        if (B(::bof3::Img(at::kCounter3.v)) != t.wait) return;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x10);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 3;
        return;
    case 3: {
        const auto left = static_cast<std::uint16_t>(Word(Mem(::bof3::Img(at::kTailTimer.v))) - 1);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), left);
        if (left != 0) return;
        AH_CALL(Field_ChangeArea)(0x1B, t.x, 0x380000, t.flags);
        const auto counter = static_cast<unsigned char>(B(::bof3::Img(at::kCounter3.v)) + 1);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 4;
        B(::bof3::Img(at::kCounter3.v)) = counter;
        return;
    }
    case 4:
        if (B(::bof3::Img(at::kCounter3.v)) != 0x31) return;
        B(::bof3::Img(at::kCounter3.v)) = 0;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), t.flag);
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    default: return;   // above 4 or negative: `ja` to the ret
    }
}

}  // namespace

// original 0x403400 (Area27_Choices[0]; PSX 0x801F35E0): message 0xFFFF; an answer with bit
// 0 clear arms the mode tail kind 1 (Area27_TailDropIn1) at state 0.
extern "C" void __cdecl Area27_ChoiceTail1(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer & 1) return;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 1;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// original 0x403430 (Area27_Choices[1]; PSX 0x801F3618): the same for tail kind 2.
extern "C" void __cdecl Area27_ChoiceTail2(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer & 1) return;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 2;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// original 0x403460 (Area27_Choices[2]; PSX 0x801F3650): message 0xFFFF; answer 0 sets
// counter 0 to 5, answer 1 to 6, any other nothing.
extern "C" void __cdecl Area27_ChoiceCounter5or6(void) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) B(::bof3::Img(at::kCounter0.v)) = 5;
    else if (answer == 1) B(::bof3::Img(at::kCounter0.v)) = 6;
}

// original 0x403490 (Area27_Choices[3]; PSX 0x801F3690): answer 0: 0x5B, 1: 0x5A.
extern "C" void __cdecl Area27_ChoiceCounter5Bor5A(void) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) B(::bof3::Img(at::kCounter0.v)) = 0x5B;
    else if (answer == 1) B(::bof3::Img(at::kCounter0.v)) = 0x5A;
}

// original 0x4034C0 (Area27_Choices[4]): answer 0: 0x5C, 1: 0x5D.
extern "C" void __cdecl Area27_ChoiceCounter5Cor5D(void) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) B(::bof3::Img(at::kCounter0.v)) = 0x5C;
    else if (answer == 1) B(::bof3::Img(at::kCounter0.v)) = 0x5D;
}

// original 0x4034F0 (Area27_Choices[5] = Area27_Handlers[0]; PSX
// 0x801F3710): a free effect slot (Effect_FindFree; 0xFF none) gets +0 = 1
// and the kind +5 = 0x2F.
extern "C" void __cdecl Area27_SpawnEffect2F(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x2F;
}

// original 0x403520 (Area27_Choices[6] = Area27_Handlers[1]; PSX
// 0x801F376C): six map cells zeroed, x 0x27 and 0x28 of rows 0x25..0x27.
extern "C" void __cdecl Area27_ClearCells(void) {
    AH_CALL(AreaMap_SetByte)(0x27, 0x25, 0);
    AH_CALL(AreaMap_SetByte)(0x27, 0x26, 0);
    AH_CALL(AreaMap_SetByte)(0x27, 0x27, 0);
    AH_CALL(AreaMap_SetByte)(0x28, 0x25, 0);
    AH_CALL(AreaMap_SetByte)(0x28, 0x26, 0);
    AH_CALL(AreaMap_SetByte)(0x28, 0x27, 0);
}

// original 0x403570 (Field_ModeTailKinds[1], armed by Area27_ChoiceTail1's
// register store): state 0 waits for Field_Request to leave 2, then
// ScriptFlags_Set40 and story flag 1; 1 Party_DropIn(1); 2 waits for counter
// 3 to reach 0x20, then a 0x10-frame timer; 3 counts it down, then
// Field_ChangeArea(0x1B, 0x4B0000, 0x380000, 0x8F) and counter 3 + 1; 4
// waits for counter 3 to reach 0x31, then clears it, ScriptFlags_Clear40,
// story flag 1 cleared, the tail disarmed. A 5-entry jump table in the body.
extern "C" void __cdecl Area27_TailDropIn1(void) { RunDropInTail({1, 1, 0x20, 0x4B0000, 0x8F}); }

// original 0x403660 (Field_ModeTailKinds[2], armed by Area27_ChoiceTail2):
// the same machine with story flag 0, Party_DropIn(2), counter 3 at 0x10, and
// Field_ChangeArea(0x1B, 0x260000, 0x380000, 0x90).
extern "C" void __cdecl Area27_TailDropIn2(void) { RunDropInTail({0, 2, 0x10, 0x260000, 0x90}); }

void Area27_Inject() {
    BOF3_INJECT(Area27_ChoiceTail1);
    BOF3_INJECT(Area27_ChoiceTail2);
    BOF3_INJECT(Area27_ChoiceCounter5or6);
    BOF3_INJECT(Area27_ChoiceCounter5Bor5A);
    BOF3_INJECT(Area27_ChoiceCounter5Cor5D);
    BOF3_INJECT(Area27_SpawnEffect2F);
    BOF3_INJECT(Area27_ClearCells);
    BOF3_INJECT(Area27_TailDropIn1);
    BOF3_INJECT(Area27_TailDropIn2);
}
