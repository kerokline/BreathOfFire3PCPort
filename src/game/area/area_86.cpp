// Moved here by tools/move_funcs.py from area_w2b.cpp (docs/source-layout-round.md).
#include "game/area/area_86.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2b.h"
#include "game/area_w2b_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2b::at;

}  // namespace

// original 0x40FC10 (area 86's init, its descriptor 0x611C38's +0x40): while
// the s8 chapter byte Cond_ByteFA is below 0xD, AreaMap_SetByte(0xB, 9, 0x50)
// and AreaMap_SetByte(0xB, 0xA, 0x50).
extern "C" void __cdecl Area86_Init(void) {
    if (Cond_ByteFA >= 0xD) return;
    AH_CALL(AreaMap_SetByte)(0xB, 9, 0x50);
    AH_CALL(AreaMap_SetByte)(0xB, 0xA, 0x50);
}

void Area86_Inject() {
    BOF3_INJECT(Area86_Init);
}
