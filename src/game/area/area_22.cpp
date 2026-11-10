// Moved here by tools/move_funcs.py from area_w0b.cpp (docs/source-layout-round.md).
#include "game/area/area_22.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0b_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0b::at;

}  // namespace

namespace {

using move_script::At;

using move_script::SetWord;

void SetMessage(unsigned id) { SetWord(At(::bof3::Img(at::kMessage.v)), id); }

signed char Choice() { return AreaW0c_ChoiceAnswer; }

// Areas 22's and 26's choice pattern: message 0xFFFF (none), then the script
// variable 0x903848 = `yes` for choice 0, `no` for choice 1, else unchanged.
void ChoiceSetVar(unsigned char yes, unsigned char no) {
    const signed char choice = Choice();
    SetMessage(0xFFFF);
    if (choice == 0) At(::bof3::Img(at::kScriptVar.v))[0] = yes;
    else if (choice == 1) At(::bof3::Img(at::kScriptVar.v))[0] = no;
}

}  // namespace

// original 0x402F90 (area 22's choice 0, the descriptor 0x5E7DE8's +0x34):
// no message; the variable 0x1E (choice 0) or 0x32 (1).
extern "C" void __cdecl Area22_ChoiceA(void) { ChoiceSetVar(0x1E, 0x32); }

// original 0x402FC0 (area 22's choice 1): 0x3C or 0x46.
extern "C" void __cdecl Area22_ChoiceB(void) { ChoiceSetVar(0x3C, 0x46); }

// original 0x402FF0 (area 22's choice 2): 0xB or 0x14.
extern "C" void __cdecl Area22_ChoiceC(void) { ChoiceSetVar(0xB, 0x14); }

// original 0x403020 (area 22's handler 0; PSX 0x801F2CC4): cells (3, 7), (3,
// 8), (4, 7), (4, 8) to 0.
extern "C" void __cdecl Area22_ClearCells(void) {
    AH_CALL(AreaMap_SetByte)(3, 7, 0);
    AH_CALL(AreaMap_SetByte)(3, 8, 0);
    AH_CALL(AreaMap_SetByte)(4, 7, 0);
    AH_CALL(AreaMap_SetByte)(4, 8, 0);
}

// original 0x403050 (one body for nine areas' choice or handler entries: 1,
// 23, 32, 40, 41, 108, 117 (twice), 141, 143; it lies in area 22's block):
// message 0xFFFF; choice 0 (the byte read before that store) arms the tail
// kind 0xA - 0x9039F3 = 0xA, 0x9039F4 = 5, 0x9039F5 = 0xFF.
extern "C" void __cdecl Area22_ArmTailOnYes(void) {
    const unsigned char choice = static_cast<unsigned char>(AreaW0c_ChoiceAnswer);
    SetMessage(0xFFFF);
    if (choice != 0) return;
    AreaW0a_TailKind = 0xA;
    AreaW0c_TailState = 5;
    AreaW0c_TailSub = 0xFF;
}

void Area22_Inject() {
    BOF3_INJECT(Area22_ChoiceA);
    BOF3_INJECT(Area22_ChoiceB);
    BOF3_INJECT(Area22_ChoiceC);
    BOF3_INJECT(Area22_ClearCells);
    BOF3_INJECT(Area22_ArmTailOnYes);
}
