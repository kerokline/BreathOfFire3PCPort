// Round thirteen group E6A (0x50C0D0..0x50E3FA): kind 0x18's sub-kinds 0x2D,
// 0x2E, 0x2F, 0x30, 0x31, 0x32 and 0x3E - their sub-state dispatchers by +2,
// the states and the panel draws - and sub-kind 0x4A's state 1. Every function
// runs with Sprite_Current an Effect_Objects record (Effect_RunObjects,
// EffectKind18_Run). docs/effect_6a.md. The functions are declared by
// symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect6A_Inject();

namespace effect_6a {
// BOF3X_SHADOW=effect_6a: the start-up fuzz, effect_6a_fuzz.cpp. Clones every
// original before Effect6A_Inject patches it.
void SelfTest();
}  // namespace effect_6a
