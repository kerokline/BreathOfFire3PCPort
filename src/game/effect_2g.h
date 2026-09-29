// Round thirteen group E2G (0x47DBE0..0x47FD92): the dispatchers of effect
// kinds 0x15, 0x54, 0x55, 0x57, 0x5A, 0x5B, 0x5D, 0x5E, 0x5F and 0x66 and of
// kind 0x18's sub-kind 0x20, and the states and draws of all but 0x5D..0x5F.
// Every function runs with Sprite_Current an Effect_Objects record
// (Effect_RunObjects) or is a cdecl draw helper of one. docs/effect_2g.md. The
// functions are declared by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect2G_Inject();

namespace effect_2g {
// BOF3X_SHADOW=effect_2g: the start-up fuzz, effect_2g_fuzz.cpp. Clones every
// original before Effect2G_Inject patches it.
void SelfTest();
}  // namespace effect_2g
