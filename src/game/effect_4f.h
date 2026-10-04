// Round thirteen group E4F (0x491D70..0x494026): effect kinds 0xAA..0xB1, 0xB9
// and 0xBA - the dispatchers, the states and helpers the cut gives the group,
// and the three unplaced rows of its band (0x492530, 0x492750, 0x492CF0). Every
// function runs with Sprite_Current an Effect_Objects record (Effect_RunObjects)
// or is a cdecl helper of one. docs/effect_4f.md. The functions are declared by
// symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect4F_Inject();

namespace effect_4f {
// BOF3X_SHADOW=effect_4f: the start-up fuzz, effect_4f_fuzz.cpp. Clones every
// original before Effect4F_Inject patches it.
void SelfTest();
}  // namespace effect_4f
