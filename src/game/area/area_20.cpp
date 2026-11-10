// Moved here by tools/move_funcs.py from area_w0b.cpp (docs/source-layout-round.md).
#include "game/area/area_20.h"

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

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

}  // namespace

// original 0x402DB0 (area 20's init, the descriptor 0x5E6C58's +0x40; PSX
// 0x801F2D2C): a five-byte `jmp 0x402DC0`, then the body - Area29_PickFieldObject's
// (0x4037B0) instruction for instruction over area 20's own tables.
// Rand() & 0x3F picks one of the first eight field objects by the chances
// Area20_Weights 0x5E6C4C (byte-wide running subtraction; 8, none, when the
// roll is past them all); the other seven get +0 = 0. The kept one goes to
// Area20_Cells[Rand() & 7] (+0x34 = x << 16, +0x38 = z << 16, +0x3E =
// AreaMap_Elevation(+0x34, z << 16)). Last, either way, Field_StepCount = the
// low word of the leader's dword +0x134 - 5.
extern "C" void __cdecl Area20_PickFieldObject(void) {
    unsigned roll = static_cast<unsigned>(AH_CALL(Rand)()) & 0x3F;
    unsigned keep = 0;
    for (; keep < 8; ++keep) {
        const unsigned chance = At(::bof3::Img(Area20_Weights_addr) + keep)[0];
        if (roll < chance) break;
        roll = (roll - chance) & 0xFF;
    }
    for (unsigned i = 0; i < 8; ++i)
        if (i != keep) Sprite_Objects[i].flags = 0;
    if (keep < 8) {
        const unsigned cell = static_cast<unsigned>(AH_CALL(Rand)()) & 7;
        unsigned char* const o = rv::Bytes(Sprite_Objects) + keep * 0xA4;
        auto* const tO = rv::As<records::SpriteObject>(o);
        const U x = static_cast<U>(At(::bof3::Img(Area20_Cells_addr) + cell * 2)[0]) << 16;
        const U z = static_cast<U>(At(::bof3::Img(Area20_Cells_addr) + cell * 2 + 1)[0]) << 16;
        tO->x = static_cast<std::int32_t>(x);
        tO->z = static_cast<std::int32_t>(z);
        const long height = AH_CALL(AreaMap_Elevation)(tO->x, static_cast<long>(z));
        rv::Store<unsigned>(rv::As<records::SpriteObject>(o)->height_whole, static_cast<unsigned>(height));
    }
    Field_StepCount = static_cast<unsigned short>(Long(At(::bof3::Img(ObjTrio_addr) + 0x134)) - 5);
}

void Area20_Inject() {
    BOF3_INJECT(Area20_PickFieldObject);
}
