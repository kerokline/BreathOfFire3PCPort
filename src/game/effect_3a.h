// Round thirteen group E3A (0x4801F0..0x4823C2): effect kinds 0x60, 0x61, 0x62,
// 0x64 and 0x68 - the dispatchers, the states of 0x61..0x68 and their draws,
// and kind 0x60's line. Kinds 0x61..0x68 are chapter 10's run 2 (scena_sc9b.cpp
// Scena10_Run2), kind 0x60 chapter 8's (scena_sc7.cpp). Every function runs
// with Sprite_Current an Effect_Objects record (Effect_RunObjects) or is a
// cdecl helper of one. docs/effect_3a.md. The functions are declared by
// symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect3A_Inject();

namespace effect_3a {
// BOF3X_SHADOW=effect_3a: the start-up fuzz, effect_3a_fuzz.cpp. Clones every
// original before Effect3A_Inject patches it.
void SelfTest();
}  // namespace effect_3a
