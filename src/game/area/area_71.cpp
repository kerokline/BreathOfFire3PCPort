// Moved here by tools/move_funcs.py from area_w1f.cpp (docs/source-layout-round.md).
#include "game/area/area_71.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1f_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = area_w1f::at;

}  // namespace

namespace {

using area_harness::Mem;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// A record's index among the field objects as the originals compute it: the
// pointer less Sprite_Objects, divided by 0xA4 as a signed dword (imul by
// 0x63E7063F, sar 6, the sign added back): C's truncating division.
unsigned char ObjectIndex(const unsigned char* record) {
    const auto delta = static_cast<std::int32_t>(Low32(record) -
                                                 ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(delta / static_cast<std::int32_t>(at::kObjectStride));
}

}  // namespace

// original 0x40D420 (Area71_Handlers[0]; PSX 0x801F2C04): Effect_FindFree into
// the scratch byte 0x903850; a slot: its +0 = 1, kind +5 = 0x3C, +0xB = the
// active member's index among the field objects (read after the call), and
// Sound_PlayEffect(0x207).
extern "C" void __cdecl Area71_SpawnEffect3C(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(::bof3::Img(at::kScratch.v)) = slot;
    if (slot == 0xFF) return;
    const unsigned char member = ObjectIndex(Field_ActiveMember);
    unsigned char* const e = EffectAt(B(::bof3::Img(at::kScratch.v)));
    e[0] = 1;
    e[5] = 0x3C;
    e[0xB] = member;
    AH_CALL(Sound_PlayEffect)(0x207);
}

// original 0x40D480 (Area71_Handlers[1]; PSX 0x801F2CF4): the same slot with
// +0 = 1 and kind 0x25, no member, no sound.
extern "C" void __cdecl Area71_SpawnEffect25(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(::bof3::Img(at::kScratch.v)) = slot;
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(B(::bof3::Img(at::kScratch.v)));
    e[0] = 1;
    e[5] = 0x25;
}

void Area71_Inject() {
    BOF3_INJECT(Area71_SpawnEffect3C);
    BOF3_INJECT(Area71_SpawnEffect25);
}
