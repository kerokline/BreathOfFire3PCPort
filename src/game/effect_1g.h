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
}  // namespace effect_1g
