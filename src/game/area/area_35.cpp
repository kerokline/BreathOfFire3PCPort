// Moved here by tools/move_funcs.py from area_w0c.cpp (docs/source-layout-round.md).
#include "game/area/area_35.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0c.h"
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

using move_script::Long;

using Handler = void (__cdecl*)();

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, std::uint32_t table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + index * 4u)))));
}

// Area 35's three counter handlers (0x404EA0, 0x404EE0, 0x404F20): the
// active member's +0x80 bit 0 cleared, ScriptFlags_Set40, the four counters
// (n, 0, 0, 0), MoveScript_Var7 = 4 and its step 0x14.
void SetCounters(unsigned char n) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] &= 0xFE;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(at::kCounter0.v)) = n;
    B(::bof3::Img(at::kCounter1.v)) = 0;
    B(::bof3::Img(at::kCounter2.v)) = 0;
    B(::bof3::Img(at::kCounter3.v)) = 0;
    MoveScript_Var7 = 4;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0x14;
}

}  // namespace

// originals 0x404EA0, 0x404EE0, 0x404F20 (Area35_Handlers[0..2]; PSX
// 0x801F2C04, 0x801F2C74, 0x801F2CE4): counter 0 to 1, 2, 3 (SetCounters).
extern "C" void __cdecl Area35_SetCounter1(void) { SetCounters(1); }

extern "C" void __cdecl Area35_SetCounter2(void) { SetCounters(2); }

extern "C" void __cdecl Area35_SetCounter3(void) { SetCounters(3); }

// original 0x404F60 (Area35_Handlers[3]; PSX 0x801F2D54): the active
// member's +0x80 bit 0 cleared and the running object's +0 = 0.
extern "C" void __cdecl Area35_HideObject(void) {
    Field_ActiveMember[0x80] &= 0xFE;
    Sprite_Current[0] = 0;
}

// original 0x404F80 (Area_StepHook's case for area 36, `(x, z)` answering
// in al): with Cond_ByteFD 2 and story flag 0x33, a step whose high words
// fall in x 0x46..0x48 and z 0x23..0x25 while the leader's byte +8 is 0, 6 or
// 7: counter 0 = 0, Party_DropIn(0), answer 1. Otherwise 0.
extern "C" int __cdecl Area36_StepHook(long x, long z) {
    if (Cond_ByteFD != 2) return 0;
    if (!AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x33)) return 0;
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(x) >> 16) - 0x46) >= 3) return 0;
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(z) >> 16) - 0x23) >= 3) return 0;
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    if (pose != 0 && pose != 7 && pose != 6) return 0;
    B(::bof3::Img(at::kCounter0.v)) = 0;
    AH_CALL(Party_DropIn)(0);
    return 1;
}

// original 0x404FE0 (Effect_KindHandlers entry 0xB5): Area36_EffectStates by
// Sprite_Current[1] - 0x40B4F0 (shared by seven areas), Area36_EffectStep.
extern "C" void __cdecl Area36_EffectRun(void) {
    StateEntry("Area36_EffectRun", ::bof3::Img(Area36_EffectStates_addr), at::kArea36EffectStateCount, Sprite_Current[1])();
}

// original 0x405000 (Area36_EffectStates[1]): the object's position
// dwords +0x34, +0x38, +0x3C copied to the stack and handed to 0x4220D0.
extern "C" void __cdecl Area36_EffectStep(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::EffectObject>(cur);
    std::int32_t position[3] = {tCur->x, tCur->z, tCur->height};
    AH_AT(void (__cdecl*)(std::int32_t*), at::kPositionHook.v)(position);
}

void Area35_Inject() {
    BOF3_INJECT(Area35_SetCounter1);
    BOF3_INJECT(Area35_SetCounter2);
    BOF3_INJECT(Area35_SetCounter3);
    BOF3_INJECT(Area35_HideObject);
    BOF3_INJECT(Area36_StepHook);
    BOF3_INJECT(Area36_EffectRun);
    BOF3_INJECT(Area36_EffectStep);
}
