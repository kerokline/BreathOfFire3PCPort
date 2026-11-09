// Moved here by tools/move_funcs.py from area_w0b.cpp (docs/source-layout-round.md).
#include "game/area/area_21.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0b.h"
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

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char* Pointer(U cell) { return At(static_cast<U>(Long(At(cell)))); }

void SetMessage(unsigned id) { SetWord(At(::bof3::Img(at::kMessage.v)), id); }

signed char Choice() { return AreaW0c_ChoiceAnswer; }

}  // namespace

// original 0x402E80 (area 21's choice 1, the descriptor 0x5E7280's +0x34):
// choice not 0: message 0x18. Else the money 0x904058 below 0x14: message
// 0x17; else the money take (0x591BC0) of 0x14 and message 0x11.
extern "C" void __cdecl Area21_ChoicePay(void) {
    if (static_cast<unsigned char>(AreaW0c_ChoiceAnswer) != 0) {
        SetMessage(0x18);
        return;
    }
    if (Party_Zenny < 0x14) {
        SetMessage(0x17);
        return;
    }
    AH_AT(unsigned char (__cdecl*)(unsigned, unsigned), area_w0b::kMoneyTake.v)(0x14, 0);
    SetMessage(0x11);
}

// original 0x402EC0 (area 21's choice 2): message = Area21_Messages 0x5E72C4
// by the s8 choice (unchecked).
extern "C" void __cdecl Area21_ChoiceMessage(void) { SetMessage(Word(At(::bof3::Img(Area21_Messages_addr) + static_cast<U>(Choice() * 2)))); }

// original 0x402EE0 (area 21's choices 3 and 4). Choice not 0: the counter
// 0x90384B + 1, and at 8 back to 0 with message 0x34, else message 0x31.
// Choice 0: Inventory_Count(0, 3, 0) (its word) 0 - message 0x32; else n =
// (counter + 1) * 10 (10 for a counter of 0), Crt_sprintf(Text_Records row 1,
// "%d", n), message 0x30, the money give (0x591BE0) of n, the inventory take
// (0x591B60) of item 3 of category 0, one, and Flags_Set(the row 0x929ED0
// points at, 0x12).
extern "C" void __cdecl Area21_ChoiceTrade(void) {
    if (static_cast<unsigned char>(AreaW0c_ChoiceAnswer) != 0) {
        const auto count = static_cast<unsigned char>(At(::bof3::Img(at::kCounter.v))[0] + 1);
        At(::bof3::Img(at::kCounter.v))[0] = count;
        if (count == 8) {
            At(::bof3::Img(at::kCounter.v))[0] = 0;
            SetMessage(0x34);
        } else {
            SetMessage(0x31);
        }
        return;
    }
    if (AH_CALL(Inventory_Count)(0, 3, 0) == 0) {
        SetMessage(0x32);
        return;
    }
    const unsigned counter = At(::bof3::Img(at::kCounter.v))[0];
    const unsigned n = counter != 0 ? (counter + 1) * 10 : 10;
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(at::kTextRow1.v))), reinterpret_cast<const char*>(At(::bof3::Img(at::kFormat.v))), n);
    SetMessage(0x30);
    AH_AT(unsigned char (__cdecl*)(unsigned, unsigned), area_w0b::kMoneyGive.v)(n, 0);
    AH_AT(unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned), area_w0b::kInventoryTake.v)(0, 3, 1, 0);
    AH_CALL(Flags_Set)(Pointer(::bof3::Img(at::kFlagBase.v)), 0x12);
}

void Area21_Inject() {
    BOF3_INJECT(Area21_ChoicePay);
    BOF3_INJECT(Area21_ChoiceMessage);
    BOF3_INJECT(Area21_ChoiceTrade);
}
