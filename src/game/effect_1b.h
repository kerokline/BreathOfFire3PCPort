// Round thirteen group E1B: the effect engine's 0x4672F0..0x46A5F2 - effect
// kind 0xF's states 26..40 and its six child sub-kinds (the run of state
// tables 0x653CA0..0x653DAC), the panels and windows they and the field
// panels draw, and the dispatch or whole handler of kinds 0x11, 0x12, 0x14 and
// 0x92. docs/effect_1b.md.
#pragma once

void Effect1B_Inject();

namespace effect_1b {
// BOF3X_SHADOW=effect_1b: the start-up fuzz, effect_1b_fuzz.cpp. Clones every
// original before Effect1B_Inject patches it.
void SelfTest();
}  // namespace effect_1b
