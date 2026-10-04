// Round thirteen group E5E (0x506A10..0x508CBB): effect kind 0x18's sub-kinds
// 0x23, 0x24, 0x25, 0x26, 0x39 and 0x3F (EffectKind18_States 35, 36, 37, 38,
// 57, 63) - their dispatchers by +2, their states and their draws. Every
// function runs with Sprite_Current an Effect_Objects record (Effect_RunObjects)
// or is a cdecl helper of one. docs/effect_5e.md. The functions are declared by
// symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect5E_Inject();

namespace effect_5e {
// BOF3X_SHADOW=effect_5e: the start-up fuzz, effect_5e_fuzz.cpp. Clones every
// original before Effect5E_Inject patches it.
void SelfTest();
}  // namespace effect_5e
