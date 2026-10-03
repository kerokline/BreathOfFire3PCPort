// Round thirteen group E5G (0x50AF90..0x50C0CA): kind 0x18's sub-kinds 0x2B,
// 0x2C, 0x36, 0x3A and 0x4A - their sub-state dispatchers by +2, the states
// and the panel draws they share. Every function runs with Sprite_Current an
// Effect_Objects record (Effect_RunObjects, EffectKind18_Run) or is a cdecl
// draw helper of one. docs/effect_5g.md. The functions are declared by
// symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect5G_Inject();

namespace effect_5g {
// BOF3X_SHADOW=effect_5g: the start-up fuzz, effect_5g_fuzz.cpp. Clones every
// original before Effect5G_Inject patches it.
void SelfTest();
}  // namespace effect_5g
