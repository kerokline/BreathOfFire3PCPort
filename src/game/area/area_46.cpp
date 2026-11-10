// Moved here by tools/move_funcs.py from area_w1b.cpp (docs/source-layout-round.md).
#include "game/area/area_46.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char& B(U address) { return move_script::At(address)[0]; }

signed char TailState() { return AreaW0c_TailState; }

}  // namespace

// original 0x408E10 (Field_ModeTailKinds slot 11, armed by Area46_StepHook):
// a switch on the s8 state (a byte table and a jump table of five inside the
// function). 0: state 1. 1: once the byte 0x903849 is 2, ScriptFlags_Clear40,
// 0x903849 = 0, the tail ended. 10: Party_DropIn(1), state 11. 11: once
// 0x903849 is 0, ScriptFlags_Clear40 and the tail ended. Any other: nothing.
extern "C" void __cdecl Area46_DropTail(void) {
    switch (TailState()) {
    case 0:
        AreaW0c_TailState = 1;
        return;
    case 1:
        if (B(::bof3::Img(at::kDrop46.v)) != 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(::bof3::Img(at::kDrop46.v)) = 0;
        AreaW0a_TailKind = 0;
        AreaW0c_TailState = 0;
        return;
    case 0xA:
        AH_CALL(Party_DropIn)(1);
        AreaW0c_TailState = 0xB;
        return;
    case 0xB:
        if (B(::bof3::Img(at::kDrop46.v)) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        AreaW0a_TailKind = 0;
        AreaW0c_TailState = 0;
        return;
    default:
        return;
    }
}

// original 0x408EB0 (Area_StepHook's handler for area 46): (x, z) 16.16
// positions. z at most 0x218000 (signed), x's high word 0xE..0x10, and key
// item 4 not held (KeyItem_Has): ScriptFlags_Set40, 0x903849 = 1, the mode
// tail armed - kind 11, state 10 when the s8 Cond_ByteFA is 8 or more, else
// 0; al 1. Otherwise al 0.
extern "C" unsigned char __cdecl Area46_StepHook(unsigned x, unsigned z) {
    if (static_cast<std::int32_t>(z) > 0x218000) return 0;
    if (static_cast<unsigned short>((x >> 16) - 0xE) >= 3) return 0;
    if (AH_CALL(KeyItem_Has)(4) != 0) return 0;
    AH_CALL(ScriptFlags_Set40)();
    const signed char chapter = Cond_ByteFA;
    B(::bof3::Img(at::kDrop46.v)) = 1;
    AreaW0a_TailKind = 0xB;
    AreaW0c_TailState = chapter >= 8 ? 0xA : 0;
    return 1;
}

// original 0x408F10 (handler 0 of areas 7, 14 and 46: one body, in area 46's
// block): Kind2_Place(0).
extern "C" void __cdecl Area46_PlaceKind2At0(void) { AH_CALL(Kind2_Place)(0); }

// original 0x408F20 (Field_ObjectTriggers id 54, called (object, 0x904030)
// and reading neither; a gap function by the tool, placed in area 46's block
// by address): story flag 0x70 set; when flags 0x6D..0x70 are all set, flag
// 0x71 too. al 0.
extern "C" unsigned char __cdecl Area46_Trigger54(void) {
    AH_CALL(Flags_Set)(Bank(), 0x70);
    for (unsigned flag = 0x6D; flag <= 0x70; ++flag)
        if (AH_CALL(Flags_Test)(Bank(), flag) == 0) return 0;
    AH_CALL(Flags_Set)(Bank(), 0x71);
    return 0;
}

void Area46_Inject() {
    BOF3_INJECT(Area46_DropTail);
    BOF3_INJECT(Area46_StepHook);
    BOF3_INJECT(Area46_PlaceKind2At0);
    BOF3_INJECT(Area46_Trigger54);
}
