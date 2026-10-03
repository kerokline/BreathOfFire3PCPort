// Round thirteen group E4D (0x48C990..0x48DF87): effect kinds 0x91, 0x94,
// 0x95, 0x96, 0x97, 0x98 and 0x9A - their dispatchers, states and draws - and
// the full-screen tint (0x48CA90) kinds 0x63, 0x75 and others draw with. Every
// function runs with Sprite_Current an Effect_Objects record (Effect_RunObjects)
// or is a cdecl helper of one. docs/effect_4d.md. The functions are declared by
// symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect4D_Inject();

namespace effect_4d {
// BOF3X_SHADOW=effect_4d: the start-up fuzz, effect_4d_fuzz.cpp. Clones every
// original before Effect4D_Inject patches it.
void SelfTest();
}  // namespace effect_4d
