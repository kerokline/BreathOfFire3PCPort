// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_01.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0a.h"
#include "game/area_w0a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0a::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

std::uint32_t Key(const void* p) { return Low32(p); }

// The index of an object from Sprite_Objects, as the original's magic
// multiply computes it: a signed 32-bit difference divided by 0xA4,
// truncated toward 0.
unsigned char ObjectIndex(const unsigned char* object) {
    const auto d = static_cast<std::int32_t>(Key(object) - ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(d / 0xA4);
}

unsigned char* Effect(std::uint32_t slot_dword) { return Mem(::bof3::Img(Effect_Objects_addr) + (slot_dword & 0xFFu) * at::kEffectStride); }

}  // namespace

// original 0x4010D0 (area 1's handler 0 and choice 2; PSX 0x801F2C88):
// variable 6 = the index of Field_ActiveMember from Sprite_Objects.
extern "C" void __cdecl Area01_ActiveMemberToVar6(void) {
    Mem(::bof3::Img(at::kVar6.v))[0] = ObjectIndex(Field_ActiveMember);
}

// original 0x401100 (area 1's handler 1 and choice 3; PSX 0x801F2CD4): with
// variable 5 above 6 (unsigned) and Frame_Counter's low three bits 0, a free
// effect record of kind 0xB: +0 1, +5 0xB, the dword +0x1C 5, +7 3. The slot
// is kept at 0x903850 and read back from it.
extern "C" void __cdecl Area01_SpawnEffectB(void) {
    if (Mem(::bof3::Img(at::kVar5.v))[0] <= 6) return;
    if ((Frame_Counter & 7u) != 0) return;
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Mem(::bof3::Img(at::kEffectSlot.v))[0] = slot;
    if (slot == 0xFF) return;
    unsigned char* const e = Effect(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(at::kEffectSlot.v)))));
    e[0] = 1;
    e[5] = 0xB;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->work_1c, 5);
    e[7] = 3;
}

void Area01_Inject() {
    BOF3_INJECT(Area01_ActiveMemberToVar6);
    BOF3_INJECT(Area01_SpawnEffectB);
}
