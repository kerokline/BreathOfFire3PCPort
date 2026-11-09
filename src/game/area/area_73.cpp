// Moved here by tools/move_funcs.py from area_w1f.cpp (docs/source-layout-round.md).
#include "game/area/area_73.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1f.h"
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

using move_script::Long;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// Field object `index` (Sprite_Objects, 0xA4 bytes); effect record `slot`
// (Effect_Objects, 0x80 bytes); party record `member` (ObjTrio, 0x14C bytes).
unsigned char* ObjectAt(unsigned index) { return rv::Bytes(Sprite_Objects) + index * at::kObjectStride; }

// Areas 72 and 73's inits (0x40D4B0, 0x40D580, one body over two tables): a
// weighted draw - Rand() & 0x3F walked down the eight weights, the first it
// falls below chosen (none: 8); field objects 0..7 but the chosen one get +0
// = 0; the chosen one is put at one of the eight cells (Rand() & 7: x and z
// bytes << 16), its word +0x3E the ground there; Field_StepCount = the
// leader's zone counter less 5 (either way).
void PlaceRandomObject(std::uint32_t cells, std::uint32_t weights) {
    auto roll = static_cast<unsigned char>(AH_CALL(Rand)() & 0x3F);
    unsigned chosen = 0;
    for (; chosen < 8; ++chosen) {
        const unsigned char weight = B(weights + chosen);
        if (roll < weight) break;
        roll = static_cast<unsigned char>(roll - weight);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (k != chosen) ObjectAt(k)[0] = 0;
    if (chosen < 8) {
        const unsigned pick = static_cast<unsigned char>(AH_CALL(Rand)() & 7);
        unsigned char* const object = ObjectAt(chosen);
        auto* const tObject = rv::As<records::SpriteObject>(object);
        const auto x = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(cells + pick * 2)) << 16);
        const auto z = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(cells + pick * 2 + 1)) << 16);
        tObject->x = x;
        tObject->z = z;
        const long ground = AH_CALL(AreaMap_Elevation)(tObject->x, z);
        rv::Store<unsigned>(rv::As<records::SpriteObject>(object)->height_whole, static_cast<unsigned>(ground));
    }
    Field_StepCount = static_cast<unsigned short>(Long(Mem(::bof3::Img(ObjTrio_addr) + 0x134)) - 5);
}

}  // namespace

// original 0x40D580 (Area_Descriptors[73] +0x40; PSX 0x801F2D2C, the same
// entry as area 72's): area 72's code over Area73_Cells / _Weights.
extern "C" void __cdecl Area73_PlaceRandomObject(void) { PlaceRandomObject(::bof3::Img(Area73_Cells_addr), ::bof3::Img(Area73_Weights_addr)); }

void Area73_Inject() {
    BOF3_INJECT(Area73_PlaceRandomObject);
}
