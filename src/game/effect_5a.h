// Round thirteen group E5A (0x4FD2E0..0x4FF318): kind 0x18's sub-kinds 1, 4, 5,
// 6, 7, 8, 9, 0xA, 0x1A and 0x1F - EffectKind18_States' entries into this band,
// their sub-state dispatchers by +2, the states and their draws - and the CLUT
// copy Gfx_ClutStripCopy16 two of them share. Every state runs with
// Sprite_Current an Effect_Objects record (Effect_RunObjects, EffectKind18_Run
// by +1). docs/effect_5a.md. The functions are declared by symbols.gen.h (each
// has an `impl` in symbols.toml).
#pragma once

void Effect5A_Inject();

namespace effect_5a {
// BOF3X_SHADOW=effect_5a: the start-up fuzz, effect_5a_fuzz.cpp. Clones every
// original before Effect5A_Inject patches it.
void SelfTest();
}  // namespace effect_5a
