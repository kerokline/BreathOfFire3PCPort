// Moved here by tools/move_funcs.py from area_w4a.cpp (docs/source-layout-round.md).
#include "game/area/area_154.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4a.h"
#include "game/area_w4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

unsigned char& B(U address) { return move_script::At(address)[0]; }

void SetMessage(unsigned v) { SetWord(At(::bof3::Img(at::kMessage.v)), v); }

std::int32_t Answer() { return AreaW0c_ChoiceAnswer; }

unsigned char* Focus() { return reinterpret_cast<unsigned char*>(AreaW1a_FocusObject); }

// The focus object's dwords +0x18 / +0x1C = the byte pair pairs[s8 answer]
// (zero-extended; the pointer and the answer read again for the second;
// unchecked), the message 0xFFFF between the two reads of the first.
void FocusPair(U pairs) {
    unsigned char* focus = Focus();
    const unsigned first = B(pairs + static_cast<U>(Answer()) * 2u);
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, static_cast<std::int32_t>(first));
    focus = Focus();
    SetLong(focus + 0x1C, B(pairs + static_cast<U>(Answer()) * 2u + 1));
}

}  // namespace

// original 0x425B60 (area 154 +0x34[0]): FocusPair over Area154_ChoicePairs
// 0x638994.
extern "C" void __cdecl Area154_ChoiceFocusPair(void) { FocusPair(::bof3::Img(Area154_ChoicePairs_addr)); }

// original 0x425BA0 (area 155 +0x34[0]): answer (byte) 2: Item_NamePtr(1,
// 0x4D)'s sixteen bytes to Text_Records row 0 (four dwords in order), then
// Inventory_Add(1, 0x4D, 1) answering (al) not 0: Sound_PlayEffect(0x106),
// message 0xB, bit 1 of the bank 0x903FF8 set, done. Any other answer, or the
// item not added: message 0xA and the same bit set.
extern "C" void __cdecl Area155_ChoiceGiveItem4D(void) {
    if (static_cast<unsigned char>(AreaW0c_ChoiceAnswer) == 2) {
        const unsigned char* const name = AH_CALL(Item_NamePtr)(1, 0x4D);
        unsigned char* const row = At(::bof3::Img(at::kTextRecords.v));
        for (U k = 0; k < 16; k += 4) SetLong(row + k, Long(name + k));
        if (AH_CALL(Inventory_Add)(1, 0x4D, 1) != 0) {
            AH_CALL(Sound_PlayEffect)(0x106);
            SetMessage(0xB);
            AH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0x68), 1);
            return;
        }
    }
    SetMessage(0xA);
    AH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0x68), 1);
}

void Area154_Inject() {
    BOF3_INJECT(Area154_ChoiceFocusPair);
    BOF3_INJECT(Area155_ChoiceGiveItem4D);
}
