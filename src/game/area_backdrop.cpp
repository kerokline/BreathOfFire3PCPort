#include "game/area_backdrop.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_backdrop_callees.h"
#include "game/record_views.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The three handlers of AreaMap_EntryHandlers 0x66329C that map_layers.cpp
// left (entries 1, 2 and 3 - entry 0 is AreaMap_ClutCycle there), and the
// world map's sprite pin - see docs/area-backdrop.md for each function, its
// PSX twin and the fuzz. The point of the group is the backdrop: under a wide
// picture (DIV-0041) the sky gradient covers the whole 426 x 240 view.

namespace area_backdrop {

const Callees kOriginals = {
    Gpu_SetDrawMode, Gfx_CommitPrim, Gpu_SetPolyG4, Gpu_SetSemiTrans,
    Area_TestCondition, Gpu_SetDrawMove,
    Gpu_SetPolyF4,
};
Callees g = kOriginals;

}  // namespace area_backdrop

namespace {

using area_backdrop::g;

void SetDword(unsigned char* p, std::uint32_t v) { std::memcpy(p, &v, sizeof v); }
void SetFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

}  // namespace

    SetFloat(quad + 0x8, left);
    SetDword(quad + 0xC, 0);
    SetFloat(quad + 0x18, right);
    SetDword(quad + 0x1C, 0);
    SetFloat(quad + 0x28, left);

void AreaBackdrop_Inject() {
    if (bof3::WantsShadow("area_backdrop")) area_backdrop::SelfTest();
}
