// Round thirteen group E4B (0x489030..0x48B1FC): effect kinds 0x88, 0x89,
// 0x8A, 0x8B, 0x8C, 0x9D, 0x9F and 0xA4 - their dispatchers, states and draws -
// and the five helpers E4A's kind 0x87 calls. Every function runs with
// Sprite_Current an Effect_Objects record (Effect_RunObjects) or is a cdecl
// helper of one. docs/effect_4b.md. The functions are declared by
// symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect4B_Inject();

namespace effect_4b {
// BOF3X_SHADOW=effect_4b: the start-up fuzz, effect_4b_fuzz.cpp. Clones every
// original before Effect4B_Inject patches it.
void SelfTest();
}  // namespace effect_4b
