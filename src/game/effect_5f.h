// Round thirteen group E5F (0x508CC0..0x50AF8D): kind 0x18's sub-kinds 0x27,
// 0x28, 0x29, 0x2A, 0x3C, 0x42, 0x48, 0x49 and 0x4B / 0x4C - their dispatchers
// by +2, their states and their draws. Every function runs with Sprite_Current
// an Effect_Objects record (Effect_RunObjects, EffectKind18_States by +1) or is
// a cdecl helper of one. docs/effect_5f.md. The functions are declared by
// symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect5F_Inject();

namespace effect_5f {
// BOF3X_SHADOW=effect_5f: the start-up fuzz, effect_5f_fuzz.cpp. Clones every
// original before Effect5F_Inject patches it.
void SelfTest();
}  // namespace effect_5f
