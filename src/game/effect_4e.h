// Round thirteen group E4E (0x48DF90..0x491C96): effect kinds 0x9B, 0x9C and
// 0xA0 whole (dispatchers, states, draws), kind 0x9E's dispatcher and four
// draws its catalog-part-6 states call, and the dispatchers of kinds 0xA1,
// 0xA2, 0xA3, 0xA7, 0xA8 and 0xA9. Every function runs with Sprite_Current an
// Effect_Objects record (Effect_RunObjects) or is a cdecl helper of one.
// docs/effect_4e.md. The functions are declared by symbols.gen.h (each has an
// `impl` in symbols.toml).
#pragma once

void Effect4E_Inject();

namespace effect_4e {
// BOF3X_SHADOW=effect_4e: the start-up fuzz, effect_4e_fuzz.cpp. Clones every
// original before Effect4E_Inject patches it.
void SelfTest();
}  // namespace effect_4e
