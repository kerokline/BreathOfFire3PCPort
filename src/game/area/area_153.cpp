// Moved here by tools/move_funcs.py from area_w4a.cpp (docs/source-layout-round.md).
#include "game/area/area_153.h"

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

// original 0x425B20 (area 153 +0x34[0], a choice): FocusPair over
// Area153_ChoicePairs 0x63841C (Area90_ChoiceFocusPair's shape).
extern "C" void __cdecl Area153_ChoiceFocusPair(void) { FocusPair(::bof3::Img(Area153_ChoicePairs_addr)); }

void Area153_Inject() {
    BOF3_INJECT(Area153_ChoiceFocusPair);
}
