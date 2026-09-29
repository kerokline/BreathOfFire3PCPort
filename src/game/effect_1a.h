// Round thirteen group E1A (0x462B00..0x4672F0): the dispatchers of effect
// kinds 1, 2, 3, 5, 7, 8, 9, 0xA..0xF, 0x10, 0x16, 0x1A and 0x5C, the states of
// kinds 1, 3, 5, 7, 0xA, 0xC, 0xD, 0xF and 0x1A, and the panel draws kinds 2,
// 3 and 0xC share (EffectHud_*). Every function runs with Sprite_Current an
// Effect_Objects record (Effect_RunObjects) or is a cdecl draw helper of one.
// docs/effect_1a.md. The functions are declared by symbols.gen.h (each has an
// `impl` in symbols.toml).
#pragma once

void Effect1A_Inject();

namespace effect_1a {
// BOF3X_SHADOW=effect_1a: the start-up fuzz, effect_1a_fuzz.cpp. Clones every
// original before Effect1A_Inject patches it.
void SelfTest();
}  // namespace effect_1a
