// Round thirteen group E3C (0x484050..0x485CA0): effect kinds 0x6D, 0x6E,
// 0x6F, 0x72, 0x73, 0x74 and 0x75 - their dispatchers, states and draws - and
// the debris draw and set-up kinds 0x1E, 0x1F and 0x4B share. Every function
// runs with Sprite_Current an Effect_Objects record (Effect_RunObjects) or is a
// cdecl helper of one. docs/effect_3c.md. The functions are declared by
// symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect3C_Inject();

namespace effect_3c {
// BOF3X_SHADOW=effect_3c: the start-up fuzz, effect_3c_fuzz.cpp. Clones every
// original before Effect3C_Inject patches it.
void SelfTest();
}  // namespace effect_3c
