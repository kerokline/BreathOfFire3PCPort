// Moved here by tools/move_funcs.py from area_w0b.cpp (docs/source-layout-round.md).
#include "game/area/area_19.h"

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

// original 0x402D60 (area 19's handler 0, Area19_Handlers 0x5E6B60; PSX
// 0x801F2C04): Camera_Distance + 0xA00 (a word, no bound), MapView_Redraw = 2.
extern "C" void __cdecl Area19_CameraOut(void) {
    Camera_Distance = static_cast<short>(Camera_Distance + 0xA00);
    MapView_Redraw = 2;
}

// original 0x402D80 (area 19's handler 1; PSX 0x801F2C2C): Camera_Distance -
// 0xA00, MapView_Redraw = 2.
extern "C" void __cdecl Area19_CameraIn(void) {
    Camera_Distance = static_cast<short>(Camera_Distance + 0xF600);
    MapView_Redraw = 2;
}

// original 0x402DA0 (area 19's handler 5; PSX 0x801F2CB4): Kind2_Place(4).
extern "C" void __cdecl Area19_PlaceKind2(void) { AH_CALL(Kind2_Place)(4); }

void Area19_Inject() {
    BOF3_INJECT(Area19_CameraOut);
    BOF3_INJECT(Area19_CameraIn);
    BOF3_INJECT(Area19_PlaceKind2);
}
