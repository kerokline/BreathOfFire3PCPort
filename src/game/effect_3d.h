// Round thirteen group E3D (0x485CB0..0x48801A): effect kinds 0x76..0x7D and
// 0x7F..0x82 - their dispatchers, states and draw helpers - and the map set-up
// of area 170's dials. Every state runs with Sprite_Current an Effect_Objects
// record (Effect_RunObjects) or is a cdecl helper of one. docs/effect_3d.md.
// The functions are declared by symbols.gen.h (each has an `impl` in
// symbols.toml).
#pragma once

void Effect3D_Inject();

namespace effect_3d {
// BOF3X_SHADOW=effect_3d: the start-up fuzz, effect_3d_fuzz.cpp. Clones every
// original before Effect3D_Inject patches it.
void SelfTest();
}  // namespace effect_3d
