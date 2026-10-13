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

// DIV-0041: one of ItemTrade_DrawBackground's POLY_FT4s for `columns` extra
// columns a side - x0..x1 in the game's 320-wide units and u0..u1 the texels
// across, as many as the columns covered (no stretch), u0 chosen so the
// 32-texel pattern keeps the original's phase at every column (u = x mod 32).
// There are ItemTrade_BackdropQuads(columns) of them. Two while both halves'
// u fit the byte (to 64 columns; 16:9's 53 among them): quad 0 the left half
// grown to 0 - columns, 1 the right grown to 320 + columns. Past that (21:9's
// 120) four: 0 and 1 the original's halves as they are, 2 a band quad
// -columns..0 and 3 a band quad 320..320 + columns. columns 0 is the
// original's two quads: (0, 0xA0) / (0xA0, 0x140), u 0..0xA0. A span whose u
// would pass 255 aborts (past 224 columns).
struct BackdropSpan {
    float x0, x1;
    unsigned char u0, u1;
};
unsigned ItemTrade_BackdropQuads(unsigned columns);
BackdropSpan ItemTrade_BackdropSpan(unsigned quad, unsigned columns);
}  // namespace effect_1g
