// Moved here by tools/move_funcs.py from rest_3g.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_0f_2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3g::at;

}  // namespace

// original 0x5171E0 (cdecl; EffectKind0F_Title, _TitleClose, _LineType): the
// characters of a string to its NUL, a byte with bit 7 taking the next byte
// with it (that byte not tested for the NUL); eax.
extern "C" int __cdecl EffectKind0F_CharCount(const unsigned char* text) {
    int count = 0;
    unsigned char c = *text++;
    if (c == 0) return 0;
    do {
        if (c & 0x80) ++text;
        c = *text;
        ++count;
        ++text;
    } while (c != 0);
    return count;
}

void EffectKind0f2_Inject() {
    BOF3_INJECT(EffectKind0F_CharCount);
}
