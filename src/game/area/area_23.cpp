// Moved here by tools/move_funcs.py from area_w0b.cpp (docs/source-layout-round.md).
#include "game/area/area_23.h"

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

namespace {

using move_script::At;

using move_script::SetWord;

void SetMessage(unsigned id) { SetWord(At(::bof3::Img(at::kMessage.v)), id); }

}  // namespace

// original 0x403080 (area 23's choice 2 and handler 0; PSX 0x801F2C88): cells
// (0x48, 0xB), (0x49, 0xB), (0x48, 0xC), (0x49, 0xC) to 0.
extern "C" void __cdecl Area23_ClearCells(void) {
    AH_CALL(AreaMap_SetByte)(0x48, 0xB, 0);
    AH_CALL(AreaMap_SetByte)(0x49, 0xB, 0);
    AH_CALL(AreaMap_SetByte)(0x48, 0xC, 0);
    AH_CALL(AreaMap_SetByte)(0x49, 0xC, 0);
}

// original 0x4030B0 (area 23's choice 3 and handler 1; PSX 0x801F2CE0): the
// same four cells back - row 0xB to 0xC0, row 0xC to 0xA1.
extern "C" void __cdecl Area23_SetCells(void) {
    AH_CALL(AreaMap_SetByte)(0x48, 0xB, 0xC0);
    AH_CALL(AreaMap_SetByte)(0x49, 0xB, 0xC0);
    AH_CALL(AreaMap_SetByte)(0x48, 0xC, 0xA1);
    AH_CALL(AreaMap_SetByte)(0x49, 0xC, 0xA1);
}

// original 0x4030F0 (area 23's choice 4 and handler 2; PSX 0x801F2D38):
// Camera_ShiftY - 0x12, MapView_Redraw = 2.
extern "C" void __cdecl Area23_CameraUp(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 0x12);
    MapView_Redraw = 2;
}

// original 0x403100 (area 23's choice 5 and handler 3; PSX 0x801F2D60):
// Camera_ShiftY + 0x12, MapView_Redraw = 2.
extern "C" void __cdecl Area23_CameraDown(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 0x12);
    MapView_Redraw = 2;
}

// original 0x403110 (area 24's choices 0 and 1): message 0xFFFF.
extern "C" void __cdecl Area24_NoMessage(void) { SetMessage(0xFFFF); }

void Area23_Inject() {
    BOF3_INJECT(Area23_ClearCells);
    BOF3_INJECT(Area23_SetCells);
    BOF3_INJECT(Area23_CameraUp);
    BOF3_INJECT(Area23_CameraDown);
    BOF3_INJECT(Area24_NoMessage);
}
