// Moved here by tools/move_funcs.py from field_modes.cpp (docs/source-layout-round.md).
#include "game/scenario/clut_strip.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/field_modes.h"
#include "game/field_modes_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace field_modes;

}  // namespace

// original 0x56C110: the first 16 colours of Gfx_ClutStrip copied back from
// Gfx_ClutStripSource, then Gfx_ClutStripDirty + 1 (PSX 0x801F8448).
extern "C" void __cdecl ClutStrip_Restore(void) {
    for (unsigned i = 0; i < 16; ++i) Gfx_ClutStrip[i] = Gfx_ClutStripSource[i];
    ++Gfx_ClutStripDirty;
}

void ClutStrip_Inject() {
    BOF3_INJECT(ClutStrip_Restore);
}
