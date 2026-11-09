// Moved here by tools/move_funcs.py from area_w2d.cpp (docs/source-layout-round.md).
#include "game/area/area_97.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2d.h"
#include "game/area_w2d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2d::at;

}  // namespace

namespace {

using area_harness::Mem;

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

}  // namespace

// original 0x4139E0 (WorldMap_FieldHooks[11], the "no world map" entry that
// tail kind 27 0x56DE30 jumps to; in area 97's block): key item 5 held, story
// flag 0x23 set. al 0.
extern "C" unsigned char __cdecl Area97_FlagIfKeyItem5(void) {
    if (AH_CALL(KeyItem_Has)(5) != 0) AH_CALL(Flags_Set)(StoryFlags(), 0x23);
    return 0;
}

void Area97_Inject() {
    BOF3_INJECT(Area97_FlagIfKeyItem5);
}
