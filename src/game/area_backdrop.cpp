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

// original 0x4FD350 (read 2026-09-30): the screen-wide gradient of a colour
// over black - Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, 0x95, 0) committed (7,
// 0xC), then a POLY_G4 at the new cursor: (0, 0)..(319, 239) as floats, the
// three argument bytes at the top corners, black at the bottom ones, the
// semi-transparency left as Gpu_SetPolyG4 set it; committed (7, 0x44). Its
// caller (EffectKind18_01_Sunset 0x4FD2E0, effect_5a.cpp) steps the colour from (0x40, 0x5A, 0xFF) toward
// (0x40 + 0x17E, ...) as a word at 0x937F88 + 0x2E climbs to 0xBF: the sunset
// of area 23's cutscene. DIV-0041: under a wide picture the corners are
// (-53, 0)..(372, 239) - the same rule as AreaMap_DrawBackdrop above, and
// the fuzz (before Widescreen_Inject) compares the original's.
extern "C" void __cdecl Gfx_DrawSkyGradient(unsigned r, unsigned g_, unsigned b) {
    g.draw_mode(Gfx_PacketNext, 0, 1, 0x95, 0);
    g.commit(7, 0xC);
    unsigned char* const quad = Gfx_PacketNext;
    g.set_poly_g4(quad);
    const float wide = static_cast<float>(Widescreen_Live());
    const float left = 0.0f - wide;   // not -wide: -0.0f when wide is 0
    const float right = 319.0f + wide;
    SetFloat(quad + 0x2C, 239.0f);
    SetFloat(quad + 0x3C, 239.0f);
    quad[0x14] = quad[4] = static_cast<unsigned char>(r);
    quad[0x15] = quad[5] = static_cast<unsigned char>(g_);
    SetFloat(quad + 0x8, left);
    SetDword(quad + 0xC, 0);
    SetFloat(quad + 0x18, right);
    SetDword(quad + 0x1C, 0);
    SetFloat(quad + 0x28, left);
    SetFloat(quad + 0x38, right);
    quad[0x16] = quad[6] = static_cast<unsigned char>(b);
    quad[0x34] = quad[0x24] = 0;
    quad[0x35] = quad[0x25] = 0;
    quad[0x36] = quad[0x26] = 0;
    g.commit(7, 0x44);
}

// original 0x4FD3E0 (read 2026-09-30): the sunset's glow over the sky above -
// Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, 0xB5, 0) committed (3, 0xC), then a
// POLY_F4 at the new cursor, semi-transparent (Gpu_SetSemiTrans 1), (0, 0)..
// (320, 240) as floats, red = Sprite_Current's word +0x2E / 8 (a signed
// division: the step the caller of Gfx_DrawSkyGradient climbs to 0xBF),
// green and blue 0; committed (3, 0x38). Caller EffectKind18_01_Sunset 0x4FD2E0.
// DIV-0041: (-53, 0)..(373, 240) under a wide picture, the fuzz (before
// Widescreen_Inject) comparing the original's.
extern "C" void __cdecl Gfx_DrawSunsetGlow(void) {
    g.draw_mode(Gfx_PacketNext, 0, 0, 0xB5, 0);
    g.commit(3, 0xC);
    unsigned char* const quad = Gfx_PacketNext;
    g.set_poly_f4(quad);
    g.set_semi(quad, 1);
    const float wide = static_cast<float>(Widescreen_Live());
    const float left = 0.0f - wide;   // not -wide: -0.0f when wide is 0
    const float right = 320.0f + wide;
    SetFloat(quad + 0x8, left);
    SetDword(quad + 0xC, 0);
    SetFloat(quad + 0x14, right);
    SetDword(quad + 0x18, 0);
    SetFloat(quad + 0x20, left);
    SetFloat(quad + 0x24, 240.0f);
    SetFloat(quad + 0x2C, right);
    SetFloat(quad + 0x30, 240.0f);
    std::int16_t step;
    std::memcpy(&step, Sprite_Current + 0x2E, sizeof step);
    quad[5] = 0;
    quad[6] = 0;
    quad[4] = static_cast<unsigned char>(static_cast<int>(step) / 8);   // cdq / and 7 / sar 3: toward zero
    g.commit(3, 0x38);
}

// original 0x4112A0 (0x18 bytes, no calls): Sprite_Current's screen point
// +0x2E / +0x30 = (160, 80). Called first thing by 33 small state handlers
// at 0x401F80..0x424E00 - eleven triads, one per world-map region overlay
// compiled into the exe - each of which then adjusts the sprite's scale,
// countdown and state and tail-jumps to Sprite_UpdateScreen. On the
// world-map route (259 calls) the callers are 0x404030 and 0x404080. The
// point is centred on the 320-wide view, so under the wide picture's
// primitive shift it stays centred: nothing to widen.
extern "C" void __cdecl WorldMap_PinSprite(void) {
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->screen_x, 0xA0);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->screen_y, 0x50);
}

void AreaBackdrop_Inject() {
    if (bof3::WantsShadow("area_backdrop")) area_backdrop::SelfTest();
    BOF3_INJECT(Gfx_DrawSkyGradient);
    BOF3_INJECT(Gfx_DrawSunsetGlow);
    BOF3_INJECT(WorldMap_PinSprite);
}
