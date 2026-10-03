// Round thirteen group E1G: the item-trade screen's rest - its fourth run step,
// its leave state and the two steps under it, the backdrop, the list, the
// ingredient and count windows and their frames, the ingredient test, the row
// count - and the item icon drawn beside a name. Round twelve's FE2 took the
// screen's first seven states (field_e2.cpp); these are what they call.
// docs/effect_1g.md.
#pragma once

void Effect1G_Inject();

namespace effect_1g {
// BOF3X_SHADOW=effect_1g: the start-up fuzz, effect_1g_fuzz.cpp. Clones every
// original before Effect1G_Inject patches it.
void SelfTest();

// DIV-0041: one of ItemTrade_DrawBackground's two POLY_FT4s (half 0 the left,
// 1 the right) for `columns` extra columns a side - x0..x1 in the game's
// 320-wide units and u0..u1 the texels across, as many as the columns
// covered (no stretch), u0 chosen so the 32-texel pattern keeps the
// original's phase at every column. columns 0 is the original's quad: (0,
// 0xA0) / (0xA0, 0x140), u 0..0xA0. A span whose u would pass 255 aborts.
struct BackdropSpan {
    float x0, x1;
    unsigned char u0, u1;
};
BackdropSpan ItemTrade_BackdropSpan(unsigned half, unsigned columns);
}  // namespace effect_1g
