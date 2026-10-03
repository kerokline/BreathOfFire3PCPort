// Round thirteen group E4C (0x48B200..0x48C985): effect kinds 0x8D, 0x8E,
// 0x8F, 0x90, 0x93 and 0x99 - their dispatchers, states, pools and draws.
// Every function runs with Sprite_Current an Effect_Objects record
// (Effect_RunObjects) or is a cdecl helper of one. docs/effect_4c.md. The
// functions are declared by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect4C_Inject();

namespace effect_4c {
// BOF3X_SHADOW=effect_4c: the start-up fuzz, effect_4c_fuzz.cpp. Clones every
// original before Effect4C_Inject patches it.
void SelfTest();
}  // namespace effect_4c
