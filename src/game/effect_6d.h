// Round thirteen group E6D (0x514270..0x516B2E): kind 0x18's sub-kinds 0x5C,
// 0x5D, 0x5E, 0x61, 0x62, 0x63, 0x64 and 0x65 - their sub-state dispatchers,
// the states and the draws they share. Every function runs with
// Sprite_Current an Effect_Objects record (Effect_RunObjects,
// EffectKind18_Run) or is a cdecl helper of one. docs/effect_6d.md. The
// functions are declared by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect6D_Inject();

namespace effect_6d {
// BOF3X_SHADOW=effect_6d: the start-up fuzz, effect_6d_fuzz.cpp. Clones every
// original before Effect6D_Inject patches it.
void SelfTest();
}  // namespace effect_6d
