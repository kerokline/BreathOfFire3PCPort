// Round thirteen group E5D (0x503DE0..0x506A00): effect kind 0x18's sub-kinds
// 0x14, 0x18, 0x19, 0x1B, 0x1C, 0x1D, 0x1E, 0x21, 0x22, 0x43, 0x52 and 0x68
// (EffectKind18_States' entries 20, 24, 25, 27..30, 33, 34, 67, 82 and 104) -
// their sub-state dispatchers by +2, their sub-states and draws - and four
// helpers of sub-kind 0x17 (E5C's). Every function runs with Sprite_Current an
// Effect_Objects record (Effect_RunObjects). docs/effect_5d.md. The functions
// are declared by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect5D_Inject();

namespace effect_5d {
// BOF3X_SHADOW=effect_5d: the start-up fuzz, effect_5d_fuzz.cpp. Clones every
// original before Effect5D_Inject patches it.
void SelfTest();
}  // namespace effect_5d
