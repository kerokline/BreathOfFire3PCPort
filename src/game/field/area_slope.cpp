// The ground's slope under a point: original 0x5722D0. docs/kind2-object.md
// section 7.
#include "game/area_slope.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

}  // namespace

// BOF3X_SHADOW=area_slope: the start-up fuzz, area_slope_fuzz.cpp (an i686
// TU, as every *_fuzz.cpp: its framed call lays out an i686 stack).
namespace area_slope {
using SlopeFn = long (__cdecl*)(long, long, unsigned long);
void SelfTest(SlopeFn theirs);
}  // namespace area_slope

void AreaSlope_Inject() {
    // 0x5722D0..0x57256F: no calls, every jump internal (capstone, 2026-09-22).
    if (bof3::WantsShadow("area_slope"))
        area_slope::SelfTest(reinterpret_cast<area_slope::SlopeFn>(bof3::CloneOriginal("AreaMap_Slope", bof3::addr::AreaMap_Slope, 0x2A0)));
}
