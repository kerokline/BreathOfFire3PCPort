// Moved here by tools/move_funcs.py from area_w1d.cpp (docs/source-layout-round.md).
#include "game/area/area_63.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1d.h"
#include "game/area_w1d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1d::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// Areas 63 and 64's inits (0x40B720, 0x40B7F0): Rand & 0x3F walked through
// eight weights (the first whose weight exceeds what is left; none: 8);
// field objects 0..7 other than that one get byte +0 = 0; the one chosen (if
// any) is put at one of eight cells by Rand & 7 - x and z the pair's bytes
// << 16, its word +0x3E the ground's elevation there (x read back). Either
// way Field_StepCount = the leader's dword +0x134 less 5.
void PlaceOneObject(std::uint32_t positions, std::uint32_t weights) {
    auto left = static_cast<unsigned char>(AH_CALL(Rand)() & 0x3F);
    unsigned pick = 0;
    for (; pick < 8; ++pick) {
        const unsigned char w = B(weights + pick);
        if (left < w) break;
        left = static_cast<unsigned char>(left - w);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (k != pick) Sprite_Objects[k].flags = 0;
    if (pick < 8) {
        const unsigned r = static_cast<unsigned char>(AH_CALL(Rand)() & 7);
        unsigned char* const object = rv::Bytes(Sprite_Objects) + pick * at::kObjectStride;
        auto* const tObject = rv::As<records::SpriteObject>(object);
        const std::int32_t z = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(positions + r * 2 + 1)) << 16);
        const std::int32_t x = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(positions + r * 2)) << 16);
        tObject->x = x;
        tObject->z = z;
        const long ground = AH_CALL(AreaMap_Elevation)(tObject->x, z);
        rv::Store<unsigned>(rv::As<records::SpriteObject>(object)->height_whole, static_cast<unsigned>(ground));
    }
    Field_StepCount = static_cast<unsigned short>(Long(Mem(::bof3::Img(ObjTrio_addr) + 0x134)) - 5);
}

}  // namespace

// original 0x40B720 (Area63 +0x40 init; PSX 0x801F2D2C): a 5-byte jmp over
// eleven nops, then PlaceOneObject with area 63's tables.
extern "C" void __cdecl Area63_InitPlaceObject(void) { PlaceOneObject(::bof3::Img(Area63_Positions_addr), ::bof3::Img(Area63_Weights_addr)); }

void Area63_Inject() {
    BOF3_INJECT(Area63_InitPlaceObject);
}
