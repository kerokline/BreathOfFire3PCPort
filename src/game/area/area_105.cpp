// Moved here by tools/move_funcs.py from area_w2e.cpp (docs/source-layout-round.md).
#include "game/area/area_105.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2e.h"
#include "game/area_w2e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2e::at;

}  // namespace

namespace {

using move_script::At;

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

}  // namespace

// original 0x416400 (area 105's init; PSX 0x801F32B8): when the party came
// from area 0x57 (the word 0x802290): story flag 0x4F set when Cond_ByteFD is
// 0; then (read again) cleared when it is 1.
extern "C" void __cdecl Area105_Init(void) {
    if (AreaW2c_LastArea != 0x57) return;
    if (Cond_ByteFD == 0) AH_CALL(Flags_Set)(Bank(), 0x4F);
    if (Cond_ByteFD == 1) AH_CALL(Flags_Clear)(Bank(), 0x4F);
}

void Area105_Inject() {
    BOF3_INJECT(Area105_Init);
}
