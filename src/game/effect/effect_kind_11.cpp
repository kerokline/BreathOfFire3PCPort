// Moved here by tools/move_funcs.py from effect_1b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_11.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1b.h"
#include "game/effect_1b_callees.h"
#include "game/fishing_text.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1b::at;

}  // namespace

namespace {

// A float stored as the originals' fild / fstp store it: every value here is a
// whole number well inside a float's 24 bits, so exact.
void F(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

}  // namespace

// original 0x46A450 (Effect_KindHandlers[0x11]): the draw mode (0, 1, tpage
// Gpu_GetTPage(2, 3, 0x140, 0x140), 0) committed (6, 0xC); a POLY_G4 of the
// screen (0, 0)..(320, 320) as floats, every colour 0, semi-transparent (1),
// committed (2, 0x44).
extern "C" void __cdecl EffectKind11_DrawShade() {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(2, 3, 0x140, 0x140) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    // DIV-0041: the left corners at -53 and the right at 373 under the wide
    // picture (widescreen.h: 0 until every self-test has run).
    const int left = -static_cast<int>(Widescreen_Fill()), right = 320 + static_cast<int>(Widescreen_Fill());
    F(p + 8, left);
    F(p + 0xC, 0);
    F(p + 0x18, right);
    F(p + 0x1C, 0);
    F(p + 0x28, left);
    F(p + 0x2C, 320);
    F(p + 0x38, right);
    F(p + 0x3C, 320);
    p[4] = p[5] = p[6] = 0;
    p[0x14] = p[0x15] = p[0x16] = 0;
    p[0x24] = p[0x25] = p[0x26] = 0;
    p[0x34] = p[0x35] = p[0x36] = 0;
    SH_CALL(Gpu_SetSemiTrans)(Gfx_PacketNext, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
}

// original 0x46A500 (Effect_KindHandlers[0x12]): the draw mode (0, 1, tpage
// Gpu_GetTPage(1, 2, 0x140, 0), 0) committed (2, 0xC); a POLY_G4 (0, 0)..(320,
// 100), 0xC8 grey above and black below, semi-transparent (set before its
// corners), committed (2, 0x44); the draw mode (0, 0, Gpu_GetTPage(2, 3, 0x140,
// 0x140), 0) committed (2, 0xC).
extern "C" void __cdecl EffectKind12_DrawGradient() {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(1, 2, 0x140, 0) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    // DIV-0041: the left corners at -53 and the right at 373 under the wide
    // picture (widescreen.h: 0 until every self-test has run).
    const int left = -static_cast<int>(Widescreen_Fill()), right = 320 + static_cast<int>(Widescreen_Fill());
    F(p + 0x2C, 100);
    F(p + 0x3C, 100);
    F(p + 8, left);
    F(p + 0xC, 0);
    F(p + 0x18, right);
    F(p + 0x1C, 0);
    F(p + 0x28, left);
    F(p + 0x38, right);
    p[4] = p[5] = p[6] = 0xC8;
    p[0x14] = p[0x15] = p[0x16] = 0xC8;
    p[0x24] = p[0x25] = p[0x26] = 0;
    p[0x34] = p[0x35] = p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    const unsigned tpage2 = SH_CALL(Gpu_GetTPage)(2, 3, 0x140, 0x140) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage2, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
}

void EffectKind11_Inject() {
    BOF3_INJECT(EffectKind11_DrawShade);
    BOF3_INJECT(EffectKind12_DrawGradient);
}
