// Moved here by tools/move_funcs.py from magic_s30.cpp (docs/source-layout-round.md).
#include "game/magic/magic_fx_2.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

unsigned char* Sc() { return Sprite_Current; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

}  // namespace

#define S30_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4E5930 (a phase of MAGIC060, 064 and 131): +9 0, +2 on.
S30_EXPORT void __cdecl MagicFx_ClearCount9(void) {
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4E5950 (a phase of sixteen overlays): +9 up by 2; on at 0x10.
S30_EXPORT void __cdecl MagicFx_CountUp9By2(void) {
    AddB(Sc()[9], 2);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

void MagicFx2_Inject() {
    BOF3_INJECT(MagicFx_ClearCount9);
    BOF3_INJECT(MagicFx_CountUp9By2);
}
