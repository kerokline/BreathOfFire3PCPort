// Round thirteen group E5B (0x4FF320..0x5014F2): effect kind 0x18's sub-kinds
// 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x13 and 0x4F - each a dispatcher by +2 through
// its own table, its states and its draws. Every state runs with Sprite_Current
// an Effect_Objects record (Effect_RunObjects, EffectKind18_Run by +1).
// docs/effect_5b.md. The functions are declared by symbols.gen.h (each has an
// `impl` in symbols.toml).
#pragma once

void Effect5B_Inject();

namespace effect_5b {
// BOF3X_SHADOW=effect_5b: the start-up fuzz, effect_5b_fuzz.cpp. Clones every
// original before Effect5B_Inject patches it.
void SelfTest();
}  // namespace effect_5b
