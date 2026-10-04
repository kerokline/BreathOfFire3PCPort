// Round thirteen group E6B (0x50E400..0x510C8A): kind 0x18's sub-kinds 0x33,
// 0x34, 0x35, 0x37, 0x38, 0x3B, 0x3D, 0x40, 0x41, 0x44 and 0x54 - their
// sub-state dispatchers, the states and the draws they share. Every function
// runs with Sprite_Current an Effect_Objects record (Effect_RunObjects,
// EffectKind18_Run) or is a cdecl draw helper of one. docs/effect_6b.md. The
// functions are declared by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect6B_Inject();

namespace effect_6b {
// BOF3X_SHADOW=effect_6b: the start-up fuzz, effect_6b_fuzz.cpp. Clones every
// original before Effect6B_Inject patches it.
void SelfTest();
}  // namespace effect_6b
